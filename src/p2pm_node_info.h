/* 88888888888888888888888888888888888888888888888888
 Created by Ruijie Fang on 1/13/18.
   88888888888888888888888888888888888888888888888888 */

#ifndef RING_MECHANISM_P2PM_NODE_INFO_H
#define RING_MECHANISM_P2PM_NODE_INFO_H

#include "p2pm_common.h"
#include "p2pm_node_info.h"
#include "p2pm_types.h"
#include "p2pm_utilities.h"

typedef enum _p2pm_reqtypes_e p2pm_reqtypes_t;
typedef enum _p2pm_reptypes_e p2pm_reptypes_t;
typedef enum _p2pm_opcodes_e p2pm_opcodes_t;
typedef enum _p2pm_status_e p2pm_status_t;
typedef struct _p2pm_node_info_s p2pm_node_info_t;
typedef struct _p2pm_s p2pm_t;

/* creates a list */
void p2pm_ni_create(p2pm_node_info_t *p2pm_node_info, unsigned r_max, const char *join_addr);

/* adds a successor */
p2pm_opcodes_t p2pm_ni_add_successor(p2pm_node_info_t *self, const char *successor);

/* resets r_max */
p2pm_opcodes_t p2pm_ni_reset_r_max(p2pm_node_info_t *self, unsigned r_max);

/* removes a successor indexed by certain addr */
p2pm_opcodes_t p2pm_ni_remove_successor(p2pm_node_info_t *self, unsigned i);

/* sets closest successor node */
p2pm_opcodes_t p2pm_ni_set_successor(p2pm_node_info_t *self, const char *successor);

/* sets a predecessor */
p2pm_opcodes_t p2pm_ni_set_predecessor(p2pm_node_info_t *self, const char *predecessor);

/* copies over a successor list on join
 * assumption:
 *  1) self->successor is already there,
 *   we don't touch it and (also adjust it into place)
 *  2) respects r_max, kicks out any member that doesn't
 *  3) the new list is non-empty
 *  */
p2pm_opcodes_t p2pm_ni_copy_successor_list(p2pm_node_info_t *self, const char *successor_list, unsigned n_r);

/* only copies a successor list and overwrites successor
 * assumption:
 *  1) completely overwrites successor using successor_list[0]
 *  2) the new list is non-empty
 * */
p2pm_opcodes_t p2pm_ni_move_successor_list(p2pm_node_info_t *self, const char *successor_list, unsigned n_r);


void p2pm_ni_print_successor_list(p2pm_node_info_t *self);

/* a helpful utility to loop over successor lists
 * @return unsigned index to the next element in the list
 *      resets to 0 if out of bound
 * assumption: we have a valid successor_list
 *  and (self) is initialized correctly
 */
unsigned p2pm_ni_successor_list_next(p2pm_node_info_t *self, unsigned previous);


/* expose this to public */
void p2pm_ni_fix_successor_list(p2pm_node_info_t *self);
/* destroys a list */
void p2pm_node_info_destroy(p2pm_node_info_t *p2pm_node_info);

#endif //RING_MECHANISM_P2PM_NODE_INFO_H
