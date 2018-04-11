#ifndef _AMCAST_H_
#define _AMCAST_H_

#include "types.h"
#include "node.h"
#include "amcast_types.h"


struct amcast_msg_proposal {
    uid_t ballot;
    enum { UNDEF, RECEIVED, CONFIRMED } status;
    uid_t lts;
};

struct amcast_msg {
    enum { START, PROPOSED, ACCEPTED, COMMITED } phase;
    uid_t lts;
    uid_t gts;
    enum { TRUE, FALSE } delivered;
    uint32_t proposals_count;
    struct amcast_msg_proposal **proposals;
};

struct amcast {
    enum { INIT, LEADER, FOLLOWER, LEADER_INIT, FOLLOWER_PREPARE, LEADER_SYNC } status;
    uid_t ballot;
    uid_t aballot;
    int clock;
    uint32_t msgs_count;
    struct amcast_msg **msgs;
};

void dispatch_amcast_command(struct node *node, struct enveloppe *env);

#endif
