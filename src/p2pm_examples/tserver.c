#include <limits.h>
#include "../p2pm_common.h"
#include "../p2pm_types.h"
#include "../p2pm_utilities.h"
#include "../p2pm_node_info.h"
#include "../p2pmd/p2pmd.h"
#include "../p2pm_init.h"
#include <czmq.h>
#include "../../contrib/zlog/src/zlog.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#define HERE puts("here");


int main(int argc, char **argv)
{
#if 0
    void *ctx = zmq_ctx_new();
    void *socket = zmq_socket(ctx, ZMQ_DEALER);
    s_set_id(socket);
    zmq_connect(socket, "tcp://127.0.0.1:5334");
    HERE
    assert(p2pm_try_live(socket)==P2PM_OP_LIVE);
    HERE
    return 0;
#endif


    char *endpoint = argv[1];
    char *predecessor_address = argv[2];
    char *successor_address = argv[3];
    p2pm_t p2pmd;
    p2pm_config_t config;
    p2pm_node_info_t node_list;

    p2pm_init();

    p2pm_config_create(&config, predecessor_address, 1, 50000, 5000);
    p2pm_ni_create(&node_list, 3, predecessor_address);
    p2pm_status_t status = P2PM_NODE_MEMBER;
    const char *zlog_place = "../etc/synergy-log.conf";
    if (zlog_init(zlog_place)) return 1;
    zlog_category_t *cat = zlog_get_category("p2pmd");
    if (!cat) return 1;
    p2pm_create(&p2pmd, endpoint, &config, &node_list, cat, status);
    zmq_ctx_set (p2pmd.zmq_context, ZMQ_MAX_SOCKETS, (int) 0);
    zmq_ctx_set (p2pmd.zmq_context, ZMQ_MAX_SOCKETS, (int) 1024);
    zmq_ctx_set (p2pmd.zmq_context, ZMQ_MAX_SOCKETS, (int) 1024);
    zmq_ctx_set (p2pmd.zmq_context, ZMQ_MAX_SOCKETS, (int) 1024);
    zmq_ctx_set (p2pmd.zmq_context, ZMQ_MAX_SOCKETS, (int) 1024);
    zmq_ctx_set (p2pmd.zmq_context, ZMQ_MAX_SOCKETS, (int) 1024);
    zmq_ctx_set (p2pmd.zmq_context, ZMQ_MAX_SOCKETS, (int) 1024);
    zmq_ctx_set (p2pmd.zmq_context, ZMQ_MAX_SOCKETS, (int) 1024);
    zmq_ctx_set (p2pmd.zmq_context, ZMQ_MAX_SOCKETS, (int) 1024);
    puts("here here here here here");
    for(int i = 0; i < 10; ++i) {
        void * ctx = zsys_init();
    }
    puts("here");
    p2pm_ni_set_successor(p2pmd.node_list, successor_address);
    memcpy(p2pmd.node_list->successor_list, successor_address, PSTRLEN(successor_address));
    p2pmd.node_list->r = 1; // XXX: Force this to 1 to avoid range check failures
    zlog_debug(p2pmd.log_handle, "connecting to successor: %s and predecessor: %s", p2pmd.node_list->successor,
               p2pmd.node_list->predecessor);
    zmq_connect(p2pmd.successor_socket, p2pmd.node_list->successor);
    zmq_connect(p2pmd.predecessor_socket, p2pmd.node_list->predecessor);
    zlog_debug(p2pmd.log_handle, "executing start: binding to %s\n", p2pmd.endpoint);
    p2pm_start_server(&p2pmd, p2pmd.endpoint);
    while (1) {
        zmq_poll(p2pmd.pollers, 3, 10);
        p2pm_loop_server(&p2pmd);
    }
    p2pm_destroy(&p2pmd);
    p2pm_config_destroy(&config);
    p2pm_node_info_destroy(&node_list);
}

#pragma clang diagnostic pop