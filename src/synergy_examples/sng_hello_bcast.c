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
#define BIG 1000000
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
    char * data = malloc(BIG);
    long i;
    for(i =0; i < BIG;++i)
        data[i] = 'k';
    sng_tuple_t bcast_tpl = {.name = "BigTuple",.len=BIG,.data=data,.anon_id=0};
    puts("broadcasting tuple...");
    ts_bcast_write(&space, &bcast_tpl);
    free(data);
    puts("sleeping 10s and anticipating results...");
    sleep(10);
    sng_tuple_t recv_tpl = {.name = "ReplyTuple",.len=0,.data=NULL,.anon_id=0};
    while(ts_out(&space,&recv_tpl, TsGet_Blocking)!=0) puts("expecting ReplyTuple...");
    puts("reply tuple received");
    printf("reply tuple name %s, len %lu\n",recv_tpl.name,recv_tpl.len);
    free(recv_tpl.name);
    free(recv_tpl.data);
    ts_close(&space);
    return 0;
}