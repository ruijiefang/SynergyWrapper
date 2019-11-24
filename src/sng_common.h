/* 88888888888888888888888888888888888888888888888888
 Created by Ruijie Fang on 2/19/18.
   88888888888888888888888888888888888888888888888888 */

#ifndef P2PMD_SNG_COMMON_H
#define P2PMD_SNG_COMMON_H

#include "p2pm_types.h"
#include "shared_memory/shared_memory.h"
#include "../contrib/libchash/libchash.h"

#define IPC_NAME "ipc:///tmp/synergy_tsd"
typedef enum {
    TsModeMaster, TsModeWorker, TsModeP2P
} ts_mode_t;
typedef enum {
    TsdModeDirect, TsdModeToken
} tsd_mode_t;
#define TS_MODE_MASTER 0
#define TS_MODE_CLIENT 1
#define TS_MODE_BLOCK 2
#define TS_MODE_P2P 3
#define TS_MODE_ERR 4

struct _sng_namespace_s {
    char name[30];
    char _direct_endpoint[P2PM_MAX_ID_LEN];
    int id;
    ts_mode_t ts_mode;  /* master or worker */
    tsd_mode_t tsd_mode;    /* direct or token */
};
typedef struct _sng_namespace_s sng_namespace_t;

struct _sng_cfg_all_s {
    p2pm_node_info_t node_info;
    char *p2pmd_endpoint;
    sng_namespace_t *spaces;
    char *tsd_endpoint;
    char *tsd_ipc_endpoint;
    char *uuid;
    p2pm_status_t p2pmd_mode;
    unsigned num_spaces;
    /* hidden fields */
    int bv; /* a bit vector indicating states */
    unsigned _r_list;
    unsigned tsd_rep_factor;
};
typedef struct _sng_cfg_all_s sng_cfg_t;

enum _tuple_commands {
    TsNone,
    TsGet,
    TsPut,
    TsAdd,
    TsRead,
    TsPop,
    TsPeek,
    TsConnect,
    TsTerm,
    TsEnf,
    TsAck,
    TsAnon,
    TsBCast_Write,
    TsBCast_Read,
    TsGet_Blocking,
    TsRead_Blocking,
    TsBCast_Read_Blocking,
    TsPop_Blocking,
    TsPeek_Blocking,
    TsExpect,
    TsDeposit, /* This answers a request sent thru ring topology with TTL set = replication factor */
    TsTransmit,
    TsRequest,
    TsTake,
    TsMigrate,
    TsReplicate
};
typedef enum _tuple_commands tuple_command_t;
struct _ts {
    void *_ctx;
    void *_socket;
    void *_socket2;
    char *uuid;
    const char *ipc_name;
    char tsname[50];
    unsigned tsnamelen;
    unsigned mode; /* 0=master, 1=worker, 2=p2p */
    shared_memory_t _ts_tsd_pipe;
    shared_memory_t _tsd_ts_pipe;//TODO create these upon ts_create
    zlistx_t *tuples;
    zlog_category_t *log_handle;
    char *remote;
    _Bool is_term;
    _Bool is_local;
};
struct _tuple {
    char *name;
    unsigned anon_id;
    size_t len;
    void *data;
};
typedef struct _tuple sng_tuple_t;
typedef struct _ts ts_t;

struct _blocking_request_s {
    char * name;
    char * clientid;
    tuple_command_t command;
    size_t extra;
    _Bool local;
};
typedef struct _blocking_request_s blocking_request_t;
struct _tsd_element_s {
    char * name;
    char direct_endpoint[P2PM_MAX_ID_LEN];
    zlistx_t *named_queue;
    zlistx_t *bcast_queue;

    zlistx_t *named_queue_requests;
    zlistx_t *bcast_queue_requests;
    zlistx_t *connect_requests;

    zlistx_t *copy_named_queue;
    zlistx_t *copy_bcast_queue;
    zlistx_t *expect_queue;//TODO initialize these

    ts_mode_t ts_mode;
    tsd_mode_t tsd_mode;
    shared_memory_t ts_tsd_pipe;    /* the 'push' pipe */
    shared_memory_t tsd_ts_pipe;    /* the 'pull' pipe */
    _Bool is_activated;
    _Bool is_requested;
};
typedef struct _tsd_element_s tsd_element_t;
struct _tsd_s {
    const char *endpoint;
    struct HashTable *space_table;
    void *_ctx;
    void *local_socket; /* local IPC */
    void *server_socket; /* remote connections */
    void *peer_socket; /* copy connections */
    zlog_category_t *log_handle;
    zsock_t *actor_pipe;
    zactor_t *p2pmd_actor;
    const char *ipc_name;
    unsigned replication_factor;
};
typedef struct _tsd_s tsd_t;
#endif //P2PMD_SNG_COMMON_H
