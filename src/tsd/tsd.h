/*
 * Created by Ruijie Fang on 2/6/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#ifndef P2PMD_TSD_H
#define P2PMD_TSD_H

#include "../p2pm_common.h"
#include "../p2pm_types.h"
#include "../parser/sng_parser.h"
#include "../p2pmd/p2pmd.h"
#include "../sng_common.h"
#include "../../contrib/zlog/src/zlog.h"
/**
 * Creates a Tuple Space Daemon instance.
 * @param self
 * @param p2pmv Arguments for creating the P2PMD ring daemon
 * @param config Configuration instance for Synergy4
 * @return An int status flag, 0 means success
 */
int tsd_create(tsd_t *self, p2pm_init_args_t *p2pmv, sng_cfg_t *config);

/**
 * Bind to the specified address and listen to connections
 * @param self An instantiated TSD instance
 * @return 0 if success.
 */
int tsd_listen(tsd_t *self);

#endif //P2PMD_TSD_H

