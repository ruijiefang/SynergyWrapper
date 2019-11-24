/*
 * Created by Ruijie Fang on 2/22/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#include "../ts/ts.h"
#include "../../contrib/zlog/src/zlog.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Synergy4-Wrapper "Hello World" example using Rajkumar's matching function
 */

int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    puts("sng_hellom_worker");
    char *pipe = argv[1];
    ts_t space;
    int r = ts_open(&space, "/tspace3", pipe, 1);
    if (r != 0) {
        puts("err opening tuple space");
        return 1;
    }
    char buf[10];
    memset(buf, '\0', 10);
    const char *m_send = "greetings from worker";
    unsigned i;
    while (1) {
        sng_tuple_t tpl = {.anon_id=0, .data=NULL, .name="!A_*", .len=0};
        r = ts_out(&space, &tpl, TsGet_Blocking);
        if (r != 0) {
            puts("worker: terminating...");
            ts_close(&space);
            return 0;
        }
        printf("worker: received tuple %s, len %lu, data= %s\n", tpl.name, tpl.len, (char *) tpl.data);
        sprintf(buf, "B_%u", atoi(&tpl.name[2]));
        printf("worker: sending back tuple %s\n", buf);
        free(tpl.data);
        tpl.len = strlen(m_send) + 1;
        tpl.anon_id = 0;
        tpl.name = buf;
        tpl.data = (void *) m_send;
        ts_in(&space, &tpl, TsPut);
        memset(buf, '\0', 10);
    }
}