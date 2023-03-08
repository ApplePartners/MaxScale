/*
 * Copyright (c) 2023 MariaDB plc
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2027-02-21
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include "pgbackendconnection.hh"

namespace
{
GWBUF create_ssl_request()
{
    std::array<uint8_t, 8> buf{};
    pg::set_uint32(buf.data(), 8);
    pg::set_uint32(buf.data() + 4, pg::SSLREQ_MAGIC);
    return GWBUF(buf.data(), buf.size());
}

GWBUF create_startup_message(const uint8_t* params, size_t size)
{
    // The parameters should be null-terminated
    mxb_assert(params[size - 1] == 0x0);

    GWBUF rval(8 + size);
    uint8_t* ptr = rval.data();

    ptr += pg::set_uint32(ptr, 8 + size);
    ptr += pg::set_uint32(ptr, pg::PROTOCOL_V3_MAGIC);
    memcpy(ptr, params, size);

    return rval;
}

GWBUF create_terminate()
{
    std::array<uint8_t, 5> buf{};
    buf[0] = 'X';
    pg::set_uint32(buf.data() + 1, 4);
    return GWBUF(buf.data(), buf.size());
}
}

PgBackendConnection::PgBackendConnection(MXS_SESSION* session, SERVER* server, mxs::Component* component)
    : m_session(session)
    , m_upstream(component)
{
}

void PgBackendConnection::ready_for_reading(DCB* dcb)
{
    bool keep_going = true;

    while (keep_going)
    {
        switch (m_state)
        {
        case State::SSL_REQUEST:
            keep_going = handle_ssl_request();
            break;

        case State::SSL_HANDSHAKE:
            keep_going = handle_ssl_handshake();
            break;

        case State::AUTH:
            keep_going = handle_auth();
            break;

        case State::STARTUP:
            keep_going = handle_startup();
            break;

        case State::BACKLOG:
            keep_going = handle_backlog();
            break;

        case State::ROUTING:
            keep_going = handle_routing();
            break;

        case State::FAILED:
            keep_going = false;
            break;

        case State::INIT:
            mxb_assert_message(!true, "We should not end up here");
            handle_error("Internal error");
            keep_going = false;
            break;
        }
    }
}

void PgBackendConnection::write_ready(DCB* dcb)
{
    m_dcb->writeq_drain();
}

void PgBackendConnection::error(DCB* dcb)
{
    m_upstream->handleError(mxs::ErrorType::TRANSIENT, "Error", nullptr, m_reply);
}

void PgBackendConnection::hangup(DCB* dcb)
{
    m_upstream->handleError(mxs::ErrorType::TRANSIENT, "Hangup", nullptr, m_reply);
}

bool PgBackendConnection::write(GWBUF&& buffer)
{
    if (m_state != State::ROUTING)
    {
        MXB_INFO("Delaying routing of '%c'", (char)buffer[0]);
        m_backlog.emplace_back(std::move(buffer));
        return true;
    }

    if (m_reply.is_complete())
    {
        // The connection is idle, start tracking the result state
        m_reply.clear();
        m_reply.set_reply_state(mxs::ReplyState::START);
        m_reply.set_command(buffer[0]);
    }
    else
    {
        // Something else is already going on, push the command byte so that we can start tracking it once the
        // current command completes.
        m_track_queue.push_back(buffer[0]);
    }

    return m_dcb->writeq_append(std::move(buffer));
}

void PgBackendConnection::finish_connection()
{
    m_dcb->writeq_append(create_terminate());
}

uint64_t PgBackendConnection::can_reuse(MXS_SESSION* session) const
{
    return false;
}

bool PgBackendConnection::reuse(MXS_SESSION* session, mxs::Component* component, uint64_t reuse_type)
{
    m_session = session;
    m_upstream = component;
    return true;
}

bool PgBackendConnection::established()
{
    return true;
}

void PgBackendConnection::set_to_pooled()
{
    m_session = nullptr;
    m_upstream = nullptr;
}

void PgBackendConnection::ping()
{
    // TODO: Figure out what's a good ping mechanism
}

bool PgBackendConnection::can_close() const
{
    return true;
}

void PgBackendConnection::set_dcb(DCB* dcb)
{
    m_dcb = static_cast<BackendDCB*>(dcb);

    if (m_state == State::INIT)
    {
        // In the Postgres protocol, the client starts by sending a message
        if (m_dcb->using_ssl())
        {
            // If the server is configured to use TLS, send a SSLRequest message to see if the server has been
            // configured with TLS.
            send_ssl_request();
        }
        else
        {
            // If TLS is not configured, skip it and send the StartupMessage immediately
            send_startup_message();
        }
    }
}

const BackendDCB* PgBackendConnection::dcb() const
{
    return m_dcb;
}

BackendDCB* PgBackendConnection::dcb()
{
    return m_dcb;
}

mxs::Component* PgBackendConnection::upstream() const
{
    return m_upstream;
}

json_t* PgBackendConnection::diagnostics() const
{
    return nullptr;
}

size_t PgBackendConnection::sizeof_buffers() const
{
    return 0;
}

void PgBackendConnection::handle_error(const std::string& error, mxs::ErrorType type)
{
    m_upstream->handleError(type, error, nullptr, m_reply);
    m_state = State::FAILED;
}

void PgBackendConnection::send_ssl_request()
{
    if (m_dcb->writeq_append(create_ssl_request()))
    {
        m_state = State::SSL_REQUEST;
    }
    else
    {
        handle_error("Failed to write SSL request");
    }
}

void PgBackendConnection::send_startup_message()
{
    // TODO: Copy these from the client. This'll only work if there's role named "maxuser".
    const char params[] = "user\0maxuser\0";

    // The parameters are a list of null-terminated strings that end with an empty string
    if (m_dcb->writeq_append(create_startup_message((uint8_t*)params, sizeof(params))))
    {
        m_state = State::AUTH;
    }
    else
    {
        handle_error("Failed to write startup message");
    }
}

bool PgBackendConnection::handle_ssl_request()
{
    if (auto [ok, buf] = m_dcb->read_strict(1, 1); ok)
    {
        mxb_assert_message(buf, "There should always be data available");
        uint8_t response = buf[0];

        if (response == pg::SSLREQ_NO)
        {
            // No SSL, send the normal startup message.
            send_startup_message();
        }
        else if (response == pg::SSLREQ_YES)
        {
            // SSL requested, start the TLS handshake.
            if (m_dcb->ssl_handshake() == -1)
            {
                handle_error("TLS handshake failed");
            }
            else
            {
                m_state = State::SSL_HANDSHAKE;
            }
        }
        else
        {
            handle_error("Unknown response to SSL request");
        }
    }
    else
    {
        handle_error("Network read failed");
    }

    return m_state != State::FAILED;
}

bool PgBackendConnection::handle_ssl_handshake()
{
    bool keep_going = false;

    switch (m_dcb->ssl_state())
    {
    case DCB::SSLState::ESTABLISHED:
        send_startup_message();
        keep_going = true;
        break;

    case DCB::SSLState::HANDSHAKE_REQUIRED:
        // Handshake is still going on, wait for more data.
        break;

    default:
        handle_error("SSL handshake failed");
        break;
    }

    return keep_going;
}

bool PgBackendConnection::handle_startup()
{
    if (auto [ok, buf] = pg::read_packet(m_dcb); ok)
    {
        if (!buf)
        {
            // Partial read, try again later
            return false;
        }

        uint8_t command = buf[0];

        switch (command)
        {
        case pg::AUTHENTICATION:
            {
                auto auth_method = pg::get_uint32(buf.data() + pg::HEADER_LEN);
                handle_error(mxb::cat("Unexpected authentication message: ", std::to_string(auth_method)));
            }
            break;

        case pg::BACKEND_KEY_DATA:
            // Stash the process ID and the key, we'll need it to kill this connection
            m_process_id = pg::get_uint32(buf.data() + pg::HEADER_LEN);
            m_secret_key = pg::get_uint32(buf.data() + pg::HEADER_LEN + 4);
            break;

        case pg::PARAMETER_STATUS:
            // Server parameters, ignore these for now
            break;

        case pg::NOTICE_RESPONSE:
            // Notification of some sorts, ignore it
            MXB_INFO("Server notification: %s", pg::format_response(buf).c_str());
            break;

        case pg::READY_FOR_QUERY:
            // Authentication is successful.
            // TODO: Track the transaction status from this packet
            m_state = State::BACKLOG;
            break;

        case pg::ERROR_RESPONSE:
            handle_error("Authentication failed: " + pg::format_response(buf), mxs::ErrorType::PERMANENT);
            break;
        }
    }
    else
    {
        handle_error("Network read failed");
    }

    return true;
}

bool PgBackendConnection::handle_auth()
{
    if (auto [ok, buf] = pg::read_packet(m_dcb); ok)
    {
        if (!buf)
        {
            // Partial read, try again later
            return false;
        }

        uint8_t command = buf[0];

        switch (command)
        {
        case pg::AUTHENTICATION:
            {
                auto auth_method = pg::get_uint32(buf.data() + pg::HEADER_LEN);

                if (auth_method == pg::AUTH_OK)
                {
                    m_state = State::STARTUP;
                }
                else
                {
                    handle_error(mxb::cat("Unsupported authentication mechanism: ",
                                          std::to_string(auth_method)));
                }
            }
            break;

        case pg::ERROR_RESPONSE:
            handle_error("Authentication failed: " + pg::format_response(buf), mxs::ErrorType::PERMANENT);
            break;

        default:
            handle_error("Unknown command: " + std::to_string(command), mxs::ErrorType::PERMANENT);
            break;
        }
    }
    else
    {
        handle_error("Network read failed");
    }

    return true;
}

bool PgBackendConnection::handle_backlog()
{
    m_state = State::ROUTING;
    auto packets = std::move(m_backlog);
    m_backlog.clear();

    for (auto& p : packets)
    {
        if (!write(std::move(p)))
        {
            handle_error("Failed to process delayed packets");
            break;
        }
    }

    return true;
}

bool PgBackendConnection::handle_routing()
{
    bool keep_going = false;

    if (auto [ok, buf] = m_dcb->read(pg::HEADER_LEN, 0); ok)
    {
        if (buf)
        {
            if (GWBUF complete_packets = process_packets(buf))
            {
                mxs::ReplyRoute down;
                m_upstream->clientReply(std::move(complete_packets), down, m_reply);

                if (m_reply.is_complete() && !m_track_queue.empty())
                {
                    // Another command was executed, try to route a response again
                    m_reply.set_reply_state(mxs::ReplyState::START);
                    m_reply.set_command(m_track_queue.front());
                    m_track_queue.pop_front();
                    keep_going = true;
                }
            }

            if (buf)
            {
                // Leftover data, either partial packets or a part of another result. Push it back into the
                // DCB and read it on the next loop. If another result is expected, the check done after
                // clientReply will cause this function to be called again.
                m_dcb->unread(std::move(buf));
            }
        }
        else
        {
            // Not even the packet header could be read
        }
    }
    else
    {
        handle_error("Network read failed");
    }

    return keep_going;
}

GWBUF PgBackendConnection::process_packets(GWBUF& buffer)
{
    mxb_assert(!m_reply.is_complete());
    size_t size = 0;
    auto it = buffer.begin();

    while (it < buffer.end() && !m_reply.is_complete())
    {
        uint8_t command = *it;
        uint32_t len = pg::get_uint32(it + 1);

        switch (command)
        {
        case pg::ERROR_RESPONSE:
            {
                auto values = pg::extract_response_fields(it, len + 1);
                std::string_view sqlstate = values['C'];
                std::string_view errmsg = values['M'];
                m_reply.set_error(0, sqlstate.begin(), sqlstate.end(), errmsg.begin(), errmsg.end());
            }
            break;

        case pg::READY_FOR_QUERY:
            // Result complete, the next result will be delivered in a separate clientReply call.
            m_reply.set_reply_state(mxs::ReplyState::DONE);
            break;

        case pg::DATA_ROW:
            m_reply.set_reply_state(mxs::ReplyState::RSET_ROWS);
            m_reply.add_rows(1);
            break;

        case pg::ROW_DESCRIPTION:
            m_reply.set_reply_state(mxs::ReplyState::RSET_COLDEF);
            break;
        }

        size += len + 1;
        it += len + 1;
    }

    mxb_assert(size <= buffer.length());
    return buffer.split(size);
}
