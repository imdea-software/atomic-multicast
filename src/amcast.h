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
    //EXTRA FIELDS - DELETION & RECOVERY
    int retry_on_accept;
    int collection;
    //EXTRA FIELDS - APP STATE
    void *shared_cb_arg;
};

#include "htable.h"

struct amcast {
    enum { INIT, LEADER, FOLLOWER, PREPARE } status;
    p_uid_t ballot;
    p_uid_t aballot;
    clk_t clock;
    htable_t *h_msgs;
    //EXTRA FIELDS - NEWLEADER_ACK COUNTERS
    unsigned int newleader_ack_groupcount;
    unsigned int *newleader_ack_count;
    //EXTRA FIELDS - NEWLEADER_SYNC_ACK COUNTERS
    unsigned int newleader_sync_ack_groupcount;
    unsigned int *newleader_sync_ack_count;
    //EXTRA FIELDS (NOT IN SPEC)
    pqueue_t *delivered_gts;
    pqueue_t *committed_gts;
    pqueue_t *pending_lts;
    g_uid_t *gts_last_delivered;
    g_uid_t gts_ginf_delivered;
    g_uid_t gts_linf_delivered;
    msginit_cb_fun msginit_cb;
    commit_cb_fun commit_cb;
    delivery_cb_fun delivery_cb;
    leader_failure_cb_fun leader_failure_cb;
    recovery_cb_fun recovery_cb;
    void *dev_cb_arg;
    void *ini_cb_arg;
    void *commit_cb_arg;
    void *leader_failure_cb_arg;
    void *recovery_cb_arg;
};

struct amcast *amcast_init(msginit_cb_fun msginit_cb, void *ini_cb_arg,
		           commit_cb_fun commit_cb, void *commit_cb_arg,
		           leader_failure_cb_fun leader_failure_cb, void *leader_failure_cb_arg,
		           recovery_cb_fun recovery_cb, void *recovery_cb_arg,
			   delivery_cb_fun delivery_cb, void *dev_cb_arg);
int amcast_free(struct amcast *amcast);
void dispatch_amcast_command(struct node *node, struct enveloppe *env);
void amcast_recover(struct node *node, xid_t peer_id);

#endif
