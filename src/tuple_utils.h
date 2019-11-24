/*
 * Created by Ruijie Fang on 2/20/18.
 * Copyright (c) 2018 TU. All rights reserved.
 */

#ifndef P2PMD_TUPLE_UTILS_H
#define P2PMD_TUPLE_UTILS_H

#include "sng_common.h"
#include <regex.h>

/* from old Synergy V3+ code, by N. Isaac Rajkumar */
 int _match(char *expr, char *name)
{
    /* Need to add app_id match checking */
    while (*expr == *name) {/* skip as long as the strings match */

        if (*name++ == '\0')
            return 0;
        expr++;
    }
    if (*expr == '?') {                           /* '?' - skip one character */
        if (*name != '\0')
            return (_match(++expr, ++name));
    } else if (*expr == '*') {
        expr++;
        do {                 /* '*' - skip 0 or more characters */
            /* try for each of the cases */
            if (!_match(expr, name))
                return 0;
        } while (*name++ != '\0');
    }
    return 1;                   /* no match found */
}


/* by comparing anon_id's we can actually establish a total ordering */
 int tplicmp(const void *tuple1, const void *tuple2)
{
    if (((sng_tuple_t *) tuple1)->anon_id < ((sng_tuple_t *) tuple2)->anon_id)
        return -1;
    else if (((sng_tuple_t *) tuple1)->anon_id > ((sng_tuple_t *) tuple2)->anon_id)
        return 1;
    else
        return 0;
}

 int tplregex(const void *tuple1, const void *tuple2)
{
    int rt;
    regex_t rx;
    rt = regcomp(&rx, ((sng_tuple_t *) tuple1)->name + 1, 0);
    if (rt) {
        regfree(&rx);
        return 1;
    }
    rt = regexec(&rx, ((sng_tuple_t *) tuple2)->name, 0, NULL, 0);
    if (!rt) {
        regfree(&rx);
        return 0;
    } else {
        regfree(&rx);
        return 1;
    }
}

 int tplcmp(const void *tuple1, const void *tuple2)
{
    /* matching strategy:
     *  1) If anon_id is set, return true directly
     *  2) If prefixed with : or !, do a special matching
     *  3) Otherwise, return result of strcmp() call
     */

    sng_tuple_t *tpl1 = (sng_tuple_t *) tuple1;
    sng_tuple_t *tpl2 = (sng_tuple_t *) tuple2;
    /* currently, just test equality */
    //printf("tplcmp: comparing %s with %s\n", tpl1->name, tpl2->name);
    if ((tpl1->name == NULL && tpl1->anon_id != 0) || (tpl2->name == NULL && tpl2->anon_id != 0))
        return tplicmp(tuple1, tuple2);
    if (streq(tpl1->name, "*") || streq(tpl2->name, "*"))
        return 0;
    //puts(" - text-based method'ing");
    /* ':' indicates regex'ing */
    if (tpl1->name[0] == ':' && tpl2->name[0] != ':')
        return tplregex(tuple1, tuple2);
    else if (tpl1->name[0] != ':' && tpl2->name[0] == ':')
        return tplregex(tuple2, tuple1);
    //puts(" - raw match");
    /* '!' indicates matching */
    if (tpl2->name[0] == '!' && tpl1->name[0] != '!')
        return _match(1 + tpl2->name, tpl1->name);
    else if (tpl1->name[0] == '!' && tpl2->name[0] != '!')
        return _match(1 + tpl1->name, tpl2->name);
    //printf(" - string match (result = %d)", strcmp(tpl1->name, tpl2->name));
    return strcmp(((sng_tuple_t *) tuple1)->name, ((sng_tuple_t *) tuple2)->name);
}

 int kcmp(const char *c1, const char *c2)
{
    sng_tuple_t tpl1 = {.name = (char *) c1, .anon_id = 0};
    sng_tuple_t tpl2 = {.name = (char *) c2, .anon_id = 0};
    return tplcmp(&tpl1, &tpl2);
}


/* a deep copy function */
 void *tpldup(const void *tuple1)
{
    sng_tuple_t *tuple2 = malloc(sizeof(sng_tuple_t)), *tpl = (sng_tuple_t *) tuple1;
    if (tuple2 == NULL)
        return NULL;
    size_t nlen;
    tuple2->name = malloc((nlen = strlen(tpl->name) + 1));
    if (tuple2->name == NULL) {
        free(tuple2);
        return NULL;
    }
    memcpy(tuple2->name, tpl->name, nlen);
    tuple2->data = malloc(tpl->len);
    if (tuple2->data == NULL) {
        free(tuple2->name);
        free(tuple2);
        return NULL;
    }
    memcpy(tuple2->data, tpl->data, tpl->len);
    tuple2->len = tpl->len;
    tuple2->anon_id = tpl->anon_id;
    return tuple2;
}

/* a shallow copy function */
 void *tplcpy(const void *tuple1)
{
    sng_tuple_t *tuple2 = malloc(sizeof(sng_tuple_t)), *tpl = (sng_tuple_t *) tuple1;
    if (tuple2 == NULL)
        return NULL;
    memcpy(tuple2, tpl, sizeof(sng_tuple_t));
    return tuple2;
}

 void tplfree(void **tuple1)
{
    if (tuple1 == NULL || *tuple1 == NULL) return;
    free(((sng_tuple_t *) (*tuple1))->name);
    free(((sng_tuple_t *) (*tuple1))->data);
    free(tuple1);
}

   void sfree_l(void **s)
{
    free((*s));
}

   void tplfree_g(sng_tuple_t *tuple1)
{
    if (tuple1 == NULL) return;
    free(((sng_tuple_t *) (tuple1))->name);
    free(((sng_tuple_t *) (tuple1))->data);
    free(tuple1);
}

 sng_tuple_t *tplnew()
{
    return malloc(sizeof(sng_tuple_t));
}


 void sfree(void **str)
{
    if (str == NULL)
        return;
    if (*str == NULL)
        return;
    free(*str);
}

 int icmp(void *itm1, void *itm2)
{
    int *i1 = (int *) itm1, *i2 = (int *) itm2;
    if (*i1 == *i2)
        return 0;
    else if (*i1 < *i2)
        return -1;
    else
        return 1;
}

 int blckcmp_str(void *itm1, void *itm2)
{                           /* ^--list element  ^--search element */

    blocking_request_t *blck1 = itm1, *blck2 = itm2;    /* czmq always uses 1st element as list element */
    sng_tuple_t t1 = {.name = blck1->name, .anon_id = (unsigned int) blck1->extra},
            t2 = {.name = blck2->name, .anon_id = (unsigned) blck1->extra};
    /* Is the list element a match-all (i.e. pop) */
    if (blck1->command == TsPop_Blocking || blck1->command == TsPeek_Blocking)
        return 0; /* always return equals in this case, prioritize pop/peek commands */
    else {
        return tplcmp(&t1, &t2);
    }
}

 int blckcmp_ext(void *itm1, void *itm2)
{
    blocking_request_t *blck1 = itm1, *blck2 = itm2;
    return blck1->extra != blck2->extra;
}

 void *blckdup(void *itm1)
{
    blocking_request_t *b = itm1, *c = malloc(sizeof(blocking_request_t));
    if (b == NULL || c == NULL) {
        free(c);
        return NULL;
    }
    c->extra = b->extra;
    c->command = b->command;
    c->name = malloc(strlen(b->name) + 1);
    memcpy(c->name, b->name, strlen(b->name) + 1);
    return c;
}

 void *blckcpy(void *itm1)
{
    blocking_request_t *b = itm1,
            *c = malloc(sizeof(blocking_request_t));
    if (b == NULL || c == NULL) {
        free(c);
        return NULL;
    }
    memcpy(c, b, sizeof(blocking_request_t));
    return c;
}

 void blckfree(void **itm1)
{
    if (itm1 == NULL) return;
    if (*itm1 == NULL) return;
    blocking_request_t *t = *itm1;
    free(t->name);
    free(t);
}

 void blckfree_g(blocking_request_t *r)
{
    if (r == NULL)
        return;
    free(r->name);
    free(r->clientid); /* needs NULL, C99+ guarantees that if you're copying from stack */
    free(r);
}


#endif //P2PMD_TUPLE_UTILS_H
