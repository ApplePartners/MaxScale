/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2027-10-10
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include <maxtest/testconnections.hh>

int inserts = 0;

const char LINE[] = "------------------------------------------";
const char WRONG_SLAVE[] = "Wrong slave was promoted or promotion failed.";
const char GTID_QUERY[] = "SELECT @@gtid_current_pos;";
const char GTID_FIELD[] = "@@gtid_current_pos";
const int bufsize = 512;

void get_output(TestConnections& test)
{
    int ec;
    test.tprintf("maxctrl output:");
    test.print_maxctrl("list servers");

    if (test.verbose())
    {
        test.tprintf("MaxScale output:");
    }

    std::string cmd = "cat /var/log/maxscale/maxscale.log | "
                      "sudo tee -a /var/log/maxscale/maxscale_backup.log "
                      "&& sudo truncate -s 0 /var/log/maxscale/maxscale.log";
    auto res = test.maxscale->ssh_output(cmd);
    if (test.verbose())
    {
        test.tprintf("%s", res.output.c_str());
    }
}

void check(TestConnections& test)
{
    MYSQL* conn = test.maxscale->open_rwsplit_connection();
    const char* query1 = "INSERT INTO test.t1 VALUES (%d)";
    const char* query2 = "SELECT * FROM test.t1";

    test.try_query(conn, "BEGIN");
    test.tprintf(query1, inserts);
    test.try_query(conn, query1, inserts++);
    mysql_query(conn, query2);

    MYSQL_RES* res = mysql_store_result(conn);
    test.add_result(res == NULL, "Query should return a result set");

    if (res)
    {
        std::string values;
        MYSQL_ROW row;
        int num_rows = mysql_num_rows(res);
        test.add_result(num_rows != inserts,
                        "Query returned %d rows when %d rows were expected",
                        num_rows,
                        inserts);
        const char* separator = "";

        while ((row = mysql_fetch_row(res)))
        {
            values += separator;
            values += row[0];
            separator = ", ";
        }
        test.tprintf("%s: %s", query2, values.c_str());
        mysql_free_result(res);
    }
    test.try_query(conn, "COMMIT");
    mysql_close(conn);
}

/**
 * Get master server id (master decided by MaxScale)
 *
 * @param maxscale_ind Which MaxScale to query
 * @param test Tester object
 * @return Master server id
 */
int get_master_server_id(TestConnections& test, int maxscale_ind = 0)
{
    MYSQL* conn = test.maxscale->open_rwsplit_connection();
    int id = -1;
    char str[1024];

    if (find_field(conn, "SELECT @@server_id, @@last_insert_id;", "@@server_id", str) == 0)
    {
        id = atoi(str);
    }

    mysql_close(conn);
    return id;
}

void basic_test(TestConnections& test)
{
    test.tprintf("Creating table and inserting data.");
    test.maxscale->connect_maxscale();
    test.try_query(test.maxscale->conn_rwsplit, "CREATE OR REPLACE TABLE test.t1(id INT)");
    test.repl->sync_slaves();

    check(test);
    get_output(test);
}

/**
 * Do inserts, check that results are as expected.
 *
 * @param test Test connections
 * @param conn Which specific connection to use
 * @param insert_count How many inserts should be done
 * @return True, if successful
 */
bool generate_traffic_and_check(TestConnections& test, MYSQL* conn, int insert_count)
{
    const char INSERT[] = "INSERT INTO test.t1 VALUES (%d);";
    const char SELECT[] = "SELECT * FROM test.t1 ORDER BY id ASC;";
    timespec short_sleep;
    short_sleep.tv_sec = 0;
    short_sleep.tv_nsec = 100000000;

    mysql_query(conn, "BEGIN");

    for (int i = 0; i < insert_count; i++)
    {
        test.try_query(conn, INSERT, inserts++);
        nanosleep(&short_sleep, NULL);
    }
    bool rval = false;

    mysql_query(conn, SELECT);
    MYSQL_RES* res = mysql_store_result(conn);
    test.expect(res != NULL, "Query did not return a result set");

    if (res)
    {
        rval = true;
        MYSQL_ROW row;
        // Check all values, they should go from 0 to 'inserts'
        int expected_val = 0;
        while ((row = mysql_fetch_row(res)))
        {
            int value_read = strtol(row[0], NULL, 0);
            if (value_read != expected_val)
            {
                test.expect(false, "Query returned %d when %d was expected", value_read, expected_val);
                rval = false;
                break;
            }
            expected_val++;
        }
        int num_rows = expected_val;
        test.expect(num_rows == inserts,
                    "Query returned %d rows when %d rows were expected",
                    num_rows,
                    inserts);
        if (num_rows != inserts)
        {
            rval = false;
        }
        mysql_free_result(res);
    }
    mysql_query(conn, "COMMIT");
    return rval;
}

void print_gtids(TestConnections& test)
{
    MYSQL* maxconn = test.maxscale->open_rwsplit_connection();
    if (maxconn)
    {
        char result_tmp[bufsize];
        if (find_field(maxconn, GTID_QUERY, GTID_FIELD, result_tmp) == 0)
        {
            test.tprintf("MaxScale gtid: %s", result_tmp);
        }
    }
    mysql_close(maxconn);
    test.repl->connect();
    for (int i = 0; i < test.repl->N; i++)
    {
        char result_tmp[bufsize];
        if (find_field(test.repl->nodes[i], GTID_QUERY, GTID_FIELD, result_tmp) == 0)
        {
            test.tprintf("Node %d gtid: %s", i, result_tmp);
        }
    }
}
