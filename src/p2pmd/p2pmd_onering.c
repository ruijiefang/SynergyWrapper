/*
 * Created by Ruijie Fang on 1/23/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */
#include "p2pmd.h"
/*
 * [CLIENT-triggered]
 * Forms one ring when no enough information.
 * Assumption:
 *
 */
p2pm_opcodes_t p2pm_form_one_ring(p2pm_t *self)
{
    assert (self);
    zmq_close(self->predecessor_socket);
    zmq_close(self->successor_socket);
    self->predecessor_socket = zmq_socket(self->zmq_context, ZMQ_DEALER);
    self->successor_socket = zmq_socket(self->zmq_context, ZMQ_DEALER);
    memcpy(self->node_list->successor, self->endpoint, PSTRLEN(self->endpoint));
    memcpy(self->node_list->predecessor, self->endpoint, PSTRLEN(self->endpoint));
    memcpy(self->node_list->successor_list, self->endpoint, PSTRLEN(self->endpoint));
    self->current_status = P2PM_NODE_ONE_RING;
    return P2PM_OP_SUCCESS;
}

/*
 * [CLIENT]
 * special case:
 * join another node which forms only one ring.
 */
p2pm_opcodes_t p2pm_join_one_ring(p2pm_t *self)
{
    assert(self);
    unsigned recv;
    if (p2pm_send_uint32(self->predecessor_socket, P2PM_ONE_RING_JOIN_REQ, 0))
        return P2PM_OP_FAILURE;
    memcpy(self->node_list->successor, self->node_list->predecessor, PSTRLEN(self->node_list->predecessor));
    memcpy(self->node_list->successor_list, self->node_list->successor, P2PM_MAX_ID_LEN);
    if (p2pm_recv_uint32(self->predecessor_socket, &recv, 0) != P2PM_OP_SUCCESS)
        return P2PM_OP_FAILURE;
    if (recv != P2PM_JOIN_CMT)
        return P2PM_OP_FAILURE;
    if (zmq_connect(self->successor_socket, self->node_list->successor))
        return P2PM_OP_FAILURE;
    self->current_status = P2PM_NODE_MEMBER;
    return P2PM_OP_SUCCESS;
}
