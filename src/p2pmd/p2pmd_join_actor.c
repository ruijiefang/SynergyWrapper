/*
 * Created by Ruijie Fang on 1/23/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#include "p2pmd.h"

/* TODO merge this into server
 * Assumptions:
 *  1) constructor  called
 *  2) has predecessor, no successor, successor list no members (but all allocated)
 *  3) set in P2PM_NODE_JOINING phase
 *  4) all sockets are allocated, ctx allocated, but not connected yet (for everything)
 *  5) no sockets are used before (otherwise we have to reallocated after close())
 *  @return: returns P2PM_NODE_FAILURE iff predecessor/successor fails. Needs two nodes to join a ring
 */
p2pm_opcodes_t p2pm_join(p2pm_t *self)
{
    /* assertions */
    assert(self);
    assert(self->node_list);
    assert(self->node_list->predecessor);
    assert(self->current_status == P2PM_NODE_JOINING);
#define _PJLP "[p2pm_join] "
    /* buffers */
    char *copy_buf = malloc(self->node_list->r_max << P2PM_MAX_ID_LEN_SHFT);
    uint32_t state_buf;

    /* main course */
    /* if we're not joining, return a FAILURE */
    if (self->current_status != P2PM_NODE_JOINING)
        return P2PM_OP_FAILURE;
    /* set a bounded wait*/
    p2pm_set_obj_wait(self);
    zlog_debug(self->log_handle, _PJLP
            "Connecting to predecessor node:%s\n", self->node_list->predecessor);
    int r = zmq_connect(self->predecessor_socket, self->node_list->predecessor);
    p2pm_opcodes_t tlresult = p2pm_try_live(self->predecessor_socket);

    if (tlresult != P2PM_OP_LIVE) {
        predecessor_connected:
        zmq_disconnect(self->predecessor_socket, self->node_list->predecessor);
        return P2PM_OP_FAILURE;
    }
    zlog_debug(self->log_handle, _PJLP
            "Sending GET_REQ...");
    /* predecessor is now live, everything next is bounded wait for 1 round */
    int rt;
    p2pm_send_uint32(self->predecessor_socket, P2PM_GET_REQ, ZMQ_SNDMORE);

    /* send our endpoint */
    p2pm_send_str(self->predecessor_socket, self->endpoint, 0);

    /* receive an ACK */
    zlog_debug(self->log_handle, _PJLP
            "Anticipating an ACK...");
    if (p2pm_recv_uint32(self->predecessor_socket, &state_buf, 0) != P2PM_OP_SUCCESS)
        goto predecessor_connected;

    /* check */
    zlog_debug(self->log_handle, _PJLP
            "Checking the GET_ACK... %d\n", state_buf);
    if (state_buf == P2PM_GET_CONTACTED_ONE_RING)
        return p2pm_join_one_ring(self);
    else if (state_buf == P2PM_GET_NACK)
        return P2PM_OP_NEED_ONE_RING;
    else if (state_buf != P2PM_GET_ACK)
        goto predecessor_connected;

    zlog_debug(self->log_handle, _PJLP
            "Receiving successor address from predecessor...");
    /* recv successor addr */
    char *buf = NULL;// = malloc(P2PM_MAX_ID_LEN);
    //memset(buf, '\0', P2PM_MAX_ID_LEN);
    if (p2pm_recv_str(self->predecessor_socket, &buf, 0) != P2PM_OP_SUCCESS) {
        free(buf);
        goto predecessor_connected;
    }
    memcpy(self->node_list->successor, buf, P2PM_MAX_ID_LEN);
    memcpy(self->node_list->successor_list, buf, P2PM_MAX_ID_LEN);
    free(buf);

    /* now we have successor address, connect to it */
    zlog_debug(self->log_handle, _PJLP
            "Connecting to successor: %s\n", self->node_list->successor);
    s_set_id(self->successor_socket);

    int rrr = zmq_connect(self->successor_socket, self->node_list->successor);
    if (rrr == -1) {
        zlog_debug(self->log_handle, "Error: %s", zmq_strerror(zmq_errno()));
        goto predecessor_connected;
    }
    zlog_debug(self->log_handle, _PJLP
            " * zmq_connect: %d\n", rrr);

    zmq_sleep(1);

    /* is our successor also live? */
    zlog_debug(self->log_handle, _PJLP
            "Trying if successor is live...");
    tlresult = p2pm_try_live(self->successor_socket);
    if (tlresult != P2PM_OP_LIVE) {
        successor_connected:
        p2pm_send_uint32(self->predecessor_socket, P2PM_JOIN_ABRT, 0);
        p2pm_send_uint32(self->successor_socket, P2PM_PRED_JOIN_ABRT, 0);
        zmq_disconnect(self->successor_socket, self->node_list->successor);
        zmq_disconnect(self->predecessor_socket, self->node_list->predecessor);
        return P2PM_OP_FAILURE;
    }
    /* good, send JOIN_REQ to successor */
    zlog_debug(self->log_handle, _PJLP
            "Sending JOIN_REQ to successor...");
    p2pm_send_uint32(self->successor_socket, P2PM_JOIN_REQ, ZMQ_SNDMORE);

    p2pm_send_str(self->successor_socket, self->endpoint, 0);

    /* expect a reply: JOIN_ACK1 */
    zlog_debug(self->log_handle, _PJLP
            "Receiving JOIN_ACK1 from successor...");
    if (p2pm_recv_uint32(self->successor_socket, &state_buf, 0) != P2PM_OP_SUCCESS)
        goto successor_connected;
    /* modify status to APPENDAGE - waiting for COMMIT */
    self->current_status = P2PM_NODE_APPENDAGE;
    /* synchronize successor lists */
    zlog_debug(self->log_handle, _PJLP
            "Receiving r from successor...");
    if (p2pm_recv_uint32(self->successor_socket, &(self->node_list->r), 0) != P2PM_OP_SUCCESS)
        goto successor_connected;
    /* range check r */
    zlog_debug(self->log_handle, _PJLP
            " - Success. Checking range...");
    if (self->node_list->r == 0 || self->node_list->r > self->node_list->r_max)
        goto successor_connected;
    zlog_debug(self->log_handle, _PJLP
            " * r=%u\n", self->node_list->r);
    /* receive all successors in a single shot */
    zlog_debug(self->log_handle, _PJLP
            "Receiving and synchronizing successor_list...");
    int ret = zmq_recv(self->successor_socket, copy_buf, self->node_list->r_max << P2PM_MAX_ID_LEN_SHFT, 0);
    if (ret == -1) {
        zlog_debug(self->log_handle, _PJLP
                "Error receiving successor_list: %s\n", zmq_strerror(zmq_errno()));
        goto successor_connected; /* TODO: review this code/? better / more tolerant error handling */
    }
    /* copy 'em to our node_list */
    zlog_debug(self->log_handle, _PJLP
            " * Receiving success. Synchronizing successor list... %s \n", copy_buf);
    p2pm_ni_copy_successor_list(self->node_list, copy_buf, self->node_list->r);
    zlog_debug(self->log_handle, _PJLP
            "==SUCCESSOR_LIST===");
    p2pm_ni_print_successor_list(self->node_list);
    zlog_debug(self->log_handle, _PJLP
            "===================");
    /* Freeing copy_buf */
    zlog_debug(self->log_handle, _PJLP
            " * Freeing copy_buf...");
    p2pm_ni_print_successor_list(self->node_list);
    free(copy_buf);

    /* COMMIT our join, we're a relaxed model */

    /* relay 'em to our predecessor */
    zlog_debug(self->log_handle, _PJLP
            "Relaying successor_list change to predecessor...");
    zlog_debug(self->log_handle, _PJLP
            " * Checking predecessor liveliness...");
    if (p2pm_try_live(self->predecessor_socket) != P2PM_OP_LIVE) /* execute try_live again... */
        goto successor_connected;
    /* sending a JOIN_COMMIT */
    zlog_debug(self->log_handle, _PJLP
            " * Notifying predecessor via a JOIN_COMMIT...");
    p2pm_send_uint32(self->predecessor_socket, P2PM_JOIN_CMT, ZMQ_SNDMORE);
    /* sending r */
    p2pm_send_uint32(self->predecessor_socket, self->node_list->r, ZMQ_SNDMORE);
    /* (actually) relay our data off */
    zlog_debug(self->log_handle, _PJLP
            " * Relaying data...");
    zmq_send(self->predecessor_socket, self->node_list->successor_list, self->node_list->r << P2PM_MAX_ID_LEN_SHFT, 0);
    zlog_debug(self->log_handle, _PJLP
            " * Success.");
    /* wait for a COMMIT */
    zlog_debug(self->log_handle, _PJLP
            "Waiting for JOIN_ACK2 from predecessor...");
    if (p2pm_recv_uint32(self->predecessor_socket, &state_buf, 0) != P2PM_OP_SUCCESS)
        goto successor_connected;
    /* check if it is JOIN_ACK2 */
    zlog_debug(self->log_handle, _PJLP
            "Checking if JOIN_ACK2==%u\n", state_buf);
    if (state_buf != P2PM_JOIN_ACK2)
        goto successor_connected;
    /* send out PRED_JOIN_CMT */
    zlog_debug(self->log_handle, _PJLP
            "All checks OK - sending final PRED_JOIN_CMT message to successor...");
    if (p2pm_send_uint32(self->successor_socket, P2PM_PRED_JOIN_CMT, 0) < 0) {
        zlog_debug(self->log_handle, _PJLP
                " ** Error: %s **", zmq_strerror(zmq_errno()));
        goto successor_connected;
    }
    /* join finished, adjust status && return OP_SUCCESS */
    self->current_status = P2PM_NODE_MEMBER;
#undef _PJLP
    return P2PM_OP_SUCCESS;
}
