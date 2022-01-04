/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-01-04
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

/**
 * @file avro.c - Avro router, allows MaxScale to act as an intermediary for
 * MySQL replication binlog files and AVRO binary files
 */

#include "avrorouter.hh"

#include <ctype.h>
#include <ini.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <glob.h>
#include <ini.h>
#include <avro/errors.h>
#include <maxbase/atomic.h>
#include <maxbase/worker.hh>
#include <maxbase/alloc.h>
#include <maxscale/dcb.hh>
#include <maxscale/modulecmd.hh>
#include <maxscale/paths.h>
#include <maxscale/protocol/mysql.hh>
#include <maxscale/random.h>
#include <maxscale/router.hh>
#include <maxscale/server.hh>
#include <maxscale/service.hh>
#include <maxscale/utils.hh>
#include <maxscale/pcre2.hh>
#include <maxscale/routingworker.hh>
#include <binlog_common.hh>

#include "avro_converter.hh"

using namespace maxscale;

/**
 * @brief Read router options from an external binlogrouter service
 *
 * This reads common options used by both the avrorouter and the binlogrouter
 * from a service that uses the binlogrouter. This way the basic configuration
 * details can be read from another service without the need to configure the
 * avrorouter with identical router options.
 *
 * @param inst Avro router instance
 * @param options The @c router_options of a binlogrouter instance
 */
void Avro::read_source_service_options(SERVICE* source)
{
    MXS_CONFIG_PARAMETER& params = source->svc_config_param;
    binlogdir = params.get_string("binlogdir");
    filestem = params.get_string("filestem");
    mxb_assert(!binlogdir.empty() && !filestem.empty());

    for (const auto& opt : mxs::strtok(params.get_string("router_options"), ", \t"))
    {
        auto kv = mxs::strtok(opt, "=");

        if (kv[0] == "binlogdir")
        {
            binlogdir = kv[1];
        }
        else if (kv[0] == "filestem")
        {
            filestem = kv[1];
        }
    }
}

// static
Avro* Avro::create(SERVICE* service, SRowEventHandler handler)
{
    SERVICE* source_service = NULL;
    std::string source_name = service->svc_config_param.get_string("source");

    if (!source_name.empty())
    {
        SERVICE* source = service_find(source_name.c_str());
        mxb_assert(source);

        if (source)
        {
            if (strcmp(source->router_name(), "binlogrouter") == 0)
            {
                MXS_INFO("Using configuration options from service '%s'.", source->name());
                source_service = source;
            }
            else
            {
                MXS_ERROR("Service '%s' uses router module '%s' instead of "
                          "'binlogrouter'.",
                          source->name(),
                          source->router_name());
                return NULL;
            }
        }
        else
        {
            MXS_ERROR("Service '%s' not found.", source_name.c_str());
            return NULL;
        }
    }

    return new(std::nothrow) Avro(service, &service->svc_config_param, source_service, handler);
}

Avro::Avro(SERVICE* service, MXS_CONFIG_PARAMETER* params, SERVICE* source, SRowEventHandler handler)
    : service(service)
    , filestem(params->get_string("filestem"))
    , binlogdir(params->get_string("binlogdir"))
    , avrodir(params->get_string("avrodir"))
    , current_pos(4)
    , binlog_fd(-1)
    , trx_count(0)
    , trx_target(params->get_integer("group_trx"))
    , row_count(0)
    , row_target(params->get_integer("group_rows"))
    , task_handle(0)
    , handler(service, handler, params->get_compiled_regex("match", 0, NULL).release(),
              params->get_compiled_regex("exclude", 0, NULL).release())
{
    if (params->contains(CN_SERVERS) || params->contains(CN_CLUSTER))
    {
        MXS_NOTICE("Replicating directly from a master server");
        cdc::Config cnf;
        cnf.service = service;
        cnf.statedir = avrodir;
        cnf.server_id = params->get_integer("server_id");
        cnf.gtid = params->get_string("gtid_start_pos");

        auto worker = mxs::RoutingWorker::get(mxs::RoutingWorker::MAIN);
        worker->execute([this, cnf]() {
                            m_replicator = cdc::Replicator::start(cnf, &this->handler);
                            mxb_assert(m_replicator);
                        }, mxs::RoutingWorker::EXECUTE_QUEUED);
    }
    else
    {
        if (source)
        {
            read_source_service_options(source);
        }

        char filename[BINLOG_FNAMELEN + 1];
        snprintf(filename,
                 sizeof(filename),
                 BINLOG_NAMEFMT,
                 filestem.c_str(),
                 static_cast<int>(params->get_integer("start_index")));
        binlog_name = filename;

        MXS_NOTICE("Reading MySQL binlog files from %s", binlogdir.c_str());
        MXS_NOTICE("First binlog is: %s", binlog_name.c_str());
    }

    MXS_NOTICE("Avro files stored at: %s", avrodir.c_str());

    // TODO: Do these in Avro::create
    avro_load_conversion_state(this);
    avro_load_metadata_from_schemas(this);
}
