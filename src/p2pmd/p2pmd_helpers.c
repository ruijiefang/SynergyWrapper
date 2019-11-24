/*
 * Created by Ruijie Fang on 1/23/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */
#include "p2pmd.h"

/*
 * p2pm_set_name(self)
 * @return void
 *
 * Sets the name for the ZeroMQ sockets. Note that the predecessor and successor pointers have
 * the same name, so they shall connect to different hosts. The case of 2-ring and 1-ring
 * must thus be handled differently.
 */
void p2pm_set_name(p2pm_t *self)
{
    assert(self);
    assert(self->endpoint);
    assert(self->successor_socket);
    assert(self->predecessor_socket);
    assert(self->node_list->successor);
    assert(self->node_list->predecessor);
    /* randomly generate client IDs because we'll never need to distinguish them */
    s_set_id(self->successor_socket);
    s_set_id(self->predecessor_socket);
    /* our server ID must be our endpointwith a '\0' */
    zmq_setsockopt(self->server_socket, ZMQ_IDENTITY, self->endpoint, PSTRLEN(self->endpoint));

}

/*
 *
 * p2pm_set_obj_wait(1)
 *
 * Sets timeout on zmq_recv sockets
 */
void p2pm_set_obj_wait(p2pm_t *self)
{
    assert(self);
    assert(self->config);
    assert(self->successor_socket);
    assert(self->predecessor_socket);
    assert(self->server_socket);
    zmq_setsockopt(self->successor_socket, ZMQ_RCVTIMEO, &self->config->wait, sizeof(self->config->wait));
    zmq_setsockopt(self->predecessor_socket, ZMQ_RCVTIMEO, &self->config->wait, sizeof(self->config->wait));
    zmq_setsockopt(self->server_socket, ZMQ_RCVTIMEO, &self->config->wait, sizeof(self->config->wait));
}

/* assumes that the socket is connected */
p2pm_opcodes_t p2pm_try_live(void *socket)
{
    assert(socket); /* we don't know whether if it's connected */
    unsigned i = 3, rt;
    do {
#if 0 /* can't do polymorphism here! */
        zclock_log("-detecting if node is live...\n");
        zclock_log("%d\n", p2pm_send_uint32(socket, P2PM_PING_REQ, 0));
        zclock_log("receiving...\n");
#endif
        p2pm_recv_uint32(socket, &rt, 0);
#if 0
        zclock_log("-rt=%u\n", rt);
#endif
        if (rt == P2PM_PONG_REP) {
            return P2PM_OP_LIVE;
        } else
            continue;
    } while (i--);
    return P2PM_OP_DEAD;
}


void p2pm_change_status(p2pm_t *self, p2pm_status_t status)
{
    assert(self);
    self->current_status = status;
}
