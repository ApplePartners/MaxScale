/**
 * Test KILL QUERY functionality
 */

#include <maxtest/testconnections.hh>

int main(int argc, char* argv[])
{
    TestConnections test(argc, argv);

    for (int i = 0; i < 3; i++)
    {
        auto a = test.maxscale->rwsplit();
        auto b = test.maxscale->rwsplit();
        test.expect(a.connect() && b.connect(), "Connections should work");

        auto id = a.thread_id();

        std::thread thr(
            [&]() {
            const char* query =
                "BEGIN NOT ATOMIC "
                "  DECLARE v1 INT DEFAULT 5; "
                "  CREATE OR REPLACE TABLE test.t1(id INT); "
                "  SET @a = NOW();"
                "  WHILE TIME_TO_SEC(TIMEDIFF(NOW(), @a)) < 30 DO "
                "    INSERT INTO test.t1 VALUES (1); "
                "    SET v1 = (SELECT COUNT(*) FROM test.t1); "
                "  END WHILE;"
                "END";

            // The query takes 30 seconds to complete and the KILL is required to interrupt it before that.
            auto start = std::chrono::steady_clock::now();
            bool ok = a.query(query);
            auto end = std::chrono::steady_clock::now();

            if (ok)
            {
                test.expect(end - start < 30s, "Query should fail in less than 30 seconds");
            }
            else
            {
                const char* expected = "Query execution was interrupted";
                test.expect(strstr(a.error(), expected),
                            "Query should fail with '%s' but it failed with '%s'",
                            expected, a.error());
            }
        });

        // Wait for a few seconds to make sure the other thread has started executing
        // the query before killing it.
        sleep(5);
        test.expect(b.query("KILL QUERY " + std::to_string(id)), "KILL QUERY failed: %s", b.error());

        test.reset_timeout();
        thr.join();
    }

    auto conn = test.maxscale->rwsplit();
    conn.connect();
    conn.query("DROP TABLE test.t1");

    return test.global_result;
}
