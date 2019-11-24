#include "p2pmd.h"

#define _P2PMDS "[p2pmd_server] "

/*
 * [SERVER]
 * book-keeping work before starting server.
 */
int p2pm_start_server(p2pm_t *self, const char *bind_addr)
{
    assert(self);
    assert(self->server_socket);
    return zmq_bind(self->server_socket, self->endpoint);
}

/*
 * handles GET_REQ.
 * Called by p2pm_loop_server(1)
 * returns:
 *  P2PM_OP_FAILURE (NACK), P2PM_OP_SUCCESS (GET_ACK), P2PM_OP_NEED_ONE_RING (GET_CONTACTED_ONE_RING)
 */
p2pm_opcodes_t _GET_REQ_handler(p2pm_t *self)
{
#define _GET_D "[GET_REQ] "
    assert(self);
    zlog_debug(self->log_handle, _P2PMDS
            _GET_D
            "GET_REQ handler called.");
    zmq_send(self->server_socket, self->clientid, strlen(self->clientid), ZMQ_SNDMORE);
    zlog_debug(self->log_handle, _P2PMDS
            _GET_D
            "Receiving client's endpoint...");
    char *buf = malloc(P2PM_MAX_ID_LEN);
    memset(buf, '\0', P2PM_MAX_ID_LEN);
    int ret = p2pm_recv_str(self->server_socket, &buf, 0);

    if (ret != P2PM_OP_SUCCESS) {
        zlog_debug(self->log_handle, _P2PMDS
                _GET_D
                "Error: p2pm_recv_str() failed.\n");
        free(buf);
        goto get_req_do_nack;
    }
    memcpy(self->node_list->temp_successor, buf, P2PM_MAX_ID_LEN);
    zlog_debug(self->log_handle, _P2PMDS
            _GET_D
            "Client endpoint received. %s\n", self->node_list->temp_successor);
    free(buf);
    /* are we one-ring? */
    if (self->current_status == P2PM_NODE_ONE_RING) {
        p2pm_send_uint32(self->server_socket, P2PM_GET_CONTACTED_ONE_RING, 0);
        return P2PM_OP_NEED_ONE_RING;
    } else if (self->current_status == P2PM_NODE_MEMBER) {
        /* answer with an ACK */
        zlog_debug(self->log_handle, _P2PMDS
                _GET_D
                "Succcess. Sending GET_ACK along with self->successor address.");
        p2pm_send_uint32(self->server_socket, P2PM_GET_ACK, ZMQ_SNDMORE);
        p2pm_send_str(self->server_socket, self->node_list->successor, 0);
        self->current_status = P2PM_NODE_HANDLING_JOIN;
        return P2PM_OP_SUCCESS;
    } else {
        /* Nah... NACK */
get_req_do_nack:
        p2pm_send_uint32(self->server_socket, P2PM_GET_NACK, 0);
        return P2PM_OP_FAILURE;
    }
#undef _GET_D
}

/*
 * JOIN_REQ handler
 * called by p2pm_loop_server(1)
 * returns:
 *  P2PM_OP_SUCCESS (sent JOIN_ACK, successor_list, etc.)
 *  P2PM_OP_FAILURE (Sent P2PM_INVALID_REP)
 *  P2PM_OP_NEED_ONE_RING (if we're ONE_RING)
 */
p2pm_opcodes_t _JOIN_REQ_handler(p2pm_t *self)
{
#define _JOINR_D
    assert (self);
    /* a node is attempting to join. feed it! */
    zlog_debug(self->log_handle, _P2PMDS
            _JOINR_D
            "JOIN_REQ handler: Client is attempting to join the network.");
    zmq_send(self->server_socket, self->clientid, strlen(self->clientid), ZMQ_SNDMORE);
    zlog_debug(self->log_handle, _P2PMDS
            _JOINR_D
            "Receiving client's endpoint from client...");
    char *buf;
    int ret = p2pm_recv_str(self->server_socket, &buf, 0);
    if (ret != P2PM_OP_SUCCESS) {
        zlog_debug(self->log_handle, _P2PMDS
                _JOINR_D
                "Error: p2pm_recv_str() error!\n");
        free(buf);
        goto do_invalid_join_rep;
    }
    memcpy(self->node_list->temp_predecessor, buf, PSTRLEN(buf));
    zlog_debug(self->log_handle, _P2PMDS
            _JOINR_D
            "Error: Received JOIN_REQ. %s", self->node_list->temp_predecessor);
    free(buf);
    if (self->current_status == P2PM_NODE_ONE_RING) {
        zlog_debug(self->log_handle, _P2PMDS
                _JOINR_D
                "I'm a ONE_RING. Execute different join protocol.");
        p2pm_send_uint32(self->server_socket, P2PM_GET_CONTACTED_ONE_RING, 0);
        return P2PM_OP_NEED_ONE_RING;
    } else if (self->current_status == P2PM_NODE_MEMBER) {
        zlog_debug(self->log_handle, _P2PMDS
                _JOINR_D
                "Sending JOIN_ACK1...");
        /* first, send JOIN_ACK1 */
        p2pm_send_uint32(self->server_socket, P2PM_JOIN_ACK1, ZMQ_SNDMORE);
        zlog_debug(self->log_handle, _P2PMDS
                _JOINR_D
                "Sending r...");
        /* next, send r */
        p2pm_send_uint32(self->server_socket, self->node_list->r, ZMQ_SNDMORE);
        zlog_debug(self->log_handle, _P2PMDS
                _JOINR_D
                "Sending successor list...");
        /* finally, send str of our successor_list */
        zmq_send(self->server_socket, self->node_list->successor_list,
                 self->node_list->r << P2PM_MAX_ID_LEN_SHFT, 0);
        zlog_debug(self->log_handle, _P2PMDS
                _JOINR_D
                "JOIN_REQ handling done.");
    } else {
do_invalid_join_rep:
        zlog_debug(self->log_handle, _P2PMDS
                _JOINR_D
                "Error: Not in status NODE_MEMBER. Sending INVALID_REP. Current state: %d\n",
                   self->current_status);
        p2pm_send_uint32(self->server_socket, P2PM_INVALID_REP, 0);
        return P2PM_OP_FAILURE;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _JOINR_D
            " === SUCCESSOR_LIST ===");
    p2pm_ni_print_successor_list(self->node_list);
    zlog_debug(self->log_handle, _P2PMDS
            _JOINR_D
            " === Predecessor ===");
    zlog_debug(self->log_handle, _P2PMDS
            _JOINR_D
            "%s", self->node_list->predecessor);
    zlog_debug(self->log_handle, _P2PMDS
            _JOINR_D
            "=====================");
    return P2PM_OP_SUCCESS;
#undef _JOINR_D
}


/*
 * _JOIN_CMT_handler(1)
 * Called-by: p2pm_loop_server(1)
 * returns:
 *  P2PM_OP_SUCCESS: Success handling JOIN_CMT request
 *  P2PM_OP_FAILURE: Failure occured
 */
p2pm_opcodes_t _JOIN_CMT_handler(p2pm_t *self)
{
#define _JOINC_D "[JOIN_CMT] "
    assert(self);
    /* Now our successor list changes accordingly */
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            "Executing JOIN_CMT...");
    zmq_send(self->server_socket, self->clientid, strlen(self->clientid), ZMQ_SNDMORE);
    char *buf = NULL;
    unsigned r;
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            "Receiving r and successor_list..");
    /* these must be atomic, in one envelope */
    if (p2pm_recv_uint32(self->server_socket, &r, 0) != P2PM_OP_SUCCESS) {
        zlog_debug(self->log_handle, _P2PMDS
                _JOINC_D
                "Error: Cannot correctly receive r.");
        p2pm_send_uint32(self->server_socket, P2PM_INVALID_REP, 0);
        return P2PM_OP_FAILURE;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            "Range-checking r: %d", r);
    /* do a range check */
    if (r == 0) {
        zlog_debug(self->log_handle, _P2PMDS
                _JOINC_D
                "Error: invalid r!");
        p2pm_send_uint32(self->server_socket, P2PM_INVALID_REP, 0);
        return P2PM_OP_FAILURE;
    }
    self->node_list->r = r;
    buf = malloc(self->node_list->r_max << P2PM_MAX_ID_LEN_SHFT);
    if (zmq_recv(self->server_socket, buf, self->node_list->r_max << P2PM_MAX_ID_LEN_SHFT, 0) < 0) {
        zlog_debug(self->log_handle, _P2PMDS
                _JOINC_D
                "Error: Invalid successor_list.");
        p2pm_send_uint32(self->server_socket, P2PM_INVALID_REP, 0);
        return P2PM_OP_FAILURE;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            "Received new successor_list. First member: %s", buf);
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            "sending JOIN_ACK2...");
    p2pm_send_uint32(self->server_socket, P2PM_JOIN_ACK2, 0);
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            "disconnecting from current successor...");
    zmq_disconnect(self->successor_socket, self->node_list->successor);
    memcpy(self->node_list->successor, self->node_list->temp_successor, P2PM_MAX_ID_LEN);
    memcpy(self->node_list->successor_list, self->node_list->successor, P2PM_MAX_ID_LEN);
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            "Resetting Temp Successor");
    memset(self->node_list->temp_successor, '\0', P2PM_MAX_ID_LEN);
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            "Copying successor_list:");
    p2pm_ni_copy_successor_list(self->node_list, buf, r);
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            "Connecting to %s", self->node_list->successor);
    zmq_connect(self->successor_socket, self->node_list->successor);
    if (self->current_status == P2PM_NODE_MEMBER &&
        zclock_mono() - self->last_stabilize_time <= self->config->stabilize_interval) {
        zlog_debug(self->log_handle, _P2PMDS
                _JOINC_D
                "All modifications complete. Now REPLICATE_BACKWARDS is triggered.");
        self->server_triggered_actions = P2PM_XPV_REPLICATE_BACKWARDS;
    } else {
        zlog_debug(self->log_handle, _P2PMDS
                _JOINC_D
                "All modifications complete. Didn't trigger REPLICATE_BACKWARDS.");
    }
    if (self->current_status == P2PM_NODE_HANDLING_JOIN)
        self->current_status = P2PM_NODE_MEMBER;
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            " === SUCCESSOR_LIST ===");
    p2pm_ni_print_successor_list(self->node_list);
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            " === Predecessor ===");
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            "%s", self->node_list->predecessor);
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            "=====================");
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            "curr buf addr: %p\n", (void *) buf);
    free(buf);
    zlog_debug(self->log_handle, _P2PMDS
            _JOINC_D
            "*===============================++++ JOIN_CMT Finished ++++==============*");
    return P2PM_OP_SUCCESS;
#undef _JOINC_D
}

/*
 *
 * _STABILIZE_NSYN_REQ_handler(1)
 * returns:
 *  P2PM_OP_SUCCESS: successfully handled NSYN_REQ
 */
p2pm_opcodes_t _STABILIZE_NSYN_REQ_handler(p2pm_t *self)
{
#define _STAB_NSYN_D "[STABILIZE_NSYN_REQ] "
    assert(self);

    zlog_debug(self->log_handle, _P2PMDS
            _STAB_NSYN_D
            "Received NSYN_REQ");
    memset(self->node_list->temp_predecessor, '\0', P2PM_MAX_ID_LEN);
    if (self->current_status == P2PM_NODE_HANDLING_STABILIZE)
        self->current_status = P2PM_NODE_MEMBER;

    zmq_send(self->server_socket, self->clientid, strlen(self->clientid), 0);
    zlog_debug(self->log_handle, _P2PMDS
            _STAB_NSYN_D
            " === SUCCESSOR_LIST ===");
    p2pm_ni_print_successor_list(self->node_list);
    zlog_debug(self->log_handle, _P2PMDS
            _STAB_NSYN_D
            " === Predecessor ===");
    zlog_debug(self->log_handle, _P2PMDS
            _STAB_NSYN_D
            "%s", self->node_list->predecessor);
    zlog_debug(self->log_handle, _P2PMDS
            _STAB_NSYN_D
            "=====================");
    return P2PM_OP_SUCCESS;
#undef _STAB_NSYN_D
}

/*
 *
 * _JOIN_ABRT_handler(1)
 * returns:
 *  P2PM_OP_SUCCESS: successfully handled JOIN_ABRT
 */
p2pm_opcodes_t _JOIN_ABRT_handler(p2pm_t *self)
{
#define _JOIN_A_D "[JOIN_ABRT] "
    assert(self);
    zlog_debug(self->log_handle, _P2PMDS
            _JOIN_A_D
            "Received JOIN_ABRT.");
    if (self->current_status == P2PM_NODE_HANDLING_JOIN)
        self->current_status = P2PM_NODE_MEMBER;
    memset(self->node_list->temp_successor, '\0', P2PM_MAX_ID_LEN);
    zlog_debug(self->log_handle, _P2PMDS
            _JOIN_A_D
            " === SUCCESSOR_LIST ===");
    p2pm_ni_print_successor_list(self->node_list);
    zlog_debug(self->log_handle, _P2PMDS
            _JOIN_A_D
            " === Predecessor ===");
    zlog_debug(self->log_handle, _P2PMDS
            _JOIN_A_D
            "%s", self->node_list->predecessor);
    zlog_debug(self->log_handle, _P2PMDS
            _JOIN_A_D
            "=====================");
    return P2PM_OP_SUCCESS;
#undef _JOIN_A_D
}

/*
 * _PRED_JOIN_CMT_handler
 */
p2pm_opcodes_t _PRED_JOIN_CMT_handler(p2pm_t *self)
{
#define _PJOINC_D "[PRED_JOIN_CMT] "
    assert(self);
    zlog_debug(self->log_handle, _P2PMDS
            _PJOINC_D
            "Received PRED_JOIN_CMT. Disconnecting current predecessor socket.");
    zlog_debug(self->log_handle, _P2PMDS
            _PJOINC_D
            "Current predecessor: %s; New predecessor: %s\n", self->node_list->predecessor,
               self->node_list->temp_predecessor);
    zmq_disconnect(self->predecessor_socket, self->node_list->predecessor); /* swap */
    memcpy(self->node_list->predecessor, self->node_list->temp_predecessor, P2PM_MAX_ID_LEN);
    memset(self->node_list->temp_predecessor, '\0', P2PM_MAX_ID_LEN);
    zlog_debug(self->log_handle, _P2PMDS
            _PJOINC_D
            "Connecting to new predecessor socket.");
    zmq_connect(self->predecessor_socket, self->node_list->predecessor);
    zlog_debug(self->log_handle, _P2PMDS
            _PJOINC_D
            "Done.");
    if (self->current_status == P2PM_NODE_HANDLING_JOIN)
        self->current_status = P2PM_NODE_MEMBER;
    zlog_debug(self->log_handle, _P2PMDS
            _PJOINC_D
            " === SUCCESSOR_LIST ===");
    p2pm_ni_print_successor_list(self->node_list);
    zlog_debug(self->log_handle, _P2PMDS
            _PJOINC_D
            " === Predecessor ===");
    zlog_debug(self->log_handle, _P2PMDS
            _PJOINC_D
            "%s", self->node_list->predecessor);
    zlog_debug(self->log_handle, _P2PMDS
            _PJOINC_D
            "=====================");
    return P2PM_OP_SUCCESS;
#undef _PJOINC_D
}

/*
 *
 */
p2pm_opcodes_t _PRED_JOIN_ABRT_handler(p2pm_t *self)
{
#define _PJOINA_D "[PRED_JOIN_ABRT] "
    assert(self);
    zlog_debug(self->log_handle, _P2PMDS
            _PJOINA_D
            "Received PRED_JOIN_ABRT.");
    memset(self->node_list->temp_predecessor, '\0', P2PM_MAX_ID_LEN);
    if (self->current_status == P2PM_NODE_HANDLING_JOIN)
        self->current_status = P2PM_NODE_MEMBER;
    zlog_debug(self->log_handle, _P2PMDS
            _PJOINA_D
            " === SUCCESSOR_LIST ===");
    p2pm_ni_print_successor_list(self->node_list);
    zlog_debug(self->log_handle, _P2PMDS
            _PJOINA_D
            " === Predecessor ===");
    zlog_debug(self->log_handle, _P2PMDS
            _PJOINA_D
            "%s", self->node_list->predecessor);
    zlog_debug(self->log_handle, _P2PMDS
            _PJOINA_D
            "=====================");
    return P2PM_OP_SUCCESS;
#undef _PJOINA_D
}

/*
 *
 */
p2pm_opcodes_t _PING_REQ_handler(p2pm_t *self)
{
#define _PR_D "[PING] "
    assert(self);
    /* easiest request, just answer with a PONG_REQ and delete client off */
    zlog_debug(self->log_handle, _P2PMDS
            _PR_D
            "received PING_REQ.");
    int rt = zmq_send(self->server_socket, self->clientid, strlen(self->clientid), ZMQ_SNDMORE);
    zlog_debug(self->log_handle, _P2PMDS
            _PR_D
            "PONG_REP sending...%d", rt);
    uint32_t s = htons(P2PM_PONG_REP);
    rt = p2pm_send_uint32(self->server_socket, P2PM_PONG_REP, 0);
    zlog_debug(self->log_handle, _P2PMDS
            _PR_D
            "PONG_REP Sent. %d\n", rt);
    return P2PM_OP_SUCCESS;
#undef _PR_D
}

/*
 *
 */
p2pm_opcodes_t _STABILIZE_REQ_handler(p2pm_t *self)
{
#define _STABR_D "[STABILIZE_REQ] "
    assert(self);
    zlog_debug(self->log_handle, _P2PMDS
            _STABR_D
            "Received STABILIZE_REQ.");
    if (self->current_status != P2PM_NODE_MEMBER && self->current_status != P2PM_NODE_HANDLING_STABILIZE) {
        zlog_debug(self->log_handle, "Err: current_status != P2PM_NODE_MEMBER; current_status = %d\n",
                   self->current_status);
        goto stabilize_send_nack;
    }
    self->current_status = P2PM_NODE_HANDLING_STABILIZE;
    char *stabilize_addr = NULL;
    zmq_send(self->server_socket, self->clientid, strlen(self->clientid), ZMQ_SNDMORE);
    zlog_debug(self->log_handle, _P2PMDS
            _STABR_D
            "Sent clientid, receiving a stabilize_addr.");
    int ret = p2pm_recv_str(self->server_socket, &stabilize_addr, 0);
    zlog_debug(self->log_handle, _P2PMDS
            _STABR_D
            "%d \n", ret);
    if (ret != P2PM_OP_SUCCESS) {
        self->current_status = P2PM_NODE_MEMBER;
        zlog_debug(self->log_handle, _P2PMDS
                _STABR_D
                "Error: Failed to receive a stabilize_addr! %d - %s", ret, stabilize_addr);
stabilize_send_nack:
        p2pm_send_uint32(self->server_socket, P2PM_STABILIZE_NACK, 0);
        return P2PM_OP_FAILURE;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _STABR_D
            "str received. Now copying it to destination.");
    memcpy(self->node_list->temp_predecessor, stabilize_addr, PSTRLEN(stabilize_addr));
    zlog_debug(self->log_handle, _P2PMDS
            _STABR_D
            "sending ack...");
    p2pm_send_uint32(self->server_socket, P2PM_STABILIZE_ACK, ZMQ_SNDMORE);
    zlog_debug(self->log_handle, _P2PMDS
            _STABR_D
            "sending r...");
    p2pm_send_uint32(self->server_socket, self->node_list->r, ZMQ_SNDMORE);
    zlog_debug(self->log_handle, _P2PMDS
            _STABR_D
            "sending successor_list...");
    zlog_debug(self->log_handle, _P2PMDS
            _STABR_D
            "%d\n", zmq_send(self->server_socket, self->node_list->successor_list,
                             self->node_list->r << P2PM_MAX_ID_LEN_SHFT, 0));
    zlog_debug(self->log_handle, _P2PMDS
            _STABR_D
            "all done.");
    free(stabilize_addr);
    zlog_debug(self->log_handle, _P2PMDS
            _STABR_D
            " === SUCCESSOR_LIST ===");
    p2pm_ni_print_successor_list(self->node_list);
    zlog_debug(self->log_handle, _P2PMDS
            _STABR_D
            " === Predecessor ===");
    zlog_debug(self->log_handle, _P2PMDS
            _STABR_D
            "%s", self->node_list->predecessor);
    zlog_debug(self->log_handle, _P2PMDS
            _STABR_D
            "=====================");
    return P2PM_OP_SUCCESS;
#undef _STABR_D
}

/*
 *
 */
p2pm_opcodes_t _STABILIZE_SYN_REQ_handler(p2pm_t *self)
{
#define _STABS_D
    assert(self);
    zlog_debug(self->log_handle, _P2PMDS
            _STABS_D
            "Received P2PM_STABILIZE_SYN_REQ.");
    if (self->current_status != P2PM_NODE_HANDLING_STABILIZE) {
        memset(self->node_list->temp_predecessor, '\0', P2PM_MAX_ID_LEN);
        return P2PM_OP_FAILURE;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _STABS_D
            "Current successor: %s. Switching to: %s.\n", self->node_list->predecessor,
               self->node_list->temp_predecessor);
    if (!streq(self->node_list->predecessor, self->node_list->temp_predecessor)) {
        zmq_disconnect(self->predecessor_socket, self->node_list->predecessor);
        memcpy(self->node_list->predecessor, self->node_list->temp_predecessor,
               PSTRLEN(self->node_list->temp_predecessor));
        zmq_connect(self->predecessor_socket, self->node_list->predecessor);
    }
    memset(self->node_list->temp_predecessor, '\0', P2PM_MAX_ID_LEN);
    self->server_triggered_actions = P2PM_NO_XPV;
    zlog_debug(self->log_handle, _P2PMDS
            _STABS_D
            "success handling SYN_REQ. All done.");
    self->current_status = P2PM_NODE_MEMBER;
    zlog_debug(self->log_handle, _P2PMDS
            _STABS_D
            " === SUCCESSOR_LIST ===");
    p2pm_ni_print_successor_list(self->node_list);
    zlog_debug(self->log_handle, _P2PMDS
            _STABS_D
            " === Predecessor ===");
    zlog_debug(self->log_handle, _P2PMDS
            _STABS_D
            "%s", self->node_list->predecessor);
    zlog_debug(self->log_handle, _P2PMDS
            _STABS_D
            "=====================");
    return P2PM_OP_SUCCESS;
#undef _STABS_D
}

/*
 *
 *
 */
p2pm_opcodes_t _SYNC_SLIST_handler(p2pm_t *self)
{
#define _SYNCS_D "[SYNC_SLIST] "
    assert(self);
    zmq_send(self->server_socket, self->clientid, strlen(self->clientid), ZMQ_SNDMORE);

    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            "Received SYNC_SLIST. sending clientid...");
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            " === SUCCESSOR_LIST ===");
    p2pm_ni_print_successor_list(self->node_list);
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            " === Predecessor ===");
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            "%s", self->node_list->predecessor);
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            "=====================");
    char *buf = NULL; /* prevents UB when freeing */
    unsigned ttl, nr;
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            "attempting to receive ttl...");
    if (p2pm_recv_uint32(self->server_socket, &ttl, 0) != P2PM_OP_SUCCESS) {
        zlog_debug(self->log_handle, "Error: TTL receiving failed!");
_sinvalid:
        zlog_debug(self->log_handle, "Error: Receiving failed!");
        return P2PM_OP_FAILURE;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            "Attempting to receive r...");
    /* next, recv r */
    if (p2pm_recv_uint32(self->server_socket, &nr, 0) != P2PM_OP_SUCCESS ||
        nr == 0 || nr > self->node_list->r_max) {
        free(buf);
        goto _sinvalid;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            "Allocating buffer and receiving successor_list...");
    buf = malloc(nr << P2PM_MAX_ID_LEN_SHFT);
    /* finally, recv str */
    if (zmq_recv(self->server_socket, buf, nr << P2PM_MAX_ID_LEN_SHFT, 0) < 0) {
        free(buf);
        zlog_debug(self->log_handle, "str receiving failed!");
        return P2PM_OP_FAILURE;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            "successor list received: %s\n", buf);
    /* handle change */
    if (--ttl > 0) {
        zlog_debug(self->log_handle, _P2PMDS
                _SYNCS_D
                "TTL unsatisfied. still going back. TTL=%d", ttl);
        p2pm_ni_copy_successor_list(self->node_list, buf, nr);
        zlog_debug(self->log_handle, _P2PMDS
                _SYNCS_D
                "copy success, going back...");
        /* and forward */
        self->last_ttl = ttl;
        self->server_triggered_actions = P2PM_XPV_RELAY_BACKWARDS;
        zlog_debug(self->log_handle, _P2PMDS
                _SYNCS_D
                "Successfully copied successor_list.");
    } else {
        /* TODO: send back a COMMIT Msg to the initiator node */
        zlog_debug(self->log_handle, _P2PMDS
                _SYNCS_D
                "Successfully copied successor_list. TTL=0");
    }
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            "Sending SYNC_SLIST_ACK...");
    p2pm_send_uint32(self->server_socket, P2PM_SYNC_SLIST_ACK, 0);
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            "SYNC_SLIST handling complete.");
    free(buf);
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            " === SUCCESSOR_LIST ===");
    p2pm_ni_print_successor_list(self->node_list);
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            " === Predecessor ===");
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            "%s", self->node_list->predecessor);
    zlog_debug(self->log_handle, _P2PMDS
            _SYNCS_D
            "=====================");
    return P2PM_OP_SUCCESS;
#undef _SYNCS_D
}

/*
 *
 */
p2pm_opcodes_t _ONE_RING_JOIN_REQ_handler(p2pm_t *self)
{
#define _ORJR_D "[ONE_RING_JOIN_REQ] "
    assert(self);
    zlog_debug(self->log_handle, _P2PMDS
            _ORJR_D
            "Received ONE_RING_JOIN_REQ.");
    zmq_send(self->server_socket, self->clientid, strlen(self->clientid), ZMQ_SNDMORE);
    zlog_debug(self->log_handle, _P2PMDS
            _ORJR_D
            "Checking our state...");
    if (self->current_status != P2PM_NODE_ONE_RING) {
_one_ring_join_invalid:
        zlog_debug(self->log_handle, _P2PMDS
                _ORJR_D
                "Error: invalid join. quitting...");
        p2pm_send_uint32(self->server_socket, P2PM_INVALID_REP, 0);
        return P2PM_OP_FAILURE;
    }

    self->current_status = P2PM_NODE_HANDLING_JOIN;

    char *buf;
    if (p2pm_recv_str(self->server_socket, &buf, 0) != P2PM_OP_SUCCESS) {
        free(buf);
        goto _one_ring_join_invalid;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _ORJR_D
            "Setting predecessor/successor to %s\n", buf);
    p2pm_ni_set_predecessor(self->node_list, buf);
    p2pm_ni_set_successor(self->node_list, buf);
    memset(self->node_list->successor_list, '\0', self->node_list->r_max << P2PM_MAX_ID_LEN_SHFT);
    memcpy(self->node_list->successor_list, self->node_list->successor, P2PM_MAX_ID_LEN);
    free(buf);
    zlog_debug(self->log_handle, _P2PMDS
            _ORJR_D
            "Success. connecting to new predecessor/successor...");
    if (zmq_connect(self->successor_socket, self->node_list->successor)) {
        zlog_debug(self->log_handle, _P2PMDS
                _ORJR_D
                "Error: connect error on successor socket.");
        goto _one_ring_join_invalid;
    }
    if (zmq_connect(self->predecessor_socket, self->node_list->predecessor)) {
        zlog_debug(self->log_handle, _P2PMDS
                _ORJR_D
                "Error: connect error on predecessor socket.");
        goto _one_ring_join_invalid;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _ORJR_D
            "Success. inducting ourselves as a member...");
    self->current_status = P2PM_NODE_MEMBER;
    p2pm_send_uint32(self->server_socket, P2PM_JOIN_CMT, 0);
    zlog_debug(self->log_handle, _P2PMDS
            _ORJR_D
            "ONE_RING_JOIN_REQ handling complete.");
    return P2PM_OP_SUCCESS;
#undef _ORJR_D
}

p2pm_opcodes_t _TAIL_FOUND_handler(p2pm_t *self)
{
#define _TF_D "[TAIL_FOUND] "
    assert(self);
    zlog_debug(self->log_handle, _P2PMDS
            _TF_D
            "TAIL_FOUND: Receiving a endpoint.");
    char *buf_endpoint = NULL;
    if (p2pm_recv_str(self->server_socket, &buf_endpoint, 0) != P2PM_OP_SUCCESS) {
        /* we're screwed */
        goto _tail_found_invalid;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _TF_D
            "New predecessor node is %s.\n", buf_endpoint);
    zlog_debug(self->log_handle, _P2PMDS
            _TF_D
            "Closing self->predecessor.");
    zmq_close(self->predecessor_socket);
    zlog_debug(self->log_handle, _P2PMDS
            _TF_D
            "Opening socket on new successor address.");
    self->successor_socket = zmq_socket(self->zmq_context, ZMQ_DEALER);
    zlog_debug(self->log_handle, _P2PMDS
            _TF_D
            "Testing current_status==P2PM_FINDING_TAIL: %d", self->current_status);
    if (self->current_status != P2PM_FINDING_TAIL) {
        goto _tail_found_invalid;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _TF_D
            "Connecting to new endpoint...");
    if (zmq_connect(self->predecessor_socket, buf_endpoint)) {
        free(buf_endpoint);
_tail_found_invalid:
        zlog_debug(self->log_handle, _P2PMDS
                _TF_D
                "Error: TAIL_FOUND request is invalid. Forming ONR_RING...");
        p2pm_form_one_ring(self);
        return P2PM_OP_FAILURE;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _TF_D
            "TAIL_FOUND handling success. ");
    memcpy(self->node_list->predecessor, buf_endpoint, PSTRLEN(buf_endpoint));
    free(buf_endpoint);
    return P2PM_OP_SUCCESS;
#undef _TF_D
}

p2pm_opcodes_t _HEAD_FOUND_handler(p2pm_t *self)
{
#define _TH_D "[HEAD_FOUND] "
    assert(self);
    zlog_debug(self->log_handle, _P2PMDS
            _TH_D
            "HEAD_FOUND: Receiving a endpoint.");
    char *buf_endpoint = NULL;
    if (p2pm_recv_str(self->server_socket, &buf_endpoint, 0) != P2PM_OP_SUCCESS) {
        /* we're screwed */
        goto _tail_found_invalid;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _TH_D
            "New successor node is %s.\n", buf_endpoint);
    zlog_debug(self->log_handle, _P2PMDS
            _TH_D
            "Closing self->successor_socket.");
    zmq_close(self->successor_socket);
    zlog_debug(self->log_handle, _P2PMDS
            _TH_D
            "Opening socket on new successor address.");
    self->successor_socket = zmq_socket(self->zmq_context, ZMQ_DEALER);
    zlog_debug(self->log_handle, _P2PMDS
            _TH_D
            "Testing current_status==P2PM_FINDING_TAIL: %d", self->current_status);
    if (self->current_status != P2PM_FINDING_TAIL) {
        goto _tail_found_invalid;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _TH_D
            "Connecting to new endpoint...");
    if (zmq_connect(self->predecessor_socket, buf_endpoint)) {
        free(buf_endpoint);
_tail_found_invalid:
        zlog_debug(self->log_handle, _P2PMDS
                _TH_D
                "Error: TAIL_FOUND request is invalid. Forming ONR_RING...");
        p2pm_form_one_ring(self);
        return P2PM_OP_FAILURE;
    }
    zlog_debug(self->log_handle, _P2PMDS
            _TH_D
            "TAIL_FOUND handling success. ");
    memset(self->node_list->successor_list, '\0', self->node_list->r_max << P2PM_MAX_ID_LEN_SHFT);
    memset(self->node_list->successor, '\0', P2PM_MAX_ID_LEN);
    memcpy(self->node_list->successor, buf_endpoint, PSTRLEN(buf_endpoint));
    memcpy(self->node_list->successor_list, buf_endpoint, PSTRLEN(buf_endpoint));
    free(buf_endpoint);
    return P2PM_OP_SUCCESS;
#undef _TH_D
}

p2pm_opcodes_t _TOKEN_handler(p2pm_t *self)
{
#define _TOKEN_D "[TOKEN] "
    /* receive a type */
    zlog_debug(self->log_handle, _P2PMDS
            _TOKEN_D
            " **Server received a token (TOKEN). **");

    /*
     * format of the token
     * | unsigned | int | size_t | size_t | [binary] | [binary] |
     * +----------+-----+--------+--------+----------+----------+
     * |   id     | ttl | len1   | len2   |  buf1    | buf2     |
     */

    p2pm_recv_uint32(self->server_socket, (uint32_t *) &self->recv_token_id, 0);
    p2pm_recv_uint32(self->server_socket, (uint32_t *) &self->recv_token_ttl, 0);
    p2pm_recv_uint64(self->server_socket, (uint64_t *) &self->recv_token_len1, 0);
    p2pm_recv_uint64(self->server_socket, (uint64_t *) &self->recv_token_len2, 0);

    self->recv_token_buf1 = malloc(self->recv_token_len1);
    self->recv_token_buf2 = malloc(self->recv_token_len2);

    zlog_debug(self->log_handle, "received lens, receiving data");

    assert(self->recv_token_buf1);
    assert(self->recv_token_buf2);

    zmq_recv(self->server_socket, (void *) self->recv_token_buf1, self->recv_token_len1, 0);
    zmq_recv(self->server_socket, (void *) self->recv_token_buf2, self->recv_token_len2, 0);

    zlog_debug(self->log_handle, _P2PMDS
            _TOKEN_D
            "Received Token: id(%u),ttl(%d),len1(%lu),len2(%lu),"
                    "buf1(%p), buf2(%p), buf1(%s), buf2(%s)\n",
               self->recv_token_id, self->recv_token_ttl,
               self->recv_token_len1, self->recv_token_len2,
               self->recv_token_buf1, self->recv_token_buf2,
               (char *) self->recv_token_buf1, (char *) self->recv_token_buf2);
    self->recv_has_token = 1;
    return P2PM_OP_SUCCESS;
#undef _TOKEN_D
}

p2pm_opcodes_t _successor_socket_handler(p2pm_t *self)
{
#define _SD "[_successor_socket_handler]"
    assert(self);
    if (self->pollers[P2PM_SUCCESSOR_POLLER].revents & ZMQ_POLLIN) {
        zlog_debug(self->log_handle, _P2PMDS
                _SD
                "successor_poller: Received an event. Clearing up a buffer");
        self->successor_lastpoll_time = zclock_mono();
        unsigned s_recv;
        while (p2pm_recv_uint32(self->successor_socket, &s_recv, ZMQ_NOBLOCK) != P2PM_OP_SUCCESS);
        zlog_debug(self->log_handle, _P2PMDS
                _SD
                "successor_poller: Switching on expected action to handle: %d\n", s_recv);
        switch (self->successor_socket_expected_actions) {
            case P2PM_XPV_PING_AND_STABILIZE_REQ:
            case P2PM_XPV_PING_AND_STABILIZE_REQ1:
            case P2PM_XPV_PING_AND_STABILIZE_REQ2: {
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "Previous request is a PING request for stabilize. Checking...");
                if (s_recv == P2PM_PONG_REP) {
                    zlog_debug(self->log_handle, _P2PMDS
                            _SD
                            "Previous request is P2PM_PONG_REP (Positive). Switching states...");
                    p2pm_send_uint32(self->successor_socket, P2PM_STABILIZE_REQ, ZMQ_SNDMORE);
                    p2pm_send_str(self->successor_socket, self->endpoint, 0);

                    self->successor_socket_expected_actions = P2PM_XPV_STABILIZE_ACK;
                    zlog_debug(self->log_handle, _P2PMDS
                            _SD
                            "Switching finished.");
                } else {
                    zlog_debug(self->log_handle, _P2PMDS
                            _SD
                            "s_recv!=P2PM_PONG_REP. Handling successor failure...");
                    p2pm_send_uint32(self->successor_socket, P2PM_PING_REQ, 0);
                    self->successor_socket_expected_actions = P2PM_XPV_STABILIZE_FAILURE;
                }
                break;
            }
            case P2PM_XPV_STABILIZE_ACK: {
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        " === SUCCESSOR_LIST ===");
                p2pm_ni_print_successor_list(self->node_list);
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        " === Predecessor ===");
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "%s", self->node_list->predecessor);
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "=====================");
                zlog_debug(self->log_handle,
                           _P2PMDS _SD
                                   "last state is P2PM_XPV_STABILIZE_ACK. Filtering out extra PONG_REP replies...");
                while (s_recv == P2PM_PONG_REP)
                    p2pm_recv_uint32(self->successor_socket, &s_recv, ZMQ_NOBLOCK);
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "Received: %d", s_recv);
                if (s_recv != P2PM_STABILIZE_ACK) {
handle_recv_failure_in_ack:
                    zlog_debug(self->log_handle, _P2PMDS
                            _SD
                            "STABILIZE_ACK failure. ");
                    p2pm_send_uint32(self->successor_socket, P2PM_STABILIZE_NSYN_REQ, 0);
                    self->successor_socket_expected_actions = P2PM_XPV_STABILIZE_FAILURE;
                    break;
                }
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "Receiving r from peer...");
                if (p2pm_recv_uint32(self->successor_socket, &s_recv, 0) != P2PM_OP_SUCCESS)
                    goto handle_recv_failure_in_ack;
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "Range-checking r=%d", s_recv);
                if (s_recv < 1 || s_recv > self->node_list->r_max) {
                    zlog_debug(self->log_handle, _P2PMDS
                            _SD
                            "Error: r is not in range!");
                    goto handle_recv_failure_in_ack;
                }
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "Setting r and receiving successor_list...");
                self->node_list->r = s_recv;
                char *temp = malloc(self->node_list->r << P2PM_MAX_ID_LEN_SHFT);
                int ret = zmq_recv(self->successor_socket, temp, self->node_list->r << P2PM_MAX_ID_LEN_SHFT, 0);
                if (ret < 0) {
                    zlog_debug(self->log_handle, _P2PMDS
                            _SD
                            "ERR: ret=%d,temp=%s\n", ret, temp);
                    goto handle_recv_failure_in_ack;
                }
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "Copying successor list (First element: %s).", temp);
                p2pm_ni_copy_successor_list(self->node_list, temp, self->node_list->r);
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "Triggering REPLICATE_BACKWARDS...");
                self->server_triggered_actions = P2PM_XPV_REPLICATE_BACKWARDS;
                free(temp);
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "sending SYN_REQ...");
                p2pm_send_uint32(self->successor_socket, P2PM_STABILIZE_SYN_REQ, 0);
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "Handling finished.");
                self->successor_socket_expected_actions = P2PM_NO_XPV;
                self->current_status = P2PM_NODE_MEMBER;
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        " === SUCCESSOR_LIST ===");
                p2pm_ni_print_successor_list(self->node_list);
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        " === Predecessor ===");
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "%s", self->node_list->predecessor);
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "=====================");
                break;

                default:
                    zlog_debug(self->log_handle, _P2PMDS
                            _SD
                            "Error: XPV Not found.");
            }
        }
    } else if ((zclock_mono() - self->successor_lastpoll_time >= self->config->wait &&
                self->successor_socket_expected_actions != P2PM_NO_XPV) ||
               self->successor_socket_expected_actions == P2PM_XPV_STABILIZE_FAILURE) {
        zlog_debug(self->log_handle, _P2PMDS
                _SD
                "Warning: A Timeout occured on successor_socket.");
        self->successor_lastpoll_time = zclock_mono();
        switch (self->successor_socket_expected_actions) {
            case P2PM_XPV_PING_AND_STABILIZE_REQ: {
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "A PING_REQ timedout. sending another one...");
                p2pm_send_uint32(self->successor_socket, P2PM_PING_REQ, 0);
                self->successor_socket_expected_actions = P2PM_XPV_PING_AND_STABILIZE_REQ1;
                self->successor_lastpoll_time = zclock_mono();
            }
            case P2PM_XPV_PING_AND_STABILIZE_REQ1: {
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "Yet another PING_REQ timed out. sending another one...");
                p2pm_send_uint32(self->successor_socket, P2PM_PING_REQ, 0);
                self->successor_lastpoll_time = zclock_mono();
                self->successor_socket_expected_actions = P2PM_XPV_PING_AND_STABILIZE_REQ2;
                break;
            }
            case P2PM_XPV_PING_AND_STABILIZE_REQ2:
            case P2PM_XPV_STABILIZE_FAILURE: {
                zlog_debug(self->log_handle, _P2PMDS
                        _SD
                        "Handling successor_list stabilize failure...");
                if (self->node_list->successor_list[P2PM_MAX_ID_LEN] != '\0') {
                    zlog_debug(self->log_handle, _P2PMDS
                            _SD
                            " * Has more successors. Switching to the next one...");
                    zmq_disconnect(self->successor_socket, self->node_list->successor);
                    memcpy(self->node_list->successor_list, self->node_list->successor_list + P2PM_MAX_ID_LEN,
                           P2PM_MAX_ID_LEN);
                    p2pm_ni_fix_successor_list(self->node_list);
                    memcpy(self->node_list->successor, self->node_list->successor_list, P2PM_MAX_ID_LEN);
                    zmq_connect(self->successor_socket, self->node_list->successor);
                    self->successor_socket_expected_actions = P2PM_XPV_PING_AND_STABILIZE_REQ;
                } else {    /* no more! */
                    zlog_debug(self->log_handle, _P2PMDS
                            _SD
                            " * Error: no more successors! Entering P2PM_FINDING_HEAD");
                    self->server_triggered_actions = P2PM_XPV_STABILIZE_FIND_HEAD;
                    self->current_status = P2PM_FINDING_HEAD;
                    self->successor_socket_expected_actions = P2PM_NO_XPV;
                    zlog_debug(self->log_handle, _P2PMDS
                            _SD
                            "Entered P2PM_FINDING_HEAD.");
                }
                break;
                default: {
                    zlog_debug(self->log_handle, _P2PMDS
                            _SD
                            "Error: unknown action: %d\n", self->successor_socket_expected_actions);

                };
            }
        }
    } else if (self->server_triggered_actions == P2PM_XPV_TRIGGER_STABILIZE) { /* we do svr-triggerd switching here */
        zlog_debug(self->log_handle, _P2PMDS
                _SD
                "Server triggered us to successor stabilize.");
        self->last_stabilize_time = zclock_mono();
        self->successor_lastpoll_time = zclock_mono();
        zlog_debug(self->log_handle, _P2PMDS
                _SD
                "Sending a PING_REQ.");
        p2pm_send_uint32(self->successor_socket, P2PM_PING_REQ, 0);
        self->successor_socket_expected_actions = P2PM_XPV_PING_AND_STABILIZE_REQ;
        self->server_triggered_actions = P2PM_NO_XPV;
        zlog_debug(self->log_handle, _P2PMDS
                _SD
                "Stabilize switching complete.");
    } else if (zclock_mono() - self->successor_lastpoll_time >= self->config->wait) {
        self->successor_lastpoll_time = zclock_mono();
    }
    if (self->send_has_token) {
        zlog_debug(self->log_handle, _P2PMDS
                _SD
                "** >> Token received FROM UPSTREAM. Sending token. **");
        self->successor_lastpoll_time = zclock_mono();
        zlog_debug(self->log_handle, _P2PMDS
                _SD
                "**>>  Token successor: %s **", self->node_list->successor);
        p2pm_send_uint32(self->successor_socket, P2PM_TOKEN, ZMQ_SNDMORE);
        p2pm_send_uint32(self->successor_socket, self->send_token_id, ZMQ_SNDMORE);
        p2pm_send_uint32(self->successor_socket, (uint32_t) self->send_token_ttl, ZMQ_SNDMORE);
        p2pm_send_uint64(self->successor_socket, self->send_token_len1, ZMQ_SNDMORE);
        p2pm_send_uint64(self->successor_socket, self->send_token_len2, ZMQ_SNDMORE);
        zmq_send(self->successor_socket, (const void *) self->send_token_buf1, self->send_token_len1, ZMQ_SNDMORE);
        zmq_send(self->successor_socket, (const void *) self->send_token_buf2, self->send_token_len2, 0);
        free((void *) self->send_token_buf1);
        free((void *) self->send_token_buf2);
        self->send_has_token = 0;
        self->send_token_id = 0;
        self->send_token_ttl = 0;
        zlog_debug(self->log_handle, _P2PMDS
                _SD
                " >> Token Sending Done.");
    }
    return P2PM_OP_SUCCESS;
#undef _SD
}

p2pm_opcodes_t _predecessor_socket_handler(p2pm_t *self)
{
#define _PD "[_predecessor_socket_handler] "
    assert(self);

    if (self->pollers[P2PM_PREDECESSOR_POLLER].revents & ZMQ_POLLIN) {
        zlog_debug(self->log_handle, _P2PMDS
                _PD
                "predecessor_socket received polling event.");
        unsigned predecessor_buf;
        p2pm_recv_uint32(self->predecessor_socket, &predecessor_buf, 0);
        zlog_debug(self->log_handle, _P2PMDS
                _PD
                "Predecessor received a %d\n", predecessor_buf);
        /* update lastpoll time */
        self->predecessor_lastpoll_time = zclock_mono();
        switch (self->predecessor_socket_expected_actions) {
            case P2PM_XPV_PRED_REP_PING1:
            case P2PM_XPV_PRED_REP_PING2:
            case P2PM_XPV_PRED_REP_PING3: {
                zlog_debug(self->log_handle, _P2PMDS
                        _PD
                        "Previous XPV was REP_PING.");
                if (predecessor_buf == P2PM_PONG_REP) {
                    zlog_debug(self->log_handle, _P2PMDS
                            _PD
                            "Current request is PONG_REP.\n");
                    switch (self->predecessor_triggered_actions) {
                        case P2PM_XPV_STABILIZE_FIND_HEAD_PINGED: {
                            self->predecessor_triggered_actions = P2PM_NO_XPV;
                            zlog_debug(self->log_handle, _P2PMDS
                                    _PD
                                    "Executing STABILIZE_FIND_HEAD...\n");
                            self->predecessor_socket_expected_actions = P2PM_XPV_STABILIZE_FIND_HEAD;
                            p2pm_send_uint32(self->predecessor_socket, P2PM_FIND_HEAD, 0);
                            self->current_status = P2PM_FINDING_HEAD;
                            break;
                        }
                        case P2PM_XPV_REPLICATE_BACKWARDS_PINGED: {
                            self->predecessor_triggered_actions = P2PM_NO_XPV;
                            zlog_debug(self->log_handle, _P2PMDS
                                    _PD
                                    "Executing REPLICATE_BACKWARDS...");
                            p2pm_send_uint32(self->predecessor_socket, P2PM_SYNC_SLIST, ZMQ_SNDMORE);
                            p2pm_send_uint32(self->predecessor_socket, self->node_list->r_max, ZMQ_SNDMORE);
                            p2pm_send_uint32(self->predecessor_socket, self->node_list->r, ZMQ_SNDMORE);
                            zmq_send(self->predecessor_socket, self->node_list->successor_list,
                                     self->node_list->r << P2PM_MAX_ID_LEN_SHFT, 0);
                            self->predecessor_socket_expected_actions = P2PM_XPV_REPLICATE_BACKWARDS;
                            break;
                        }
                        case P2PM_XPV_RELAY_BACKWARDS_PINGED: {
                            self->predecessor_triggered_actions = P2PM_NO_XPV;
                            zlog_debug(self->log_handle, _P2PMDS
                                    _PD
                                    "Executing RELAY_BACKWARDS...");
                            p2pm_send_uint32(self->predecessor_socket, P2PM_SYNC_SLIST, ZMQ_SNDMORE);
                            p2pm_send_uint32(self->predecessor_socket, self->last_ttl, ZMQ_SNDMORE);
                            p2pm_send_uint32(self->predecessor_socket, self->node_list->r, ZMQ_SNDMORE);
                            zmq_send(self->predecessor_socket, self->node_list->successor_list,
                                     self->node_list->r << P2PM_MAX_ID_LEN_SHFT, 0);
                            self->server_triggered_actions = P2PM_NO_XPV;
                            self->predecessor_socket_expected_actions = P2PM_XPV_RELAY_BACKWARDS;
                            break;
                        }
                        default: {
                            zlog_debug(self->log_handle, _P2PMDS
                                    _PD
                                    "Unknown action: %d\n", self->server_triggered_actions);
                            self->predecessor_socket_expected_actions = P2PM_NO_XPV;
                            self->predecessor_triggered_actions = P2PM_NO_XPV;
                            break;
                        };
                    }
                    break;
                }
            }
            case P2PM_XPV_RELAY_BACKWARDS: // ***********ERROR (TODO: Temporarily solved )
            case P2PM_XPV_REPLICATE_BACKWARDS: {
                zlog_debug(self->log_handle, _P2PMDS
                        _PD
                        "RELAY/REPLICATE success.");
                self->predecessor_socket_expected_actions = P2PM_NO_XPV;
                self->server_triggered_actions = P2PM_NO_XPV;
                break;
            }
            default:;
        }
    } else if (zclock_mono() - self->predecessor_lastpoll_time > self->config->wait
               && self->predecessor_socket_expected_actions != P2PM_NO_XPV) {
        zlog_debug(self->log_handle, _P2PMDS
                _PD
                "Predecessor socket: timeout occured. Last state: %d\n",
                   self->predecessor_socket_expected_actions);
        self->predecessor_lastpoll_time = zclock_mono();
        switch (self->predecessor_socket_expected_actions) {
            case P2PM_XPV_PRED_REP_PING1: {
                zlog_debug(self->log_handle, _P2PMDS
                        _PD
                        " * Is XPV_REP_PING1, switching to PING2");
                self->predecessor_lastpoll_time = zclock_mono();
                p2pm_send_uint32(self->predecessor_socket, P2PM_PING_REQ, 0);
                self->predecessor_socket_expected_actions = P2PM_XPV_PRED_REP_PING2;
                break;
            }
            case P2PM_XPV_PRED_REP_PING2: {
                zlog_debug(self->log_handle, _P2PMDS
                        _PD
                        " * Is XPV_REP_PING2, switching to PING3");
                p2pm_send_uint32(self->predecessor_socket, P2PM_PING_REQ, 0);
                self->predecessor_lastpoll_time = zclock_mono();
                self->predecessor_socket_expected_actions = P2PM_XPV_PRED_REP_PING3;
                break;
            }
            case P2PM_XPV_PRED_REP_PING3:
            case P2PM_XPV_PRED_FAILURE: {
                zlog_debug(self->log_handle, _P2PMDS
                        _PD
                        " * Is PRED_FAILURE. Triggering FIND_HEAD.");
                if (self->current_status == P2PM_NODE_MEMBER) {
                    self->server_triggered_actions = P2PM_XPV_STABILIZE_FIND_HEAD;
                    self->predecessor_socket_expected_actions = P2PM_NO_XPV;
                } else {
                    self->predecessor_socket_expected_actions = P2PM_NO_XPV;
                    self->server_triggered_actions = P2PM_NO_XPV;
                    self->successor_socket_expected_actions = P2PM_NO_XPV;
                    self->predecessor_triggered_actions = P2PM_NO_XPV;
                    p2pm_form_one_ring(self);
                    return P2PM_OP_FAILURE;
                }
                break;
            }
            case P2PM_XPV_RELAY_BACKWARDS: {
                zlog_debug(self->log_handle, _P2PMDS
                        _PD
                        " * RELAY_BACKWARDS TIMEOUT! (Using recursion) Re-transmitting DATA");
                self->server_triggered_actions = P2PM_XPV_RELAY_BACKWARDS;
                _predecessor_socket_handler(self); /***RECURSION!! */
                break;
            }
            case P2PM_XPV_REPLICATE_BACKWARDS: {
                zlog_debug(self->log_handle, _P2PMDS
                        _PD
                        " * REPLICATE TIMEOUT! (Using recursion) Re-transmitting DATA");
                self->server_triggered_actions = P2PM_XPV_REPLICATE_BACKWARDS;
                _predecessor_socket_handler(self); /******RECURSION!!! */
                break;
            }
            default: {
                zlog_debug(self->log_handle, "***TIMEOUT***: Action was: %d\n",
                           self->predecessor_socket_expected_actions);
            };
        }
    } else if (zclock_mono() - self->predecessor_lastpoll_time > self->config->wait &&
               self->predecessor_socket_expected_actions == P2PM_NO_XPV) {
        self->predecessor_lastpoll_time = zclock_mono();
    } else if (self->server_triggered_actions == P2PM_XPV_STABILIZE_FIND_HEAD ||
               self->server_triggered_actions == P2PM_XPV_REPLICATE_BACKWARDS ||
               self->server_triggered_actions == P2PM_XPV_RELAY_BACKWARDS) {
        /* change server_triggered_actions for EFSM switching (do not send too many
         * ping request */
        switch (self->server_triggered_actions) {
            /* not the best solution, TODO improve */
            case P2PM_XPV_STABILIZE_FIND_HEAD:
                self->predecessor_triggered_actions = P2PM_XPV_STABILIZE_FIND_HEAD_PINGED;
                break;
            case P2PM_XPV_REPLICATE_BACKWARDS:
                self->predecessor_triggered_actions = P2PM_XPV_REPLICATE_BACKWARDS_PINGED;
                break;
            case P2PM_XPV_RELAY_BACKWARDS:
                self->predecessor_triggered_actions = P2PM_XPV_RELAY_BACKWARDS_PINGED;
            default:;
        }
        self->server_triggered_actions = P2PM_NO_XPV;
        zlog_debug(self->log_handle, _P2PMDS
                _PD
                "Predecessor: executing server_triggered_actions...");
        p2pm_send_uint32(self->predecessor_socket, P2PM_PING_REQ, 0);
        self->predecessor_socket_expected_actions = P2PM_XPV_PRED_REP_PING1;
        self->predecessor_lastpoll_time = zclock_mono();
        zlog_debug(self->log_handle, _P2PMDS
                _PD
                " === SUCCESSOR_LIST ===");
        p2pm_ni_print_successor_list(self->node_list);
        zlog_debug(self->log_handle, _P2PMDS
                _PD
                " === Predecessor ===");
        zlog_debug(self->log_handle, _P2PMDS
                _PD
                "%s", self->node_list->predecessor);
        zlog_debug(self->log_handle, _P2PMDS
                _PD
                "=====================");
    }
    return P2PM_OP_SUCCESS;
#undef _PD
}

/*
 * [SERVER]
 * Assumes that sockets are already polled
 * iterative method. Must be called within a loop.
 * Handles one cycle of requests;
 *  - Must be COMPLETE
 */
p2pm_opcodes_t p2pm_loop_server(p2pm_t *self)
{
#define _PLS "[loop_server] "
    assert(self);
    assert(self->server_socket);
    assert(self->node_list);
    /* decls */
    int len;
    unsigned ubuf;


    if (self->pollers[P2PM_SERVER_POLLER].revents & ZMQ_POLLIN) {
        zlog_debug(self->log_handle, _P2PMDS
                _PLS
                "*-----------------------------------------------*");
        zlog_debug(self->log_handle, _P2PMDS
                _PLS
                "server: Receiving connection...");
        len = zmq_recv(self->server_socket, self->clientid, P2PM_MAX_ID_LEN, 0);
        if (len < 0)
            return P2PM_OP_TRYAGAIN;
        zlog_debug(self->log_handle, _P2PMDS
                _PLS
                "server: Estabilized connection with client %s. Receiving action. \n", self->clientid);
        p2pm_recv_uint32(self->server_socket, &ubuf, 0);
        /* do we need to send something back? */
        zlog_debug(self->log_handle, _P2PMDS
                _PLS
                "server: Action %d received.", ubuf);
        switch (ubuf) {
            case P2PM_GET_REQ: {
                _GET_REQ_handler(self);
                break;
            }
            case P2PM_JOIN_REQ: {
                _JOIN_REQ_handler(self);
                break;
            }
            case P2PM_JOIN_CMT: {
                _JOIN_CMT_handler(self);
                break;
            }
            case P2PM_STABILIZE_NSYN_REQ: {
                _STABILIZE_NSYN_REQ_handler(self);
                break;
            }
            case P2PM_JOIN_ABRT: {
                _JOIN_ABRT_handler(self);
                break;
            }
            case P2PM_PRED_JOIN_CMT: {
                _PRED_JOIN_CMT_handler(self);
                break;
            }
            case P2PM_PRED_JOIN_ABRT: {
                _PRED_JOIN_ABRT_handler(self);
                break;
            }
            case P2PM_PING_REQ: {
                _PING_REQ_handler(self);
                break;
            }
            case P2PM_STABILIZE_REQ: {
                _STABILIZE_REQ_handler(self);
                break;
            }
            case P2PM_STABILIZE_SYN_REQ: {
                _STABILIZE_SYN_REQ_handler(self);
                break;
            }
            case P2PM_SYNC_SLIST: {
                _SYNC_SLIST_handler(self);
                break;
            }
            case P2PM_ONE_RING_JOIN_REQ: {
                _ONE_RING_JOIN_REQ_handler(self);
                break;
            }
            case P2PM_TAIL_FOUND: {
                _TAIL_FOUND_handler(self);

                break;
            }
            case P2PM_HEAD_FOUND: {
                _HEAD_FOUND_handler(self);
                break;
            }
            case P2PM_TOKEN: {
                _TOKEN_handler(self);
                break;
            }
            case P2PM_FIND_HEAD: {
                zlog_debug(self->log_handle, _P2PMDS
                        _PLS
                        "********************* RECVD FIND_HEAD ***************************");
                break;
            }
            case P2PM_FIND_TAIL: {
                zlog_debug(self->log_handle, _P2PMDS
                        _PLS
                        "********************* RECVD FIND_TAIL ****************************");
                break;
            }
            default: {
                zlog_debug(self->log_handle, "Server: ERROR: cannot handle request from: %s. Ignoring request!\n",
                           self->clientid);
            }
        }
        zlog_debug(self->log_handle, _P2PMDS
                _PLS
                "*-----------------------------------------------*");
    }

    if (self->current_status == P2PM_NODE_MEMBER &&
        zclock_mono() - self->last_stabilize_time > self->config->stabilize_interval) {
        zlog_debug(self->log_handle, _P2PMDS
                _PLS
                " - Timer triggered. Switching to stabilize.");
        self->server_triggered_actions = P2PM_XPV_TRIGGER_STABILIZE;
        self->current_status = P2PM_NODE_HANDLING_STABILIZE;
    }

    /* handle succcessor socket status */
    _successor_socket_handler(self);

    /* handle predecessor socket status */
    _predecessor_socket_handler(self);

    self->server_triggered_actions = P2PM_NO_XPV;
    return P2PM_OP_SUCCESS;
#undef _PLS
}

/*
 * [SERVER]
 * handles properly closing all functions
 * in the server socket.
 * MUST be called before class destructor.
 */
int p2pm_close_server(p2pm_t *self)
{
    assert(self);
    assert(self->endpoint);
    assert(self->server_socket);
    return zmq_unbind(self->server_socket, self->endpoint);
}

#undef _P2PMDS

