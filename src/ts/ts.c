/*
 * Created by Ruijie Fang on 2/7/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#include "ts.h"
#include "../p2pm_utilities.h"
#include "../zhelpers.h"

#define _MODE_MASTER 0
#define _MODE_WORKER 1
static
__thread void *__ts_ctx = NULL;

static inline void init_request(ts_t *self)
{
    void *using_socket = NULL;
    assert(self);
    using_socket = self->is_local ? self->_socket : self->_socket2;
    assert(using_socket);
    p2pm_send_uint32(using_socket, (uint32_t) self->tsnamelen, ZMQ_SNDMORE);
    zmq_send(using_socket, self->tsname, strlen(self->tsname), ZMQ_SNDMORE);
}

/*
 * Typical Synergy Workflow
 *
 * ts_open(): Upon creation, connect to tsd (using _socket), wait for tsd to tell us master/worker type,
 * and if is worker, block until either is direct (sent by tsd immediately) or token (until token arrives
 * and gets resolved by tsd); if is master, then create a shared memory region for putting stuff to
 * tsd's zlistx table.
 */

/**
 * The "Traditional" ts_open function.
 * @param self
 * @param name
 * @param ipc_name
 * @return
 */
int ts_open(ts_t *self, const char *name, const char *ipc_name, int mode)
{
    int mode_;
    /* currently treat all client endpoints as remote, although it's subject to change */
    assert(self);
    srand(time(NULL));
    srandom(time(NULL));
    unsigned tsnamelen;
    tsnamelen = (unsigned) strlen(name) + 1;
    self->tsnamelen = tsnamelen;
    memcpy(self->tsname, name, tsnamelen);

    if (__ts_ctx == NULL)
        __ts_ctx = zmq_ctx_new();

    if (tsnamelen > 50)
        return 1;

    self->ipc_name = ipc_name;
    /* now, use a bunch of sprintf's to gobble our names */

    struct {
        char name[50];
        char wsem_name[50];
        char rsem_name[50];
    } _ts_tsd_name, _tsd_ts_name;

    sprintf(_ts_tsd_name.name, "%s-push", name);
    sprintf(_tsd_ts_name.name, "%s-pull", name);
    sprintf(_ts_tsd_name.wsem_name, "%s-wsem", _ts_tsd_name.name);
    sprintf(_ts_tsd_name.rsem_name, "%s-rsem", _ts_tsd_name.name);
    sprintf(_tsd_ts_name.wsem_name, "%s-wsem", _tsd_ts_name.name);
    sprintf(_tsd_ts_name.rsem_name, "%s-rsem", _tsd_ts_name.name);

    /* end of all the naming gobble */

    //puts("ts_open()");

    /* handle zmq stuff */
    self->_ctx = __ts_ctx;
    if (self->_ctx == NULL) {
        //puts("err: context cannot be initialised");
        return 1;
    }
    self->_socket = zmq_socket(self->_ctx, ZMQ_DEALER);
    s_set_id(self->_socket);
    int rt = zmq_connect(self->_socket, ipc_name);
    sleep(1);
    if (rt != 0) {
_:
        //puts("err: cannot connect to socket");
        zmq_close(self->_socket);
        zmq_ctx_destroy(self->_ctx);
        return 1;
    }

    p2pm_send_uint32(self->_socket, tsnamelen, ZMQ_SNDMORE);
    zmq_send(self->_socket, name, tsnamelen, ZMQ_SNDMORE);

    if (p2pm_send_uint32(self->_socket, TsConnect, ZMQ_SNDMORE) < 0) {
        //puts("err: didn't send TsConnect command");
        goto _;
    }

    p2pm_send_uint32(self->_socket, (uint32_t) mode, 0);

    puts("ts: attempting to receive an integer");

    /* RECEIVE an indicator, telling us if it's master or not */

    if (p2pm_recv_uint32(self->_socket, &mode_, 0) != P2PM_OP_SUCCESS)
        return 1;

    switch (mode_) {
        case 2: /* is blocking */
        case 1: { /* worker tsd, or "proxy" */
_worker:
            /* worker */
            puts("ts: is worker");
            self->mode = (unsigned int) mode_;
            self->is_local = 0;
            p2pm_recv_str(self->_socket, &self->remote, 0);
            printf("ts: remote endpoint: %s\n", self->remote);
            self->_socket2 = zmq_socket(self->_ctx, ZMQ_DEALER);
            if (self->_socket2 == NULL) {
abort:
                puts("ts: err: socket2 cannot be initialized");
                zmq_close(self->_socket);
                zmq_close(self->_socket2);
                zmq_ctx_destroy(self->_ctx);
                return 1;
            }
            s_set_id(self->_socket2);
            if (zmq_connect(self->_socket2, self->remote) < 0) {
                puts("ts: err: connecting to remote endpoint");
                goto abort;
            }
	    sleep(2);
	    printf("ts: connecting to remote endpoint: %s\n", self->remote);
            p2pm_send_uint32(self->_socket2, tsnamelen, ZMQ_SNDMORE);
            zmq_send(self->_socket2, name, tsnamelen, ZMQ_SNDMORE);
            p2pm_send_uint32(self->_socket2, TsConnect, ZMQ_SNDMORE);
            p2pm_send_uint32(self->_socket2, (uint32_t) mode, 0);
	    printf("ts: attempting to receive mode from remote\n");
            p2pm_recv_uint32(self->_socket2, (uint32_t *) &mode, 0);
            break;
        }
        case 0: {
            puts("ts: local tsd is master");
            self->is_local = 1;
            self->mode = (unsigned int) mode_;
            /* only the master node needs fast in-memory pipelines */

            /* in below case, we're the publisher, so we call create */
            if (create_shared_memory(&self->_ts_tsd_pipe, _ts_tsd_name.name, _ts_tsd_name.wsem_name,
                                     _ts_tsd_name.rsem_name) != 0) {
                //puts("err: cannot create shared memory");
                goto abort;
            }
            /* in below case, we're the consumer, so we call open */
            if (open_shared_memory(&self->_tsd_ts_pipe, _tsd_ts_name.name, _tsd_ts_name.wsem_name,
                                   _tsd_ts_name.rsem_name) != 0) {
                //puts("err: cannot create shared memory");
                goto abort;
            }
            int rsemv, wsemv;
            sem_getvalue(self->_ts_tsd_pipe.r_sem, &rsemv);
            sem_getvalue(self->_ts_tsd_pipe.w_sem, &wsemv);
            //printf("ts_tsd_pipe rsem=%d,wsem=%d\n", rsemv, wsemv);
            sem_getvalue(self->_tsd_ts_pipe.r_sem, &rsemv);
            sem_getvalue(self->_tsd_ts_pipe.w_sem, &wsemv);
            //printf("tsd_ts_pipe rsem=%d,wsem=%d\n", rsemv, wsemv);
            /* ... and we're done ! */
            break;
        }
        case 3: {
            /* P2P mode */
            int p2p;
            p2pm_recv_uint32(self->_socket, (uint32_t *) &p2p, 0);
            break;
        }
        default: {
_fail:
            zmq_close(self->_socket);
            zmq_ctx_destroy(self->_ctx);
            return 1;
        }
    }

    return 0;
}

/**
 * Fill queues of local tsd with tuples of a specific pattern
 * @param self
 * @param pattern
 * @return
 */
int ts_expect(ts_t *self, const char *pattern)
{
    void *using_socket = NULL;
    assert(self);
    assert(pattern);
    using_socket = self->is_local ? self->_socket : self->_socket2;
    if (self->mode != TS_MODE_P2P)
        return 1;
    init_request(self);
    p2pm_send_uint32(using_socket, TsExpect, ZMQ_SNDMORE);
    p2pm_send_str(using_socket, (char *) pattern, 0);
    return 0;
}

#define OUT_BLOCK(x)\
((x)==TsGet_Blocking||(x)==TsRead_Blocking||(x)==TsPop_Blocking||(x)==TsPeek_Blocking)

#define IS_OUT_CMD(x)\
(((x)==TsGet||(x)==TsRead||(x)==TsPop||(x)==TsPeek)||OUT_BLOCK(x))

#define IS_FETCH(x)\
((x)!=TsGet_Blocking&&(x)!=TsGet&&(x)!=TsRead_Blocking&&(x)!=TsRead)

/**
 * Reads a tuple
 * @param self
 * @param tuple
 * @param cmd
 * @return
 */
int ts_out(ts_t *self, sng_tuple_t *tuple, tuple_command_t cmd)
{
    void *using_socket = NULL;
    tuple_command_t response = TsEnf;
    _Bool block = 0, fetch;
    assert(self);
    assert(tuple);
    assert(IS_OUT_CMD(cmd));
    block = OUT_BLOCK(cmd);
    fetch = IS_FETCH(cmd);
    if (fetch)
        assert(!tuple->name);
    else
        assert(tuple->name);

    /* do work here */

    using_socket = self->is_local ? self->_socket : self->_socket2;
    init_request(self);

    if (fetch) {
        p2pm_send_uint32(using_socket, cmd, 0);
    } else {
        p2pm_send_uint32(using_socket, cmd, ZMQ_SNDMORE);
        p2pm_send_str(using_socket, tuple->name, 0);
    }

    //printf("need_block? %d \n", block);

_handle_reply:

    p2pm_recv_uint32(using_socket, (uint32_t *) &response, 0);
    switch (response) {
        case TsAck: {
            //puts("ts: received TsACk");
            p2pm_recv_str(using_socket, &tuple->name, 0);
            if (self->is_local) {
                //puts("ts: is_local, returning recv_item");
                return recv_item(&self->_tsd_ts_pipe, &tuple->data, &tuple->len);
            } else {
                //puts("ts: is_remote, returning remotely received data");
                p2pm_recv_uint64(using_socket, (uint64_t *) &tuple->len, 0);
                //printf("ts: data len %lu\n", tuple->len);
                if (tuple->len <= 0)
                    return 1;
                tuple->data = malloc(tuple->len);
                if (zmq_recv(using_socket, tuple->data, tuple->len, 0) < 0) {
                    //puts("err: recv failure");
                    free(tuple->data);
                    tuple->len = 0;
                    return 1;
                }
                //puts("recv success");
                return 0;
            }
        }

        case TsAnon: {
            //puts("ts: received TsAnon");
            p2pm_recv_uint32(using_socket, &tuple->anon_id, 0);
            if (self->is_local)
                return recv_item(&self->_tsd_ts_pipe, &tuple->data, &tuple->len);
            else {
                p2pm_recv_uint64(using_socket, &tuple->len, 0);
                if (tuple->len <= 0)
                    return 1;
                tuple->data = malloc(tuple->len);
                if (zmq_recv(using_socket, tuple->data, tuple->len, 0) < 0) {
                    free(tuple->data);
                    tuple->len = 0;
                    return 1;
                }
                return 0;
            }
        }

        case TsEnf: {
            //puts("ts: received TsEnf");
            if (block) {
                //puts(" - blocking...");
                goto _handle_reply;
            }
            break;
        }

        case TsTerm: {
            //puts("ts: received TsTerm");
            return -1;
        }

        default:;
    }
    /* do work above */
    return 1;
}

/**
 * Writes a tuple
 * @param self
 * @param tuple
 * @param cmd
 * @return
 */
int ts_in(ts_t *self, sng_tuple_t *tuple, tuple_command_t cmd)
{
    void *using_socket = NULL;

    assert(cmd == TsPut || cmd == TsAdd);
    assert(cmd == TsPut ? tuple->name != NULL && !tuple->anon_id
                        : tuple->name == NULL && tuple->anon_id);
    assert(self);
    assert(tuple);
    assert(tuple->len > 0);
    assert(tuple->data != NULL);


    using_socket = self->is_local ? self->_socket : self->_socket2;

    init_request(self);

    p2pm_send_uint32(using_socket, cmd, ZMQ_SNDMORE);

    if (cmd == TsPut)
        p2pm_send_str(using_socket, tuple->name, self->is_local ? 0 : ZMQ_SNDMORE);
    else
        p2pm_send_uint32(using_socket, tuple->anon_id, self->is_local ? 0 : ZMQ_SNDMORE);

    if (self->is_local) {
        return send_item(&self->_ts_tsd_pipe, tuple->data, tuple->len);
    } else {
        p2pm_send_uint64(using_socket, tuple->len, ZMQ_SNDMORE);
        return zmq_send(using_socket, tuple->data, tuple->len, 0) < 0 ? 1 : 0;
    }

}

int ts_bcast_write(ts_t *self, sng_tuple_t *tuple)
{
    void *using_socket = NULL;
    tuple_command_t command;
    assert(self);
    assert(tuple);
    assert(tuple->name);
    assert(tuple->len > 0);
    assert(tuple->data);

    using_socket = self->is_local ? self->_socket : self->_socket2;
    init_request(self);

    p2pm_send_uint32(using_socket, TsBCast_Write, ZMQ_SNDMORE);

    if (self->is_local) {
        p2pm_send_str(using_socket, tuple->name, 0);
        return send_item(&self->_ts_tsd_pipe, tuple->data, tuple->len);
    } else {
        p2pm_send_str(using_socket, tuple->name, ZMQ_SNDMORE);
        p2pm_send_uint64(using_socket, tuple->len, ZMQ_SNDMORE);
        return zmq_send(using_socket, tuple->data, tuple->len, 0);
    }
}

int ts_bcast_read(ts_t *self, sng_tuple_t *tuple, int blocking)
{

    void *using_socket = NULL;
    tuple_command_t cmd = blocking ? TsBCast_Read : TsBCast_Read_Blocking;
    assert(self);
    assert(tuple);
    assert(tuple->name);

    using_socket = self->is_local ? self->_socket : self->_socket2;
    init_request(self);

    p2pm_send_uint32(using_socket, cmd, ZMQ_SNDMORE);
    p2pm_send_str(using_socket, tuple->name, 0);
_restart:

    p2pm_recv_uint32(using_socket, (uint32_t *) &cmd, 0);

    switch (cmd) {
        case TsTerm:
            return -1;
        case TsEnf:
            if (blocking) goto _restart;
            else return 1;
        case TsAck:
            if (self->is_local) {
                return recv_item(&self->_tsd_ts_pipe, &tuple->data, &tuple->len);
            } else {
                p2pm_recv_uint64(using_socket, &tuple->len, 0);
                if (tuple->len == 0)
                    return 1;
                tuple->data = malloc(tuple->len);
                if (tuple->data == 0) {
                    tuple->len = 0;
                    return 1;
                }
                return zmq_recv(using_socket, tuple->data, tuple->len, 0) < 0 ? 1 : 0;
            }
        default:;
    }
    return 1;
}

int ts_close(ts_t *self)
{

    assert(self);
    init_request(self);
    p2pm_send_uint32(self->is_local ? self->_socket : self->_socket2, TsTerm, 0);

    zmq_close(self->_socket);
    if(!self->is_local)
        zmq_close(self->_socket2);
    zmq_ctx_destroy(self->_ctx);
    self->is_term = 1;
    return 0;/* TODO */
}
