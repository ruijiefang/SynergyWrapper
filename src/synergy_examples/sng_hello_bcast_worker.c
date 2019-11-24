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
    char buf[10], buf2[10];
    memset(buf, '\0', 10);
    memset(buf2, '\0', 10);
    sng_tuple_t bcast_tuple = {.name = "BigTuple"};
    sng_tuple_t recv_tuple = {.name="ReplyTuple"};
    ts_bcast_read(&space, &bcast_tuple, TsRead_Blocking);
    puts("bcast tuple received");
    printf("t: len %lu, name %s, data[99]=%c\n", bcast_tuple.len, bcast_tuple.name, ((char *) bcast_tuple.data)[99]);
    free(bcast_tuple.name);
    free(bcast_tuple.data);
    recv_tuple.data = malloc(999);
    recv_tuple.len = 999;
    ts_in(&space, &recv_tuple, TsPut);
    sleep(3);
    free(recv_tuple.data);
    puts("resting...");
    ts_close(&space);
    return 0;
}