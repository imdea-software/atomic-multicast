#ifndef _AMCAST_TYPES_H_
#define _AMCAST_TYPES_H_

#include <stdlib.h>

#include "types.h"

//DATATYPES
typedef enum { START, PROPOSED, ACCEPTED, COMMITTED } phase_t;
typedef unsigned int clk_t;
struct pair {
    clk_t time;
    xid_t id;
};
typedef struct pair p_uid_t;
typedef struct pair g_uid_t;

//CLIENT_CMD_TYPES
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

//NODE_CMD_TYPES
typedef struct accept {
    m_uid_t 		mid;
    xid_t		grp;
    p_uid_t		ballot;
    g_uid_t		lts;
    message_t           msg;
} accept_t;

typedef struct accept_ack {
    m_uid_t 		mid;
    xid_t		grp;
    p_uid_t		ballot;
    g_uid_t		gts;
} accept_ack_t;

typedef struct deliver {
    m_uid_t 		mid;
    p_uid_t		ballot;
    g_uid_t		lts;
    g_uid_t		gts;
} deliver_t;

typedef struct newleader {
    p_uid_t		ballot;
} newleader_t;

typedef struct newleader_ack {
    p_uid_t		ballot;
    p_uid_t		aballot;
    clk_t 		clock;
    int 		msg_count;
    struct {
        m_uid_t 	mid;
        phase_t 	phase;
        g_uid_t 	lts;
        g_uid_t 	gts;
    } *messages;
} newleader_ack_t;

typedef struct newleader_sync {
    p_uid_t 		ballot;
    clk_t 		clock;
    int 		msg_count;
    struct {
        m_uid_t 	mid;
        phase_t 	phase;
        g_uid_t 	lts;
        g_uid_t 	gts;
    } *messages;
} newleader_sync_t;

typedef struct newleader_sync_ack {
    p_uid_t 		ballot;
} newleader_sync_ack_t;

#endif
