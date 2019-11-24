/* 88888888888888888888888888888888888888888888888888
 Created by Ruijie Fang on 1/13/18.
   88888888888888888888888888888888888888888888888888 */

#include "p2pm_node_info.h"

void p2pm_ni_create(p2pm_node_info_t *p2pm_node_info, unsigned r_max, const char *join_addr)
{
    assert(p2pm_node_info);
    size_t sizeof_join_addr = PSTRLEN(join_addr);
    unsigned i;
    p2pm_node_info->successor = malloc(P2PM_MAX_ID_LEN);
    p2pm_node_info->predecessor = malloc(P2PM_MAX_ID_LEN);
    p2pm_node_info->temp_predecessor = malloc(P2PM_MAX_ID_LEN);
    p2pm_node_info->temp_successor = malloc(P2PM_MAX_ID_LEN);
    p2pm_node_info->successor_list = malloc(sizeof(char) * r_max << P2PM_MAX_ID_LEN_SHFT);
    memset(p2pm_node_info->successor_list, '\0', sizeof(char) * r_max << P2PM_MAX_ID_LEN_SHFT);
    memcpy(p2pm_node_info->predecessor, join_addr, sizeof_join_addr * sizeof(char));
    p2pm_node_info->r = 0;
    p2pm_node_info->r_max = r_max;
}

static void
_p2pm_ni_condense_successor_list(p2pm_node_info_t *self)
{
    assert(self);
    assert(self->successor_list);
    if (self->r >= self->r_max)
        return;

    unsigned ptr, eptr, ok = 0;
    for (ptr = 0, eptr = 0; ptr < self->r_max << P2PM_MAX_ID_LEN_SHFT; ptr += P2PM_MAX_ID_LEN) {
        if (ptr == '\0') {
            eptr = ptr;
            ok = 1;
        } else if (ok) {
            memcpy(self->successor_list + ptr, self->successor_list + eptr, P2PM_MAX_ID_LEN);
            memset(self->successor_list + ptr, '\0', P2PM_MAX_ID_LEN);
            ok = 0;
        }
    }
}

p2pm_opcodes_t p2pm_ni_add_successor(p2pm_node_info_t *self, const char *successor)
{
    assert(self);
    assert(self->successor_list);
    size_t successor_len = PSTRLEN(successor);
    unsigned i;
    if (successor_len > P2PM_MAX_ID_LEN)
        return P2PM_OP_INVALID;
    _p2pm_ni_condense_successor_list(self);
    for (i = 0; i < self->r_max << P2PM_MAX_ID_LEN_SHFT; i += P2PM_MAX_ID_LEN)
        if (self->successor_list[i] == '\0')
            break;
    if (i == self->r_max << P2PM_MAX_ID_LEN_SHFT)
        return P2PM_OP_FAILURE;
    else if (!i)
        memcpy(self->successor, successor, successor_len), memcpy(self->successor_list, successor, successor_len);
    else
        memcpy(self->successor_list + i, successor, successor_len);

    return P2PM_OP_SUCCESS;
}

p2pm_opcodes_t p2pm_ni_reset_r_max(p2pm_node_info_t *self, unsigned r_max)
{
    assert(self);
    assert(self->successor_list);
    if (r_max < self->r_max)
        return P2PM_OP_INVALID;
    void *ptr = realloc(self->successor_list, (sizeof(char) * r_max) << P2PM_MAX_ID_LEN_SHFT);
    if (ptr == NULL)
        return P2PM_OP_FAILURE;
    self->successor_list = ptr;
    self->r_max = r_max;
    return P2PM_OP_SUCCESS;
}

p2pm_opcodes_t p2pm_ni_remove_successor(p2pm_node_info_t *self, unsigned i)
{
    assert(self);
    assert(self->successor_list);
    if (i >= self->r_max)
        return P2PM_OP_INVALID;
    *(self->successor_list + (i << P2PM_MAX_ID_LEN_SHFT)) = '\0';
    _p2pm_ni_condense_successor_list(self);
    return P2PM_OP_SUCCESS;
}

p2pm_opcodes_t p2pm_ni_set_successor(p2pm_node_info_t *self, const char *successor)
{
    assert(self);
    assert(successor);
    size_t successor_len = PSTRLEN(successor);
    if (successor_len > P2PM_MAX_ID_LEN)
        return P2PM_OP_INVALID;
    memcpy(self->successor, successor, successor_len);
    memcpy(self->successor_list, successor, successor_len);
    return P2PM_OP_SUCCESS;
}

p2pm_opcodes_t p2pm_ni_set_predecessor(p2pm_node_info_t *self, const char *predecessor)
{
    assert(self);
    assert(predecessor);
    if (PSTRLEN(predecessor) > P2PM_MAX_ID_LEN)
        return P2PM_OP_FAILURE;
    memcpy(self->predecessor, predecessor, sizeof(char) * PSTRLEN(predecessor));
    return P2PM_OP_SUCCESS;
}

void p2pm_node_info_destroy(p2pm_node_info_t *p2pm_node_info)
{
    assert(p2pm_node_info);
    free(p2pm_node_info->successor);
    free(p2pm_node_info->predecessor);
    free(p2pm_node_info->successor_list);
    free(p2pm_node_info->temp_predecessor);
    free(p2pm_node_info->temp_successor);
    p2pm_node_info->r_max = 0;
    p2pm_node_info->r = 0;
}

p2pm_opcodes_t p2pm_ni_copy_successor_list(p2pm_node_info_t *self, const char *successor_list, unsigned n_r)
{
    assert(self);
    assert(successor_list);
    unsigned i;
    dzlog_debug(" >> Copying successor_list...");
    if (PSTRLEN(successor_list) > P2PM_MAX_ID_LEN)
        return P2PM_OP_INVALID;
    if (n_r + 1 > self->r_max) {
        self->r = self->r_max;
        n_r = self->r_max - 1;
    } else {
        self->r = n_r + 1;
    }
    for (i = P2PM_MAX_ID_LEN; i < (n_r + 1) << P2PM_MAX_ID_LEN_SHFT; i += P2PM_MAX_ID_LEN)
        memcpy(self->successor_list + i, successor_list + i - P2PM_MAX_ID_LEN, P2PM_MAX_ID_LEN);
    dzlog_debug(" >> Done.");
    return P2PM_OP_SUCCESS;

}

p2pm_opcodes_t p2pm_ni_move_successor_list(p2pm_node_info_t *self, const char *successor_list, unsigned n_r)
{
    assert(self);
    assert(successor_list);
    memset(self->successor_list, '\0', self->r_max << P2PM_MAX_ID_LEN_SHFT);
    unsigned i;
    if (PSTRLEN(successor_list) > P2PM_MAX_ID_LEN)
        return P2PM_OP_INVALID;
    if (n_r + 1 > self->r_max) {
        self->r = self->r_max;
        n_r = self->r_max - 1;
    } else
        self->r = n_r;
    for (i = 0; i < n_r << P2PM_MAX_ID_LEN_SHFT; i += P2PM_MAX_ID_LEN)
        memcpy(self->successor_list + i, successor_list + i, P2PM_MAX_ID_LEN);
    _p2pm_ni_condense_successor_list(self);
    return P2PM_OP_SUCCESS;
}


/* TODO: Test this */
unsigned p2pm_ni_successor_list_next(p2pm_node_info_t *self, unsigned previous)
{
    /* some assertions */
    assert(self);
    assert(self->successor_list);

    /* if our next index is out of range, just return 0 */
    if (previous + P2PM_MAX_ID_LEN >= (self->r << P2PM_MAX_ID_LEN_SHFT))
        return 0;

    /* otherwise, return next index (calculated by addiont MAX_ID_LEN */
    return previous + P2PM_MAX_ID_LEN;
}
/* TODO: Test this */
/* this is basically assertions + condense list
 * made for stabilize() call
 */
void p2pm_ni_fix_successor_list(p2pm_node_info_t *self)
{
    /* assertions */
    assert(self);
    assert(self->successor_list);

    /* call */
    _p2pm_ni_condense_successor_list(self);
}

void p2pm_ni_print_successor_list(p2pm_node_info_t *self)
{
    assert(self);
    unsigned i;
    for (i = 0; i < self->r << P2PM_MAX_ID_LEN_SHFT; i += P2PM_MAX_ID_LEN) {
        dzlog_debug("> Successor: %s\n", self->successor_list + i);
    }

}
