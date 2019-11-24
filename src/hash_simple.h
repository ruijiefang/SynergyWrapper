/*
 * Created by Ruijie Fang on 2/19/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#ifndef P2PMD_HASH_SIMPLE_H
#define P2PMD_HASH_SIMPLE_H

#include "../contrib/libchash/libchash.h"

typedef struct _simple_table {
    struct HashTable* table;
    int data_len;
    _Bool copy_keys;
} hash_table_t;

inline static int
simplehash_create(hash_table_t * self, int sizeof_data, _Bool copy_keys)
{

    assert(self);
    self->table = AllocateHashTable(sizeof_data, copy_keys);
    if (self->table == NULL)
        return 1;
    self->copy_keys = copy_keys;
    self->data_len = sizeof_data;
    return 0;
}

inline static int
simplehash_insert(hash_table_t * self, const char * key, void * data)
{
    assert(self);
    assert(key);
    assert(data);
    HashInsert(self->table, PTR_KEY(self->table, key), PTR_KEY(self->table, data));
}

#endif //P2PMD_HASH_SIMPLE_H
