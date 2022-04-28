#include <maxtest/testconnections.hh>
#include "test_base.hh"

class MasterSelectTest : public TestCase
{
public:
    using TestCase::TestCase;

    void setup() override
    {
        test.expect(maxscale.connect(), "Pinloki connection should work: %s", maxscale.error());
        test.expect(master.connect(), "Master connection should work: %s", master.error());
        test.expect(slave.connect(), "Slave connection should work: %s", slave.error());

        // Use the latest GTID in case the binlogs have been purged and the complete history is not available
        auto gtid = master.field("SELECT @@gtid_current_pos");

        maxscale.query("STOP SLAVE");
        maxscale.query("SET GLOBAL gtid_slave_pos = '" + gtid + "'");
        maxscale.query("START SLAVE");

        sync(master, maxscale);

        slave.query("STOP SLAVE; RESET SLAVE ALL;");
        slave.query(change_master_sql(test.maxscale->ip(), test.maxscale->rwsplit_port));
        slave.query("START SLAVE");
        sync(maxscale, slave);
    }

    void run() override
    {
        test.expect(!maxscale.query(change_master_sql(test.repl->ip(0), test.repl->port[0])),
                    "CHANGE MASTER should fail");
        test.expect(maxscale.query("STOP SLAVE"), "STOP SLAVE should work: %s", maxscale.error());
        test.expect(maxscale.query("START SLAVE"), "START SLAVE should work: %s", maxscale.error());

        check_gtid();

        test.expect(master.query("CREATE TABLE test.t1(id INT)"), "CREATE failed: %s", master.error());
        test.expect(master.query("INSERT INTO test.t1 VALUES (1)"), "INSERT failed: %s", master.error());
        test.expect(master.query("DROP TABLE test.t1"), "DROP failed: %s", master.error());

        sync(master, maxscale);
        sync(maxscale, slave);

        check_gtid();
    }
};
