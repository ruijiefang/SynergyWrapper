/*
 * Created by Ruijie Fang on 2/6/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#include "tsd.h"
#include "../tuple_utils.h"
#include "../bv.h"

#define _SFREE(x) do { free((x)); (x) = NULL; } while(0)
#define _CLEAR_ARGS(x) do {_SFREE((x)->clientid_buffer); _SFREE((x)->namespace_buffer); } while (0)
#define _SAFE_RETURN(x, se, args, s) do { _CLEAR_ARGS((args)); if((x)==1) _ts_fail((se),(s)); return (x); } while (0)

struct _tsd_handler_arg {
    tsd_element_t *this_space;
    const char *this_space_name;
    char *clientid_buffer;
    char *namespace_buffer;
    tuple_command_t action;
    _Bool is_local;
    _Bool is_master;
};

/**
 * Sends a generic token to p2pmd ring.
 * @param self tsd_t structure
 * @param token a user-allocated token; the two pointers need be heap-allocated and never accessed/freed by
 * user because we're just doing pointer passing around threads.
 */
void send_generic_token(tsd_t *self, generic_token_t *token)
{
    const int send_token_command = TSD_P2PMD_SEND_TOKEN;
    assert(self);
    assert(token);
    assert(self->actor_pipe);
    void *acs = zsock_resolve(self->actor_pipe);
    assert(acs);
    zmq_send(acs, &send_token_command, sizeof(int), ZMQ_SNDMORE);
    zmq_send(acs, token, sizeof(generic_token_t), 0);
}

/**
 * Sends an "activate tuple space" signal
 * @param self tsd_t object
 * @param args fully-initialized args
 */
void ts_connect_send_activate_signal(tsd_t *self, struct _tsd_handler_arg *args)
{
    assert(self);
    if (!args->this_space->is_activated) { /* enable this space */
        args->this_space->is_activated = 1;
        args->this_space->is_activated = 1;
        /* now, construct a token */
        unsigned token_id = 0; /* activate */
        int token_ttl = -1;
        generic_token_t send_token = {
                .id = token_id,
                .ttl = token_ttl,
                .buf1 = strdup(args->this_space_name),
                .len1 = strlen(args->this_space_name) + 1,
                .buf2 = strdup(self->endpoint),
                .len2 = strlen(self->endpoint) + 1
        };
        send_generic_token(self, &send_token);
    }
}

/**
 * Sends a TsTerm signal around the ring to deactivate all tuple space daemon's tuple space handles of that space
 * @param self a tsd_t handle
 * @param args a fully-initialized args list
 */
void ts_term_send_decomission_signal(tsd_t *self, struct _tsd_handler_arg *args)
{
    assert(self);
    assert(args);
    if (args->this_space->is_activated) {
        args->this_space->is_activated = 0;
        generic_token_t send_token =
                {.id = 1,
                        .ttl = -1,
                        .buf1 = (void *) strdup(args->this_space_name),
                        .len1 = strlen(args->this_space_name) + 1,
                        .buf2 = (void *) strdup(self->endpoint),
                        .len2 = strlen(self->endpoint) + 1};
        send_generic_token(self, &send_token);
    }
}


/*
 * Token Prefix-Naming structure
 *     Tuple Space      Named/Anon   Tuple Name/Tuple Anon ID
 *  +------------------+-----------+----------------------+
 *  | namespace  [\0]  |   <N|A>   |    name / anon_id    |
 *  +------------------+-----------+----------------------+
 */

inline static void *_generate_named_tuple_token_str(const char *namespace_name, const char *tuple_name, size_t *tlen)
{
    assert(namespace_name);
    assert(tuple_name);
    assert(tlen);
    size_t len1 = strlen(namespace_name) + 1, len2 = strlen(tuple_name) + 1, tot = len1 + len2 + 1;
    char *dest = malloc(tot);
    memcpy(dest, namespace_name, len1);
    dest[++len1] = 'N'; /* for named */
    memcpy(dest + len1, tuple_name, len2);
    *tlen = tot;
    return (void *) dest;
}

inline static void *_generate_anon_tuple_token_str(const char *namespace_name, size_t anon_id, size_t *tlen)
{
    assert(namespace_name);
    assert(anon_id != 0);
    assert(tlen);
    size_t len = strlen(namespace_name) + 1, tot = len + sizeof(size_t) + 1;
    char *dest = malloc(tot);
    memcpy(dest, namespace_name, len);
    dest[++len] = 'A'; /* for anonymous */
    memcpy(dest + len, &anon_id, sizeof(size_t));
    *tlen = tot;
    return (void *) dest;
}


/**
 * Constructor method.
 * @param self tsd_t object
 * @param p2pmv arguments for starting p2pmd
 * @param config detailed configs from parsed config
 * @return
 */
int tsd_create(tsd_t *self, p2pm_init_args_t *p2pmv, sng_cfg_t *config)
{

    assert(self);

    /* set logger */
    self->log_handle = zlog_get_category("tsd");
    if (!self->log_handle)
        return 1;

    /* allocate hash table */
    self->space_table = AllocateHashTable(0, 1);

    /* set sockets */
    self->endpoint = config->tsd_endpoint;
    self->_ctx = zmq_ctx_new();
    self->local_socket = zmq_socket(self->_ctx, ZMQ_ROUTER);
    self->server_socket = zmq_socket(self->_ctx, ZMQ_ROUTER);
    self->peer_socket = zmq_socket(self->_ctx, ZMQ_DEALER);

    /* open sockets */
    zlog_debug(self->log_handle, "binding to ipc transport %s.", self->ipc_name);
    int rt = zmq_bind(self->local_socket, self->ipc_name);
    if (rt != 0) {
        zlog_debug(self->log_handle, "zmq_bind() error");
        return 1;
    }
    zlog_debug(self->log_handle, "binding to remote transport: %s.", config->tsd_endpoint);
    rt = zmq_bind(self->server_socket, config->tsd_endpoint);
    if (rt != 0) {
        zlog_debug(self->log_handle, "zmq_bind() error 2");
        return 1;
    }

    /* iterate through each space and initialize them */
    unsigned i;
    for (i = 0; i < config->num_spaces; ++i) {
        /* create an element */
        tsd_element_t *element = malloc(sizeof(tsd_element_t));

        /*set its name */
        element->name = config->spaces[i].name;

        /* set its mode */
        element->ts_mode = config->spaces[i].ts_mode;
        element->tsd_mode = config->spaces[i].tsd_mode;

        /* Storage queues */
        element->bcast_queue = zlistx_new();
        element->named_queue = zlistx_new();

        /* Request queues */
        element->named_queue_requests = zlistx_new();
        element->bcast_queue_requests = zlistx_new();

        /* Connect request queue */
        element->connect_requests = zlistx_new();

        /* expect queue */
        element->expect_queue = zlistx_new();

        /* Copy direct endpoint. In case it doesn't exist, we just copy \0's */
        memcpy(element->direct_endpoint, config->spaces[i]._direct_endpoint, P2PM_MAX_ID_LEN);

        /* Utilize this anonymous struct to set the name of shared memory segments */
        struct {
            char name[50];
            char rsem_name[50];
            char wsem_name[50];
        } _tsd_ts_name, _ts_tsd_name;
        sprintf(_tsd_ts_name.name, "%s-pull", config->spaces[i].name);
        sprintf(_tsd_ts_name.rsem_name, "%s-rsem", _tsd_ts_name.name);
        sprintf(_tsd_ts_name.wsem_name, "%s-wsem", _tsd_ts_name.name);
        sprintf(_ts_tsd_name.name, "%s-push", config->spaces[i].name);
        sprintf(_ts_tsd_name.rsem_name, "%s-rsem", _ts_tsd_name.name);
        sprintf(_ts_tsd_name.wsem_name, "%s-wsem", _ts_tsd_name.name);
        /* Can't do the following; there might be multiple tsd's on one host
         * For testing purposes only. Re-add upon production code */
        /* close_shared_memory(_tsd_ts_name.name, _tsd_ts_name.wsem_name, _tsd_ts_name.rsem_name);
           close_shared_memory(_ts_tsd_name.name, _ts_tsd_name.wsem_name, _ts_tsd_name.rsem_name); */
        /* Create shared memory */
        if (create_shared_memory(&element->tsd_ts_pipe, _tsd_ts_name.name, _tsd_ts_name.wsem_name,
                                 _tsd_ts_name.rsem_name) != 0) {
            zlog_debug(self->log_handle, "creating shared memory for %s failed\n", _tsd_ts_name.name);
            return 1;
        }
        if (open_shared_memory(&element->ts_tsd_pipe, _ts_tsd_name.name, _ts_tsd_name.wsem_name,
                               _ts_tsd_name.rsem_name) != 0) {
            zlog_debug(self->log_handle, "creating shared memory for %s failed\n", _ts_tsd_name.name);
            return 1;
        }

        /* set initial states */

        element->is_activated = (element->tsd_mode == TsdModeDirect) || (element->ts_mode == TsModeP2P);
        element->is_requested = 0;

        /* configure the queues */

        zlistx_set_comparator(element->named_queue, tplcmp);
        zlistx_set_duplicator(element->named_queue, NULL);
        zlistx_set_destructor(element->named_queue, tplfree);

        zlistx_set_comparator(element->bcast_queue, tplcmp);
        zlistx_set_duplicator(element->bcast_queue, NULL);
        zlistx_set_destructor(element->bcast_queue, tplfree);

        zlistx_set_comparator(element->named_queue_requests, (zlistx_comparator_fn *) blckcmp_str);
        zlistx_set_duplicator(element->named_queue_requests, (zlistx_duplicator_fn *) NULL);
        zlistx_set_destructor(element->named_queue_requests, (zlistx_destructor_fn *) blckfree);

        zlistx_set_comparator(element->bcast_queue_requests, (zlistx_comparator_fn *) blckcmp_str);
        zlistx_set_duplicator(element->bcast_queue_requests, (zlistx_duplicator_fn *) NULL);
        zlistx_set_destructor(element->bcast_queue_requests, (zlistx_destructor_fn *) blckfree);


        zlistx_set_comparator(element->expect_queue, (zlistx_comparator_fn *) kcmp);
        zlistx_set_duplicator(element->expect_queue, (zlistx_duplicator_fn *) NULL);
        zlistx_set_destructor(element->expect_queue, (zlistx_destructor_fn *) sfree_l);

        /* we don't need to set anything for connect_requests, anything needs to be processed  */

        HashInsert(self->space_table, PTR_KEY(self->space_table, config->spaces[i].name),
                   PTR_KEY(self->space_table, element));
        zlog_debug(self->log_handle, "creating %uth element of hash table, name %s", i, config->spaces[i].name);
    }
    zlog_debug(self->log_handle, " *** creation complete. starting p2pmd... ***");
    self->p2pmd_actor = zactor_new((zactor_fn *) p2pmd_actor, p2pmv);
    printf("*** p2pmd initialization complete. ***\n");
    self->actor_pipe = zactor_sock(self->p2pmd_actor);
    zlog_debug(self->log_handle, " *** p2pmd started. returning... ***");
    return 0;
}

/**
 * _ts_fail(2)
 * \brief Flushes a socket after an experienced failure.
 */
void _ts_fail(tsd_t *self, void *socket)
{
    char useless_buf[50];
    int has_more = 0;
    size_t hm_size = sizeof(has_more);
    zmq_getsockopt(socket, ZMQ_RCVMORE, &has_more, &hm_size);
    while (has_more) {    /* of course, we don't really hope this to occur */
        zlog_debug(self->log_handle, " - draining...");
        zmq_recv(socket, useless_buf, 50, ZMQ_NOBLOCK);
        zmq_getsockopt(socket, ZMQ_RCVMORE, &has_more, &hm_size);
    }
}

/**
 * handle_named_queue_read_response(6)
 * \brief Handles responses to read requests
 */
int
handle_named_queue_read_response(tsd_t *self, void *socket, sng_tuple_t *tuple, tsd_element_t *space, _Bool is_local)
{
    assert(self);
    assert(socket);
    assert(tuple);
    assert(space);

    zlog_debug(self->log_handle, "Found tuple(%s) len(%lu). Sending...", tuple->name, tuple->len);
    zlog_debug(self->log_handle, is_local ? "Request is local" : "Request is remote");
    /* TsMore indicates we have more data to follow, a.k.a. a result tuple */
    p2pm_send_uint32(socket, TsAck, ZMQ_SNDMORE);

    /* send name of result tuple */
    p2pm_send_str(socket, tuple->name, is_local ? 0 : ZMQ_SNDMORE);

    /* send using different paths */
    if (is_local) {
        zlog_debug(self->log_handle, " - sending via shared memory...");
        /* pass thru shared memory */
        send_item(&space->tsd_ts_pipe, tuple->data, tuple->len);
    } else {
        /* is remote */
        p2pm_send_uint64(self->server_socket, tuple->len, ZMQ_SNDMORE);
        zlog_debug(self->log_handle, "zmq_send'ing tuple's data...");
        zmq_send(self->server_socket, tuple->data, tuple->len, 0);
        zlog_debug(self->log_handle, "Done.");
    }

    /* successfully handled query */
    zlog_debug(self->log_handle, "(TsRead/TsGet) success. Freeing tuples...");
    return 0;
}

/**
 * handle_named_queue_fetch_response(6)
 * \brief Handles responses to anonymous requests to named_queue.
 */
int
handle_named_queue_fetch_response(tsd_t *self, void *socket, sng_tuple_t *tuple, tsd_element_t *space, _Bool is_local)
{
    assert(self);
    assert(socket);
    assert(tuple);
    assert(space);

    zlog_debug(self->log_handle, "Response tuple name %s id %u len %lu)", tuple->name, tuple->anon_id, tuple->len);

    /* Different send procedures for anonymous tuples */
    if (tuple->name == NULL && tuple->anon_id != 0) {
        /* is anon */
        zlog_debug(self->log_handle, "Tuple is anonymous tuple. Sending info...");

        /* Tell our client we're anonymous */
        p2pm_send_uint32(socket, TsAnon, ZMQ_SNDMORE);
        /* Send according to different location */
        if (is_local) {
            p2pm_send_uint32(socket, tuple->anon_id, 0);
            send_item(&space->tsd_ts_pipe, tuple->data, tuple->len);
        } else {
            /* use remote */
            p2pm_send_uint32(socket, tuple->anon_id, ZMQ_SNDMORE);
            p2pm_send_uint64(socket, tuple->len, ZMQ_SNDMORE);
            zmq_send(self->server_socket, tuple->data, tuple->len, 0);
        }
    } else {

        /* is normal tuple */
        zlog_debug(self->log_handle, "tuple is normal tuple. Sending TsMore...");
        /* Tell client we're normal tuple */
        p2pm_send_uint32(socket, TsAck, ZMQ_SNDMORE);
        /* Send according to different platforms */
        if (is_local) {
            p2pm_send_str(socket, tuple->name, ZMQ_SNDMORE);
            send_item(&space->tsd_ts_pipe, tuple->data, tuple->len);
        } else {
            p2pm_send_str(socket, tuple->name, ZMQ_SNDMORE);
            p2pm_send_uint64(socket, tuple->len, ZMQ_SNDMORE);
            zmq_send(socket, tuple->data, tuple->len, 0);
        }
    }
    zlog_debug(self->log_handle, "Successfully handled TsPop/TsPeek.");

    return 0;
}

/**
 * handle_bcast_queue_read_response(6)
 * \brief Handles bcast queue read responses.
 */
int
handle_bcast_queue_read_response(tsd_t *self, void *socket, sng_tuple_t *tuple, tsd_element_t *space, _Bool is_local)
{
    assert(self);
    assert(socket);
    assert(tuple);
    assert(space);

    zlog_debug(self->log_handle, "sending BCast responses...");

    p2pm_send_uint32(socket, TsAck, ZMQ_SNDMORE);

    if (is_local) {
        p2pm_send_str(socket, tuple->name, 0);
        send_item(&space->tsd_ts_pipe, tuple->data, tuple->len);
    } else {
        p2pm_send_str(socket, tuple->name, ZMQ_SNDMORE);
        p2pm_send_uint64(socket, tuple->len, ZMQ_SNDMORE);
        zmq_send(socket, tuple->data, tuple->len, 0);
    }

    zlog_debug(self->log_handle, "Done");

    return 0;

}

/**
 * \brief Processes blocking requests upon any write actions
 * \param self the tsd object
 * \param space the current space
 * \param tuple the written tuple
 * \param action the write action
 * \return
 */
int match_blocking_requests(tsd_t *self, tsd_element_t *space, sng_tuple_t *tuple, tuple_command_t action)
{
    /* declarations */
    zlistx_t *op_queue = NULL, *_queue = NULL;
    void *_handle = NULL,  /* handle for list element */
            *using_socket = NULL; /* using this socket */
    _Bool need_append = 1; /* do we need to add to queue? */
    blocking_request_t *matched = NULL; /* matched request */
    blocking_request_t search_request = {.name = tuple->name, .extra = tuple->anon_id};
    int r;
    /* assertions */

    assert(self);
    assert(space);
    assert(tuple);


    zlog_debug(self->log_handle, "Dispatched match_blocking_requests");

    /* assign op_queue value according to different actions */
    switch (action) {
        case TsPut:
        case TsAdd:
            zlog_debug(self->log_handle, "focusing on named_queue_requests");
            op_queue = space->named_queue_requests;
            _queue = space->named_queue;
            break;
        case TsBCast_Write:
            zlog_debug(self->log_handle, "focusing on BCast queue requests");
            op_queue = space->bcast_queue_requests;
            _queue = space->bcast_queue;
            break;
        default:
            zlog_debug(self->log_handle, "Sorry, the request is not supported");
            return 1; /* not implemented */
    }

    /* terminates whenever 1) cannot process further work (work has been grabbed) 2) no further work available */
    need_append = 1;
    do {
        zlog_debug(self->log_handle, " >> finding a request tuple...");
        _handle = zlistx_find(op_queue, &search_request);
        if (_handle == NULL || !need_append) {
            zlog_debug(self->log_handle, "-- handle == NULL, returning...");
            if (_handle != NULL) {
                zlog_debug(self->log_handle, "-- !need_append");
                matched = zlistx_detach(op_queue, _handle);
                blckfree_g(matched);
            }
            if (need_append) {
                zlog_debug(self->log_handle, "-- need_append, appending to queue");
                zlistx_add_end(_queue, tplcpy(tuple));
                if (space->ts_mode == TsModeP2P) {
                    size_t _len;
                    char *_name = action == TsAdd ?
                                  _generate_anon_tuple_token_str(space->name, tuple->anon_id, &_len) :
                                  _generate_named_tuple_token_str(space->name, tuple->name, &_len);
                    generic_token_t _repl_token = {
                            .ttl = self->replication_factor,
                            .id = 3,
                            .buf1 = _name,
                            .len2 = _len,
                            .buf2 = strdup(self->endpoint),
                            .len2 = strlen(self->endpoint) + 1
                    };
                    send_generic_token(self, &_repl_token);
                }
            }
            break;
        }
        zlog_debug(self->log_handle, ">> found a match");
        matched = zlistx_detach(op_queue, _handle);
        using_socket = matched->local ? self->local_socket : self->server_socket;
        if (matched->command == TsPop_Blocking || matched->command == TsGet_Blocking)
            need_append = 0; /* from now on don't add to queue this tuple anymore; it has been consumed */
        /* send it */
        zlog_debug(self->log_handle, "sending match to %s", matched->clientid);
        zlog_debug(self->log_handle, "match data: local? %d, command %d, name %s", matched->local, matched->command,
        matched->name);
        zmq_send(using_socket, matched->clientid, strlen(matched->clientid), ZMQ_SNDMORE);
        switch (matched->command) {
            case TsGet_Blocking:
            case TsRead_Blocking:
            case TsPop_Blocking:
            case TsPeek_Blocking:
                r = handle_named_queue_read_response(self, using_socket, tuple, space, matched->local);
                break;
            case TsBCast_Read_Blocking:
                r = handle_bcast_queue_read_response(self, using_socket, tuple, space, matched->local);
                break;
            default:
                return -1;
        }

        /* free the matched object */
        blckfree_g(matched);
    } while (1);

    return 0;
}

/**
 * named_queue_write_handler(2)
 * \brief Handles TsPut/TsAdd requests
 * \param self a tsd_t* object
 * \param args arguments, must be _fully filled_ -- namespace_buf/clientid_buf must all be present
 * \return 0 for success, 2 for incomplete match (for blocking requests), -1 for serious error, 1 for dilemma
 */
int named_queue_write_handler(tsd_t *self, struct _tsd_handler_arg *args)
{

    sng_tuple_t write_tuple = {.name = NULL, .data = NULL, .len = 0, .anon_id = 0};
    void *using_socket;
    int rt;
    /* assertions */
    assert(self);
    assert(args);
    assert(args->clientid_buffer);
    assert(args->namespace_buffer);
    assert(args->this_space);
    assert(args->this_space_name);
    assert(args->action == TsPut || args->action == TsAdd);

    zlog_debug(self->log_handle, "Dispatched TsWrite _NamedQueue.");
    using_socket = args->is_local ? self->local_socket : self->server_socket;

    switch (args->action) {
        case TsPut: {

            /* receive write tuple name */
            rt = p2pm_recv_str(using_socket, &write_tuple.name, 0);

            if (rt != P2PM_OP_SUCCESS || write_tuple.name == NULL) {
                zlog_debug(self->log_handle, "Receive tuple name failed. Draining socket and returning...");
                free(write_tuple.name);
                _SAFE_RETURN(1, self, args, using_socket);
            }

            zlog_debug(self->log_handle, "Received tuple name: %s.", write_tuple.name);
            if (args->is_local) {
                zlog_debug(self->log_handle, "Receiving tuple via shared memory...");
                rt = recv_item(&args->this_space->ts_tsd_pipe, &write_tuple.data, &write_tuple.len);
                zlog_debug(self->log_handle, "Tuple len = %lu", write_tuple.len);
            } else {
                zlog_debug(self->log_handle, "receiving tuple len...");
                rt = p2pm_recv_uint64(using_socket, (uint64_t *) &write_tuple.len, 0);
                if (rt != P2PM_OP_SUCCESS || write_tuple.len == 0) {
                    zlog_debug(self->log_handle, "err: len == 0");
                    free(write_tuple.name);
                    _SAFE_RETURN(1, self, args, using_socket);
                }
                zlog_debug(self->log_handle, "receiving data... (len = %lu)", write_tuple.len);
                /* allocate space for data */
                write_tuple.data = malloc(write_tuple.len);
                /* receive data */
                zmq_recv(using_socket, write_tuple.data, write_tuple.len, 0);
            }
            /* match or not match ? */
            if (args->this_space->ts_mode == TsModeP2P) {
                zlog_debug(self->log_handle, "Current mode == P2P. Broadcast info...");
                zlistx_add_end(args->this_space->named_queue, tplcpy(&write_tuple));
                /* send such info */
                size_t token_name_len;
                void *token_name1 = _generate_named_tuple_token_str(args->this_space_name, write_tuple.name,
                                                                    &token_name_len);
                generic_token_t wtuple_token = {
                        .id = 2,
                        .ttl = -1,
                        .buf1 = token_name1,
                        .buf2 = (void *)strdup(self->endpoint),
                        .len1 = token_name_len,
                        .len2 = strlen(self->endpoint) + 1};
                send_generic_token(self, &wtuple_token);
            } else {
                zlog_debug(self->log_handle, "Calling match_blocking_requests()...");
                rt = match_blocking_requests(self, args->this_space, &write_tuple, args->action);
            }
            _SAFE_RETURN(0, self, args, using_socket);
        }
        case TsAdd: {
            rt = p2pm_recv_uint32(using_socket, &write_tuple.anon_id, 0);
            zlog_debug(self->log_handle, "Received id=%u", write_tuple.anon_id);
            if (rt != P2PM_OP_SUCCESS || write_tuple.anon_id == 0) {
                zlog_debug(self->log_handle, "Error: tuple length == 0");
                _SAFE_RETURN(1, self, args, using_socket);
            }
            if (args->is_local) {
                zlog_debug(self->log_handle, "Receiving tuple via shared memory...");
                rt = recv_item(&args->this_space->ts_tsd_pipe, &write_tuple.data, &write_tuple.len);
                zlog_debug(self->log_handle, "Tuple len = %lu", write_tuple.len);

                if (rt != 0 || write_tuple.data == NULL) {
                    free(write_tuple.data);
                    _SAFE_RETURN(1, self, args, using_socket);
                }
                rt = match_blocking_requests(self, args->this_space, &write_tuple, args->action);
                return rt;
            } else {
                p2pm_recv_uint64(using_socket, (uint64_t *) &write_tuple.len, 0);
                /* allocate space */
                write_tuple.data = malloc(write_tuple.len);
                rt = zmq_recv(using_socket, write_tuple.data, write_tuple.len, 0);
                if (rt < 0) {
                    zlog_debug(self->log_handle, "Error when receiving tuple data");
                    free(write_tuple.data);
                    _SAFE_RETURN(1, self, args, using_socket);
                }
                if (args->this_space->ts_mode == TsModeP2P) {
                    zlog_debug(self->log_handle, "curr mode = P2P, brodcasting info...");
                    size_t token_name_len;
                    void *token_name1 =
                            _generate_anon_tuple_token_str(args->this_space_name, write_tuple.anon_id, &token_name_len);
                    generic_token_t wtuple_token = {
                            .id = 2,
                            .ttl = -1,
                            .buf1 = token_name1,
                            .buf2 = (void *) strdup(self->endpoint),
                            .len1 = token_name_len,
                            .len2 = strlen(self->endpoint + 1)
                    };
                    send_generic_token(self, &wtuple_token);
                } else {
                    zlog_debug(self->log_handle, "calling match_blocking_requests()...");
                    rt = match_blocking_requests(self, args->this_space, &write_tuple, args->action);
                }
                _SAFE_RETURN(rt, self, args, using_socket);
            }
        }
        default:
            _SAFE_RETURN(-1, self, args, using_socket);
    }
}

/**
 * named_queue_fetch_handler(2)
 * \brief Handles "fetch" requests on named_queue, i.e. TsPop and TsPeek
 * \param self an initialized tsd_t* object
 * \param args arguments (MUST be fully filled, i.e. clientid_buffer and namespace_buffer)
 * \return 0 on success
 */
int named_queue_fetch_handler(tsd_t *self, struct _tsd_handler_arg *args)
{

    /* declarations */
    sng_tuple_t *reply_tuple = NULL;
    void *using_socket = NULL;
    _Bool is_blocking, is_pop, is_peek;


    /* assertions */
    assert(self);
    assert(args);
    assert(args->clientid_buffer);
    assert(args->namespace_buffer);
    assert(args->this_space);
    assert(args->this_space_name);
    assert(args->action == TsPop || args->action == TsPeek
           || args->action == TsPop_Blocking || args->action == TsPeek_Blocking);

    /* assignments */
    using_socket = args->is_local ? self->local_socket : self->server_socket;
    is_blocking = args->action == TsPop_Blocking || args->action == TsPeek_Blocking;
    is_pop = args->action == TsPop || args->action == TsPop_Blocking;
    is_peek = args->action == TsPeek || args->action == TsPeek_Blocking;

    /* send reply first */
    zmq_send(using_socket, args->clientid_buffer, strlen(args->clientid_buffer), ZMQ_SNDMORE);

    /* check size of queue */
    zlog_debug(self->log_handle, "checking size of named_queue != 0");

    if (zlistx_size(args->this_space->named_queue) == 0) {
        if (is_blocking) {
            zlog_debug(self->log_handle, "queue is empty, but adding request to incoming queue...");
            blocking_request_t blck = {
                    .local=args->is_local,
                    .name=NULL,
                    .clientid=args->clientid_buffer,
                    .command=args->action,
                    .extra=0};
            blocking_request_t * blck_ = blckcpy(&blck);
            assert(blck_->clientid == args->clientid_buffer);
            assert(blck_->command == args->action);
            assert(args->this_space->named_queue_requests);
            zlistx_add_end(args->this_space->named_queue_requests, blck_);
            args->clientid_buffer = NULL; /* destroy reference */
        }
        /* send Enf */
        zlog_debug(self->log_handle, "error: named_queue is empty. Sending TsEof...");
        p2pm_send_uint32(using_socket, TsEnf, 0);

        _SAFE_RETURN(2, self, args, using_socket);
    }

    zlog_debug(self->log_handle, "done. popping an item...");

    /* attempt to select one */
    if (is_pop) {
        reply_tuple = zlistx_detach(args->this_space->named_queue, NULL);
    } else
        reply_tuple = zlistx_first(args->this_space->named_queue);

    /* NULL-check */
    if (reply_tuple == NULL) {
        if (is_blocking) {
            blocking_request_t blck = {
                    .local = args->is_local,
                    .clientid = args->clientid_buffer,
                    .name = NULL, .command = args->action,
                    .extra = 0};
            zlistx_add_end(args->this_space->named_queue_requests, blckcpy(&blck));
        }
        /* send Enf */
        zlog_debug(self->log_handle, "Error: tuple is NULL. Sending TsEnf and break.");
        p2pm_send_uint32(self->local_socket, TsEnf, 0);
        _SAFE_RETURN(2, self, args, using_socket);
    }

    zlog_debug(self->log_handle, "detached an item. (Tuple name %s id %u len %lu)", reply_tuple->name,
               reply_tuple->anon_id, reply_tuple->len);

    handle_named_queue_fetch_response(self, using_socket, reply_tuple, args->this_space, args->is_local);

    if (args->action == TsPop) {
        zlog_debug(self->log_handle, " - TsPop so freeing tuple...");
        tplfree_g(reply_tuple);
        zlog_debug(self->log_handle, " - done.");
    }
    _SAFE_RETURN(0, self, args, using_socket);
}

/**
 * named_queue_read_handler(2)
 * \brief
 *   processes TsGet_*,TsRead_* requests from
 *   both local and remote client/master sources;
 *   requires namespace/clientid already be read
 *   and processed.
 * \param self A constructed tsd_t object
 * \param args Arguments structure, all its members _must be set_
 * \return 0 on success, 1 on failure, 2 on not found (not found should be treated as falid)
 */
int named_queue_read_handler(tsd_t *self, struct _tsd_handler_arg *args)
{
     /* declarations */
    char *_name = NULL; /* name of tuple */
    void *tpl_handle = NULL, /* zlistx handle for result tuple () */
            *using_socket = NULL; /* the socket we're using */
    sng_tuple_t *result_tuple = NULL, /* result tuple of (of the query, if any) */
            search_tuple = {.name = NULL, .data = NULL, .anon_id = 0, .len = 0};/* tuple used to construct the search */
    int rt;

    /* assertions */
    assert(self);
    assert(args);
    assert(args->clientid_buffer);
    assert(args->namespace_buffer);
    assert(args->this_space);
    assert(args->this_space_name);
    assert(args->action == TsRead || args->action == TsRead_Blocking || args->action == TsGet ||
           args->action == TsGet_Blocking);

   zlistx_set_comparator(args->this_space->named_queue, tplcmp);

    /* determine which socket to use */

    using_socket = args->is_local ? self->local_socket : self->server_socket;

    /* begin dispatch actions */

    zlog_debug(self->log_handle, "Dispatched (TsRead|TsGet)\nReceiving tuple name (string)...");

    /* receive query tuple name */
    rt = p2pm_recv_str(using_socket, &_name, 0);
    zlog_debug(self->log_handle, "Checking tuple name (string)...");

    /* check receive */
    if (rt != P2PM_OP_SUCCESS || _name == NULL) {
        zlog_debug(self->log_handle, "Error: no successful receive. returning with an error (%s)", _name);
        _SAFE_RETURN(1, self, args, using_socket);
    }

    search_tuple.name = _name;
    search_tuple.anon_id = 0;
    /* now this is considered a valid action - enqueue the reply header as multipart msg in zeromq */
    zmq_send(using_socket, args->clientid_buffer, strlen(args->clientid_buffer), ZMQ_SNDMORE);

    /* */
    if (!args->is_local && !args->this_space->is_activated) {
        zlog_debug(self->log_handle, "ERR: namespace isn't activated yet!");
        free(_name);
        p2pm_send_uint32(self->server_socket, TsEnf, 0);
        _SAFE_RETURN(1, self, args, using_socket);
    }

    zlog_debug(self->log_handle, "Searching for tuple name (%s)", _name);

    /* construct search */
    search_tuple.name = _name;

    /* separate procedures for Get and Read */
    if (args->action == TsGet || args->action == TsGet_Blocking) {
        /* if it's Get, detach item */
        tpl_handle = zlistx_find(args->this_space->named_queue, &search_tuple);
        /* if handle's NULL, fail */
        if (tpl_handle == NULL) goto tpl_nf;

        /* else, detach item */
       result_tuple = zlistx_detach(args->this_space->named_queue, tpl_handle);
    } else {
        /* if it's Read, just read item */
        tpl_handle = zlistx_find(args->this_space->named_queue, &search_tuple);
        /* if handle's NULL, fail */
        if (tpl_handle == NULL) goto tpl_nf;

        /* else, read the item */
        result_tuple = zlistx_handle_item(tpl_handle);
    }

    zlog_debug(self->log_handle, "Finished searching.");

    /* check if search completed with a NULL */
    if (result_tuple == NULL) {

tpl_nf:

        zlog_debug(self->log_handle, "ERR: Cannot find such tuple named %s", _name);

        /* If action is blocking, then add request to blocking request queue */
        if (args->action == TsRead_Blocking || args->action == TsGet_Blocking) {

            zlog_debug(self->log_handle, " - Is blocking request, adding...");

            /* construct a blocking request */
            blocking_request_t blck = {
                    .name=_name,    /* tuple name contains our request */
                    .command=args->action,  /* our action required by request */
                    .extra=0,
                    .clientid= args->clientid_buffer,   /* returning client id */
                    .local = args->is_local  /* use local path */
            };

            /* add the blocking request */
            zlistx_add_end(args->this_space->named_queue_requests, blckcpy(&blck));

            zlog_debug(self->log_handle, " - Finished.");

            args->clientid_buffer = NULL; /* destroy reference; we need to hold it in blocking requests*/
        }

        /* return an answer telling client request is not found */

        zlog_debug(self->log_handle, " - sending ENF...");

        p2pm_send_uint32(using_socket, TsEnf, 0);

        /* clean up */
        _SAFE_RETURN(2, self, args, using_socket);
    }

    /* We have now found a tuple, send its data */

    zlog_debug(self->log_handle, "Found tuple(%s) -> (%s) len(%lu). Sending... [%d]", _name, result_tuple->name,
               result_tuple->len, tplcmp(result_tuple, &search_tuple));

   // assert(tplcmp(result_tuple, &search_tuple) == 0);
    assert(tplcmp(&search_tuple, result_tuple) == 0);
    handle_named_queue_read_response(self, using_socket, result_tuple, args->this_space, args->is_local);

    /* successfully handled query */
    zlog_debug(self->log_handle, "(TsRead/TsGet) success. Freeing tuples...");

    /* If is TsGet, free our result tuple */
    if (args->action == TsGet || args->action == TsGet_Blocking) {
        zlog_debug(self->log_handle, "Is TsGet, freeing tuple.");
        tplfree_g(result_tuple);
    }

    /* free query name */
    free(_name);

    _SAFE_RETURN(0, self, args, using_socket);
}

/**
 * Writes to the bcast queue
 * @param self
 * @param args
 * @return
 */
int bcast_queue_write_handler(tsd_t *self, struct _tsd_handler_arg *args)
{
    void *using_socket = NULL;
    sng_tuple_t write_tuple = {.name = NULL, .anon_id = 0, .len = 0, .data = NULL};
    int rt;
    /* assertions */
    assert(self);
    assert(args);
    assert(args->clientid_buffer);
    assert(args->namespace_buffer);
    assert(args->this_space);
    assert(args->this_space_name);
    assert(args->action == TsBCast_Write);

    zlog_debug(self->log_handle, "Dispatching TsBCast_Write.");

    using_socket = args->is_local ? self->local_socket : self->server_socket;

    /* receive write tuple name */
    rt = p2pm_recv_str(using_socket, &write_tuple.name, 0);
    if (write_tuple.name == NULL || rt != P2PM_OP_SUCCESS) {
        zlog_debug(self->log_handle, "Err: Cannot receive tuple name");
        free(write_tuple.name);
        _SAFE_RETURN(1, self, args, using_socket);
    }
    zlog_debug(self->log_handle, "received tuple name = %s", write_tuple.name);

    if (args->is_local) {
        zlog_debug(self->log_handle, "receiving data through shared memory...");

        rt = recv_item(&args->this_space->ts_tsd_pipe, &write_tuple.data, &write_tuple.len);
        if (rt != 0 || write_tuple.data == NULL || write_tuple.len == 0) {
            free(write_tuple.name);
            free(write_tuple.data);
            _SAFE_RETURN(-1, self, args, using_socket);
        }
    } else {
        /* remote */
        rt = p2pm_recv_uint64(using_socket, (uint64_t *) &write_tuple.len, 0);
        if (rt != P2PM_OP_SUCCESS || write_tuple.len == 0) {
            free(write_tuple.name);
            _SAFE_RETURN(2, self, args, using_socket);
        }
        write_tuple.data = malloc(write_tuple.len);
        if (write_tuple.data == NULL) {
            free(write_tuple.name);
            _SAFE_RETURN(2, self, args, using_socket);
        }
        rt = zmq_recv(using_socket, &write_tuple.data, write_tuple.len, 0);
        if (rt < 0) {
            free(write_tuple.data);
            free(write_tuple.name);
            _SAFE_RETURN(-1, self, args, using_socket);
        }
    }

    zlog_debug(self->log_handle, "Adding tuple %s len %lu to bcast_queue...", write_tuple.name, write_tuple.len);
    zlistx_add_end(args->this_space->bcast_queue, tplcpy(&recv));
    zlog_debug(self->log_handle, "successfully handled TsBCast_Write command.");
    _SAFE_RETURN(0, self, args, using_socket);
}

/**
 * Reads from the bcast queue
 * @param self
 * @param args
 * @return
 */
int bcast_queue_read_handler(tsd_t *self, struct _tsd_handler_arg *args)
{   /* originally for remote, now for everyone */

    void *using_socket = NULL;
    sng_tuple_t *reply_tuple = NULL, /* reply tuple */
            match_tuple = {.data = NULL, .anon_id = 0, .len = 0, .name = NULL}; /* tuple used for matching */
    int rt;
    /* assertions */
    assert(self);
    assert(args);
    assert(args->clientid_buffer);
    assert(args->namespace_buffer);
    assert(args->this_space);
    assert(args->this_space_name);
    assert(args->action == TsBCast_Read || args->action == TsBCast_Read_Blocking);

    using_socket = args->is_local ? self->local_socket : self->server_socket;
    zlog_debug(self->log_handle, "Received TsBCast_Read command.");
    zmq_send(using_socket, args->clientid_buffer, strlen(args->clientid_buffer), ZMQ_SNDMORE);

    zlog_debug(self->log_handle, "Receiving tuple name");
    rt = p2pm_recv_str(using_socket, &match_tuple.name, 0);

    /* NULL-check on name */
    if (rt != P2PM_OP_SUCCESS || match_tuple.name == NULL) {
        zlog_debug(self->log_handle, "err receiving tuple name");
        free(match_tuple.name);
        p2pm_send_uint32(using_socket, TsEnf, 0);
        _SAFE_RETURN(1, self, args, using_socket);
    }

    zlog_debug(self->log_handle, "checking space status (name=%s)", match_tuple.name);

    /* check current space status */
    if (!args->this_space->is_activated) {
        free(match_tuple.name);
        zlog_debug(self->log_handle, "err: space %s requested by client %s is already down.",
                   args->namespace_buffer, args->clientid_buffer);
        p2pm_send_uint32(using_socket, TsEnf, 0);
        _SAFE_RETURN(1, self, args, using_socket);
    }

    zlog_debug(self->log_handle, "doing tuple match...");

    reply_tuple = zlistx_handle_item(zlistx_find(args->this_space->bcast_queue, &match_tuple));

    if (reply_tuple == NULL) {
        zlog_debug(self->log_handle, "err: no matching tuples.");
        if (args->action == TsBCast_Read_Blocking) {
            blocking_request_t blck = {
                    .command = args->action, .extra = 0,
                    .clientid = args->clientid_buffer,
                    .local = args->is_local,
                    .name = match_tuple.name};
            zlistx_add_end(args->this_space->bcast_queue_requests, blckcpy(&blck));
        }
        free(match_tuple.name);
        _ts_fail(self, using_socket);
        p2pm_send_uint32(using_socket, TsEnf, 0);
        _SAFE_RETURN(1, self, args, using_socket);
    }
    zlog_debug(self->log_handle, "found matching tuple (id=%u,len=%lu,name=%s)",
               reply_tuple->anon_id, reply_tuple->len, reply_tuple->name == NULL ? "" : reply_tuple->name);
    zlog_debug(self->log_handle, "Sending reply tuple....");

    rt = handle_bcast_queue_read_response(self, using_socket, reply_tuple, args->this_space, args->is_local);

    free(match_tuple.name);

    /* don't free reply_tuple, since bcast is persistent & immutable */
    _SAFE_RETURN(rt, self, args, socket);
}

/**
 * Handles connections on TsConnect
 * @param self
 * @param args
 * @return
 */
int ts_connect_handler(tsd_t *self, struct _tsd_handler_arg *args)
{

    void *using_socket = NULL,
            *blocking_reply_socket = NULL;
    const int send_token = TSD_P2PMD_SEND_TOKEN;
    unsigned mode;
    /* assertions */
    assert(self);
    assert(args);
    assert(args->clientid_buffer);
    assert(args->namespace_buffer);
    assert(args->this_space);
    assert(args->this_space_name);
    assert(args->action == TsConnect);

    zlog_debug(self->log_handle, "Dispatched TsConnect. Setting space to requested.");
    args->this_space->is_requested = 1;
    using_socket = args->is_local ? self->local_socket : self->server_socket;

    p2pm_recv_uint32(using_socket, &mode, 0);

    zmq_send(using_socket, args->clientid_buffer, strlen(args->clientid_buffer), ZMQ_SNDMORE);
    zlog_debug(self->log_handle, "Client ID = [%s]. Checking reply status.", args->clientid_buffer);

    /* important assumption: client mode must MATCH our mode EXACTLY, i.e. 2 cannot match 0,1 and not oppositely */
    /* but 0 or 1 always work as long as tsd is not configured in 2 mode */
    if (args->this_space->ts_mode == TsModeMaster) {
        zlog_debug(self->log_handle, "Client connected to main namespace server [master]");
        switch (mode) {

            case TS_MODE_MASTER: /* client connects as master */

                ts_connect_send_activate_signal(self, args); /* notify everybody else */
                p2pm_send_uint32(using_socket, TS_MODE_MASTER, 0); /* tell client we're master */
                memcpy(args->this_space->direct_endpoint, self->endpoint,
                       PSTRLEN(self->endpoint)); /* set direct_endpoint */
                args->this_space->is_activated = 1; /* set to being activated */

                break;
            case TS_MODE_CLIENT: /* client connects as worker */

                /* !! different: in the new version we make a second TsConnect handshake for ts activation */

                ts_connect_send_activate_signal(self, args); /* notify everybody else, and...  */
                p2pm_send_uint32(using_socket, TS_MODE_MASTER, 0); /* proceed, let'em block! */
                args->this_space->is_activated = 1;

                break;
            default: /* other cases, ignore */
                p2pm_send_uint32(using_socket, TS_MODE_ERR, 0); /* fail */
        }
    } else if (args->this_space->ts_mode == TsModeWorker) {
        /* in this case, we serve as a proxy server that notifies the client the master node address */
        zlog_debug(self->log_handle, "Client connected to slave namespace server [client]");

        if (mode != TS_MODE_MASTER && mode != TS_MODE_CLIENT) {
            p2pm_send_uint32(using_socket, TS_MODE_ERR, 0); /* fail */
            _SAFE_RETURN(1, self, args, using_socket);
        }

        if (args->this_space->is_activated) {
            /* exists endpoint */
            zlog_debug(self->log_handle, "Preexisting master endpoint @ %s, sending...",
                       args->this_space->direct_endpoint);
            p2pm_send_uint32(using_socket, TS_MODE_CLIENT, ZMQ_SNDMORE); /* send back our identity */
            zlog_debug(self->log_handle, "sending string...[%s]", args->this_space->direct_endpoint);
            p2pm_send_str(using_socket, args->this_space->direct_endpoint, 0);
            _SAFE_RETURN(0,self,args,using_socket);
        } else {
            zlog_debug(self->log_handle,
                       "* No endpoint is existing. Blocking client until receiving a token...");
            blocking_request_t
                    blck1 =
                    {.clientid = args->clientid_buffer,
                            .name=NULL,
                            .extra=mode,/* store mode as extra field :-) */
                            .local=args->is_local,
                            .command=args->action};
            zlistx_add_end(args->this_space->connect_requests, blckcpy(&blck1));
            args->clientid_buffer = NULL;
            p2pm_send_uint32(using_socket, TS_MODE_BLOCK, 0); /* continue to block */
            _SAFE_RETURN(0, self, args, using_socket);
        }
    } else { /* TsModeP2P */
        if (mode != TS_MODE_P2P) {
            p2pm_send_uint32(using_socket, TS_MODE_ERR, 0); /* fail */
            _SAFE_RETURN(1, self, args, using_socket);
        }
        p2pm_send_uint32(using_socket, TS_MODE_P2P, 0); /* p2p */
        _SAFE_RETURN(0, self, args, using_socket);
    }
}

int tsd_shutdown(tsd_t *self)
{
    /* shut down TSD */
    zlog_debug(self->log_handle, "tsd received an interrupt signal. Exiting...");
    zactor_destroy(&self->p2pmd_actor);
    zmq_close(self->local_socket);
    zmq_close(self->server_socket);
    zlog_fini();
    zmq_ctx_destroy(self->_ctx);
    FreeHashTable(self->space_table);
    return 0;
}

int tsd_as_client(tsd_t *self, struct _tsd_handler_arg *args)
{
    void *using_socket = NULL;
    char *connect_endpoint = NULL;
    char *tuple_identity = NULL;
    void *_queue = NULL;
    void *handle = NULL;
    sng_tuple_t *reply_tuple;
    int r;
    _Bool remove;
    assert(self);
    assert(args);
    assert(args->action == TsRequest || args->action == TsTake);

    using_socket = args->is_local ? self->local_socket : self->server_socket;
    remove = args->action == TsTake;

    /* clean up old stuff */
    zmq_close(self->peer_socket);
    self->peer_socket = zmq_socket(self->_ctx, ZMQ_DEALER);

    r = p2pm_recv_str(using_socket, &connect_endpoint, 0);
    if (r != P2PM_OP_SUCCESS || connect_endpoint == NULL)
        _SAFE_RETURN(1, self, args, using_socket); /* on failure, _SAFE_RETURN cleans up! */
    r = p2pm_recv_str(using_socket, &tuple_identity, 0);
    if (r != P2PM_OP_SUCCESS || connect_endpoint == NULL)
        _SAFE_RETURN(1, self, args, using_socket);
    r = zmq_connect(self->peer_socket, connect_endpoint);
    if (r < 0)
        _SAFE_RETURN(0, self, args, using_socket); /* this is valid; client just disappeared, we don't care */

    sng_tuple_t match_tuple = {
            .anon_id = (unsigned int) ((*tuple_identity == 'A') ? strtoul(tuple_identity + 1, NULL, 10) : 0),
            .name = (*tuple_identity == 'A') ? NULL : tuple_identity + 1};
    /* 'A' or 'N' or 'B' */
    _queue = *(tuple_identity + 1) == 'B' ? args->this_space->bcast_queue : args->this_space->named_queue;

    handle = zlistx_find(_queue, &match_tuple);
    if (handle == NULL)
        _SAFE_RETURN(0, self, args, using_socket); /* the case where we merely encountered a race condition wdc */

    reply_tuple = remove ? zlistx_detach(_queue, handle) : zlistx_handle_item(handle);

    /* time to act as client! */

    p2pm_send_uint32(self->peer_socket, (uint32_t) (strlen(args->this_space_name) + 1), ZMQ_SNDMORE);
    zmq_send(self->peer_socket, args->this_space_name, strlen(args->this_space_name) + 1, ZMQ_SNDMORE);
    p2pm_send_uint32(self->peer_socket, TsTransmit, ZMQ_SNDMORE);
    p2pm_send_str(self->peer_socket, tuple_identity, ZMQ_SNDMORE);
    p2pm_send_uint64(self->peer_socket, reply_tuple->len, ZMQ_SNDMORE);
    zmq_send(self->peer_socket, reply_tuple->data, reply_tuple->len, 0);

    if (remove)
        tplfree_g(reply_tuple);

    _SAFE_RETURN(0, self, args, using_socket);
}

int tsd_accept_transmit(tsd_t *self, struct _tsd_handler_arg *args)
{

    void *using_socket = NULL;
    zlistx_t *_queue = NULL;
    assert(self);
    assert(args);
    assert(!args->is_local);
    using_socket = self->server_socket; /* such connection must be from the remote end, as dictated by endpoint addr */

    sng_tuple_t recv_tuple;
    char *tuple_identity = NULL;

    int r = p2pm_recv_str(using_socket, &tuple_identity, 0);
    if (r != P2PM_OP_SUCCESS || tuple_identity == NULL)
        _SAFE_RETURN(1, self, args, using_socket);

    r = p2pm_recv_uint64(using_socket, &recv_tuple.len, 0);
    if (r != P2PM_OP_SUCCESS || recv_tuple.len == 0) {
_:
        free(tuple_identity);
        _SAFE_RETURN(1, self, args, using_socket);
    }
    recv_tuple.data = malloc(recv_tuple.len);
    if (recv_tuple.data == NULL)
        goto _;
    r = zmq_recv(using_socket, recv_tuple.data, recv_tuple.len, 0);
    if (r < 0) {
        free(recv_tuple.data);
        goto _;
    }
    _queue = (*tuple_identity) == 'B' ? args->this_space->bcast_queue : args->this_space->named_queue;

    if ((*tuple_identity) == 'N') {
        recv_tuple.name = malloc(strlen(tuple_identity));
        memcpy(recv_tuple.name, tuple_identity + 1, strlen(tuple_identity));
        recv_tuple.anon_id = 0;
        free(tuple_identity);
        match_blocking_requests(self, args->this_space, &recv_tuple, (*tuple_identity == 'B') ? TsBCast_Write : TsPut);
    } else if ((*tuple_identity) == 'A') {
        recv_tuple.name = NULL;
        recv_tuple.anon_id = (unsigned int) strtoul(tuple_identity + 1, NULL, 10);
        free(tuple_identity);
        match_blocking_requests(self, args->this_space, &recv_tuple, TsAdd);
    } else {
        free(recv_tuple.data);
        goto _;
    }
    _SAFE_RETURN(0, self, args, using_socket);
}

int tsd_add_expect(tsd_t *self, struct _tsd_handler_arg *args)
{
    assert(self);
    assert(args);
    zlog_debug(self->log_handle, "tsd_add_expect() called");
    void *using_socket = args->is_local ? self->local_socket : self->server_socket;
    char *pattern;
    p2pm_recv_str(using_socket, &pattern, 0);
    zlistx_add_end(args->this_space->expect_queue, pattern);
    zlog_debug(self->log_handle, "expect added.");
}

int ts_term_clear_requests(tsd_t *self, struct _tsd_handler_arg *args)
{
    assert(self);
    assert(args);
    void *using_socket;
    zlog_debug(self->log_handle, "ts_term_clear_requests() called, clearing requests...");
    while (zlistx_size(args->this_space->named_queue_requests)) {
        blocking_request_t *blck = zlistx_detach(args->this_space->named_queue_requests, NULL);
        zlog_debug(self->log_handle, "blck: is_local ? %d, clientid: %s, req: %d, name %s",
                   blck->local, blck->clientid, blck->command, blck->name);
        using_socket = blck->local ? self->local_socket : self->server_socket;
        zmq_send(using_socket, blck->clientid, strlen(blck->clientid), ZMQ_SNDMORE);
        p2pm_send_uint32(using_socket, TsTerm, 0);
        blckfree_g(blck);
    }
    while(zlistx_size(args->this_space->bcast_queue_requests)) {
        blocking_request_t *blck = zlistx_detach(args->this_space->bcast_queue_requests, NULL);
        zlog_debug(self->log_handle, "blck: is_local ? %d, clientid: %s, req: %d, name %s",
                   blck->local, blck->clientid, blck->command, blck->name);
        using_socket = blck->local ? self->local_socket : self->server_socket;
        zmq_send(using_socket, blck->clientid, strlen(blck->clientid), ZMQ_SNDMORE);
        p2pm_send_uint32(using_socket, TsTerm, 0);
        blckfree_g(blck);
    }
    /* clear the queue */
    while(zlistx_size(args->this_space->named_queue))
        tplfree_g(zlistx_detach(args->this_space->named_queue, NULL));
    while(zlistx_size(args->this_space->bcast_queue))
        tplfree_g(zlistx_detach(args->this_space->named_queue, NULL));
    return 0;
}

int handler(tsd_t *self, bool is_local)
{

    zlog_debug(self->log_handle, "\n");


    void *using_socket = NULL;

    /* declarations */
    char *clientid_buffer = NULL, *namespace_buffer = NULL, useless_buffer[50]; /* sessioning */
    HTItem *hash_entry = NULL; /* hash table entry for current session */
    tsd_element_t *this_space = NULL; /* handle for current session's requested tuple space */
    const char *this_space_name = NULL; /* handle for the name of current tuple space as in hash table */
    tuple_command_t action; /* dispatch command */
    int buf_read;
    unsigned buf_len;

    /* assertions */
    assert(self);

    /* allocate heap memory for the two buffers */
    clientid_buffer = malloc(50);
    namespace_buffer = malloc(50);

    memset(clientid_buffer, '\0', 50);
    memset(namespace_buffer, '\0', 50);

    using_socket = is_local ? self->local_socket : self->server_socket;
    zlog_debug(self->log_handle, is_local ? "tsd server socket = local" :
                                 "tsd server socket = remote");
    zlog_debug(self->log_handle, "tsd server socket activated.");

    /* receive clientid */
    buf_read = zmq_recv(using_socket, clientid_buffer, 50, 0);

    if (buf_read < 0) {
        zlog_debug(self->log_handle, "err: cannot read clientid_buf. skipping...");
        free(clientid_buffer);
        free(namespace_buffer);
        return 1;
    }
//    clientid_buffer[buf_read] = '\0';

    zlog_debug(self->log_handle, "Received client request[%s], %d", clientid_buffer, buf_read);

    zlog_debug(self->log_handle, "reading namespace_buf...");

    /* receive namespace */
    buf_read = p2pm_recv_uint32(using_socket, &buf_len, 0);

    if (buf_read != P2PM_OP_SUCCESS || buf_len == 0 || buf_len > 50)
        goto handle_second_error;

    zlog_debug(self->log_handle, "buf_len = %u", buf_len);
    buf_read = zmq_recv(using_socket, namespace_buffer, 50, 0);
    namespace_buffer[buf_len - 1] = '\0'; /* account for the empty \0 on client side */

    /* if namespace is blank */
    if (buf_read < 0) {
_:
        zlog_debug(self->log_handle, "err: cannot read namespace_buf. draining socket...");
handle_second_error:
        zmq_send(using_socket, clientid_buffer, strlen(clientid_buffer), ZMQ_SNDMORE);
        p2pm_send_uint32(using_socket, TsEnf, 0);
        /* drain the incoming socket */
        zlog_debug(self->log_handle, "draining socket...");
        int has_more = 0;
        size_t hm_size = sizeof(has_more);
        zmq_getsockopt(using_socket, ZMQ_RCVMORE, &has_more, &hm_size);  /* drain the socket */
        while (has_more) {    /* of course, we don't really hope this to occur */
            zlog_debug(self->log_handle, "draining socket...");
            zmq_recv(using_socket, useless_buffer, 50, 0);
            zmq_getsockopt(using_socket, ZMQ_RCVMORE, &has_more, &hm_size);
        }
        zlog_debug(self->log_handle, "draining done. skipping...");
        free(namespace_buffer);
        free(clientid_buffer);
        return 1;
    }

    zlog_debug(self->log_handle, "querying hash table for item %s issued by client %s", namespace_buffer,
               clientid_buffer);

    /* received namespace/clientid, query hash table for entry */
    hash_entry = HashFind(self->space_table, PTR_KEY(self->space_table, namespace_buffer));
    if (hash_entry == NULL) {
        zlog_debug(self->log_handle, "error: no such item as %s issued by client %s", namespace_buffer,
                   clientid_buffer);
        goto handle_second_error;
    }

    /* query success, get handle for tuple space and handle name (from key and value) */
    this_space = (tsd_element_t *) KEY_PTR(self->space_table, hash_entry->data);
    this_space_name = (const char *) KEY_PTR(self->space_table, hash_entry->key);

    /* receive an action for dispatch */
    if (p2pm_recv_uint32(using_socket, (uint32_t *) &action, 0) != P2PM_OP_SUCCESS) {
        zlog_debug(self->log_handle, "error recieving action.");
        goto handle_second_error;
    }

    zlog_debug(self->log_handle, "query success, dispatching action (%d) ...", action);

    /* is our action good? action is bad if attempts read/write on an uninitialized space */
    /* i.e. read/write before malloc() is a no-op */
    if (action != TsConnect && !this_space->is_activated) {
        /* handle case where illegal tuple case is got */
        zlog_debug(self->log_handle, "Error: Action %d is attempting on uncreated tuple space %s", action,
                   namespace_buffer);
        goto _;
    }


    /* dispatch action */
    struct _tsd_handler_arg args = {
            .is_local=is_local,
            .clientid_buffer=clientid_buffer,
            .namespace_buffer=namespace_buffer,
            .action=action, .is_master=0,
            .this_space=this_space,
            .this_space_name=this_space_name};
    int a_rt;
    switch (action) {
        default:
        case TsNone:
	    printf("Received invalid request: %d", action);
            a_rt = -1;
            break;
            /* named queue write requests */
        case TsExpect://TODO send out active search cmd!!!<------------------------------------
            //TODO implement
            a_rt = tsd_add_expect(self, &args);
            break;
        case TsGet_Blocking:
        case TsRead_Blocking:
        case TsRead:
        case TsGet:
            a_rt = named_queue_read_handler(self, &args);
            break;

        case TsPeek:
        case TsPop:
        case TsPop_Blocking:
        case TsPeek_Blocking:
            a_rt = named_queue_fetch_handler(self, &args);
            break;
            /* named queue read requests */
        case TsPut:
        case TsAdd:
            a_rt = named_queue_write_handler(self, &args);
            break;
            /* Misc requests */
        case TsConnect:
            a_rt = ts_connect_handler(self, &args);
            break;            /* BCast queue write requests */
        case TsBCast_Write:
            a_rt = bcast_queue_write_handler(self, &args);
            break;
            /* BCast queue read requests */
        case TsBCast_Read:
        case TsBCast_Read_Blocking:
            a_rt = bcast_queue_read_handler(self, &args);
            break;
        case TsTerm:
            ts_term_send_decomission_signal(self, &args);
            ts_term_clear_requests(self, &args);
            args.this_space->is_activated = 0;
            break;
        case TsDeposit: /* This answers a request sent thru ring topology with TTL set = replication factor */
            break;//TODO here
        case TsTransmit: /* 3rd step for a TsRequest command lifecycle */
            a_rt = tsd_accept_transmit(self, &args);//TODO dedup dedup dedup!!!
            break;
        case TsRequest:
        case TsTake:
            a_rt = tsd_as_client(self, &args);
            break;
        case TsMigrate:
            break;
        case TsReplicate:
            break;
    }

    return a_rt;
}

void connect_token_next(tsd_t *self, generic_token_t *tk)
{

    const unsigned send_token_command = TSD_P2PMD_SEND_TOKEN;   /* command for sending token */
    tsd_element_t *element;    /* state of current tuple space, to be assigned */

    assert(self);
    assert(tk);

    char *namespace = (char *) tk->buf1;
    char *endpt = (char *) tk->buf2;
    zlog_debug(self->log_handle, "connect_token_next()");

    zlog_debug(self->log_handle, "Connect token namespace = %s, endpt = %s\n",
               namespace, endpt);

    /* query hash table: do we store this tuple space? */
    zlog_debug(self->log_handle, "querying hash table for item %s", (char *) tk->buf1); /* buf1 = name */
    HTItem *item = HashFind(self->space_table,
                            PTR_KEY(self->space_table, (char *) tk->buf1));

    int ttl = tk->ttl;

    if (item == NULL) {

        /* no such token, skip */

        zlog_debug(self->log_handle, "err: frame not found!");

        if (ttl > 0 || ttl == -1) {
            zlog_debug(self->log_handle, "relaying frame off (ttl = %u)", ttl);
            send_generic_token(self, tk);
        } else {
            free((void *) tk->buf1);
            free((void *) tk->buf2);
            free(tk);
        }
        return;
    } else {

        /* tuple space found */

        element = (tsd_element_t *) KEY_PTR(self->space_table, item->data);
        if (element->is_activated)  /* space is already activated */
            return; /* ignore duplicates ?? */

        element->is_activated = 1;  /* activate this space */

        zlog_debug(self->log_handle, "tuple space %s received token with direct endpoint %s",
                   KEY_PTR(self->space_table, item->key), (char *) tk->buf2);

        /* copy and write to element->direct_endpoint */
        memcpy(element->direct_endpoint, (char *) tk->buf2, tk->len2);    /* assign direct_endpoint to this space */

        /* do we have outstanding requests? */
        if (element->is_requested) {

            /* Yes, process them */

            zlog_debug(self->log_handle, "is requested by element. so sending it to namespace %s",
                       KEY_PTR(self->space_table, item->key));

            while (zlistx_size(element->connect_requests) > 0) {
                blocking_request_t *connect_request = zlistx_detach(element->connect_requests,
                                                                    NULL); /* pop front */
                void *_cs = connect_request->local ? self->local_socket : self->server_socket;
                zmq_send(_cs, connect_request->clientid, strlen(connect_request->clientid), ZMQ_SNDMORE);
                p2pm_send_str(_cs, element->direct_endpoint, 0);
                blckfree_g(connect_request);
            }

        }

        /* check TTL & send (if TTL hasn't expired) */

        if (ttl > 0 || ttl == -1) {
            zlog_debug(self->log_handle, "ttl == %d > 0, relaying token off...", ttl);
            send_generic_token(self, tk);
        } else {
            free((void *) tk->buf1);
            free((void *) tk->buf2);
            free(tk);
        }
    }
}

void term_token_next(tsd_t *self, generic_token_t *tk)
{
    const int send_token = TSD_P2PMD_SEND_TOKEN;
    assert(self);
    assert(tk);



    char *term_space_name = (char *) tk->buf1;
    char *term_originator = (char *) tk->buf2;

    assert(term_space_name);
    assert(term_originator);

    HTItem *item = NULL;
    item = HashFind(self->space_table, PTR_KEY(self->space_table, term_space_name));

    if (item == NULL) return;

    tsd_element_t *element = (tsd_element_t *) KEY_PTR(self->space_table, item->data);

    assert(element != NULL);

    if (tk->ttl > 0 || tk->ttl == -1 && element->is_activated) {
        send_generic_token(self, tk);
    } else {
        free((void *) tk->buf1);
        free((void *) tk->buf2);
        free(tk);
    }

    element->is_activated = 0;
}

void match_token_next(tsd_t *self, generic_token_t *token)
{

    const char *identity = NULL;
    const char *destination = NULL;
    const char *tuple_name = NULL;
    HTItem *item = NULL;
    tsd_element_t *element = NULL;
    size_t namespace_len;
    size_t name_len;
    tuple_command_t command = TsRequest;
    char who;

    assert(self);
    assert(token);
    assert(token->buf1);
    assert(token->buf2);
    assert(token->id == 2);

    identity = (char*)token->buf1;
    destination = (char*)token->buf2;

    namespace_len = strlen(identity);
    name_len = strlen(identity + namespace_len + 2);
    tuple_name = identity + namespace_len + 2;
    who = *(identity + namespace_len + 1);

    if (who != 'N' && who != 'A')
        goto _end;

    item = HashFind(self->space_table, PTR_KEY(self->space_table, identity));
    if (item == NULL)
        goto _end;

    element = (tsd_element_t *) KEY_PTR(self->space_table, item->data);

    void *_match_handle = zlistx_find(element->expect_queue, (void *) tuple_name);
    if (_match_handle == NULL)
        goto _end;
    else {
        zmq_close(self->peer_socket); /* close stuff from last connection */
        self->peer_socket = zmq_socket(self->_ctx, ZMQ_DEALER);
        s_set_id(self->peer_socket);
        int r = zmq_connect(self->peer_socket, destination);
        if (r < 0) goto _end; /* unsuccessful */
        p2pm_send_uint32(self->peer_socket, (uint32_t) namespace_len, ZMQ_SNDMORE);
        zmq_send(self->peer_socket, identity, namespace_len, ZMQ_SNDMORE);
        p2pm_send_uint32(self->peer_socket, command, ZMQ_SNDMORE);
        p2pm_send_str(self->peer_socket, (char *) self->endpoint, 0);
        p2pm_send_str(self->peer_socket, (char *) (identity + namespace_len + 1), 0);
    }

    /* end */
_end:
    if (token->ttl > 0 || token->ttl == -1) {
        send_generic_token(self, token);
    } else {
        free((void *) token->buf1);
        free((void *) token->buf2);
        free(token);
    }
}

void deposit_token_next(tsd_t *self, generic_token_t *token)
{

    const char *identity = NULL;
    const char *destination = NULL;
    const char *tuple_name = NULL;
    HTItem *item = NULL;
    void *_queue = NULL;
    tsd_element_t *element = NULL;
    size_t namespace_len;
    size_t name_len;
    tuple_command_t command;
    sng_tuple_t recv_tuple;
    char who;

    assert(self);
    assert(token);
    assert(token->buf1);
    assert(token->buf2);
    assert(token->id == 2);

    identity = (char*)token->buf1;
    destination = (char*)token->buf2;

    namespace_len = strlen(identity);
    name_len = strlen(identity + namespace_len + 2);
    tuple_name = identity + namespace_len + 2;
    who = *(identity + namespace_len + 1);
    command = who == 'B' ? TsBCast_Read : (who == 'N' ? TsRead : TsPeek);

    if (who != 'N' && who != 'A')
        goto _end;

    item = HashFind(self->space_table, PTR_KEY(self->space_table, identity));
    if (item == NULL)
        goto _end;

    element = (tsd_element_t *) KEY_PTR(self->space_table, item->data);
    _queue = who == 'B' ? element->bcast_queue : element->named_queue;
    zmq_close(self->peer_socket); /* close stuff from last connection */
    self->peer_socket = zmq_socket(self->_ctx, ZMQ_DEALER);
    s_set_id(self->peer_socket);
    int r = zmq_connect(self->peer_socket, destination);
    if (r < 0) goto _end; /* unsuccessful */
    p2pm_send_uint32(self->peer_socket, (uint32_t) namespace_len, ZMQ_SNDMORE);
    zmq_send(self->peer_socket, identity, namespace_len, ZMQ_SNDMORE);
    if (who == 'A') {
        p2pm_send_uint32(self->peer_socket, command, 0);
        p2pm_recv_uint32(self->peer_socket, (uint32_t *) &command, 0);
        if (command != TsAck) goto _end;
        p2pm_recv_uint32(self->peer_socket, &recv_tuple.anon_id, 0);
        p2pm_recv_uint64(self->peer_socket, &recv_tuple.len, 0);
    } else {
        p2pm_send_uint32(self->peer_socket, command, ZMQ_SNDMORE);
        p2pm_recv_uint32(self->peer_socket, (uint32_t *) &command, 0);
        if (command != TsAck) goto _end;
        p2pm_send_str(self->peer_socket, (char *) tuple_name, 0);
        p2pm_recv_str(self->peer_socket, &recv_tuple.name, 0);
    }
    if (recv_tuple.len == 0) goto _end;
    recv_tuple.data = malloc(recv_tuple.len);
    zmq_recv(self->peer_socket, recv_tuple.data, recv_tuple.len, 0);

    zlistx_add_end(_queue, tplcpy(&recv_tuple));

    /* end */
_end:
    if (token->ttl > 0 || token->ttl == -1) {
        send_generic_token(self, token);
    } else {
        free((void *) token->buf1);
        free((void *) token->buf2);
        free(token);
    }
}

void search_token_next(tsd_t *self, generic_token_t *token)
{
    assert(self);
    assert(token);
}

int token_handler(tsd_t *self)
{

    assert(self);

    /*
     * Attention!!
     *
     * Here we do NOT modify the TTL.
     * The p2pmd actor automatically applies the "-1" for us.
     *
     */

    zlog_debug(self->log_handle, "A token has been received by tsd.");

    /* receive a token and get its content */
    void *_tsock = zsock_resolve(self->actor_pipe);

    zlog_debug(self->log_handle, "allocating token");

    generic_token_t *_token = malloc(sizeof(generic_token_t));
    zlog_debug(self->log_handle, "receiving token");
    zmq_recv(_tsock, _token, sizeof(generic_token_t), 0);

    zlog_debug(self->log_handle, "Read token");
    zlog_debug(self->log_handle, "Token id=%u,ttl=%d,len1=%lu,len2=%lu,"
            "buf1_addr=%p, buf2_addr=%p",
               _token->id, _token->ttl, _token->len1, _token->len2,
               _token->buf1, _token->buf2);

    switch (_token->id) {
        case 0:
            zlog_debug(self->log_handle, "namespace: %s", (char*) _token->buf1);
            zlog_debug(self->log_handle, "endpoint: %s", (char*) _token->buf2);
            connect_token_next(self, _token);
            break;
        case 1:
            term_token_next(self, _token);
            break;
        case 2:
            match_token_next(self, _token);
            break;
        case 3:
            deposit_token_next(self, _token);
            break;
        case 4:
            search_token_next(self, _token); /* a request searching for a tuple in a distributed fashion */
    }

end: /* currently, we check duplications */
    return 0;
}

int tsd_listen(tsd_t *self)
{
    assert(self);
    zmq_pollitem_t items[3] = {{.socket = self->local_socket, .events = ZMQ_POLLIN}, /* 1, local IPC socket */
                               {.socket = self->server_socket, .events = ZMQ_POLLIN}, /* 2, TCP socket for tsd */
                               {.socket = zsock_resolve(
                                       self->actor_pipe), .events = ZMQ_POLLIN} /*3,p2pmd socket */  };
    int _read;
    do {
        zlog_debug(self->log_handle, "tsd listening to connections...");
        _read = zmq_poll(items, 3, -1);
        if (_read < 0) {
            zlog_debug(self->log_handle, "tsd: err: read<0. quitting...");
	    return tsd_shutdown(self);
        }
	if (items[0].revents & ZMQ_POLLIN) {
	    zlog_debug(self->log_handle, "tsd_listen(): received data from local socket...");
            handler(self, 1);
        }
        zmq_poll(&items[1], 2, 0);
        if (items[1].revents & ZMQ_POLLIN) {
            	zlog_debug(self->log_handle, "tsd_listen(): received data from remote socket...");
		handler(self, 0);
	}
        zmq_poll(&items[2], 1, 0);
        if (items[2].revents & ZMQ_POLLIN) {
	    zlog_debug(self->log_handle, "tsd_listen(): received data from p2pmd socket...");
            token_handler(self);
        }

    } while (1);
}
