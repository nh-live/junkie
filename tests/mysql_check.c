// -*- c-basic-offset: 4; c-backslash-column: 79; indent-tabs-mode: nil -*-
// vim:sw=4 ts=4 sts=4 expandtab
#include <stdlib.h>
#undef NDEBUG
#include <assert.h>
#include <time.h>
#include <junkie/cpp.h>
#include <junkie/tools/ext.h>
#include <junkie/tools/objalloc.h>
#include <junkie/proto/cursor.h>
#include <junkie/proto/tcp.h>
#include <junkie/proto/ip.h>
#include <junkie/proto/eth.h>
#include <junkie/proto/pkt_wait_list.h>
#include "lib_test_junkie.h"
#include "proto/mysql.c"
#include "sql_test.h"


static struct parse_test {
    uint8_t const *packet;
    int size;
    enum proto_parse_status ret;         // Expected proto status
    struct sql_proto_info expected;
    enum query_command last_command;
    enum way way;
} parse_tests[] = {

    // Server greetings
    {
        .packet = (uint8_t const []) {
            0x56, 0x00, 0x00, 0x00, 0x0a, 0x35, 0x2e, 0x35, 0x2e, 0x33, 0x32, 0x2d,
            0x4d, 0x61, 0x72, 0x69, 0x61, 0x44, 0x42, 0x2d, 0x6c, 0x6f, 0x67, 0x00,
            0x2e, 0x05, 0x01, 0x00, 0x3f, 0x27, 0x43, 0x5e, 0x2e, 0x6a, 0x4c, 0x7b,
            0x00, 0xff, 0xf7, 0x21, 0x02, 0x00, 0x0f, 0xa0, 0x15, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4a, 0x3f, 0x3c, 0x50, 0x5e,
            0x66, 0x68, 0x55, 0x52, 0x79, 0x2f, 0x77, 0x00, 0x6d, 0x79, 0x73, 0x71,
            0x6c, 0x5f, 0x6e, 0x61, 0x74, 0x69, 0x76, 0x65, 0x5f, 0x70, 0x61, 0x73,
            0x73, 0x77, 0x6f, 0x72, 0x64, 0x00
        },
        .size = 0x5a,
        .ret = PROTO_OK,
        .way = FROM_SERVER,
        .last_command = 0,
        .expected = {
            .info = { .head_len = 0x5a, .payload = 0},
            .set_values = SQL_VERSION,
            .msg_type = SQL_STARTUP,
            .version_maj = 10,
            .version_min = 0,
        },
    },

    // Login request
    {
        .packet = (uint8_t const []) {
            0x41, 0x00, 0x00, 0x01, 0x8d, 0xa2, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x01,
            0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x66, 0x72, 0x65, 0x64, 0x00, 0x00, 0x74, 0x65, 0x73, 0x74, 0x00, 0x6d,
            0x79, 0x73, 0x71, 0x6c, 0x5f, 0x6e, 0x61, 0x74, 0x69, 0x76, 0x65, 0x5f,
            0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x00
        },
        .size = 0x45,
        .ret = PROTO_OK,
        .way = FROM_CLIENT,
        .last_command = 0,
        .expected = {
            .info = { .head_len = 0x45, .payload = 0},
            .set_values = SQL_DBNAME | SQL_USER | SQL_PASSWD | SQL_ENCODING,
            .msg_type = SQL_STARTUP,
            .u = { .startup = { .user = "fred", .dbname = "test", .passwd = "", .encoding = SQL_ENCODING_UTF8 } },
        },
    },

    // Login error
    {
        .packet = (uint8_t const []) {
            0x5d, 0x00, 0x00, 0x02, 0xff, 0x15, 0x04, 0x23, 0x32, 0x38, 0x30, 0x30,
            0x30, 0x41, 0x63, 0x63, 0x65, 0x73, 0x73, 0x20, 0x64, 0x65, 0x6e, 0x69,
            0x65, 0x64, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x75, 0x73, 0x65, 0x72, 0x20,
            0x27, 0x67, 0x75, 0x65, 0x73, 0x74, 0x27, 0x40, 0x27, 0x64, 0x6c, 0x65,
            0x74, 0x6f, 0x75, 0x72, 0x6e, 0x65, 0x6c, 0x2e, 0x72, 0x64, 0x2e, 0x73,
            0x65, 0x63, 0x75, 0x72, 0x61, 0x63, 0x74, 0x69, 0x76, 0x65, 0x2e, 0x6c,
            0x61, 0x6e, 0x27, 0x20, 0x28, 0x75, 0x73, 0x69, 0x6e, 0x67, 0x20, 0x70,
            0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x3a, 0x20, 0x59, 0x45, 0x53,
            0x29
        },
        .size = 0x61,
        .ret = PROTO_OK,
        .way = FROM_SERVER,
        .last_command = 0,
        .expected = {
            .info = { .head_len = 0x61, .payload = 0},
            .set_values = SQL_REQUEST_STATUS | SQL_ERROR_CODE | SQL_ERROR_SQL_STATUS | SQL_ERROR_MESSAGE,
            .msg_type = SQL_STARTUP,
            .request_status = SQL_REQUEST_ERROR,
            .error_code = "1045",
            .error_message = "Access denied for user 'guest'@'dletournel.rd.securactive.lan' (using password: YES)",
            .u = { .startup = { .user = "fred", .dbname = "test", .passwd = "", .encoding = SQL_ENCODING_UTF8 } },
        },
    },


    // Some table description
    {
        .packet = (uint8_t const []) {
            0x33, 0x00, 0x00, 0x01, 0x03, 0x64, 0x65, 0x66, 0x04, 0x74, 0x65, 0x73,
            0x74, 0x09, 0x74, 0x65, 0x73, 0x74, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x09,
            0x74, 0x65, 0x73, 0x74, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x03, 0x66, 0x6f,
            0x6f, 0x03, 0x66, 0x6f, 0x6f, 0x0c, 0x3f, 0x00, 0x0b, 0x00, 0x00, 0x00,
            0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfb, 0x33, 0x00, 0x00, 0x02, 0x03,
            0x64, 0x65, 0x66, 0x04, 0x74, 0x65, 0x73, 0x74, 0x09, 0x74, 0x65, 0x73,
            0x74, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x09, 0x74, 0x65, 0x73, 0x74, 0x74,
            0x61, 0x62, 0x6c, 0x65, 0x03, 0x62, 0x61, 0x72, 0x03, 0x62, 0x61, 0x72,
            0x0c, 0x08, 0x00, 0x0a, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x00, 0x00,
            0x00, 0xfb, 0x05, 0x00, 0x00, 0x03, 0xfe, 0x00, 0x00, 0x02, 0x00
        },
        .size = 0x77,
        .ret = PROTO_OK,
        .way = FROM_SERVER,
        .last_command = COM_FIELD_LIST,
        .expected = {
            .info = { .head_len = 0x77, .payload = 0x0},
            .msg_type = SQL_QUERY,
            .set_values = SQL_NB_FIELDS | SQL_REQUEST_STATUS,
            .request_status = SQL_REQUEST_COMPLETE,
            .u = { .query = { .nb_fields = 2 } } ,
        },
    },

    // Some select result
    {
        .packet = (uint8_t const []) {
            0x01, 0x00, 0x00, 0x01, 0x01, 0x27, 0x00, 0x00, 0x02, 0x03, 0x64, 0x65,
            0x66, 0x00, 0x00, 0x00, 0x11, 0x40, 0x40, 0x76, 0x65, 0x72, 0x73, 0x69,
            0x6f, 0x6e, 0x5f, 0x63, 0x6f, 0x6d, 0x6d, 0x65, 0x6e, 0x74, 0x00, 0x0c,
            0x08, 0x00, 0x08, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x1f, 0x00, 0x00,
            0x05, 0x00, 0x00, 0x03, 0xfe, 0x00, 0x00, 0x02, 0x00, 0x09, 0x00, 0x00,
            0x04, 0x08, 0x28, 0x55, 0x62, 0x75, 0x6e, 0x74, 0x75, 0x29, 0x05, 0x00,
            0x00, 0x05, 0xfe, 0x00, 0x00, 0x02, 0x00
        },
        .size = 0x4f,
        .ret = PROTO_OK,
        .way = FROM_SERVER,
        .last_command = COM_QUERY,
        .expected = {
            .info = { .head_len = 0x4f, .payload = 0x0},
            .set_values = SQL_NB_FIELDS | SQL_NB_ROWS | SQL_REQUEST_STATUS,
            .msg_type = SQL_QUERY,
            .request_status = SQL_REQUEST_COMPLETE,
            .u = { .query = { .nb_fields = 1, .nb_rows = 1 } } ,
        },
    },

    // Response to prepare statement
    {
        .packet = (uint8_t const []) {
            0x0c, 0x00, 0x00, 0x01, 0x00, 0xfe, 0x0c, 0x00, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x02, 0x03, 0x64, 0x65, 0x66,
            0x00, 0x00, 0x00, 0x01, 0x3f, 0x00, 0x0c, 0x3f, 0x00, 0x00, 0x00, 0x00,
            0x00, 0xfd, 0x80, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x03, 0xfe,
            0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x04, 0x03, 0x64, 0x65, 0x66,
            0x06, 0x6f, 0x63, 0x74, 0x6f, 0x6e, 0x69, 0x0f, 0x4f, 0x43, 0x54, 0x4f,
            0x5f, 0x54, 0x45, 0x53, 0x54, 0x5f, 0x5f, 0x54, 0x59, 0x50, 0x45, 0x0f,
            0x4f, 0x43, 0x54, 0x4f, 0x5f, 0x54, 0x45, 0x53, 0x54, 0x5f, 0x5f, 0x54,
            0x59, 0x50, 0x45, 0x04, 0x43, 0x4f, 0x44, 0x45, 0x04, 0x43, 0x4f, 0x44,
            0x45, 0x0c, 0x08, 0x00, 0xff, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x05, 0x00, 0x00, 0x05, 0xfe, 0x00, 0x00, 0x00, 0x00
        },
        .size = 0x83,
        .ret = PROTO_OK,
        .way = FROM_SERVER,
        .last_command = COM_STMT_PREPARE,
        .expected = {
            .info = { .head_len = 0x83, .payload = 0x0},
            .set_values = SQL_REQUEST_STATUS,
            .msg_type = SQL_QUERY,
            .request_status = SQL_REQUEST_COMPLETE,
        },
    },

    // Response to execute statement
    {
        .packet = (uint8_t const []) {
            0x01, 0x00, 0x00, 0x01, 0x01, 0x42, 0x00, 0x00, 0x02, 0x03, 0x64, 0x65,
            0x66, 0x06, 0x6f, 0x63, 0x74, 0x6f, 0x5f, 0x74, 0x0f, 0x4f, 0x43, 0x54,
            0x4f, 0x5f, 0x54, 0x45, 0x53, 0x54, 0x5f, 0x5f, 0x54, 0x59, 0x50, 0x45,
            0x0f, 0x4f, 0x43, 0x54, 0x4f, 0x5f, 0x54, 0x45, 0x53, 0x54, 0x5f, 0x5f,
            0x54, 0x59, 0x50, 0x45, 0x04, 0x43, 0x4f, 0x44, 0x45, 0x04, 0x43, 0x4f,
            0x44, 0x45, 0x0c, 0x08, 0x00, 0xff, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x03, 0xfe, 0x00, 0x00, 0x00, 0x00,
            0x08, 0x00, 0x00, 0x04, 0x00, 0x00, 0x05, 0x4d, 0x59, 0x53, 0x51, 0x4c,
            0x05, 0x00, 0x00, 0x05, 0xfe, 0x00, 0x00, 0x00, 0x00
        },
        .size = 0x69,
        .ret = PROTO_OK,
        .way = FROM_SERVER,
        .last_command = COM_STMT_EXECUTE,
        .expected = {
            .info = { .head_len = 0x69, .payload = 0x0},
            .set_values = SQL_NB_FIELDS | SQL_NB_ROWS | SQL_REQUEST_STATUS,
            .msg_type = SQL_QUERY,
            .request_status = SQL_REQUEST_COMPLETE,
            .u = { .query = { .nb_fields = 1, .nb_rows = 1 } } ,
        },
    },

};

static unsigned cur_test;

static void mysql_info_check(struct proto_subscriber unused_ *s, struct proto_info const *info_, size_t unused_ cap_len, uint8_t const unused_ *packet, struct timeval const unused_ *now)
{
    // Check info against parse_tests[cur_test].expected
    struct sql_proto_info const *const info = DOWNCAST(info_, info, sql_proto_info);
    struct sql_proto_info const *const expected = &parse_tests[cur_test].expected;
    assert(!compare_expected_sql(info, expected));
}

static void parse_check(void)
{
    struct timeval now;
    timeval_set_now(&now);
    struct parser *parser = proto_mysql->ops->parser_new(proto_mysql);
    struct mysql_parser *mysql_parser = DOWNCAST(parser, parser, mysql_parser);
    assert(mysql_parser);
    struct proto_subscriber sub;
    hook_subscriber_ctor(&pkt_hook, &sub, mysql_info_check);

    for (cur_test = 0; cur_test < NB_ELEMS(parse_tests); cur_test++) {
        struct parse_test const *test = parse_tests + cur_test;
        printf("Check packet %d of size 0x%x (%d)\n", cur_test, test->size, test->size);
        mysql_parser->phase = NONE;
        mysql_parser->last_command = test->last_command;
        enum proto_parse_status ret = mysql_parse(parser, NULL, test->way, test->packet, test->size,
                test->size, &now, test->size, test->packet);
        assert(ret == test->ret);
    }
    hook_subscriber_dtor(&pkt_hook, &sub);
}

int main(void)
{
    log_init();
    ext_init();
    objalloc_init();
    ref_init();
    proto_init();
    port_muxer_init();
    pkt_wait_list_init();
    streambuf_init();
    eth_init();
    ip_init();
    ip6_init();
    tcp_init();
    mysql_init();
    tds_msg_init();
    log_set_level(LOG_DEBUG, NULL);
    log_set_file("mysql_check.log");

    parse_check();

    doomer_stop();
    tds_msg_fini();
    mysql_fini();
    tcp_fini();
    ip6_fini();
    ip_fini();
    eth_fini();
    streambuf_fini();
    pkt_wait_list_fini();
    port_muxer_fini();
    proto_fini();
    ref_fini();
    objalloc_fini();
    ext_fini();
    log_fini();
    return EXIT_SUCCESS;
}

