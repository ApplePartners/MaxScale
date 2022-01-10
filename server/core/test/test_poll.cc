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

// To ensure that ss_info_assert asserts also when builing in non-debug mode.
#if !defined (SS_DEBUG)
#define SS_DEBUG
#endif
#if defined (NDEBUG)
#undef NDEBUG
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <maxscale/dcb.hh>
#include <maxscale/listener.hh>
#include <maxscale/service.hh>

#include "test_utils.hh"
#include "../internal/service.hh"
#include "../internal/session.hh"

/**
 * test1    Allocate a service and do lots of other things
 *
 */

static int test1()
{
    DCB* dcb;
    int eno = 0;

    /* Poll tests */
    fprintf(stderr,
            "testpoll : Initialise the polling system.");
    init_test_env(NULL);
    fprintf(stderr, "\t..done\nAdd a DCB");
    MXS_CONFIG_PARAMETER parameters;
    parameters.set(CN_MAX_RETRY_INTERVAL, "10s");
    parameters.set(CN_CONNECTION_TIMEOUT, "10s");
    parameters.set(CN_NET_WRITE_TIMEOUT, "10s");
    auto service = service_alloc("service", "readconnroute", &parameters);

    MXS_CONFIG_PARAMETER listener_params;
    listener_params.set(CN_ADDRESS, "0.0.0.0");
    listener_params.set(CN_PORT, "3306");
    listener_params.set(CN_PROTOCOL, "mariadbclient");
    listener_params.set(CN_SERVICE, service->name());

    auto listener = Listener::create("listener", "mariadbclient", listener_params);

    auto session = new mxs::Session(listener);
    dcb = dcb_alloc(DCB::Role::CLIENT, session);

    if (dcb == NULL)
    {
        fprintf(stderr, "\nError on function call: dcb_alloc() returned NULL.\n");
        return 1;
    }

    dcb->fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (dcb->fd < 0)
    {
        char errbuf[MXS_STRERROR_BUFLEN];
        fprintf(stderr,
                "\nError on function call: socket() returned %d: %s\n",
                errno,
                strerror_r(errno, errbuf, sizeof(errbuf)));
        return 1;
    }


    if ((eno = poll_add_dcb(dcb)) != 0)
    {
        fprintf(stderr, "\nError on function call: poll_add_dcb() returned %d.\n", eno);
        return 1;
    }

    if ((eno = poll_remove_dcb(dcb)) != 0)
    {
        fprintf(stderr, "\nError on function call: poll_remove_dcb() returned %d.\n", eno);
        return 1;
    }

    if ((eno = poll_add_dcb(dcb)) != 0)
    {
        fprintf(stderr, "\nError on function call: poll_add_dcb() returned %d.\n", eno);
        return 1;
    }

    fprintf(stderr, "\t..done\nStart wait for events.");
    sleep(10);
    // TODO, fix this for workers: poll_shutdown();
    fprintf(stderr, "\t..done\nTidy up.");
    dcb_close(dcb);
    fprintf(stderr, "\t..done\n");

    return 0;
}

int main(int argc, char** argv)
{
    int result = 0;

    result += test1();

    exit(result);
}
