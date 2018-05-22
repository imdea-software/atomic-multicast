#ifndef _AMCAST_H_
#define _AMCAST_H_

#include "types.h"
#include "node.h"
#include "amcast_types.h"
#include "message.h"
#include "pqueue.h"


struct amcast_msg {
    phase_t phase;
    p_uid_t *lballot;
    g_uid_t *lts;
    g_uid_t gts;
    enum { TRUE, FALSE } delivered;
    message_t msg;
    //EXTRA FIELDS - ACCEPT COUNTERS
    unsigned int accept_totalcount;
    unsigned int *accept_groupcount;
    g_uid_t accept_max_lts;
    //EXTRA FIELDS - ACCEPT_ACK COUNTERS
    unsigned int groups_count;
    unsigned int accept_ack_totalcount;
    unsigned int *accept_ack_groupready;
    unsigned int *accept_ack_groupcount;
    unsigned int *accept_ack_counts;
};

#include "htable.h"

struct amcast {
    enum { INIT, LEADER, FOLLOWER, LEADER_INIT, FOLLOWER_PREPARE, LEADER_SYNC } status;
    p_uid_t ballot;
    p_uid_t aballot;
    clk_t clock;
    htable_t *h_msgs;
    //EXTRA FIELDS (NOT IN SPEC)
    pqueue_t *committed_gts;
    pqueue_t *pending_lts;
    delivery_cb_fun delivery_cb;
    void *cb_arg;
};

struct amcast *amcast_init(delivery_cb_fun delivery_cb, void *cb_arg);
int amcast_free(struct amcast *amcast);
void dispatch_amcast_command(struct node *node, struct enveloppe *env);

#endif
