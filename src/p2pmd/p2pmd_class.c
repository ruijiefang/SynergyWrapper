/*
 * Created by Ruijie Fang on 1/23/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */
#include "p2pmd.h"

/* does not free other types */
void p2pm_destroy(p2pm_t *self)
{
    assert(self);
    zmq_close(self->predecessor_socket);
    zmq_close(self->successor_socket);
    zmq_ctx_destroy(self->server_socket);
    free(self->endpoint);
}

void p2pm_create(p2pm_t *self, const char *endpoint, p2pm_config_t *config, p2pm_node_info_t *node_list,
                 zlog_category_t * log_handle,
                 p2pm_status_t status)
{
    /* assertions */
    assert(self);
    assert(config);
    assert(node_list);
    assert(endpoint);
    assert(PSTRLEN(endpoint) <= P2PM_MAX_ID_LEN);
    assert(log_handle);


    self->send_has_token = 0;
    self->recv_has_token = 0;

    /* zlog */
    self->log_handle = log_handle;

    /* objects from friends */
    self->current_status = status;
    self->config = config;
    self->node_list = node_list;
    self->zmq_context = zmq_ctx_new();
    self->endpoint = malloc(P2PM_MAX_ID_LEN);
    memcpy(self->endpoint, endpoint, PSTRLEN(endpoint));
    zmq_ctx_set(self->zmq_context, ZMQ_IO_THREADS, config->context_thread_count);

    /* sockets */
    self->predecessor_socket = zmq_socket(self->zmq_context, ZMQ_DEALER);
    self->successor_socket = zmq_socket(self->zmq_context, ZMQ_DEALER);
    self->server_socket = zmq_socket(self->zmq_context, ZMQ_ROUTER);
    self->shortlived_socket = zmq_socket(self->zmq_context, ZMQ_DEALER);

    /* a poller for all 3 sockets */
    self->pollers[P2PM_SERVER_POLLER].socket = self->server_socket;
    self->pollers[P2PM_PREDECESSOR_POLLER].socket = self->predecessor_socket;
    self->pollers[P2PM_SUCCESSOR_POLLER].socket = self->successor_socket;
    self->pollers[P2PM_OTHER_POLLER].socket = self->shortlived_socket;
    self->pollers[P2PM_SERVER_POLLER].events = ZMQ_POLLIN;
    self->pollers[P2PM_PREDECESSOR_POLLER].events = ZMQ_POLLIN;
    self->pollers[P2PM_SUCCESSOR_POLLER].events = ZMQ_POLLIN;
    self->pollers[P2PM_OTHER_POLLER].events = ZMQ_POLLIN;
    self->npollers = 4;
    /* set client_id buffer to all \0's */
    memset(self->clientid, '\0', P2PM_MAX_ID_LEN);

    /* set a timeout */
    p2pm_set_obj_wait(self);

    /* actions */
    self->server_triggered_actions = P2PM_NO_XPV;
    self->predecessor_socket_expected_actions = P2PM_NO_XPV;
    self->successor_socket_expected_actions = P2PM_NO_XPV;
    self->predecessor_triggered_actions = P2PM_NO_XPV;

    /* record our time */
    self->last_stabilize_time = zclock_mono();
    self->successor_lastpoll_time = zclock_mono();
    self->predecessor_lastpoll_time = zclock_mono();

    /* set a name for sockets */
    p2pm_set_name(self);

    /* empty tokens */
    self->recv_token_id = 0;
    self->recv_token_buf1 = NULL;
    self->recv_token_ttl = 0;
    self->send_token_id = 0;
    self->send_token_ttl = 0;
    self->send_token_buf1 = NULL;
    self->recv_token_buf2 = NULL;
    self->send_token_buf2 = NULL;
}
