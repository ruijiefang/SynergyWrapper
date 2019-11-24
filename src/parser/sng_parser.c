/*
 * Created by Ruijie Fang on 2/9/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#include "sng_parser.h"
#include "../../contrib/zlog/src/zlog.h"


#define IS_BSON_SUBDOCUMENT(itr_ref, itr_child_ref, doc, name) \
    (bson_iter_init_find(itr_ref, doc, name) && \
    bson_iter_recurse(itr_ref, itr_child_ref))

#include <zmq.h>
#include <czmq.h>
#include <bson.h>
#include <sys/uio.h>

#define PERROR(x) do { zlog_debug(loghandle, "Error while parsing: %s \n", x); return P2PM_OP_FAILURE; } while(0)

static const char magic0 = 0x3F; /* b00111111 */
static const char magic1 = 0b00001111;
#define _bvset(b, x) (b |= (1 << x))
#define _bvunset(b, x) (b &= ~(1 << x))
#define _bvtoggle(b, x) (b ^= (1 << x))
#define _bvget(b, x) ((b >> x) & 1)
#define _BVMODE 0
#define _BVENDPT 1
#define _BVR 2
#define _BVRMAX 3
#define _BVSUCCLIST 4
#define _BVPRED 5
#define _BVSN 0
#define _BVSI 1
#define _BVSM 2
#define _BVST 3

static inline p2pm_opcodes_t _handle_parse_failure(sng_cfg_t *self, short section)
{
    assert(self);
    switch (section) {
        case 1: {
            free(self->spaces);
        }
        case 0: {
            if (_bvget(self->bv, _BVENDPT)) {
                /* endpoint is set, free */
                free(self->p2pmd_endpoint);
            }
            if (_bvget(self->bv, _BVPRED)) {
                /* free node_list's predecessor */
                free(self->node_info.predecessor);
            }
            if (_bvget(self->bv, _BVSUCCLIST)) {
                /* free succList */
                free(self->node_info.successor_list);
            }
            break;
        }

    }
    return P2PM_OP_SUCCESS;

}

/* layout for bit vector
 * section p2pmd
 * [0 0 1(predecessor) 1(successor_list)  1(r_max) 1(r) 1(endpoint) 1(mode)]
 * [* * ^              ^                  ^        ^    ^           ^      ]
 * section spaces[n]
 * [0 0 0 1(has tsd_mode endpoint) 1(isdirect/token) 1 (mode) 1(id) 1(name)]
 */


/*!
 * sng_parse_config(1)
 * \brief Gets a parsed config file for Synergy.
 * \param config a user-allocated memory for config.
 * \return A success code indicating status.
 */
p2pm_opcodes_t sng_parse_config(sng_cfg_t *sng_config, const char *file, zlog_category_t *loghandle)
{
    bson_json_reader_t *reader;
    bson_t bson;
    bson_error_t err;
    sng_config->bv = 0; /* clear all bits */
    bson_init(&bson);
    reader = bson_json_reader_new_from_file(file, &err);
    assert(reader != NULL);
    int rt = bson_json_reader_read(reader, &bson, &err);
    if (rt < 0) {
        zlog_debug(loghandle, "err reading file!\n");
        zlog_debug(loghandle, "err: %s (%d)\n", err.message, err.code);
        exit(1);
    }
    {
        /* parser p2pmd section */
        char **_slist_entries = NULL;
        unsigned num_entries = 0;
        bson_iter_t iter, iter_ch;

        if (bson_iter_init_find(&iter, &bson, "p2pmd") &&
            BSON_ITER_HOLDS_DOCUMENT (&iter) && bson_iter_recurse(&iter, &iter_ch)) {
            while (bson_iter_next(&iter_ch)) {
                if (streq(bson_iter_key(&iter_ch), "mode")) {
                    /* parse mode */
                    if (!BSON_ITER_HOLDS_UTF8(&iter_ch)) {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("mode");

                    }
                    unsigned len;
                    const char *mode_s = bson_iter_utf8(&iter_ch, &len);
                    if (streq(mode_s, "member")) {
                        sng_config->p2pmd_mode = P2PM_NODE_MEMBER;
                        zlog_debug(loghandle, "verified: mode = member");
                    } else if (streq(mode_s, "standalone")) {
                        sng_config->p2pmd_mode = P2PM_NODE_JOINING;
                        zlog_debug(loghandle, "verified: mode = standalone");
                    } else {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("mode");
                    }
                    if (_bvget(sng_config->bv, _BVMODE)) {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("p2pmd.mode field set more than once");
                    }
                    _bvset(sng_config->bv, _BVMODE);
                } else if (streq(bson_iter_key(&iter_ch), "endpoint")) {
                    /* parse p2pmd endpoint */
                    if (BSON_ITER_HOLDS_UTF8(&iter_ch)) {
                        unsigned len;
                        char *endpoint_s = bson_iter_dup_utf8(&iter_ch, &len);
                        sng_config->p2pmd_endpoint = endpoint_s;
                        zlog_debug(loghandle, "endpoint=%s\n", endpoint_s);
                    } else {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("endpoint");
                    }
                    if (_bvget(sng_config->bv, _BVENDPT)) {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("p2pmd.endpoint set more than once");
                    }
                    _bvset(sng_config->bv, _BVENDPT);
                } else if (streq(bson_iter_key(&iter_ch), "r")) {
                    if (BSON_ITER_HOLDS_INT32(&iter_ch)) {
                        int r_s = bson_iter_int32(&iter_ch);
                        sng_config->node_info.r = (unsigned int) r_s;
                        zlog_debug(loghandle, "r=%d\n", r_s);
                    } else {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("r");
                    }
                    if (_bvset(sng_config->bv, _BVSUCCLIST) &&
                        sng_config->_r_list != sng_config->node_info.r) {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("r != len(list)");
                    }
                    if (_bvget(sng_config->bv, _BVR)) {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("p2pmd.r set more than once");
                    }
                    _bvset(sng_config->bv, _BVR);
                } else if (streq(bson_iter_key(&iter_ch), "r_max")) {
                    if (BSON_ITER_HOLDS_INT32(&iter_ch)) {
                        int r_max_s = bson_iter_int32(&iter_ch);
                        sng_config->node_info.r_max = (unsigned int) r_max_s;
                        zlog_debug(loghandle, "r_max=%d\n", r_max_s);
                    } else {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("r_max");
                    }
                    if (_bvget(sng_config->bv, _BVRMAX)) {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("p2pmd.r_max set more than once");
                    }
                    _bvset(sng_config->bv, _BVRMAX);
                } else if (streq(bson_iter_key(&iter_ch), "successor_list")) {
                    bson_iter_t iter_ch_ch;
                    if (BSON_ITER_HOLDS_ARRAY(&iter_ch) &&
                        bson_iter_recurse(&iter_ch, &iter_ch_ch)) {
                        zlog_debug(loghandle, "Recursively parsing successor_list...");
                        unsigned cnt = 1;
                        sng_config->_r_list = 0;
                        {
                            bson_iter_t walk;
                            if (bson_iter_recurse(&iter_ch, &walk)) {
                                while (bson_iter_next(&walk)) {
                                    ++sng_config->_r_list;
                                }
                            } else {
                                _handle_parse_failure(sng_config, 0);
                                PERROR("successor_list");
                            }
                        }
                        if (_bvget(sng_config->bv, _BVR) && sng_config->_r_list != sng_config->node_info.r) {
                            _handle_parse_failure(sng_config, 0);
                            PERROR("len(list) != len(r)");
                        }
                        sng_config->node_info.successor_list = malloc(P2PM_MAX_ID_LEN * sng_config->_r_list);
                        memset(sng_config->node_info.successor_list, '\0', P2PM_MAX_ID_LEN * sng_config->_r_list);
                        char *ptr = sng_config->node_info.successor_list;
                        while (bson_iter_next(&iter_ch_ch))
                            if (BSON_ITER_HOLDS_UTF8(&iter_ch_ch)) {
                                unsigned len;
                                const char *successor_x = bson_iter_utf8(&iter_ch_ch, &len);
                                zlog_debug(loghandle, "successor %d = %s\n", cnt, successor_x);
                                ++cnt;
                                if (len > P2PM_MAX_ID_LEN) {
                                    free(sng_config->node_info.successor_list);
                                    _handle_parse_failure(sng_config, 0);
                                    PERROR("successor list element too long");
                                }
                                memcpy(ptr, successor_x, len);
                                ptr += P2PM_MAX_ID_LEN;
                            } else {
                                _handle_parse_failure(sng_config, 0);
                                PERROR("successor_list");
                            }
                    } else {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("successor_list");
                    }
                    if (_bvget(sng_config->bv, _BVSUCCLIST)) {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("p2pmd.successor_list set more than once");
                    }
                    _bvset(sng_config->bv, _BVSUCCLIST);
                } else if (streq(bson_iter_key(&iter_ch), "predecessor")) {
                    if (BSON_ITER_HOLDS_UTF8(&iter_ch)) {
                        if (BSON_ITER_HOLDS_UTF8(&iter_ch)) {
                            unsigned len;
                            const char *predecessor_s = bson_iter_utf8(&iter_ch, &len);
                            if (len > P2PM_MAX_ID_LEN) {
                                _handle_parse_failure(sng_config, 0);
                                PERROR("predecessor length too big");
                            }
                            sng_config->node_info.predecessor = malloc(P2PM_MAX_ID_LEN);
                            memset(sng_config->node_info.predecessor, '\0', P2PM_MAX_ID_LEN);
                            memcpy(sng_config->node_info.predecessor, predecessor_s, len);
                            zlog_debug(loghandle, "predecessor = %s\n", predecessor_s);
                        } else {
                            _handle_parse_failure(sng_config, 0);
                            PERROR("predecessor (not a string)");
                        }
                    } else {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("predecessor");
                    }
                    if (_bvget(sng_config->bv, _BVPRED)) {
                        _handle_parse_failure(sng_config, 0);
                        PERROR("predecessor is set more than once");
                    }
                    _bvset(sng_config->bv, _BVPRED);
                } else {
                    _handle_parse_failure(sng_config, 0);
                    zlog_debug(loghandle, "Error: Encountered invalid json field: %s\n", bson_iter_key(&iter_ch));
                    exit(1);
                }
            }
        } else {
            _handle_parse_failure(sng_config, 0);
            PERROR("subfield p2pmd not a document");
        }
        if (sng_config->bv != magic0) {
            zlog_debug(loghandle, "error: some fields are unparsed: ");

            for (int i = 0; i < 6; ++i)
                if (!_bvget(sng_config->bv, i)) {
                    zlog_debug(loghandle, "%d ", i);
                }
            zlog_debug(loghandle, "\n");
            exit(1);
        }
        sng_config->bv = 0; /* reset our bit vector */
        zlog_debug(loghandle, "> mode: %d\n", sng_config->p2pmd_mode);
        zlog_debug(loghandle, "> endpoint: %s\n", sng_config->p2pmd_endpoint);
        zlog_debug(loghandle, "> r: %d\n", sng_config->node_info.r);
        zlog_debug(loghandle, "> r_max: %d\n", sng_config->node_info.r_max);
        zlog_debug(loghandle, "> successor_list: [");
        unsigned _i;
        for (_i = 0; _i < sng_config->node_info.r; ++_i)
            zlog_debug(loghandle, "%s, ", sng_config->node_info.successor_list + (_i * P2PM_MAX_ID_LEN));
        zlog_debug(loghandle, "]\n");
        zlog_debug(loghandle, "> predecessor: %s\n", sng_config->node_info.predecessor);

    }
    {
        /* parser tsd section */
        bson_iter_t iter, iter_ch;
        zlog_debug(loghandle, "parsing tsd section");
        if (bson_iter_init_find(&iter, &bson, "tsd") &&
            BSON_ITER_HOLDS_DOCUMENT(&iter) && bson_iter_recurse(&iter, &iter_ch)) {
            zlog_debug(loghandle, "endpoint");
            if (bson_iter_find(&iter_ch, "endpoint")) {
                unsigned len;
                sng_config->tsd_endpoint = bson_iter_dup_utf8(&iter_ch, &len);
            } else {
                _handle_parse_failure(sng_config, 0);
                PERROR("'tsd' subfield endpoint not found");
            }
            zlog_debug(loghandle, "\t=%s\nipc", sng_config->tsd_endpoint);
            if (bson_iter_find(&iter_ch, "ipc")) {
                unsigned len;
                sng_config->tsd_ipc_endpoint
                        = bson_iter_dup_utf8(&iter_ch, &len);
            } else {
                _handle_parse_failure(sng_config, 0);
                PERROR("'tsd' subfield ipc not found");
            }
            zlog_debug(loghandle, "\t=%s\nrf", sng_config->tsd_ipc_endpoint);
            if (bson_iter_find(&iter_ch, "replication_factor")) {
                unsigned len;
                sng_config->tsd_rep_factor = (unsigned int) bson_iter_int32(&iter_ch);
            } else {
                _handle_parse_failure(sng_config, 0);
                PERROR("'tsd' subfield replication_factor not found");
            }
            zlog_debug(loghandle, "\t=%ul\n", sng_config->tsd_rep_factor);
        } else {
            _handle_parse_failure(sng_config, 0);
            PERROR("'tsd' subsection not found (subdocument missing)");
        }
        zlog_debug(loghandle, "tsd endpoint: %s\n", sng_config->tsd_endpoint);
        zlog_debug(loghandle, "tsd ipc endpoint: %s\n",
                   sng_config->tsd_ipc_endpoint);
    }
    {
        /* parser spaces section */
        bson_iter_t iter, iter_ch;
        if (bson_iter_init_find(&iter, &bson, "spaces") &&
            BSON_ITER_HOLDS_ARRAY(&iter) && bson_iter_recurse(&iter, &iter_ch)) {
            zlog_debug(loghandle, "spaces found\n");
            unsigned _cnt = 0;
            {/* count number of entries */
                bson_iter_t walker;
                bson_iter_recurse(&iter, &walker);
                sng_config->num_spaces = 0;
                while (bson_iter_next(&walker))
                    ++sng_config->num_spaces;
                sng_config->spaces = malloc(sizeof(sng_namespace_t) * sng_config->num_spaces);
            }
            while (bson_iter_next(&iter_ch)) {
                bson_iter_t iter_ch_ch;
                if (BSON_ITER_HOLDS_DOCUMENT(&iter_ch) &&
                    bson_iter_recurse(&iter_ch, &iter_ch_ch)) {
                    sng_config->bv = 0;
                    zlog_debug(loghandle, "{\n");
                    /* member of spaces array */
                    while (bson_iter_next(&iter_ch_ch)) {
                        if (streq(bson_iter_key(&iter_ch_ch), "name") &&
                            BSON_ITER_HOLDS_UTF8(&iter_ch_ch)) {
                            unsigned len;
                            const char *name_s = bson_iter_utf8(&iter_ch_ch, &len);
                            zlog_debug(loghandle, "\tname = %s;\n", name_s);
                            if (len > 25) {
                                _handle_parse_failure(sng_config, 1);
                                PERROR("\tfield name too long");
                            }
                            if (_bvget(sng_config->bv, _BVSN)) {
                                _handle_parse_failure(sng_config, 1);
                                PERROR("\tfield name set more than once");
                            }

                            _bvset(sng_config->bv, _BVSN);
                            memset(sng_config->spaces[_cnt].name, '\0', 30);
                            memcpy(sng_config->spaces[_cnt].name, name_s, len);
                        } else if (streq(bson_iter_key(&iter_ch_ch), "id") &&
                                   BSON_ITER_HOLDS_INT32(&iter_ch_ch)) {
                            int id_s = bson_iter_int32(&iter_ch_ch);
                            zlog_debug(loghandle, "\tid = %d;\n", id_s);
                            if (_bvget(sng_config->bv, _BVSI)) {
                                _handle_parse_failure(sng_config, 1);
                                PERROR("\tfield id set more than once");
                            }
                            _bvset(sng_config->bv, _BVSI);
                            sng_config->spaces[_cnt].id = id_s;
                        } else if (streq(bson_iter_key(&iter_ch_ch), "mode") &&
                                   BSON_ITER_HOLDS_UTF8(&iter_ch_ch)) {
                            unsigned len;
                            const char *mode_s = bson_iter_utf8(&iter_ch_ch, &len);
                            if (streq(mode_s, "master")) {
                                zlog_debug(loghandle, "\tmode = master\n");
                                sng_config->spaces[_cnt].ts_mode = TsModeMaster;
                            } else if (streq(mode_s, "worker")) {
                                zlog_debug(loghandle, "\tmode = worker\n");
                                sng_config->spaces[_cnt].ts_mode = TsModeWorker;
                            } else if (streq(mode_s, "p2p")) {
                                zlog_debug(loghandle, "\tmode = p2p\n");
                                sng_config->spaces[_cnt].ts_mode = TsModeP2P;
                            } else {
                                _handle_parse_failure(sng_config, 1);
                                PERROR("\t**mode field is illegal.\n");
                            }
                            if (_bvget(sng_config->bv, _BVSM)) {
                                _handle_parse_failure(sng_config, 1);
                                PERROR("\tmode field set more than once");
                            }
                            _bvset(sng_config->bv, _BVSM);
                        } else if (streq(bson_iter_key(&iter_ch_ch), "tsd_mode") &&
                                   BSON_ITER_HOLDS_ARRAY(&iter_ch_ch)) {
                            bson_iter_t tsd_mode_iter;
                            if (bson_iter_recurse(&iter_ch_ch, &tsd_mode_iter)) {
                                char expect = 'x';
                                unsigned cnt = 0; /* either 'd' or 'a' */
                                while (bson_iter_next(&tsd_mode_iter)) {
                                    if (atoi(bson_iter_key(&tsd_mode_iter)) == 0 &&
                                        BSON_ITER_HOLDS_UTF8(&tsd_mode_iter)) {
                                        unsigned len;
                                        const char *tsd_mode_x = bson_iter_utf8(&tsd_mode_iter, &len);
                                        if (streq(tsd_mode_x, "token")) {
                                            expect = 'a';
                                            zlog_debug(loghandle, "\t\t tsd_mode = token (no more)\n");
                                            sng_config->spaces[_cnt].tsd_mode = TsdModeToken;
                                        } else if (streq(tsd_mode_x, "direct")) {
                                            expect = 'd';
                                            zlog_debug(loghandle, "\t\t tsd_mode = direct (parsing more)\n");
                                            sng_config->spaces[_cnt].tsd_mode = TsdModeDirect;
                                        } else {
                                            _handle_parse_failure(sng_config, 1);
                                            PERROR("'tsd_mode' has invalid subfield at 0");
                                        }
                                    } else if (atoi(bson_iter_key(&tsd_mode_iter)) == 1 &&
                                               expect == 'd') {
                                        unsigned len;
                                        const char *tsd_master_x = bson_iter_utf8(&tsd_mode_iter, &len);
                                        if (len > P2PM_MAX_ID_LEN) {
                                            _handle_parse_failure(sng_config, 1);
                                            PERROR("tsd_master_len too big");
                                        }
                                        zlog_debug(loghandle, "\t\t tsd_master_addr = %s\n", tsd_master_x);
                                        memset(sng_config->spaces[_cnt]._direct_endpoint, '\0', P2PM_MAX_ID_LEN);
                                        memcpy(sng_config->spaces[_cnt]._direct_endpoint, tsd_master_x, len);
                                    } else {
                                        _handle_parse_failure(sng_config, 1);
                                        PERROR("'tsd_mode' has too many or invalid subfields");
                                    }
                                    ++cnt;
                                }
                                if ((!(expect == 'a' && cnt == 1) && !(expect == 'd' && cnt == 2)) || expect == 'x') {
                                    _handle_parse_failure(sng_config, 1);
                                    PERROR("'tsd_mode' subfield does not match expected amount of subfields");
                                }
                                if (_bvget(sng_config->bv, _BVST)) {
                                    _handle_parse_failure(sng_config, 1);
                                    PERROR("multiple 'tsd_mode' fields");
                                }
                                _bvset(sng_config->bv, _BVST);
                            } else {
                                _handle_parse_failure(sng_config, 1);
                                PERROR("Cannot traverse 'tsd_mode' subarray.");
                            }
                        } else /* if(...) else if (...) else if (...) matcher */ {
                            _handle_parse_failure(sng_config, 1);
                            PERROR("spaces subarray member contains illegal field");
                        }
                    } /* while (bson_iter_next(&iter_ch_ch) */
                    zlog_debug(loghandle, "}\n");
                    ++_cnt; /* point to next empty spaces array entry */
                } else /* if (BSON_ITER_HOLDS_DOCUMENT...) */ {
                    _handle_parse_failure(sng_config, 1);
                    PERROR("an element in spaces subarray is not a subdocument");
                }
                if (sng_config->bv != magic1) {
                    zlog_debug(loghandle, "Err: fields: ");
                    int i;
                    for (i = 0; i < 4; ++i)
                        if (!_bvget(sng_config->bv, i))
                            zlog_debug(loghandle, "%d ", i);
                    zlog_debug(loghandle, " are not set.\n");
                    _handle_parse_failure(sng_config, 1);
                    exit(1);
                }
            }   /* while(bson_iter_next(&iter_chh)) */

        } else {
            _handle_parse_failure(sng_config, 0);
            zlog_debug(loghandle, "spaces section not found");
        }

    }
    // print
    {
        zlog_debug(loghandle, " *** *** *** *** *** ");
        unsigned i;
        for (i = 0; i < sng_config->num_spaces; ++i) {
            zlog_debug(loghandle, "[%d]\n", i);
            zlog_debug(loghandle, "\tname: %s\n", sng_config->spaces[i].name);
            zlog_debug(loghandle, "\tid:%d\n", sng_config->spaces[i].id);
            zlog_debug(loghandle, "\tTS Mode: %s\n",
                       sng_config->spaces[i].ts_mode == TsModeWorker ? "Worker" : "Master");
            zlog_debug(loghandle, sng_config->spaces[i].tsd_mode == TsdModeToken ? "\tTSD Mode: Token%s\n"
                                                                                 : "\tTSD Mode: Direct (%s)\n",
                       sng_config->spaces[i].tsd_mode == TsdModeToken ? "." : sng_config->spaces[i]._direct_endpoint);
        }
    }
    zlog_debug(loghandle, " *** PARSING SUCCESS *** (Is legal config document.)");
    sng_config->node_info.successor = malloc(P2PM_MAX_ID_LEN);
    sng_config->node_info.temp_predecessor = malloc(P2PM_MAX_ID_LEN);
    sng_config->node_info.temp_successor = malloc(P2PM_MAX_ID_LEN);

    memset(sng_config->node_info.successor, '\0', P2PM_MAX_ID_LEN);
    memset(sng_config->node_info.temp_predecessor, '\0', P2PM_MAX_ID_LEN);
    memset(sng_config->node_info.temp_successor, '\0', P2PM_MAX_ID_LEN);

    memcpy(sng_config->node_info.successor, sng_config->node_info.successor_list,
           PSTRLEN(sng_config->node_info.successor_list));
    bson_destroy(&bson);
    bson_json_reader_destroy(reader);
    sng_config->bv = 0;
    return P2PM_OP_SUCCESS;
}

/*!
 * sng_destroy_config(1)
 * \brief Destroys a sng_config_t object.
 * \param config A parsed config object. Only frees a non-modified config object as parsed by parse function.
 * \return A success code indicating status.
 */
p2pm_opcodes_t sng_destroy_config(sng_cfg_t *config)
{
    free(config->p2pmd_endpoint);
    free(config->tsd_endpoint);
    free(config->spaces);
    free(config->node_info.predecessor);
    free(config->node_info.successor_list);
    return P2PM_OP_SUCCESS;
}