

#ifndef RING_MECHANISM_P2PM_UTILITIES_H
#define RING_MECHANISM_P2PM_UTILITIES_H

#include "p2pm_types.h"
#include "p2pm_common.h"

typedef struct _p2pm_config_s p2pm_config_t;
typedef enum _p2pm_opcodes_e p2pm_opcodes_t;
void p2pm_config_destroy(p2pm_config_t *config);

void p2pm_get_config_str(p2pm_config_t *config, char **str);

unsigned p2pm_get_config_ctx_thread(p2pm_config_t *config);

unsigned p2pm_get_config_recv_wait(p2pm_config_t *config);

p2pm_opcodes_t p2pm_config_create(p2pm_config_t *config, const char *joinaddr, unsigned ctx_thread_count,
                                  int64_t stabilize_interval_ms, unsigned recv_wait);

int
p2pm_send_uint16(void *zmq_socket, uint16_t s, int flags);

int
p2pm_send_uint32(void *zmq_socket, uint32_t s, int flags);

int
p2pm_send_uint64(void *zmq_socket, uint64_t s, int flags);

p2pm_opcodes_t
p2pm_recv_uint16(void *zmq_socket, uint16_t *recv, int flags);

p2pm_opcodes_t
p2pm_recv_uint32(void *zmq_socket, uint32_t *recv, int flags);

p2pm_opcodes_t
p2pm_recv_uint64(void *zmq_socket, uint64_t *recv, int flags);

int
p2pm_send_str(void *zmq_socket, char *str, int flags);

p2pm_opcodes_t
p2pm_recv_str(void *zmq_socket, char **str, int flags);

int p2pm_set_wait(void *zmq_socket, unsigned wait);

int p2pm_set_custom_identity(void *zmq_socket, char *identity);

_Bool p2pm_is_valid_request(unsigned request_id);
#endif //RING_MECHANISM_P2PM_UTILITIES_H
