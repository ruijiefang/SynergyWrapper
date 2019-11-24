/*
 * Created by Ruijie Fang on 2/6/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#ifndef P2PMD_P2PM_INIT_H
#define P2PMD_P2PM_INIT_H

#include "p2pmd/p2pmd.h"

inline static p2pm_opcodes_t p2pm_init() {
    srand(time(NULL));
    srandom(time(NULL));
    return P2PM_OP_SUCCESS;
}

inline static void p2pm_rand_reseed() {
    srand(time(NULL));
    srandom(time(NULL));
}
#endif //P2PMD_P2PM_INIT_H
