/*
 * Created by Ruijie Fang on 2/11/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

/*
 * Utilities for using simple bit vectors
 * for len <= 32bit use: *i*, *u*
 * for len >= 32bit <= 64bit use: *l*, *ul*
 * for len >= 64bit use: *ll*, *ull*
 * (Recommended use for best balance between safe shifting and space efficiency)
 *
 * set(b,x) set b's x position to 1
 * get(b,x) get b's x position's bit value (0|1)
 * unset(b,x) set b's x position to 0
 * toggle(b,x) XOR b's x position (set to 0 if 1, set to 1 if 0)
 *
 * \author: Ruijie Fang, rjf
 */

#define _bviset(b, x) (b |= (1 << x))
#define _bviunset(b, x) (b &= ~(1 << x))
#define _bvitoggle(b, x) (b ^= (1 << x))
#define _bviget(b, x) ((b >> x) & 1)

#define _bvuset(b, x) (b |= (1U << x))
#define _bvuunset(b, x) (b &= ~(1U << x))
#define _bvutoggle(b, x) (b ^= (1U << x))
#define _bvuget(b, x) ((b >> x) & 1U)

#define _bvlset(b, x) (b |= (1L << x))
#define _bvlunset(b, x) (b &= ~(1L << x))
#define _bvltoggle(b, x) (b ^= (1L << x))
#define _bvlget(b, x) ((b >> x) & 1L)

#define _bvulset(b, x) (b |= (1UL << x))
#define _bvulunset(b, x) (b &= ~(1UL << x))
#define _bvultoggle(b, x) (b ^= (1UL << x))
#define _bvulget(b, x) ((b >> x) & 1UL)


#define _bvllset(b, x) (b |= (1LL << x))
#define _bvllunset(b, x) (b &= ~(1LL << x))
#define _bvlltoggle(b, x) (b ^= (1LL << x))
#define _bvllget(b, x) ((b >> x) & 1LL)

#define _bvullset(b, x) (b |= (1ULL << x))
#define _bvullunset(b, x) (b &= ~(1ULL << x))
#define _bvulltoggle(b, x) (b ^= (1ULL << x))
#define _bvullget(b, x) ((b >> x) & 1ULL)

typedef int bitset_t;
typedef long lbitset_t;
typedef unsigned ubitset_t;
typedef unsigned long ulbitset_t;
typedef long long llbitset_t;
typedef unsigned long long ullbitset_t;