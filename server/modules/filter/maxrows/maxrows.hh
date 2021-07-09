/*
 * Copyright (c) 2018 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2025-07-14
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#pragma once

#define MXS_MODULE_NAME "maxrows"

#include <maxscale/ccdefs.hh>
#include <maxscale/config2.hh>
#include <maxscale/filter.hh>


class MaxRowsConfig : public mxs::config::Configuration
{
public:
    enum Mode
    {
        EMPTY, ERR, OK
    };

    MaxRowsConfig(const char* zName);

    MaxRowsConfig(MaxRowsConfig&& rhs) = default;

    int64_t max_rows;
    int64_t max_size;
    int64_t debug;
    Mode    mode;
};


class MaxRows;

class MaxRowsSession : public maxscale::FilterSession
{
public:
    MaxRowsSession(const MaxRowsSession&) = delete;
    MaxRowsSession& operator=(const MaxRowsSession&) = delete;

    // Create a new filter session
    static MaxRowsSession* create(MXS_SESSION* pSession, SERVICE* pService, MaxRows* pFilter)
    {
        return new(std::nothrow) MaxRowsSession(pSession, pService, pFilter);
    }

    bool routeQuery(GWBUF* pPacket) override;

    bool clientReply(GWBUF* pPacket, const mxs::ReplyRoute& down, const mxs::Reply& reply) override;

private:
    // Used in the create function
    MaxRowsSession(MXS_SESSION* pSession, SERVICE* pService, MaxRows* pFilter)
        : FilterSession(pSession, pService)
        , m_instance(pFilter)
    {
    }

    MaxRows*    m_instance;
    mxs::Buffer m_buffer;   // Contains the partial resultset
    bool        m_collect {true};
};

class MaxRows : public mxs::Filter
{
public:
    MaxRows(const MaxRows&) = delete;
    MaxRows& operator=(const MaxRows&) = delete;

    using Config = MaxRowsConfig;

    static constexpr uint64_t CAPABILITIES = RCAP_TYPE_REQUEST_TRACKING;

    // Creates a new filter instance
    static MaxRows* create(const char* name);

    // Creates a new session for this filter
    MaxRowsSession* newSession(MXS_SESSION* session, SERVICE* service) override
    {
        return MaxRowsSession::create(session, service, this);
    }

    // Returns JSON form diagnostic data
    json_t* diagnostics() const override
    {
        return nullptr;
    }

    // Get filter capabilities
    uint64_t getCapabilities() const override
    {
        return CAPABILITIES;
    }

    mxs::config::Configuration& getConfiguration() override
    {
        return m_config;
    }

    // Return reference to filter config
    const Config& config() const
    {
        return m_config;
    }

private:
    MaxRows(const char* name)
        : m_name(name)
        , m_config(name)
    {
    }

private:
    std::string m_name;
    Config      m_config;
};
