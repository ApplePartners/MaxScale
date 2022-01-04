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

#define MXS_MODULE_NAME "maxscaled"

#include <maxscale/ccdefs.hh>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <maxscale/dcb.hh>
#include <maxscale/buffer.hh>
#include <maxscale/protocol.hh>
#include <maxscale/service.hh>
#include <maxscale/session.hh>
#include <sys/ioctl.h>
#include <errno.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <maxscale/router.hh>
#include <maxscale/poll.hh>
#include <maxbase/atomic.h>
#include <maxscale/adminusers.h>
#include <maxscale/modinfo.h>
#include "maxscaled.hh"
#include <maxscale/maxadmin.h>
#include <maxbase/alloc.h>

/**
 * @file maxscaled.c - MaxScale administration protocol
 *
 *
 * @verbatim
 * Revision History
 * Date         Who                     Description
 * 13/06/2014   Mark Riddoch            Initial implementation
 * 07/07/2015   Martin Brampton         Correct failure handling
 * 17/05/2016   Massimiliano Pinto      Check for UNIX socket address
 *
 * @endverbatim
 */

#define GETPWUID_BUF_LEN 255

static int   maxscaled_read_event(DCB* dcb);
static int   maxscaled_write_event(DCB* dcb);
static int   maxscaled_write(DCB* dcb, GWBUF* queue);
static int   maxscaled_error(DCB* dcb);
static int   maxscaled_hangup(DCB* dcb);
static int   maxscaled_accept(DCB*);
static int   maxscaled_close(DCB* dcb);
static char* mxsd_default_auth();

static bool authenticate_unix_socket(MAXSCALED* protocol, DCB* dcb)
{
    bool authenticated = false;

    struct ucred ucred;
    socklen_t len = sizeof(struct ucred);

    /* Get UNIX client credentials from socket*/
    if (getsockopt(dcb->fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len) == 0)
    {
        struct passwd pw_entry;
        struct passwd* pw_tmp;
        char buf[GETPWUID_BUF_LEN];

        /* Fetch username from UID */
        if (getpwuid_r(ucred.uid, &pw_entry, buf, sizeof(buf), &pw_tmp) == 0)
        {
            GWBUF* username;

            /* Set user in protocol */
            protocol->username = strdup(pw_entry.pw_name);

            username = gwbuf_alloc(strlen(protocol->username) + 1);

            strcpy((char*)GWBUF_DATA(username), protocol->username);

            /* Authenticate the user */
            if (dcb->authfunc.extract(dcb, username)
                && dcb->authfunc.authenticate(dcb) == 0)
            {
                dcb_printf(dcb, MAXADMIN_AUTH_SUCCESS_REPLY);
                protocol->state = MAXSCALED_STATE_DATA;
                dcb->user = strdup(protocol->username);
            }
            else
            {
                dcb_printf(dcb, MAXADMIN_AUTH_FAILED_REPLY);
            }

            gwbuf_free(username);

            authenticated = true;
        }
        else
        {
            MXS_ERROR("Failed to get UNIX user %ld details for 'MaxScale Admin'",
                      (unsigned long)ucred.uid);
        }
    }
    else
    {
        MXS_ERROR("Failed to get UNIX domain socket credentials for 'MaxScale Admin'.");
    }

    return authenticated;
}


static bool authenticate_inet_socket(MAXSCALED* protocol, DCB* dcb)
{
    dcb_printf(dcb, MAXADMIN_AUTH_USER_PROMPT);
    return true;
}

static bool authenticate_socket(MAXSCALED* protocol, DCB* dcb)
{
    bool authenticated = false;

    struct sockaddr address;
    socklen_t address_len = sizeof(address);

    if (getsockname(dcb->fd, &address, &address_len) == 0)
    {
        if (address.sa_family == AF_UNIX)
        {
            authenticated = authenticate_unix_socket(protocol, dcb);
        }
        else
        {
            authenticated = authenticate_inet_socket(protocol, dcb);
        }
    }
    else
    {
        MXS_ERROR("Could not get socket family of client connection: %s",
                  mxs_strerror(errno));
    }

    return authenticated;
}

extern "C"
{
/**
 * The module entry point routine. It is this routine that
 * must populate the structure that is referred to as the
 * "module object", this is a structure with the set of
 * external entry points for this module.
 *
 * @return The module object
 */
MXS_MODULE* MXS_CREATE_MODULE()
{
    MXS_INFO("Initialise MaxScaled Protocol module.");

    static MXS_PROTOCOL MyObject =
    {
        maxscaled_read_event,           /**< Read - EPOLLIN handler        */
        maxscaled_write,                /**< Write - data from gateway     */
        maxscaled_write_event,          /**< WriteReady - EPOLLOUT handler */
        maxscaled_error,                /**< Error - EPOLLERR handler      */
        maxscaled_hangup,               /**< HangUp - EPOLLHUP handler     */
        maxscaled_accept,               /**< Accept                        */
        NULL,                           /**< Connect                       */
        maxscaled_close,                /**< Close                         */
        NULL,                           /**< Authentication                */
        mxsd_default_auth,              /**< Default authenticator         */
        NULL,                           /**< Connection limit reached      */
        NULL,
        NULL,
    };

    static MXS_MODULE info =
    {
        MXS_MODULE_API_PROTOCOL,
        MXS_MODULE_GA,
        MXS_PROTOCOL_VERSION,
        "A maxscale protocol for the administration interface",
        "V2.0.0",
        MXS_NO_MODULE_CAPABILITIES,
        &MyObject,
        NULL,       /* Process init. */
        NULL,       /* Process finish. */
        NULL,       /* Thread init. */
        NULL,       /* Thread finish. */
        {
            {MXS_END_MODULE_PARAMS}
        }
    };

    return &info;
}
}
/*lint +e14 */

/**
 * The default authenticator name for this protocol
 *
 * @return name of authenticator
 */
static char* mxsd_default_auth()
{
    return const_cast<char*>("MaxAdminAuth");
}

/**
 * Read event for EPOLLIN on the maxscaled protocol module.
 *
 * @param dcb   The descriptor control block
 * @return
 */
static int maxscaled_read_event(DCB* dcb)
{
    int n;
    GWBUF* head = NULL;
    MAXSCALED* maxscaled = (MAXSCALED*)dcb->protocol;

    if ((n = dcb_read(dcb, &head, 0)) != -1)
    {
        if (head)
        {
            if (GWBUF_LENGTH(head))
            {
                switch (maxscaled->state)
                {
                case MAXSCALED_STATE_LOGIN:
                    {
                        size_t len = GWBUF_LENGTH(head);
                        char user[len + 1];
                        memcpy(user, GWBUF_DATA(head), len);
                        user[len] = '\0';
                        maxscaled->username = MXS_STRDUP_A(user);
                        dcb->user = MXS_STRDUP_A(user);
                        maxscaled->state = MAXSCALED_STATE_PASSWD;
                        dcb_printf(dcb, MAXADMIN_AUTH_PASSWORD_PROMPT);
                        gwbuf_free(head);
                    }
                    break;

                case MAXSCALED_STATE_PASSWD:
                    {
                        char* password = strndup((char*)GWBUF_DATA(head), GWBUF_LENGTH(head));
                        if (admin_verify_inet_user(maxscaled->username, password))
                        {
                            dcb_printf(dcb, MAXADMIN_AUTH_SUCCESS_REPLY);
                            maxscaled->state = MAXSCALED_STATE_DATA;
                        }
                        else
                        {
                            dcb_printf(dcb, MAXADMIN_AUTH_FAILED_REPLY);
                            maxscaled->state = MAXSCALED_STATE_LOGIN;
                        }
                        gwbuf_free(head);
                        free(password);
                    }
                    break;

                case MAXSCALED_STATE_DATA:
                    {
                        MXS_SESSION_ROUTE_QUERY(dcb->session, head);
                        dcb_printf(dcb, "OK");
                    }
                    break;
                }
            }
            else
            {
                // Force the free of the buffer header
                gwbuf_free(head);
            }
        }
    }
    return n;
}

/**
 * EPOLLOUT handler for the maxscaled protocol module.
 *
 * @param dcb   The descriptor control block
 * @return
 */
static int maxscaled_write_event(DCB* dcb)
{
    return dcb_drain_writeq(dcb);
}

/**
 * Write routine for the maxscaled protocol module.
 *
 * Writes the content of the buffer queue to the socket
 * observing the non-blocking principles of MaxScale.
 *
 * @param dcb   Descriptor Control Block for the socket
 * @param queue Linked list of buffes to write
 */
static int maxscaled_write(DCB* dcb, GWBUF* queue)
{
    int rc;
    rc = dcb_write(dcb, queue);
    return rc;
}

/**
 * Handler for the EPOLLERR event.
 *
 * @param dcb   The descriptor control block
 */
static int maxscaled_error(DCB* dcb)
{
    return 0;
}

/**
 * Handler for the EPOLLHUP event.
 *
 * @param dcb   The descriptor control block
 */
static int maxscaled_hangup(DCB* dcb)
{
    dcb_close(dcb);
    return 0;
}

/**
 * Handler for the EPOLLIN event when the DCB refers to the listening
 * socket for the protocol.
 *
 * @param dcb   The descriptor control block
 * @return The number of new connections created
 */
static int maxscaled_accept(DCB* client_dcb)
{
    socklen_t len = sizeof(struct ucred);
    struct ucred ucred;

    MAXSCALED* maxscaled_protocol = (MAXSCALED*)calloc(1, sizeof(MAXSCALED));

    if (!maxscaled_protocol)
    {
        dcb_close(client_dcb);
        return 0;
    }

    maxscaled_protocol->username = NULL;
    maxscaled_protocol->state = MAXSCALED_STATE_LOGIN;

    bool authenticated = false;

    if (!authenticate_socket(maxscaled_protocol, client_dcb))
    {
        dcb_close(client_dcb);
        free(maxscaled_protocol);
        return 0;
    }

    pthread_mutex_init(&maxscaled_protocol->lock, NULL);
    client_dcb->protocol = (void*)maxscaled_protocol;

    if (!session_start(client_dcb->session) || poll_add_dcb(client_dcb))
    {
        dcb_close(client_dcb);
        return 0;
    }

    return 1;
}

/**
 * The close handler for the descriptor. Called by the gateway to
 * explicitly close a connection.
 *
 * @param dcb   The descriptor control block
 */

static int maxscaled_close(DCB* dcb)
{
    MAXSCALED* maxscaled = static_cast<MAXSCALED*>(dcb->protocol);

    if (!maxscaled)
    {
        return 0;
    }

    pthread_mutex_lock(&maxscaled->lock);
    if (maxscaled->username)
    {
        MXS_FREE(maxscaled->username);
        maxscaled->username = NULL;
    }
    pthread_mutex_unlock(&maxscaled->lock);

    return 0;
}
