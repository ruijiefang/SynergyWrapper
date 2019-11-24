/*
 * Created by Ruijie Fang on 2/9/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#ifndef P2PMD_SNG_PARSER_H
#define P2PMD_SNG_PARSER_H
#include <bson.h>
#include "../p2pm_common.h"
#include "../p2pm_utilities.h"
#include "../p2pm_types.h"
#include "../p2pm_node_info.h"
#include "../ts/ts.h"
#include "../sng_common.h"
#include "../../contrib/zlog/src/zlog.h"


/*!
 * sng_parse_config(1)
 * \brief Gets a parsed config file for Synergy.
 * \param config a user-allocated memory for config.
 * \return A success code indicating status.
 */
p2pm_opcodes_t sng_parse_config(sng_cfg_t *sng_config, const char *file, zlog_category_t * loghandle);

/*!
 * sng_destroy_config(1)
 * \brief Destroys a sng_config_t object.
 * \param config A parsed config object. Only frees a non-modified config object as parsed by parse function.
 * \return A success code indicating status.
 */
p2pm_opcodes_t sng_destroy_config(sng_cfg_t *config);


#endif //P2PMD_SNG_PARSER_H
