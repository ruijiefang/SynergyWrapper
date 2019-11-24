/*
 * Created by Ruijie Fang on 2/10/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#include "p2pmd.h"


/*!
 * p2pmd(2)
 * \brief p2pmd operating under zactor framework.
 * \param pipe shared-memory pipe
 * \param args must be a casted, heap-allocated (p2pmd_t*) object
 */
void p2pmd_actor(zsock_t *pipe, void *args)
{
    assert(pipe);
    assert(args);
    puts("p2pmd here...");
    sleep(5);
    void *piped;
    p2pm_t *self;
    int64_t at, a, b, c;
    unsigned i;
    int64_t next_timeo;
    p2pm_init_args_t *_s_init = (p2pm_init_args_t *) args;
    self = malloc(sizeof(p2pm_t));
    p2pm_create(self, _s_init->endpoint, _s_init->config, _s_init->node_list, _s_init->log_handle, _s_init->status);
    srand(time(NULL));
    srandom(time(NULL));
    p2pm_set_obj_wait(self);
    p2pm_set_name(self);
    zlog_debug(self->log_handle, " *** I am %s ***", self->endpoint);
    if (_s_init->status == P2PM_NODE_MEMBER) {
        zlog_debug(self->log_handle, "connecting to predecessor %s, successor %s", self->node_list->predecessor,
                   self->node_list->successor);
        zmq_connect(self->predecessor_socket, self->node_list->predecessor);
        zmq_connect(self->successor_socket, self->node_list->successor);
    } else {
        zlog_debug(self->log_handle, "joining ring based on predecessor %s", self->node_list->predecessor);
        p2pm_opcodes_t jr = p2pm_join(self);
        if (jr != P2PM_OP_SUCCESS) {
            p2pm_destroy(self);
            free(self);
            return;
        }
    }
    p2pm_start_server(self, self->endpoint);
    self->npollers = 5;
    self->pollers[4].socket = zsock_resolve(pipe);
    self->pollers[4].events = ZMQ_POLLIN;
    next_timeo = self->config->wait;
    self->successor_lastpoll_time = zclock_mono();
    self->predecessor_lastpoll_time = zclock_mono();
    self->last_stabilize_time = zclock_mono();
    zlog_debug(self->log_handle, " ** p2pmd init complete **");
    /* a simple event loop, deliberately avoids using a queue */
    zsock_signal(pipe, 0);
    zlog_debug(self->log_handle, "** starting loop **");
    do {
        int rt = zmq_poll(self->pollers, self->npollers, next_timeo);
        if (rt < 0) goto _interrupted;
        if (self->pollers[4].revents & ZMQ_POLLIN) {

            void * psock = zsock_resolve(pipe);
            int type;
            zmq_recv(psock, &type, sizeof(int), 0);
            switch (type) {
                case TSD_P2PMD_GET_PREDECESSOR_ENDPT:
                zlog_debug(self->log_handle, "GET_PREDECESSOR_ENDPT called by tsd");
                    zmq_send(psock, self->node_list->predecessor, strlen(self->node_list->predecessor) + 1, 0);
                    break;
                case TSD_P2PMD_GET_SUCCESSOR_ENDPT:
                    zlog_debug(self->log_handle, "GET_SUCCESSOR_ENDPT called by tsd");
                    zmq_send(psock, self->node_list->successor, strlen(self->node_list->successor) + 1, 0);
                    break;

                case TSD_P2PMD_SEND_TOKEN:
                    zlog_debug(self->log_handle, "SEND_TOKEN called by tsd");
                    generic_token_t token;
                    zmq_recv(psock, &token, sizeof(generic_token_t), 0);
                    self->send_has_token = 1;
                    self->send_token_id = token.id;
                    self->send_token_ttl = token.ttl;
                    self->send_token_buf1 = token.buf1;
                    self->send_token_buf2 = token.buf2;
                    self->send_token_len1 = token.len1;
                    self->send_token_len2 = token.len2;
                    zlog_debug(self->log_handle, "p2pmd event loop: send_token copying complete.");
                    break;
                default: {
                    _interrupted:
                    zlog_debug(self->log_handle, "$TERM is issued");
                    p2pm_destroy(self);
                    free(self);
                    zlog_debug(self->log_handle, "return");
                    return;
                }
            }

        }
        /* handle timeo's */
        _process_timeo:
        p2pm_loop_server(self);
        /* if received a token, send to sink of pipe and nullify it */
        if (self->recv_has_token) {

            zlog_debug(self->log_handle, "p2pmd: received a token from REMOTE! id = %u,ttl=%d, len1=%lu, len2=%lu",
                       self->recv_token_id, self->recv_token_ttl, self->recv_token_len1, self->recv_token_len2);
            generic_token_t token =
                    {.buf1 = self->recv_token_buf1,
                    .buf2 = self->recv_token_buf2,
                    .len1 = self->recv_token_len1,
                    .len2 = self->recv_token_len2,
                    .id = self->recv_token_id,
                    .ttl = self->recv_token_ttl};
            zlog_debug(self->log_handle, "buf1(%s), buf2(%s)",
            self->recv_token_buf1, self->recv_token_buf2);
            zlog_debug(self->log_handle, "sending it up the pipe");
            void * psock = zsock_resolve(pipe);
            zmq_send(psock, &token, sizeof(generic_token_t), 0);
            self->recv_token_id = 0;
            self->recv_token_ttl = 0;
            self->recv_has_token = 0;
            self->recv_token_buf1 = NULL; /* destroy references */
            self->recv_token_buf2 = NULL; /* destroy references */
            self->recv_token_len1 = 0;
            self->recv_token_len2 = 0;
        }
        at = zclock_mono();
        /* calculate next wait interval */
        a = self->config->stabilize_interval - (at - self->last_stabilize_time);
        b = self->config->wait - (at - self->successor_lastpoll_time);
        c = self->config->wait - (at - self->predecessor_lastpoll_time);
        if (a < 0 || b < 0 || c < 0) /* this is expected behaviour, shall be handled in loop body */
            goto _process_timeo;
        if (a <= b && a <= c)
            next_timeo = a;
        else if (b <= a && b <= c)
            next_timeo = b;
        else
            next_timeo = c;
        zlog_debug(self->log_handle, "next timeout interval: %lu", next_timeo);
    } while (1);
}
