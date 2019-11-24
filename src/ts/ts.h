/*
 * Created by Ruijie Fang on 2/7/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#ifndef P2PMD_TS_H
#define P2PMD_TS_H

#include "../p2pm_common.h"
#include "../p2pm_types.h"
#include "../p2pmd/p2pmd.h"
#include "../p2pm_utilities.h"
#include "../tsd/tsd.h"

#include "../shared_memory/shared_memory.h"
#include "../sng_common.h"

/*!
 * ts_open(2)
 * \brief Opens a tuple space associated with a handle, blocks until the token /direct broadcast is finished.
 * \param self a tuple space handle
 * \param name name of the tuple space
 * \return 0= success, 1 = failure
 */
int ts_open(ts_t *self, const char *name, const char * ipc_name, int mode);
int ts_out(ts_t *self, sng_tuple_t *tuple, tuple_command_t cmd);
int ts_in(ts_t *self, sng_tuple_t *tuple, tuple_command_t cmd);
int ts_bcast_write(ts_t *self, sng_tuple_t *tuple);
int ts_bcast_read(ts_t *self, sng_tuple_t *tuple, int blocking);
/*!
 * \brief Closes the tuple space handle.
 * \param self
 * \return
 */
int ts_close(ts_t *self);
#endif