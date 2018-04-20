#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <event2/bufferevent.h>

#include "types.h"
#include "node.h"
#include "amcast_types.h"


typedef enum cmd_type {
    MULTICAST,
} cmd_type;

struct enveloppe {
    id_t		sid;
    cmd_type		cmd_type;
    union {
        message_t 		multicast;
    } cmd;
};

void dispatch_message(struct node *node, struct enveloppe *env);
void read_enveloppe(struct bufferevent *bev, struct enveloppe *env);
void write_enveloppe(struct bufferevent *bev, struct enveloppe *env);

#endif
