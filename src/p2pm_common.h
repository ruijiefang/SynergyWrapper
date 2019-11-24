
#ifndef RING_MECHANISM_P2PM_COMMON_H
#define RING_MECHANISM_P2PM_COMMON_H

#include <zmq.h>
#include "zhelpers.h"
#include "../contrib/libchash/libchash.h"
#include <memory.h>
#include <pthread.h>
#include <arpa/inet.h> /* for htonl(),htons(),ntohl() family */
#include <czmq.h>
#define TSD_P2PMD_SEND_TOKEN 0x88
#define TSD_P2PMD_GET_SUCCESSOR_ENDPT 0x89
#define TSD_P2PMD_GET_PREDECESSOR_ENDPT 0x90
#define TSD_P2PMD_GET_TOKEN_SIGNAL 0x91
#define PSTRLEN(x) (strlen(x)+1)
#define P2PM_MAX_ID_LEN 256
#define P2PM_MAX_ID_LEN_SHFT 8 /* for left shifting */
#endif //RING_MECHANISM_P2PM_COMMON_H
