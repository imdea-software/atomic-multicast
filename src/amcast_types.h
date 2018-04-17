#ifndef _AMCAST_TYPES_H_
#define _AMCAST_TYPES_H_

#include <stdlib.h>

#include "types.h"


typedef uint32_t uid_t;
typedef uint32_t phase_t;

//CLIENT TYPES
typedef struct payload {
    unsigned int	len;
    char		*val;
} payload_t;

typedef struct message {
    m_uid_t 		mid;
    unsigned int 	destgrps_count;
    xid_t 		destgrps[10];
    struct payload	value;
} message_t;

//NODE_TYPES
typedef struct accept {
    m_uid_t 		mid;
    uid_t		ballot;
    uid_t		lts;
    xid_t		grp;
    message_t           msg;
} accept_t;

typedef struct accept_ack {
    m_uid_t 		mid;
    uid_t		ballot;
    uid_t		gts;
    xid_t		grp;
} accept_ack_t;

typedef struct deliver {
    m_uid_t 		mid;
    uid_t		ballot;
    uid_t		lts;
    uid_t		gts;
} deliver_t;

typedef struct newleader {
    uid_t		ballot;
} newleader_t;

typedef struct newleader_ack {
    uid_t		ballot;
    uid_t		aballot;
    int 		clock;
    int 		msg_count;
    struct {
        m_uid_t 	mid;
        phase_t 	phase;
        uid_t 		lts;
        uid_t 		gts;
    } *messages;
} newleader_ack_t;

typedef struct newleader_sync {
    uid_t 		ballot;
    int 		clock;
    int 		msg_count;
    struct {
        m_uid_t 	mid;
        phase_t 	phase;
        uid_t 		lts;
        uid_t 		gts;
    } *messages;
} newleader_sync_t;

typedef struct newleader_sync_ack {
    uid_t 		ballot;
} newleader_sync_ack_t;

#endif
