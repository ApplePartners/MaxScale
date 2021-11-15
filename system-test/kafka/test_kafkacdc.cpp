#include <maxtest/testconnections.hh>
#include <maxtest/kafka.hh>

using namespace std::literals::string_literals;

void read_messages(TestConnections& test, Consumer& consumer, int n_expected)
{
    int i = consumer.try_consume_messages(n_expected);
    test.expect(i == n_expected, "Expected %d messages, got %d", n_expected, i);
}

int main(int argc, char** argv)
{
    TestConnections::skip_maxscale_start(true);
    TestConnections test(argc, argv);
    Kafka kafka(test);
    kafka.create_topic("kafkacdc");

    test.repl->stop_slaves();
    auto conn = test.repl->get_connection(0);
    test.maxscale->start();

    // Connect to Kafka
    Consumer consumer(test, "kafkacdc");

    test.tprintf("Inserting data");
    conn.connect();
    conn.query("RESET MASTER");
    conn.query("CREATE TABLE t1(id INT)");
    conn.query("INSERT INTO t1 VALUES (1), (2), (3)");
    conn.query("UPDATE t1 SET id = 4 WHERE id = 2");
    conn.query("DELETE FROM t1 WHERE id = 3");
    auto gtid = conn.field("SELECT @@gtid_binlog_pos");

    test.tprintf("Give MaxScale some time to process the events");
    sleep(5);

    read_messages(test, consumer, 7);

    conn.query("INSERT INTO t1 VALUES (4), (5), (6)");
    sleep(5);

    read_messages(test, consumer, 3);

    test.tprintf("Restarting MaxScale and inserting data");
    test.maxscale->stop();
    test.maxscale->ssh_output("rm /var/lib/maxscale/Kafka-CDC/current_gtid.txt");
    test.maxscale->start();

    conn.query("INSERT INTO t1 VALUES (7), (8), (9)");
    sleep(5);

    read_messages(test, consumer, 3);
    return test.global_result;
}
