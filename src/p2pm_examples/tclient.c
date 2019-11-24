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

    char *endpoint = argv[1];
    char *predecessor_address = argv[2];

    /* a node that needs to join the ring */
    p2pm_t p2pmd;
    p2pm_config_t config;
    p2pm_node_info_t node_list;

    /* init */
    p2pm_init();

    p2pm_config_create(&config, predecessor_address, 1, 70000, 5000);
    p2pm_ni_create(&node_list, 3, predecessor_address);
    p2pm_status_t status = P2PM_NODE_JOINING;
    const char * zlog_place = "../etc/synergy-log.conf";
    if(zlog_init(zlog_place)) return 1;
    zlog_category_t * cat = zlog_get_category("p2pmd");
    if (!cat) return 1;
    p2pm_create(&p2pmd, endpoint, &config, &node_list, cat, status);
    zclock_log("binding to endpoint: %s", p2pmd.endpoint);
    p2pm_start_server(&p2pmd, endpoint);
    zclock_log("joining the ring based on predecessor: %s", p2pmd.node_list->predecessor);
    s_set_id(p2pmd.predecessor_socket);
    char id[25];
    size_t idlen;
    zmq_getsockopt(p2pmd.predecessor_socket, ZMQ_IDENTITY, id, &idlen);
    zclock_log("Our ID[%lu]: %s",idlen, id);
    p2pm_set_obj_wait(&p2pmd);
    p2pm_join(&p2pmd);
    zclock_log("looping server...\n");
    while (1) {

        /* poll all three sockets */
        zmq_poll(p2pmd.pollers, 3, 10);
        p2pm_loop_server(&p2pmd);
    }
}

#pragma clang diagnostic pop