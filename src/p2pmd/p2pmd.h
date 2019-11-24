/*
 * Created by Ruijie Fang on 1/23/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#ifndef RING_MECHANISM_P2PMD_H
#define RING_MECHANISM_P2PMD_H

/* 88888888888888888888888888888888888888888888888888
 Created by Ruijie Fang on 1/13/18.
   88888888888888888888888888888888888888888888888888 */

#include "../p2pm_common.h"
#include "../p2pm_utilities.h"
#include "../p2pm_types.h"
#include "../p2pm_node_info.h"
#include "../../contrib/zlog/src/zlog.h"


struct _t_token {
    volatile void *buf1;
    volatile void *buf2;
    volatile size_t len1;
    volatile size_t len2;
    volatile unsigned id;
    volatile int ttl;
};
typedef struct _t_token generic_token_t;

typedef enum _p2pm_reqtypes_e p2pm_reqtypes_t;
typedef enum _p2pm_reptypes_e p2pm_reptypes_t;
typedef enum _p2pm_opcodes_e p2pm_opcodes_t;
typedef enum _p2pm_status_e p2pm_status_t;
typedef struct _p2pm_node_info_s p2pm_node_info_t;
typedef struct _p2pm_s p2pm_t;

void p2pm_create(p2pm_t *self, const char *endpoint, p2pm_config_t *config, p2pm_node_info_t *node_list,
                 zlog_category_t * log_handle,
                 p2pm_status_t status);

void p2pm_set_name(p2pm_t *self);

void p2pm_destroy(p2pm_t *self);

void p2pmd_actor(zsock_t *pipe, void *args);


p2pm_opcodes_t
p2pm_handle_successor_list_changes(p2pm_t *self, char *new_successor_list, char *source_node_endpoint, unsigned n_r,
                                   unsigned ttl);

p2pm_opcodes_t p2pm_try_live(void *socket);

/* Forms a ring of self */
p2pm_opcodes_t p2pm_form_one_ring(p2pm_t *self);

p2pm_opcodes_t p2pm_join(p2pm_t *self);

p2pm_opcodes_t p2pm_join_one_ring(p2pm_t *self);

int p2pm_start_server(p2pm_t *self, const char *bind_addr);

p2pm_opcodes_t p2pm_loop_server(p2pm_t *self);

int p2pm_close_server(p2pm_t *self);

void p2pm_set_obj_wait(p2pm_t *self);

#endif //RING_MECHANISM_P2PMD_H



