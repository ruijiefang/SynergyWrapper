#ifndef _P2PM_TYPES_H
#define _P2PM_TYPES_H

#include "../contrib/zlog/src/zlog.h"
#include "p2pm_common.h"

enum _p2pm_reqtypes_e {
    P2PM_GET_REQ = 55,
    P2PM_JOIN_REQ = 56,
    P2PM_JOIN_ABRT = 57,
    P2PM_JOIN_CMT = 58,
    P2PM_PRED_JOIN_CMT = 59,
    P2PM_PRED_JOIN_ABRT = 60,
    P2PM_CUSTOM_PAYLOAD_REQ = 61,
    P2PM_PING_REQ = 62,
    P2PM_STABILIZE_REQ = 63,
    P2PM_STABILIZE_SYN_REQ = 64,
    P2PM_STABILIZE_NSYN_REQ = 65, /* Although we're your live predecessor, we don't want you anymore, REMOVE US */
    P2PM_SYNC_SLIST = 66,
    P2PM_ONE_RING_JOIN_REQ = 68,
    P2PM_FIND_HEAD = 69,
    P2PM_FIND_TAIL = 70,
    P2PM_TOKEN = 71
};
typedef enum _p2pm_reqtypes_e p2pm_reqtypes_t;

enum _p2pm_reptypes_e {
    P2PM_PONG_REP = 7,
    P2PM_CUSTOM_PAYLOAD_REP = 63,
    P2PM_INVALID_REP = 64,
    P2PM_JOIN_ACK1 = 65,
    P2PM_JOIN_ACK2 = 66,
    P2PM_STABILIZE_ACK = 67,
    P2PM_STABILIZE_NACK = 68,
    P2PM_GET_ACK = 69,
    P2PM_GET_NACK = 70,
    P2PM_GET_CONTACTED_ONE_RING = 71,
    P2PM_SYNC_SLIST_ACK = 72,
    P2PM_HEAD_FOUND = 73,
    P2PM_TAIL_FOUND = 74
};
typedef enum _p2pm_reptypes_e p2pm_reptypes_t;

enum _p2pm_expecting_action_e {
    P2PM_NO_XPV = 0,
    P2PM_XPV_JOIN_CMT = 1,
    P2PM_XPV_JOIN_ONERING = 2,
    P2PM_XPV_TRIGGER_JOIN = 3,
    P2PM_XPV_TRIGGER_STABILIZE = 4,
    P2PM_XPV_PING_AND_STABILIZE_REQ = 5,
    P2PM_XPV_PING_AND_STABILIZE_REQ1 = 6,
    P2PM_XPV_PING_AND_STABILIZE_REQ2 = 7,
    P2PM_XPV_STABILIZE_FAILURE = 8,
    P2PM_XPV_STABILIZE_ACK = 9,
    P2PM_XPV_NXT_SUCCESSOR = 10,
    P2PM_XPV_PERFORM_JOIN = 11,
    P2PM_XPV_REPLICATE_BACKWARDS = 12,
    P2PM_XPV_REPLICATE_BACKWARDS_PINGED = 13, /* for EFSM switching on predecessor socket */
    P2PM_XPV_RELAY_BACKWARDS = 14,
    P2PM_XPV_RELAY_BACKWARDS_PINGED = 15, /* for EFSM switching on predecessor socket */
    P2PM_XPV_PRED_REP_PING1 = 16,
    P2PM_XPV_PRED_REP_PING2 = 17,
    P2PM_XPV_PRED_REP_PING3 = 18,
    P2PM_XPV_PRED_FAILURE = 19,
    P2PM_XPV_STABILIZE_FIND_TAIL = 20,
    P2PM_XPV_STABILIZE_RELAY_FIND_HEAD = 21,
    P2PM_XPV_STABILIZE_RELAY_FIND_TAIL = 22,
    P2PM_XPV_STABILIZE_FIND_HEAD = 23,
    P2PM_XPV_STABILIZE_FIND_HEAD_PINGED = 24 /* for EFSM switching on predecessor socket */
};
typedef enum _p2pm_expecting_action_e p2pm_expected_actions_t;
enum _p2pm_opcodes_e {
    P2PM_OP_FAILURE = 0,
    P2PM_OP_SUCCESS = 1,
    P2PM_OP_TRYAGAIN = 2,
    P2PM_OP_TIMEOUT = 3,
    P2PM_OP_LIVE = 4,
    P2PM_OP_DEAD = 5,
    P2PM_OP_TODO = 6,
    P2PM_OP_INVALID = 7,
    P2PM_OP_FINDING_HEAD = 8,
    P2PM_OP_FINDING_TAIL = 9,
    P2PM_OP_FORMED_ONE_RING = 10,
    P2PM_OP_LIST_UNSYNCED = 11,
    P2PM_OP_NEED_ONE_RING = 12
};
typedef enum _p2pm_opcodes_e p2pm_opcodes_t;

enum _p2pm_status_e {
    P2PM_NODE_ALONE = 0,
    P2PM_NODE_ONE_RING = 1,
    P2PM_NODE_JOINING = 2,
    P2PM_NODE_MEMBER = 3,
    P2PM_NODE_APPENDAGE = 4,
    P2PM_SOCKET_LIVE = 5,
    P2PM_SOCKET_DEAD = 6,
    P2PM_SOCKET_BOUNDED_WAIT = 7,
    P2PM_FINDING_HEAD = 8 /* TODO: identify where find_head shall be called */,
    P2PM_FINDING_TAIL = 9,
    P2PM_NODE_HANDLING_JOIN = 10,
    P2PM_NODE_HANDLING_STABILIZE = 11,
    P2PM_NODE_BUSY = 12
};
typedef enum _p2pm_status_e p2pm_status_t;


struct _p2pm_node_info_s {
    char *successor_list;   /* marshalled, get node by successor_list+(P2PM_MAX_ID_LEN*r) */
    char *predecessor;
    char *successor;
    char *temp_predecessor;
    char *temp_successor;
    unsigned r;
    unsigned r_max;
};
typedef struct _p2pm_node_info_s p2pm_node_info_t;

struct _p2pm_config_s {

    char *join_address;
    int64_t stabilize_interval;
    unsigned context_thread_count;
    unsigned wait;
};
typedef struct _p2pm_config_s p2pm_config_t;

struct _p2pm_s {
    void *zmq_context;
    void *server_socket;
    void *successor_socket;
    void *predecessor_socket;
    void *api_socket;
    void *shortlived_socket;
    char *endpoint;
    volatile void *send_token_buf1;
    volatile void *recv_token_buf1;
    volatile void *send_token_buf2;
    volatile void *recv_token_buf2;
    volatile size_t send_token_len1;
    volatile size_t send_token_len2;
    volatile size_t recv_token_len1;
    volatile size_t recv_token_len2;
    zlog_category_t *log_handle;
    struct _p2pm_node_info_s *node_list;
    p2pm_config_t *config;
    zmq_pollitem_t pollers[5];
    short npollers;
#define P2PM_SERVER_POLLER 0
#define P2PM_SUCCESSOR_POLLER 1
#define P2PM_PREDECESSOR_POLLER 2
#define P2PM_OTHER_POLLER 3
#define P2PM_USER_POLLER 4
    char clientid[P2PM_MAX_ID_LEN];
    int64_t predecessor_lastpoll_time;
    int64_t successor_lastpoll_time;
    int64_t last_stabilize_time;
    enum _p2pm_status_e current_status;
    enum _p2pm_expecting_action_e predecessor_socket_expected_actions;
    enum _p2pm_expecting_action_e successor_socket_expected_actions;
    enum _p2pm_expecting_action_e server_triggered_actions;
    enum _p2pm_expecting_action_e predecessor_triggered_actions;
    enum _p2pm_expecting_action_e api_actions;
    volatile int recv_token_ttl;
    volatile int send_token_ttl;
    volatile unsigned last_ttl;
    volatile unsigned recv_token_id;
    volatile unsigned send_token_id;
    _Bool recv_has_token;
    _Bool send_has_token;
};
typedef struct _p2pm_s p2pm_t;

struct _p2pm_init_args_s {
    const char *endpoint;
    p2pm_config_t *config;
    p2pm_node_info_t *node_list;
    zlog_category_t *log_handle;
    p2pm_status_t status;
};
typedef struct _p2pm_init_args_s p2pm_init_args_t;

#endif