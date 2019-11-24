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
    char *pipe = argv[1];
    ts_t space;
    int r = ts_open(&space, "/tspace3", pipe, 0);
    puts("sng_hellom_master");
    if (r != 0) {
        puts("err opening tuple space");
        return 1;
    }
    char buf[10];
    memset(buf, '\0', 10);
    const char *m_send = "greetings from master";
    const char *_collect = "!B*";
    unsigned i;
    for (i = 0; i < 50; ++i) {
        sprintf(buf, "A_%u", i);
        printf("putting tuple %s", buf);
        sng_tuple_t tpl = {.name = buf, .data = (void *) m_send, .len=strlen(m_send) + 1, .anon_id=0};
        ts_in(&space, &tpl, TsPut);
        memset(buf, '\0', 10);
    }
    puts("work done. sleeping for 10s and collecting results...");
    for (i = 0; i < 50; ++i) {
        printf("collecting tuple %u\n", i);
        sng_tuple_t tpl = {.data = NULL, .len = 0, .name = (char *) _collect, .anon_id=0};
        while (ts_out(&space, &tpl, TsGet_Blocking) != 0)
            printf("  - wait: tuple %u\n", i);
        printf("successfully collected tuple %u, len %lu, data=%s\n", i, tpl.len, (char *) tpl.data);
        free(tpl.data);
        free(tpl.name);
    }
    puts("all tuples received. terminating...");
    ts_close(&space);
    return 0;
}