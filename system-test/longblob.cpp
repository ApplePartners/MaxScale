/**
 * @file longblob.cpp - trying to use LONGBLOB
 * - try to insert large BLOB, MEDIUMBLOB and LONGBLOB via RWSplit, ReadConn Master and directly to backend
 */

#include <maxtest/blob_test.hh>
#include <maxtest/testconnections.hh>

int main(int argc, char* argv[])
{
    TestConnections* Test = new TestConnections(argc, argv);
    Test->reset_timeout();

    MYSQL*& rc_master = Test->maxscale->conn_master;
    Test->repl->execute_query_all_nodes((char*) "set global max_allowed_packet=67108864");

    Test->maxscale->connect_maxscale();
    Test->repl->connect();
    Test->tprintf("LONGBLOB: Trying send data via RWSplit\n");
    test_longblob(Test, Test->maxscale->conn_rwsplit[0], (char*) "LONGBLOB", 1000000, 20, 1);
    Test->repl->close_connections();
    Test->maxscale->close_maxscale_connections();

    Test->maxscale->connect_maxscale();
    Test->repl->connect();
    Test->tprintf("LONGBLOB: Trying send data via ReadConn master\n");
    test_longblob(Test, rc_master, (char*) "LONGBLOB", 1000000, 20, 1);
    Test->repl->close_connections();
    Test->maxscale->close_maxscale_connections();

    Test->maxscale->connect_maxscale();
    Test->repl->connect();
    Test->tprintf("BLOB: Trying send data via RWSplit\n");
    test_longblob(Test, Test->maxscale->conn_rwsplit[0], (char*) "BLOB", 1000, 8, 1);
    Test->repl->close_connections();
    Test->maxscale->close_maxscale_connections();

    Test->maxscale->connect_maxscale();
    Test->repl->connect();
    Test->tprintf("BLOB: Trying send data via ReadConn master\n");
    test_longblob(Test, rc_master, (char*) "BLOB", 1000, 8, 1);
    Test->repl->close_connections();
    Test->maxscale->close_maxscale_connections();

    Test->maxscale->connect_maxscale();
    Test->repl->connect();
    Test->tprintf("MEDIUMBLOB: Trying send data via RWSplit\n");
    test_longblob(Test, Test->maxscale->conn_rwsplit[0], (char*) "MEDIUMBLOB", 1000000, 2, 1);
    Test->repl->close_connections();
    Test->maxscale->close_maxscale_connections();

    Test->maxscale->connect_maxscale();
    Test->repl->connect();
    Test->tprintf("MEDIUMBLOB: Trying send data via ReadConn master\n");
    test_longblob(Test, rc_master, (char*) "MEDIUMBLOB", 1000000, 2, 1);
    Test->repl->close_connections();
    Test->maxscale->close_maxscale_connections();

    Test->repl->connect();
    Test->try_query(Test->repl->nodes[0], "DROP TABLE long_blob_table");
    Test->repl->disconnect();

    int rval = Test->global_result;
    delete Test;
    return rval;
}
