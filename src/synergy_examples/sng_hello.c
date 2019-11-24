/* 88888888888888888888888888888888888888888888888888
 Created by Ruijie Fang on 2/21/18.
   88888888888888888888888888888888888888888888888888 */

#include "../ts/ts.h"
#include "../../contrib/zlog/src/zlog.h"
int main(int argc, char **argv)
{
    if (argc != 2)return 1;
    dzlog_init("../etc/simple-log.conf","default");
    puts("hello world tuple space example");
    ts_t tuple_space;
    int r = ts_open(&tuple_space, "/tspace3", argv[1], 0);
    printf("ts_open() result: %d\n", r);
    if (r != 0) {
        return 1;
    }
    const char * hellome="hello world!";
    sng_tuple_t t1 = {.anon_id=0,.name="A*",.data=(void *) hellome,.len=strlen(hellome) + 1};
    r = ts_in(&tuple_space,&t1, TsPut);
    printf("ts_put() result: r=%d\n", r);
    if (r != 0)
        return 1;
    puts("t2");
    sng_tuple_t t2 = {.anon_id = 0, .name = "B*",.data=(void*)hellome,.len=strlen(hellome) + 1};
    r = ts_in(&tuple_space,&t2, TsPut);
    printf("ts_put() 2 result: r=%d\n", r);
    if (r != 0)
        return 1;
    puts("t3");
    sng_tuple_t t3;
    const char * t3name = "A*";
    t3.name = (char *) t3name;
    r = ts_out(&tuple_space,&t3, TsGet);
    if (r != 0)
        return 1;
    printf("got result: %s\n", t3.name);
    sng_tuple_t t4;
    const char * t4name = "B*";
    t4.name = (char *) t4name;
    r = ts_out(&tuple_space, &t4, TsGet);
    if (r != 0)
        return 1;
    printf("t3: %s\nt4: %s\n", (char *) t3.data, (char *) t4.data);
    return 0;
}