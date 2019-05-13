#ifndef _AMCAST_TYPES_H_
#define _AMCAST_TYPES_H_

#include <stdlib.h>

#include "types.h"

#define MAX_NUMBER_OF_GROUPS 10
//#define MAX_PAYLOAD_LEN 156
#define MAX_PAYLOAD_LEN 20

//DATATYPES
typedef enum { START, PROPOSED, ACCEPTED, COMMITTED } phase_t;
typedef unsigned int clk_t;
struct pair {
    clk_t time;
    xid_t id;
};
typedef struct pair p_uid_t;
typedef struct pair g_uid_t;
typedef struct pair m_uid_t;

struct amcast;
struct amcast_msg;

//OPERATIONS
int paircmp(struct pair *p1, struct pair *p2);
int pairequ(struct pair *p1, struct pair *p2);
unsigned int pairhash(struct pair *p);

//CLIENT_CMD_TYPES
typedef struct payload {
    unsigned int	len;
    char		val[MAX_PAYLOAD_LEN];
} payload_t;

typedef struct message {
    m_uid_t 		mid;
    unsigned int 	destgrps_count;
    //TODO Change that fixed size array
    xid_t		proxy;
    xid_t 		destgrps[MAX_NUMBER_OF_GROUPS];
    struct payload	value;
} message_t;

typedef struct msgstate {
    phase_t    phase;
    p_uid_t    lballot[MAX_NUMBER_OF_GROUPS];
    g_uid_t    lts[MAX_NUMBER_OF_GROUPS];
    g_uid_t    gts;
    message_t  msg;
} msgstate_t;

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
    p_uid_t		ballot[MAX_NUMBER_OF_GROUPS];
    g_uid_t		gts;
    g_uid_t		gts_last_delivered;
} accept_ack_t;

typedef struct deliver {
    m_uid_t 		mid;
    p_uid_t		ballot;
    g_uid_t		lts;
    g_uid_t		gts;
    g_uid_t		gts_linf_delivered;
    g_uid_t		gts_ginf_delivered;
} deliver_t;

typedef struct reaccept {
    m_uid_t     mid;
    xid_t       grp;
    p_uid_t     ballot[MAX_NUMBER_OF_GROUPS];
    g_uid_t     gts;
    message_t   msg;
} reaccept_t;

typedef struct newleader {
    p_uid_t		ballot;
} newleader_t;

typedef struct newleader_ack {
    p_uid_t		ballot;
    p_uid_t		aballot;
    clk_t 		clock;
    int 		msg_count;
    msgstate_t		*messages;
} newleader_ack_t;

typedef struct newleader_sync {
    p_uid_t 		ballot;
    clk_t 		clock;
    int 		msg_count;
    msgstate_t		*messages;
} newleader_sync_t;

typedef struct newleader_sync_ack {
    p_uid_t 		ballot;
} newleader_sync_ack_t;

#endif
