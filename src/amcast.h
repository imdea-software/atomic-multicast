#ifndef _AMCAST_H_
#define _AMCAST_H_

#include "types.h"
#include "node.h"
#include "amcast_types.h"
#include "message.h"


struct amcast_msg_proposal {
    p_uid_t ballot;
    enum { UNDEF, RECEIVED, CONFIRMED } status;
    g_uid_t lts;
    //EXTRA FIELDS (NOT IN SPEC)
    unsigned int accept_ack_groupcount;
    unsigned int accept_ack_counts_size;
    unsigned int *accept_ack_counts;
};

struct amcast_msg {
    phase_t phase;
    g_uid_t lts;
    g_uid_t gts;
    enum { TRUE, FALSE } delivered;
    uint32_t proposals_count;
    message_t msg;
    struct amcast_msg_proposal **proposals;
    //EXTRA FIELDS (NOT IN SPEC)
    unsigned int accept_totalcount;
    unsigned int accept_ack_totalcount;
    g_uid_t accept_max_lts;
};

struct amcast {
    enum { INIT, LEADER, FOLLOWER, LEADER_INIT, FOLLOWER_PREPARE, LEADER_SYNC } status;
    p_uid_t ballot;
    p_uid_t aballot;
    clk_t clock;
    uint32_t msgs_count;
    struct amcast_msg **msgs;
};

struct amcast *amcast_init();
int amcast_free(struct amcast *amcast);
void dispatch_amcast_command(struct node *node, struct enveloppe *env);

#endif
