#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <maxbase/alloc.h>
#include <maxscale/buffer.hh>
#include <maxscale/protocol/mysql.hh>

uint8_t hack[128] = {};     // Avoids having to allocate services, sessions and DCBs just to test the protocol
MySQLProtocol proto((MXS_SESSION*)hack, (SERVER*)hack, (mxs::Component*)hack);

static const uint8_t resultset1[] =
{
    /* BEGIN;*/
    0x29, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
    0x40, 0x00, 0x00, 0x00, 0x20, 0x05, 0x09, 0x08,
    0x54, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F,
    0x04, 0x13, 0x12, 0x53, 0x54, 0x41, 0x52, 0x54,
    0x20, 0x54, 0x52, 0x41, 0x4E, 0x53, 0x41, 0x43,
    0x54, 0x49, 0x4F, 0x4E, 0x3B,
    /* COMMIT;*/
    0x17, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    0x40, 0x00, 0x00, 0x00, 0x0E, 0x05, 0x09, 0x08,
    0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F,
    0x04, 0x01, 0x00,
    /* START TRANSACTION;*/
    0x29, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
    0x40, 0x00, 0x00, 0x00, 0x20, 0x05, 0x09, 0x08,
    0x54, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F,
    0x04, 0x13, 0x12, 0x53, 0x54, 0x41, 0x52, 0x54,
    0x20, 0x54, 0x52, 0x41, 0x4E, 0x53, 0x41, 0x43,
    0x54, 0x49, 0x4F, 0x4E, 0x3B,
    /* START TRANSACTION READ ONLY;*/
    0x28, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
    0x60, 0x00, 0x00, 0x00, 0x1F, 0x04, 0x1D, 0x1C,
    0x53, 0x54, 0x41, 0x52, 0x54, 0x20, 0x54, 0x52,
    0x41, 0x4E, 0x53, 0x41, 0x43, 0x54, 0x49, 0x4F,
    0x4E, 0x20, 0x52, 0x45, 0x41, 0x44, 0x20, 0x4F,
    0x4E, 0x4C, 0x59, 0x3B,
    /* COMMIT;*/
    0x07, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00,
    /* SET AUTOCOMMIT=0;*/
    0x1D, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x14, 0x00, 0x0F, 0x0A,
    0x61, 0x75, 0x74, 0x6F, 0x63, 0x6F, 0x6D, 0x6D,
    0x69, 0x74, 0x03, 0x4F, 0x46, 0x46, 0x02, 0x01,
    0x31,
    /* INSERT INTO t1 VALUES(1);*/
    0x14, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x0B, 0x05, 0x09, 0x08,
    0x49, 0x5F, 0x5F, 0x5F, 0x57, 0x5F, 0x5F, 0x5F,
    /* COMMIT;*/
    0x14, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x0B, 0x05, 0x09, 0x08,
    0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F
};

#define PACKET_HDR_LEN 4

#define PACKET_1_IDX 0
#define PACKET_1_LEN (PACKET_HDR_LEN + 0x29)
#define PACKET_2_IDX (PACKET_1_IDX + PACKET_1_LEN)
#define PACKET_2_LEN (PACKET_HDR_LEN + 0x17)
#define PACKET_3_IDX (PACKET_2_IDX + PACKET_2_LEN)
#define PACKET_3_LEN (PACKET_HDR_LEN + 0x29)
#define PACKET_4_IDX (PACKET_3_IDX + PACKET_3_LEN)
#define PACKET_4_LEN (PACKET_HDR_LEN + 0x28)
#define PACKET_5_IDX (PACKET_4_IDX + PACKET_4_LEN)
#define PACKET_5_LEN (PACKET_HDR_LEN + 0x07)
#define PACKET_6_IDX (PACKET_5_IDX + PACKET_5_LEN)
#define PACKET_6_LEN (PACKET_HDR_LEN + 0x1D)
#define PACKET_7_IDX (PACKET_6_IDX + PACKET_6_LEN)
#define PACKET_7_LEN (PACKET_HDR_LEN + 0x14)
#define PACKET_8_IDX (PACKET_7_IDX + PACKET_7_LEN)
#define PACKET_8_LEN (PACKET_HDR_LEN + 0x14)

/* multi statments;*/
static const uint8_t resultset2[] =
{
    /**
     *  set autocommit=0;
     *  create table t1(id int);
     *  insert into t1 select seq from seq_0_to_20;
     *  select '' from t1;
     **/
    0x1D, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x14, 0x00, 0x0F, 0x0A,
    0x61, 0x75, 0x74, 0x6F, 0x63, 0x6F, 0x6D, 0x6D,
    0x69, 0x74, 0x03, 0x4F, 0x46, 0x46, 0x02, 0x01,
    0x31, 0x07, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x01,
    0x00, 0x15, 0x00, 0x21, 0x40, 0x00, 0x00, 0x27,
    0x52, 0x65, 0x63, 0x6F, 0x72, 0x64, 0x73, 0x3A,
    0x20, 0x32, 0x31, 0x20, 0x20, 0x44, 0x75, 0x70,
    0x6C, 0x69, 0x63, 0x61, 0x74, 0x65, 0x73, 0x3A,
    0x20, 0x30, 0x20, 0x20, 0x57, 0x61, 0x72, 0x6E,
    0x69, 0x6E, 0x67, 0x73, 0x3A, 0x20, 0x30, 0x0B,
    0x05, 0x09, 0x08, 0x49, 0x5F, 0x52, 0x5F, 0x57,
    0x5F, 0x5F, 0x5F, 0x01, 0x00, 0x00, 0x01, 0x01,
    0x16, 0x00, 0x00, 0x02, 0x03, 0x64, 0x65, 0x66,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x21, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFD, 0x01, 0x00, 0x27,
    0x00, 0x00, 0x05, 0x00, 0x00, 0x03, 0xFE, 0x00,
    0x00, 0x21, 0x00, 0x01, 0x00, 0x00, 0x04, 0x00,
    0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00,
    0x06, 0x00, 0x01, 0x00, 0x00, 0x07, 0x00, 0x01,
    0x00, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00, 0x09,
    0x00, 0x01, 0x00, 0x00, 0x0A, 0x00, 0x01, 0x00,
    0x00, 0x0B, 0x00, 0x01, 0x00, 0x00, 0x0C, 0x00,
    0x01, 0x00, 0x00, 0x0D, 0x00, 0x01, 0x00, 0x00,
    0x0E, 0x00, 0x01, 0x00, 0x00, 0x0F, 0x00, 0x01,
    0x00, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00, 0x11,
    0x00, 0x01, 0x00, 0x00, 0x12, 0x00, 0x01, 0x00,
    0x00, 0x13, 0x00, 0x01, 0x00, 0x00, 0x14, 0x00,
    0x01, 0x00, 0x00, 0x15, 0x00, 0x01, 0x00, 0x00,
    0x16, 0x00, 0x01, 0x00, 0x00, 0x17, 0x00, 0x01,
    0x00, 0x00, 0x18, 0x00, 0x05, 0x00, 0x00, 0x19,
    0xFE, 0x00, 0x00, 0x21, 0x40
};

/**
 * SET AUTOCOMMIT=0;
 * CREATE TABLE t1(a VARCHAR(20), b INT, c INT, d INT);
 * INSERT INTO t1 VALUES ('', 100, 200, 300);
 * SELECT * FROM t1;
 **/
static const uint8_t resultset3[] =
{
    0x28, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x0F, 0x0A,
    0x61, 0x75, 0x74, 0x6F, 0x63, 0x6F, 0x6D, 0x6D,
    0x69, 0x74, 0x03, 0x4F, 0x46, 0x46, 0x02, 0x01,
    0x31, 0x05, 0x09, 0x08, 0x49, 0x5F, 0x52, 0x5F,
    0x5F, 0x5F, 0x53, 0x5F, 0x1D, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x0F, 0x0A, 0x61, 0x75, 0x74, 0x6F,
    0x63, 0x6F, 0x6D, 0x6D, 0x69, 0x74, 0x03, 0x4F,
    0x46, 0x46, 0x02, 0x01, 0x31, 0x07, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x0B, 0x05, 0x09, 0x08,
    0x49, 0x5F, 0x5F, 0x5F, 0x57, 0x5F, 0x5F, 0x5F,
    0x01, 0x00, 0x00, 0x01, 0x04, 0x20, 0x00, 0x00,
    0x02, 0x03, 0x64, 0x65, 0x66, 0x04, 0x74, 0x65,
    0x73, 0x74, 0x02, 0x74, 0x31, 0x02, 0x74, 0x31,
    0x01, 0x61, 0x01, 0x61, 0x0C, 0x21, 0x00, 0x3C,
    0x00, 0x00, 0x00, 0xFD, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x20, 0x00, 0x00, 0x03, 0x03, 0x64, 0x65,
    0x66, 0x04, 0x74, 0x65, 0x73, 0x74, 0x02, 0x74,
    0x31, 0x02, 0x74, 0x31, 0x01, 0x62, 0x01, 0x62,
    0x0C, 0x3F, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00,
    0x04, 0x03, 0x64, 0x65, 0x66, 0x04, 0x74, 0x65,
    0x73, 0x74, 0x02, 0x74, 0x31, 0x02, 0x74, 0x31,
    0x01, 0x63, 0x01, 0x63, 0x0C, 0x3F, 0x00, 0x0B,
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x20, 0x00, 0x00, 0x05, 0x03, 0x64, 0x65,
    0x66, 0x04, 0x74, 0x65, 0x73, 0x74, 0x02, 0x74,
    0x31, 0x02, 0x74, 0x31, 0x01, 0x64, 0x01, 0x64,
    0x0C, 0x3F, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00,
    0x06, 0xFE, 0x00, 0x00, 0x21, 0x00, 0x05, 0x00,
    0x00, 0x07, 0xFE, 0x00, 0x00, 0x21, 0x00
};

/* functional test , test packet by packet */
void test1()
{
    GWBUF* buffer;
    proto.server_capabilities = GW_MYSQL_CAPABILITIES_SESSION_TRACK;
    proto.num_eof_packets = 0;
    fprintf(stderr, "test_session_track : Functional tests.\n");
    // BEGIN
    buffer = gwbuf_alloc_and_load(PACKET_1_LEN, resultset1 + PACKET_1_IDX);
    mxs_mysql_get_session_track_info(buffer, &proto);
    mxb_assert(strncmp(gwbuf_get_property(buffer, (char*)"trx_state"), "T_______", 8) == 0);
    gwbuf_free(buffer);
    // COMMIT
    buffer = gwbuf_alloc_and_load(PACKET_2_LEN, resultset1 + PACKET_2_IDX);
    mxs_mysql_get_session_track_info(buffer, &proto);
    mxb_assert(strncmp(gwbuf_get_property(buffer, (char*)"trx_state"), "________", 8) == 0);
    gwbuf_free(buffer);
    // START TRANSACTION
    buffer = gwbuf_alloc_and_load(PACKET_3_LEN, resultset1 + PACKET_3_IDX);
    mxs_mysql_get_session_track_info(buffer, &proto);
    mxb_assert(strncmp(gwbuf_get_property(buffer, (char*)"trx_state"), "T_______", 8) == 0);
    gwbuf_free(buffer);
    // START TRANSACTION READ ONLY
    buffer = gwbuf_alloc_and_load(PACKET_4_LEN, resultset1 + PACKET_4_IDX);
    mxs_mysql_get_session_track_info(buffer, &proto);
    mxb_assert(strncmp(gwbuf_get_property(buffer, (char*)"trx_characteristics"),
                       "START TRANSACTION READ ONLY;",
                       28) == 0);
    gwbuf_free(buffer);
    // COMMIT
    buffer = gwbuf_alloc_and_load(PACKET_5_LEN, resultset1 + PACKET_5_IDX);
    mxs_mysql_get_session_track_info(buffer, &proto);
    mxb_assert(gwbuf_get_property(buffer, (char*)"trx_characteristics") == NULL);
    mxb_assert(gwbuf_get_property(buffer, (char*)"trx_state") == NULL);
    gwbuf_free(buffer);
    // SET AUTOCOMMIT=0;
    buffer = gwbuf_alloc_and_load(PACKET_6_LEN, resultset1 + PACKET_6_IDX);
    mxs_mysql_get_session_track_info(buffer, &proto);
    mxb_assert(strncmp(gwbuf_get_property(buffer, (char*)"autocommit"), "OFF", 3) == 0);
    gwbuf_free(buffer);
    // INSERT INTO t1 VALUES(1);
    buffer = gwbuf_alloc_and_load(PACKET_7_LEN, resultset1 + PACKET_7_IDX);
    mxs_mysql_get_session_track_info(buffer, &proto);
    mxb_assert(strncmp(gwbuf_get_property(buffer, (char*)"trx_state"), "I___W___", 8) == 0);
    gwbuf_free(buffer);
    // COMMIT
    buffer = gwbuf_alloc_and_load(PACKET_8_LEN, resultset1 + PACKET_8_IDX);
    mxs_mysql_get_session_track_info(buffer, &proto);
    mxb_assert(strncmp(gwbuf_get_property(buffer, (char*)"trx_state"), "________", 8) == 0);
    gwbuf_free(buffer);
}

/* multi results combine in one buffer, test for check boundary handle properly */
void test2()
{
    GWBUF* buffer;
    fprintf(stderr, "test_session_track: multi results test\n");
    proto.server_capabilities = GW_MYSQL_CAPABILITIES_SESSION_TRACK;
    proto.num_eof_packets = 0;
    buffer = gwbuf_alloc_and_load(sizeof(resultset2), resultset2);
    mxs_mysql_get_session_track_info(buffer, &proto);
    mxb_assert(strncmp(gwbuf_get_property(buffer, (char*)"trx_state"), "I_R_W___", 8) == 0);
    gwbuf_free(buffer);
}

void test3()
{
    GWBUF* buffer;
    proto.server_capabilities = GW_MYSQL_CAPABILITIES_SESSION_TRACK;
    proto.num_eof_packets = 0;
    fprintf(stderr, "test_session_track: protocol state test\n");
    buffer = gwbuf_alloc_and_load(sizeof(resultset2), resultset2);
    mxs_mysql_get_session_track_info(buffer, &proto);
    mxb_assert(strncmp(gwbuf_get_property(buffer, (char*)"trx_state"), "I_R_W___", 8) == 0);
    gwbuf_free(buffer);
}

int main(int argc, char** argv)
{
    test1();
    test2();
    test3();
    return 0;
}
