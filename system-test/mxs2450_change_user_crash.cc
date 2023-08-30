/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2027-08-18
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

/**
 * MXS-2450: Crash on COM_CHANGE_USER with disable_sescmd_history=true
 * https://jira.mariadb.org/browse/MXS-2450
 */

#include <maxtest/testconnections.hh>

int main(int argc, char *argv[])
{
    TestConnections test(argc, argv);
    Connection conn = test.maxscale->rwsplit();
    test.expect(conn.connect(), "Connection failed: %s", conn.error());

    for (int i = 0; i < 10; i++)
    {
        test.expect(conn.reset_connection(), "Connection reset failed: %s", conn.error());
    }

    return test.global_result;
}