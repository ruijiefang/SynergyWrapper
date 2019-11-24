/*
 * Created by Ruijie Fang on 2/21/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */


#include "../ts/ts.h"
#include "../../contrib/zlog/src/zlog.h"
#include "../tuple_utils.h"

int main(int argc, char **argv)
{
    if (argc != 2)return 1;
    dzlog_init("../etc/simple-log.conf", "default");
    puts("hello worker example");
    ts_t tuple_space;
    ts_t solution_ts;
    int r = ts_open(&tuple_space, "/tspace3", argv[1], 1);
    printf("ts_open() 1 result: %d\n", r);
    if (r != 0) {
        puts("error: ts_open() failed");
        return 1;
    }
    r = ts_open(&solution_ts, "/tspace4", argv[1], 0);
    printf("ts_open() 2 result: %d\n", r);
    if (r != 0) {
        puts("error: ts_open() failed");
        return 1;
    }
    char *worker_reply = "reply from worker: bravo!", namebuf[8];
    while (1) {
        puts("getting a chunk of work...");
        sng_tuple_t tpl = {.data=NULL, .name=NULL, .len=0, .anon_id=0};
        r = ts_out(&tuple_space, &tpl, TsPop_Blocking);
        if (r != 0) {
            printf("err: %d", r);
            return 0;
        }
        printf("worker: got work name %s content %s len %lu]\n", tpl.name, (char *) tpl.data, tpl.len);
        int rtnum = atoi(&tpl.name[2]);
        free(tpl.name);
        free(tpl.data);
        tpl.data = (void *) worker_reply;
        sprintf(namebuf, "B_%d", rtnum);
        printf("worker: sending tuple of name %s back\n", namebuf);
        tpl.name = (char *) namebuf;
        tpl.len = strlen(worker_reply) + 1;
        tpl.anon_id = 0;
        ts_in(&solution_ts, &tpl, TsPut);
        puts("worker: done, getting another chunk...");
        memset(namebuf, '\0', 8);
    }
}