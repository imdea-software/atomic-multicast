#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <event2/bufferevent.h>

#include "types.h"
#include "node.h"
#include "amcast_types.h"


typedef enum cmd_type {
    //AMCAST COMMANDS
    MULTICAST,
    ACCEPT,
    ACCEPT_ACK,
    COMMIT,
    DELIVER,
    NEWLEADER,
    NEWLEADER_ACK,
    NEWLEADER_SYNC,
    NEWLEADER_SYNC_ACK
} cmd_type;

struct enveloppe {
    id_t		sid;
    cmd_type		cmd_type;
    union {
        message_t		multicast;
        accept_t		accept;
        accept_ack_t		accept_ack;
        deliver_t		deliver;
        newleader_t		newleader;
        newleader_ack_t 	newleader_ack;
        newleader_sync_t	newleader_sync;
        newleader_sync_ack_t	newleader_sync_ack;
    } cmd;
};

void dispatch_message(struct node *node, struct enveloppe *env);
void read_enveloppe(struct bufferevent *bev, struct enveloppe *env);
void write_enveloppe(struct bufferevent *bev, struct enveloppe *env);
void send_to_destgrps(struct node *node, struct enveloppe *env, id_t *destgrps, unsigned int count);
void send_to_group(struct node *node, struct enveloppe *env, id_t group_id);
void send_to_peer(struct node *node, struct enveloppe *env, id_t peer_id);

#endif
