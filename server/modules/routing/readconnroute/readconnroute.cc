/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2025-10-29
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

/**
 * @file readconnroute.c - Read Connection Load Balancing Query Router
 *
 * This is the implementation of a simple query router that balances
 * read connections. It assumes the service is configured with a set
 * of slaves and that the application clients already split read and write
 * queries. It offers a service to balance the client read connections
 * over this set of slave servers. It does this once only, at the time
 * the connection is made. It chooses the server that currently has the least
 * number of connections by keeping a count for each server of how
 * many connections the query router has made to the server.
 *
 * When two servers have the same number of current connections the one with
 * the least number of connections since startup will be used.
 *
 * The router may also have options associated to it that will limit the
 * choice of backend server. Currently two options are supported, the "master"
 * option will cause the router to only connect to servers marked as masters
 * and the "slave" option will limit connections to routers that are marked
 * as slaves. If neither option is specified the router will connect to either
 * masters or slaves.
 */

#include "readconnroute.hh"

#include <maxscale/protocol/mysql.hh>
#include <maxscale/modutil.hh>

/**
 * The module entry point routine. It is this routine that
 * must populate the structure that is referred to as the
 * "module object", this is a structure with the set of
 * external entry points for this module.
 *
 * @return The module object
 */
extern "C" MXS_MODULE* MXS_CREATE_MODULE()
{
    MXS_NOTICE("Initialise readconnroute router module.");

    static MXS_MODULE info =
    {
        MXS_MODULE_API_ROUTER,
        MXS_MODULE_GA,
        MXS_ROUTER_VERSION,
        "A connection based router to load balance based on connections",
        "V2.0.0",
        RCAP_TYPE_RUNTIME_CONFIG,
        &RCR::s_object,
        nullptr,    /* Process init. */
        nullptr,    /* Process finish. */
        nullptr,    /* Thread init. */
        nullptr,    /* Thread finish. */
        {
            {MXS_END_MODULE_PARAMS}
        }
    };

    return &info;
}

/*
 * This routine returns the master server from a MariaDB replication tree. The server must be
 * running, not in maintenance and have the master bit set. If multiple masters are found,
 * the one with the highest weight is chosen.
 *
 * @param servers The list of servers
 *
 * @return The Master server
 */
SERVER_REF* RCR::get_root_master()
{
    auto best_rank = std::numeric_limits<int64_t>::max();
    SERVER_REF* master_host = nullptr;

    for (SERVER_REF* ref = m_pService->dbref; ref; ref = ref->next)
    {
        if (server_ref_is_active(ref) && ref->server->is_master())
        {
            auto rank = ref->server->rank();

            if (!master_host)
            {
                // No master found yet
                master_host = ref;
            }
            else if (rank < best_rank
                     || (rank == best_rank && ref->server_weight > master_host->server_weight))
            {
                best_rank = rank;
                master_host = ref;
            }
        }
    }

    return master_host;
}

bool RCR::configure(MXS_CONFIG_PARAMETER* params)
{
    uint64_t bitmask = 0;
    uint64_t bitvalue = 0;
    bool ok = true;

    for (const auto& opt : mxs::strtok(params->get_string("router_options"), ", \t"))
    {
        if (!strcasecmp(opt.c_str(), "master"))
        {
            bitmask |= (SERVER_MASTER | SERVER_SLAVE);
            bitvalue |= SERVER_MASTER;
        }
        else if (!strcasecmp(opt.c_str(), "slave"))
        {
            bitmask |= (SERVER_MASTER | SERVER_SLAVE);
            bitvalue |= SERVER_SLAVE;
        }
        else if (!strcasecmp(opt.c_str(), "running"))
        {
            bitmask |= (SERVER_RUNNING);
            bitvalue |= SERVER_RUNNING;
        }
        else if (!strcasecmp(opt.c_str(), "synced"))
        {
            bitmask |= (SERVER_JOINED);
            bitvalue |= SERVER_JOINED;
        }
        else
        {
            MXS_ERROR("Unsupported router option \'%s\' for readconnroute. "
                      "Expected router options are [slave|master|synced|running]",
                      opt.c_str());
            ok = false;
        }
    }


    if (bitmask == 0 && bitvalue == 0)
    {
        /** No parameters given, use RUNNING as a valid server */
        bitmask |= (SERVER_RUNNING);
        bitvalue |= SERVER_RUNNING;
    }

    if (ok)
    {
        uint64_t mask = bitmask | (bitvalue << 32);
        atomic_store_uint64(&m_bitmask_and_bitvalue, mask);
    }

    return ok;
}


RCR::RCR(SERVICE* service)
    : mxs::Router<RCR, RCRSession>(service)
{
}

// static
RCR* RCR::create(SERVICE* service, MXS_CONFIG_PARAMETER* params)
{
    RCR* inst = new(std::nothrow) RCR(service);

    if (inst && !inst->configure(params))
    {
        delete inst;
        inst = nullptr;
    }

    return inst;
}

RCRSession::RCRSession(RCR* inst, MXS_SESSION* session, SERVER_REF* backend, DCB* dcb,
                       uint32_t bitmask, uint32_t bitvalue)
    : mxs::RouterSession(session)
    , m_instance(inst)
    , m_backend(backend)
    , m_dcb(dcb)
    , m_client_dcb(session->client_dcb)
    , m_bitmask(bitmask)
    , m_bitvalue(bitvalue)
{
}

RCRSession::~RCRSession()
{
    mxb::atomic::add(&m_backend->connections, -1, mxb::atomic::RELAXED);
}

void RCRSession::close()
{
    mxb_assert(m_dcb);
    dcb_close(m_dcb);
}

RCRSession* RCR::newSession(MXS_SESSION* session)
{
    uint64_t mask = atomic_load_uint64(&m_bitmask_and_bitvalue);
    uint32_t bitmask = mask;
    uint32_t bitvalue = mask >> 32;

    /**
     * Find the Master host from available servers
     */
    SERVER_REF* master_host = get_root_master();

    bool connectable_master = master_host ? master_host->server->is_connectable() : false;

    /**
     * Find a backend server to connect to. This is the extent of the
     * load balancing algorithm we need to implement for this simple
     * connection router.
     */
    SERVER_REF* candidate = nullptr;
    auto best_rank = std::numeric_limits<int64_t>::max();

    /*
     * Loop over all the servers and find any that have fewer connections
     * than the candidate server.
     *
     * If a server has less connections than the current candidate we mark this
     * as the new candidate to connect to.
     *
     * If a server has the same number of connections currently as the candidate
     * and has had less connections over time than the candidate it will also
     * become the new candidate. This has the effect of spreading the
     * connections over different servers during periods of very low load.
     */
    for (SERVER_REF* ref = m_pService->dbref; ref; ref = ref->next)
    {
        if (!server_ref_is_active(ref) || !ref->server->is_connectable())
        {
            continue;
        }

        mxb_assert(ref->server->is_usable());

        /* Check server status bits against bitvalue from router_options */
        if (ref && (ref->server->status & bitmask & bitvalue))
        {
            if (master_host && connectable_master)
            {
                if (ref == master_host
                    && (bitvalue & (SERVER_SLAVE | SERVER_MASTER)) == SERVER_SLAVE)
                {
                    /* Skip root master here, as it could also be slave of an external server that
                     * is not in the configuration.  Intermediate masters (Relay Servers) are also
                     * slave and will be selected as Slave(s)
                     */

                    continue;
                }
                if (ref == master_host && bitvalue == SERVER_MASTER)
                {
                    /* If option is "master" return only the root Master as there could be
                     * intermediate masters (Relay Servers) and they must not be selected.
                     */

                    candidate = master_host;
                    break;
                }
            }
            else if (bitvalue == SERVER_MASTER)
            {
                /* Master_host is nullptr, no master server.  If requested router_option is 'master'
                 * candidate will be nullptr.
                 */
                candidate = nullptr;
                break;
            }

            /* If no candidate set, set first running server as our initial candidate server */
            if (!candidate || ref->server->rank() < best_rank)
            {
                best_rank = ref->server->rank();
                candidate = ref;
            }
            else if (ref->server->rank() == best_rank)
            {
                if (ref->server_weight == 0 || candidate->server_weight == 0)
                {
                    if (ref->server_weight)     // anything with a weight is better
                    {
                        candidate = ref;
                    }
                }
                else if ((ref->connections + 1) / ref->server_weight
                         < (candidate->connections + 1) / candidate->server_weight)
                {
                    /* ref has a better score. */
                    candidate = ref;
                }
            }
        }
    }

    /* If we haven't found a proper candidate yet but a master server is available, we'll pick that
     * with the assumption that it is "better" than a slave.
     */
    if (!candidate)
    {
        if (master_host && connectable_master)
        {
            candidate = master_host;
            // Even if we had 'router_options=slave' in the configuration file, we
            // will still end up here if there are no slaves, but a sole master. So
            // that the server will be considered valid in connection_is_valid(), we
            // turn on the SERVER_MASTER bit.
            //
            // We must do that so that readconnroute in MaxScale 2.2 will again behave
            // the same way as it did up until 2.1.12.
            if (bitvalue & SERVER_SLAVE)
            {
                bitvalue |= SERVER_MASTER;
            }
        }
        else
        {
            if (!master_host)
            {
                MXS_ERROR("Failed to create new routing session. Couldn't find eligible"
                          " candidate server. Freeing allocated resources.");
            }
            else
            {
                mxb_assert(!connectable_master);
                MXS_ERROR("The only possible candidate server (%s) is being drained "
                          "and thus cannot be used.", master_host->server->address);
            }
            return nullptr;
        }
    }
    else
    {
        mxb_assert(candidate->server->is_connectable());
    }

    /** Open the backend connection */
    DCB* backend_dcb = dcb_connect(candidate->server, session, candidate->server->protocol().c_str());

    if (!backend_dcb)
    {
        /** The failure is reported in dcb_connect() */
        return nullptr;
    }

    RCRSession* client_rses = new(std::nothrow) RCRSession(this, session, candidate, backend_dcb,
                                                           bitmask, bitvalue);

    if (!client_rses)
    {
        return nullptr;
    }

    mxb::atomic::add(&candidate->connections, 1, mxb::atomic::RELAXED);

    m_stats.n_sessions++;

    MXS_INFO("New session for server %s. Connections : %d",
             candidate->server->name(),
             candidate->connections);

    return client_rses;
}

/** Log routing failure due to closed session */
static void log_closed_session(mxs_mysql_cmd_t mysql_command, SERVER_REF* ref)
{
    char msg[SERVER::MAX_ADDRESS_LEN + 200] = "";   // Extra space for message

    if (ref->server->is_down())
    {
        sprintf(msg, "Server '%s' is down.", ref->server->name());
    }
    else if (ref->server->is_in_maint())
    {
        sprintf(msg, "Server '%s' is in maintenance.", ref->server->name());
    }
    else
    {
        sprintf(msg, "Server '%s' no longer qualifies as a target server.", ref->server->name());
    }

    MXS_ERROR("Failed to route MySQL command %d to backend server. %s", mysql_command, msg);
}

/**
 * Check if the server we're connected to is still valid
 *
 * @param inst           Router instance
 * @param router_cli_ses Router session
 *
 * @return True if the backend connection is still valid
 */
bool RCRSession::connection_is_valid() const
{
    bool rval = false;

    // m_instance->bitvalue and m_bitvalue are different, if we had
    // 'router_options=slave' in the configuration file and there was only
    // the sole master available at session creation time.

    if (m_backend->server->is_usable() && (m_backend->server->status & m_bitmask & m_bitvalue))
    {
        // Note the use of '==' and not '|'. We must use the former to exclude a
        // 'router_options=slave' that uses the master due to no slave having been
        // available at session creation time. Its bitvalue is (SERVER_MASTER | SERVER_SLAVE).
        if (m_bitvalue == SERVER_MASTER && m_backend->active)
        {
            // If we're using an active master server, verify that it is still a master
            rval = m_backend == m_instance->get_root_master();
        }
        else
        {
            /**
             * Either we don't use master type servers or the server reference
             * is deactivated. We let deactivated connection close gracefully
             * so we simply assume it is OK. This allows a server to be taken
             * out of use in a manner that won't cause errors to the connected
             * clients.
             */
            rval = true;
        }
    }

    return rval;
}

int RCRSession::routeQuery(GWBUF* queue)
{
    int rc = 0;
    MySQLProtocol* proto = static_cast<MySQLProtocol*>(m_client_dcb->protocol);
    mxs_mysql_cmd_t mysql_command = proto->current_command;

    mxb::atomic::add(&m_instance->stats().n_queries, 1, mxb::atomic::RELAXED);

    // Due to the streaming nature of readconnroute, this is not accurate
    mxb::atomic::add(&m_backend->server->stats.packets, 1, mxb::atomic::RELAXED);

    DCB* backend_dcb = m_dcb;
    mxb_assert(backend_dcb);
    char* trc = nullptr;

    if (!connection_is_valid())
    {
        log_closed_session(mysql_command, m_backend);
        gwbuf_free(queue);
        return rc;
    }

    switch (mysql_command)
    {
    case MXS_COM_CHANGE_USER:
        rc = backend_dcb->func.auth(backend_dcb,
                                    nullptr,
                                    backend_dcb->session,
                                    queue);
        break;

    case MXS_COM_QUERY:
        if (mxs_log_is_priority_enabled(LOG_INFO))
        {
            trc = modutil_get_SQL(queue);
        }

    default:
        rc = backend_dcb->func.write(backend_dcb, queue);
        break;
    }

    MXS_INFO("Routed [%s] to '%s'%s%s",
             STRPACKETTYPE(mysql_command),
             backend_dcb->server->name(),
             trc ? ": " : ".",
             trc ? trc : "");
    MXS_FREE(trc);

    return rc;
}

void RCR::diagnostics(DCB* dcb)
{
    const char* weightby = serviceGetWeightingParameter(m_pService);

    dcb_printf(dcb,
               "\tNumber of router sessions:    %lu\n",
               m_stats.n_sessions);
    dcb_printf(dcb,
               "\tCurrent no. of router sessions:	%lu\n",
               m_pService->stats.n_current);
    dcb_printf(dcb,
               "\tNumber of queries forwarded:      %lu\n",
               m_stats.n_queries);
    if (*weightby)
    {
        dcb_printf(dcb,
                   "\tConnection distribution based on %s "
                   "server parameter.\n",
                   weightby);
        dcb_printf(dcb,
                   "\t\tServer               Target %% Connections\n");
        for (SERVER_REF* ref = m_pService->dbref; ref; ref = ref->next)
        {
            dcb_printf(dcb,
                       "\t\t%-20s %3.1f%%     %d\n",
                       ref->server->name(),
                       ref->server_weight * 100,
                       ref->connections);
        }
    }
}

json_t* RCR::diagnostics_json() const
{
    json_t* rval = json_object();

    json_object_set_new(rval, "connections", json_integer(m_stats.n_sessions));
    json_object_set_new(rval, "current_connections", json_integer(m_pService->stats.n_current));
    json_object_set_new(rval, "queries", json_integer(m_stats.n_queries));

    const char* weightby = serviceGetWeightingParameter(m_pService);

    if (*weightby)
    {
        json_object_set_new(rval, "weightby", json_string(weightby));
    }

    return rval;
}

/**
 * Client Reply routine
 *
 * The routine will reply to client data from backend server
 *
 * @param       backend_dcb     The backend DCB
 * @param       queue           The GWBUF with reply data
 */
void RCRSession::clientReply(GWBUF* queue, DCB* backend_dcb)
{
    mxb_assert(backend_dcb->session->client_dcb);
    MXS_SESSION_ROUTE_REPLY(backend_dcb->session, queue);
}

/**
 * Error Handler routine
 *
 * The routine will handle errors that occurred in writes.
 *
 * @param       message         The error message to reply
 * @param       problem_dcb     The DCB related to the error
 * @param       action      The action: ERRACT_NEW_CONNECTION or ERRACT_REPLY_CLIENT
 * @param   succp       Result of action: true if router can continue
 */
void RCRSession::handleError(GWBUF* errbuf, DCB* problem_dcb, mxs_error_action_t action, bool* succp)

{
    mxb_assert(problem_dcb->role == DCB::Role::BACKEND);
    mxb_assert(problem_dcb->session->state == SESSION_STATE_STARTED);
    MXS_INFO("Server '%s' failed", problem_dcb->server->name());

    DCB* client_dcb = problem_dcb->session->client_dcb;
    client_dcb->func.write(client_dcb, gwbuf_clone(errbuf));

    // The DCB will be closed once the session closes, no need to close it here
    *succp = false;
}

uint64_t RCR::getCapabilities()
{
    return RCAP_TYPE_RUNTIME_CONFIG;
}
