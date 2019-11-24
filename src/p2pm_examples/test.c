/*
 * Created by Ruijie Fang on 2/5/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#include "../p2pm_types.h"
#include <zmq.h>
#include <czmq.h>
#include <bson.h>
#include "../parser/sng_parser.h"
#include "../p2pm_common.h"
#include "../../contrib/zlog/src/zlog.h"
#if 0
static void TestInsert()
{
    struct HashTable *ht;
    HTItem *bck;

    ht = AllocateHashTable(1, 0);    /* value is 1 byte, 0: don't copy keys */

    HashInsert(ht, PTR_KEY(ht, "January"), 31);  /* 0: don't overwrite old val */
    bck = HashInsert(ht, PTR_KEY(ht, "February"), 28);
    bck = HashInsert(ht, PTR_KEY(ht, "March"), 31);

    bck = HashFind(ht, PTR_KEY(ht, "February"));

    assert(bck);
    assert(bck->data == 28);

    FreeHashTable(ht);
}

static void TestFindOrInsert()
{
    struct HashTable *ht;
    int i;
    int iterations = 1000000;
    int range = 30;         /* random number between 1 and 30 */

    ht = AllocateHashTable(4, 0);    /* value is 4 bytes, 0: don't copy keys */

    /* We'll test how good rand() is as a random number generator */
    for (i = 0; i < iterations; ++i) {
        int key = rand() % range;
        HTItem *bck = HashFindOrInsert(ht, key, 0);     /* initialize to 0 */
        bck->data++;                   /* found one more of them */
    }

    for (i = 0; i < range; ++i) {
        HTItem *bck = HashFind(ht, i);
        if (bck) {
            printf("%3d: %d\n", bck->key, bck->data);
        } else {
            printf("%3d: 0\n", i);
        }
    }

    FreeHashTable(ht);
}

void test(struct HashTable * table)
{
    int b = 75;
    HashInsert(table, PTR_KEY(table, "invalid"), PTR_KEY(table, &b));
    printf("str inserted at %p\n", (void *) &b);
}

int main(void)
{
    struct HashTable *table;
    table = AllocateHashTable(0, 1);
    test(table);

    sleep(3);

    HTItem * item = HashFind(table, PTR_KEY(table, "invalid"));
    int * str = (int *) KEY_PTR(table, item->data);
    char * key = KEY_PTR(table, item->key);
    printf("htable key %s, data %d, %p\n", key, *str, (void*)str);
    return 0;
}
#endif
int main(void)
{
    puts(" -- catastrophic failure --");
    zsys_init();
    return 0;
}