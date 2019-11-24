
#include "p2pm_utilities.h"


#ifdef HTONLL
#undef HTONLL
#endif
#ifdef NTOHLL
#undef NTOHLL
#endif
#ifndef __MACH__
#define HTONLL(x) ((1==htonl(1)) ? (x) : (((uint64_t)htonl((x) & 0xFFFFFFFFUL)) << 32) | htonl((uint32_t)((x) >> 32)))
#define NTOHLL(x) ((1==ntohl(1)) ? (x) : (((uint64_t)ntohl((x) & 0xFFFFFFFFUL)) << 32) | ntohl((uint32_t)((x) >> 32)))
#else
#define HTONLL(x) (htonll(x))
#define NTOHLL(x) (ntohll(x))
#endif


p2pm_opcodes_t p2pm_config_create(p2pm_config_t *config, const char *joinaddr, unsigned ctx_thread_count,
                                  int64_t stabilize_time_ms,
                                  unsigned recv_wait)
{
    assert(config);
    size_t joinaddr_len = PSTRLEN(joinaddr);
    if (joinaddr_len > P2PM_MAX_ID_LEN)
        return P2PM_OP_FAILURE;
    bzero(config, sizeof(p2pm_config_t));
    config->join_address = malloc(sizeof(char) * joinaddr_len);
    config->stabilize_interval = stabilize_time_ms;
    memcpy(config->join_address, joinaddr, PSTRLEN(joinaddr));
    config->context_thread_count = ctx_thread_count;
    config->wait = recv_wait;
    return P2PM_OP_SUCCESS;
}

void p2pm_get_config_str(p2pm_config_t *config, char **str)
{
    assert(config);
    *str = malloc(sizeof(char) * PSTRLEN(config->join_address));
    assert(*str);
    memcpy(*str, config->join_address, PSTRLEN(config->join_address));
}

unsigned p2pm_get_config_ctx_thread(p2pm_config_t *config)
{
    assert(config);
    return config->context_thread_count;
}

void p2pm_config_destroy(p2pm_config_t *config)
{
    assert(config);
    free(config->join_address);
    config->context_thread_count = 0;
    config->wait = 0;
}


unsigned p2pm_get_config_recv_wait(p2pm_config_t *config)
{
    assert(config);
    return config->wait;
}

int p2pm_send_uint16(void *zmq_socket, uint16_t s, int flags)
{
    assert(zmq_socket);
    s = htons(s);
    int rt = zmq_send(zmq_socket, (void *) &s, sizeof(uint16_t), flags);
    return rt;
}

int p2pm_send_uint32(void *zmq_socket, uint32_t s, int flags)
{
    assert(zmq_socket);
    s = htonl(s);
    int rt = zmq_send(zmq_socket, (void *) &s, sizeof(uint32_t), flags);
    return rt;
}

int p2pm_send_uint64(void *zmq_socket, uint64_t s, int flags)
{
    assert(zmq_socket);
    s = (HTONLL(s));
    return zmq_send(zmq_socket, (void *) &s, sizeof(uint64_t), flags);
}

p2pm_opcodes_t p2pm_recv_uint16(void *zmq_socket, uint16_t *recv, int flags)
{
    assert(zmq_socket);
    assert(recv);
    int ret;
    if (zmq_recv(zmq_socket, recv, sizeof(uint16_t), flags) < 0)
        return P2PM_OP_FAILURE;
    *recv = ntohs(*recv);
    return P2PM_OP_SUCCESS;
}

p2pm_opcodes_t p2pm_recv_uint32(void *zmq_socket, uint32_t *recv, int flags)
{
    assert(zmq_socket);
    assert(recv);
    int ret;
    if (zmq_recv(zmq_socket, recv, sizeof(uint32_t), flags) < 0)
        return P2PM_OP_FAILURE;
    *recv = ntohl(*recv);
    return P2PM_OP_SUCCESS;
}

p2pm_opcodes_t p2pm_recv_uint64(void *zmq_socket, uint64_t *recv, int flags)
{
    assert(zmq_socket);
    assert(recv);
    int ret;
    if (zmq_recv(zmq_socket, recv, sizeof(uint64_t), flags) < 0)
        return P2PM_OP_FAILURE;
    *recv = (NTOHLL((*recv)));
    return P2PM_OP_SUCCESS;
}

int p2pm_send_str(void *zmq_socket, char *str, int flags)
{
    assert(zmq_socket);
    p2pm_send_uint32(zmq_socket, (uint32_t) PSTRLEN(str), flags | ZMQ_SNDMORE);
    return zmq_send(zmq_socket, str, PSTRLEN(str), flags);
}

p2pm_opcodes_t p2pm_recv_str(void *zmq_socket, char **str, int flags)
{
    assert(zmq_socket);
    int stralen;
    int ret;
    if ((ret = p2pm_recv_uint32(zmq_socket, (uint32_t *) &stralen, 0)) != P2PM_OP_SUCCESS)
        return P2PM_OP_FAILURE;
    *str = malloc(sizeof(char) * stralen);
    bzero(*str, sizeof(char) * stralen);
    if ((stralen = zmq_recv(zmq_socket, *str, sizeof(char) * stralen, flags)) < 0) {
        free(*str);
        return P2PM_OP_FAILURE;
    }
    void *ptr = realloc(*str, (size_t) stralen);
    if (ptr == NULL) {
        free(*str);
        return P2PM_OP_FAILURE;
    } else
        *str = ptr;
    return P2PM_OP_SUCCESS;
}

int p2pm_set_wait(void *zmq_socket, unsigned wait)
{
    return zmq_setsockopt(zmq_socket, ZMQ_RCVTIMEO, &wait, sizeof(unsigned));
}

int p2pm_set_custom_identity(void *zmq_socket, char *identity)
{
    return zmq_setsockopt(zmq_socket, ZMQ_IDENTITY, identity, sizeof(char) * PSTRLEN(identity));
}

_Bool p2pm_is_valid_request(unsigned request_id)
{
    return (request_id < 69 && request_id > 54);    /* tentative, subject to change */
}
