#include <vector>
#include <tuple>

#include <maxscale/monitor.hh>

//
// Do not edit this file manually, just format it with the code formatter.
//

std::vector<std::tuple<uint64_t, uint64_t, mxs_monitor_event_t>> test_monitor_test_cases = {
    {
        SERVER_DOWN,
        SERVER_RUNNING,
        SERVER_UP_EVENT
    },
    {
        SERVER_DOWN,
        SERVER_RUNNING | SERVER_MASTER,
        MASTER_UP_EVENT
    },
    {
        SERVER_DOWN,
        SERVER_RUNNING | SERVER_SLAVE,
        SLAVE_UP_EVENT
    },
    {
        SERVER_DOWN,
        SERVER_RUNNING | SERVER_JOINED,
        SYNCED_UP_EVENT
    },
    {
        SERVER_DOWN,
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        MASTER_UP_EVENT
    },
    {
        SERVER_DOWN,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        SLAVE_UP_EVENT
    },
    {
        SERVER_DOWN,
        SERVER_RUNNING | SERVER_RELAY,
        RELAY_UP_EVENT
    },
    {
        SERVER_DOWN,
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        MASTER_UP_EVENT
    },
    {
        SERVER_DOWN,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        SLAVE_UP_EVENT
    },
    {
        SERVER_DOWN,
        SERVER_RUNNING | SERVER_BLR,
        BLR_UP_EVENT
    },
    {
        SERVER_RUNNING,
        SERVER_DOWN,
        SERVER_DOWN_EVENT
    },
    {
        SERVER_RUNNING,
        SERVER_RUNNING | SERVER_MASTER,
        NEW_MASTER_EVENT
    },
    {
        SERVER_RUNNING,
        SERVER_RUNNING | SERVER_SLAVE,
        NEW_SLAVE_EVENT
    },
    {
        SERVER_RUNNING,
        SERVER_RUNNING | SERVER_JOINED,
        NEW_SYNCED_EVENT
    },
    {
        SERVER_RUNNING,
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        NEW_MASTER_EVENT
    },
    {
        SERVER_RUNNING,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        NEW_SLAVE_EVENT
    },
    {
        SERVER_RUNNING,
        SERVER_RUNNING | SERVER_RELAY,
        NEW_RELAY_EVENT
    },
    {
        SERVER_RUNNING,
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        NEW_MASTER_EVENT
    },
    {
        SERVER_RUNNING,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        NEW_SLAVE_EVENT
    },
    {
        SERVER_RUNNING,
        SERVER_RUNNING | SERVER_BLR,
        NEW_BLR_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER,
        SERVER_DOWN,
        MASTER_DOWN_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER,
        SERVER_RUNNING,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER,
        SERVER_RUNNING | SERVER_SLAVE,
        NEW_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER,
        SERVER_RUNNING | SERVER_JOINED,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER,
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        NEW_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER,
        SERVER_RUNNING | SERVER_RELAY,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER,
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        NEW_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER,
        SERVER_RUNNING | SERVER_BLR,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE,
        SERVER_DOWN,
        SLAVE_DOWN_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE,
        SERVER_RUNNING,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE,
        SERVER_RUNNING | SERVER_MASTER,
        NEW_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE,
        SERVER_RUNNING | SERVER_JOINED,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE,
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        NEW_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE,
        SERVER_RUNNING | SERVER_RELAY,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE,
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        NEW_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE,
        SERVER_RUNNING | SERVER_BLR,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_JOINED,
        SERVER_DOWN,
        SYNCED_DOWN_EVENT
    },
    {
        SERVER_RUNNING | SERVER_JOINED,
        SERVER_RUNNING,
        LOST_SYNCED_EVENT
    },
    {
        SERVER_RUNNING | SERVER_JOINED,
        SERVER_RUNNING | SERVER_MASTER,
        LOST_SYNCED_EVENT
    },
    {
        SERVER_RUNNING | SERVER_JOINED,
        SERVER_RUNNING | SERVER_SLAVE,
        LOST_SYNCED_EVENT
    },
    {
        SERVER_RUNNING | SERVER_JOINED,
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        LOST_SYNCED_EVENT
    },
    {
        SERVER_RUNNING | SERVER_JOINED,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        LOST_SYNCED_EVENT
    },
    {
        SERVER_RUNNING | SERVER_JOINED,
        SERVER_RUNNING | SERVER_RELAY,
        LOST_SYNCED_EVENT
    },
    {
        SERVER_RUNNING | SERVER_JOINED,
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        LOST_SYNCED_EVENT
    },
    {
        SERVER_RUNNING | SERVER_JOINED,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        LOST_SYNCED_EVENT
    },
    {
        SERVER_RUNNING | SERVER_JOINED,
        SERVER_RUNNING | SERVER_BLR,
        LOST_SYNCED_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        SERVER_DOWN,
        MASTER_DOWN_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        SERVER_RUNNING,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        SERVER_RUNNING | SERVER_MASTER,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        SERVER_RUNNING | SERVER_SLAVE,
        NEW_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        SERVER_RUNNING | SERVER_JOINED,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        NEW_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        SERVER_RUNNING | SERVER_RELAY,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        NEW_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        SERVER_RUNNING | SERVER_BLR,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        SERVER_DOWN,
        SLAVE_DOWN_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        SERVER_RUNNING,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        SERVER_RUNNING | SERVER_MASTER,
        NEW_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        SERVER_RUNNING | SERVER_SLAVE,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        SERVER_RUNNING | SERVER_JOINED,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        NEW_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        SERVER_RUNNING | SERVER_RELAY,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        NEW_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        SERVER_RUNNING | SERVER_BLR,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_RELAY,
        SERVER_DOWN,
        RELAY_DOWN_EVENT
    },
    {
        SERVER_RUNNING | SERVER_RELAY,
        SERVER_RUNNING,
        LOST_RELAY_EVENT
    },
    {
        SERVER_RUNNING | SERVER_RELAY,
        SERVER_RUNNING | SERVER_MASTER,
        LOST_RELAY_EVENT
    },
    {
        SERVER_RUNNING | SERVER_RELAY,
        SERVER_RUNNING | SERVER_SLAVE,
        LOST_RELAY_EVENT
    },
    {
        SERVER_RUNNING | SERVER_RELAY,
        SERVER_RUNNING | SERVER_JOINED,
        LOST_RELAY_EVENT
    },
    {
        SERVER_RUNNING | SERVER_RELAY,
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        LOST_RELAY_EVENT
    },
    {
        SERVER_RUNNING | SERVER_RELAY,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        LOST_RELAY_EVENT
    },
    {
        SERVER_RUNNING | SERVER_RELAY,
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        LOST_RELAY_EVENT
    },
    {
        SERVER_RUNNING | SERVER_RELAY,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        LOST_RELAY_EVENT
    },
    {
        SERVER_RUNNING | SERVER_RELAY,
        SERVER_RUNNING | SERVER_BLR,
        LOST_RELAY_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        SERVER_DOWN,
        MASTER_DOWN_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        SERVER_RUNNING,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        SERVER_RUNNING | SERVER_MASTER,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        SERVER_RUNNING | SERVER_SLAVE,
        NEW_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        SERVER_RUNNING | SERVER_JOINED,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        NEW_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        SERVER_RUNNING | SERVER_RELAY,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        NEW_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        SERVER_RUNNING | SERVER_BLR,
        LOST_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        SERVER_DOWN,
        SLAVE_DOWN_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        SERVER_RUNNING,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        SERVER_RUNNING | SERVER_MASTER,
        NEW_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        SERVER_RUNNING | SERVER_SLAVE,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        SERVER_RUNNING | SERVER_JOINED,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        NEW_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        SERVER_RUNNING | SERVER_RELAY,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        NEW_MASTER_EVENT
    },
    {
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        SERVER_RUNNING | SERVER_BLR,
        LOST_SLAVE_EVENT
    },
    {
        SERVER_RUNNING | SERVER_BLR,
        SERVER_DOWN,
        BLR_DOWN_EVENT
    },
    {
        SERVER_RUNNING | SERVER_BLR,
        SERVER_RUNNING,
        LOST_BLR_EVENT
    },
    {
        SERVER_RUNNING | SERVER_BLR,
        SERVER_RUNNING | SERVER_MASTER,
        LOST_BLR_EVENT
    },
    {
        SERVER_RUNNING | SERVER_BLR,
        SERVER_RUNNING | SERVER_SLAVE,
        LOST_BLR_EVENT
    },
    {
        SERVER_RUNNING | SERVER_BLR,
        SERVER_RUNNING | SERVER_JOINED,
        LOST_BLR_EVENT
    },
    {
        SERVER_RUNNING | SERVER_BLR,
        SERVER_RUNNING | SERVER_MASTER | SERVER_JOINED,
        LOST_BLR_EVENT
    },
    {
        SERVER_RUNNING | SERVER_BLR,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_JOINED,
        LOST_BLR_EVENT
    },
    {
        SERVER_RUNNING | SERVER_BLR,
        SERVER_RUNNING | SERVER_RELAY,
        LOST_BLR_EVENT
    },
    {
        SERVER_RUNNING | SERVER_BLR,
        SERVER_RUNNING | SERVER_MASTER | SERVER_RELAY,
        LOST_BLR_EVENT
    },
    {
        SERVER_RUNNING | SERVER_BLR,
        SERVER_RUNNING | SERVER_SLAVE | SERVER_RELAY,
        LOST_BLR_EVENT
    },
};