#include <assert.h>
#include <printf.h>
#include "../../contrib/zlog/src/zlog.h"
#include "../parser/sng_parser.h"

/* 88888888888888888888888888888888888888888888888888
 Created by Ruijie Fang on 2/19/18.
   88888888888888888888888888888888888888888888888888 */
#define THREAD_COUNT 8
#define STABILIZE_INTERVAL_MS 600000
#define RECV_WAIT 5000

int main(int argc, char **argv)
{
    if (argc != 3) {
        handle_error:
        printf("tsd: usage: %s <config file> <log config file>\n",
               argv[0]);
        puts("");
        puts(" ++   COPYRIGHT (C) 2017 TEMPLE UNIVERSITY    ++");
        puts(" ++ Dr. Justin Y. Shi's Group, shi@temple.edu ++");
        puts(" ++ Rui-Jie Fang                              ++");
        puts(" ++ Yasin Celik                               ++");
        puts(" +++++++++++++++++++++++++++++++++++++++++++++++");
        puts("");
        return 1;
    }
    const char *config_file = argv[1];
    const char *log_config_file = argv[2];
    puts("starting logging service...");
    int rt = zlog_init(log_config_file);
    if (rt) {
        puts("error: zlog init failure");
        goto handle_error;
    }
    zlog_category_t *loghandle = zlog_get_category("sng_parser");
    if (!loghandle)printf("sng_parser ");
    zlog_category_t *p2pmd_loghandle = zlog_get_category("p2pmd");
    if (!p2pmd_loghandle) printf("p2pmd ");
    rt = dzlog_set_category("synergy");
    if (rt) printf("default ");
    if (!loghandle || rt || !p2pmd_loghandle) {
        puts("error: zlog category init failure");
        zlog_fini();
        goto handle_error;
    }
    puts("parsing synergy config...");
    sng_cfg_t *config = malloc(sizeof(sng_cfg_t)); /* we use heap memory */
    p2pm_opcodes_t prt = sng_parse_config(config, config_file, loghandle);
    if (prt != P2PM_OP_SUCCESS) {
        free(config);
        goto handle_error;
    }
    p2pm_config_t *p2pm_config = malloc(sizeof(p2pm_config_t));
    puts("creating config file...");
    p2pm_config_create(p2pm_config, config->node_info.predecessor, THREAD_COUNT, STABILIZE_INTERVAL_MS, RECV_WAIT);
    puts("creating p2pm config file...");
    tsd_t *tsd = malloc(sizeof(tsd_t));
    puts("creating tsd...");
    p2pm_init_args_t * args = malloc(sizeof(p2pm_init_args_t));
    args->endpoint = config->p2pmd_endpoint; /* our p2pmd BIND endpoint */
    args->log_handle = p2pmd_loghandle;
    args->config = p2pm_config;
    args->node_list = &config->node_info;
    args->status = config->p2pmd_mode;
    tsd->ipc_name = config->tsd_ipc_endpoint;
    tsd->replication_factor = config->tsd_rep_factor;
    int i = tsd_create(tsd, args, config);
    if (i != 0) {
        puts("error: tsd creation failed.");
        p2pm_config_destroy(p2pm_config);
        free(tsd);
        sng_destroy_config(config);
        free(config);
        return 1;
    }
    puts("starting tsd...");
    tsd_listen(tsd); /* let's do it! */
    return 0;
}
