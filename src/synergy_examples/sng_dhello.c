/*
 * Created by Ruijie Fang on 2/21/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */


#include "../ts/ts.h"
#include "../../contrib/zlog/src/zlog.h"

int main(int argc, char **argv)
{
    if (argc != 2)return 1;
    dzlog_init("../etc/simple-log.conf", "default");
    puts("hello world tuple space example");
    ts_t tuple_space;
    ts_t solution_ts;
    int r = ts_open(&tuple_space, "/tspace3", argv[1], 0);
    printf("ts_open() result: %d\n", r);

    if (r != 0) {
        printf("err: ts_open() failure\n");
        return 1;
    }

    puts("opening up solution space...");
    r = ts_open(&solution_ts, "/tspace4", argv[1], 0);
    printf("ts_open() result: %d\n", r);

    if (r != 0) {
        return 1;
    }

    char *hellome = "hello workers!", namebuf[8];
    unsigned i;
    for (i = 0; i < 30; ++i) {
        sprintf(namebuf, "A_%u", i);
        printf("master: sending %u/30: %s\n", i, namebuf);
        sng_tuple_t snd = {.anon_id =0, .data=(void *) hellome, .len=strlen(hellome) + 1, .name=(char *) namebuf};
        r = ts_in(&tuple_space, &snd, TsPut);
        if (r != 0) {
            puts("error: ts_put failed");
            return 1;
        }
        memset(namebuf, '\0', 8);
    }
    memset(namebuf, '\0', 8);
    puts("waiting for workers to finish...");


    puts("Now anticipating responses...");
    puts("");
    for (i = 0; i < 30; ++i) {
        sprintf(namebuf, "B_%u", i);
        sng_tuple_t recv = {.name = (char *) namebuf};
        while (ts_out(&solution_ts, &recv, TsGet_Blocking) != 0)
            printf("\rstill expecting reply tuple %d to arrive...\n", i);
        printf("\rTuple %u arrived, len %lu, content %s (name=%s)\n", i, recv.len, (char *) recv.data, recv.name);
        puts("");
        memset(namebuf, '\0', 8);
    }
    puts("successfully collected all tuples. Bye!");
    ts_close(&solution_ts);
    ts_close(&tuple_space);
    return 0;
}