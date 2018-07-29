#include <string.h>

#include "types.h"
#include "node.h"
#include "message.h"
#include "amcast_types.h"
#include "amcast.h"
#include "pqueue.h"
#include "htable.h"


int paircmp(struct pair *p1, struct pair *p2) {
    if(p1 == NULL || p2 == NULL) {
        puts("ERROR: paircmp called with null pointer");
        exit(EXIT_FAILURE);
    }
    if(p1->time < p2->time)
        return -1;
    if(p1->time > p2->time)
        return 1;
    if(p1->time == p2->time && p1->id < p2->id)
        return -1;
    if(p1->time == p2->time && p1->id > p2->id)
        return 1;
    if(p1->time == p2->time && p1->id == p2->id)
        return 0;
    else {
	printf("Bad pair comparison\n");
        exit(EXIT_FAILURE);
    }
}
struct pair default_pair = { .time = 0, .id = -1};

int pairequ(struct pair *p1, struct pair *p2) {
    if(p1 == NULL || p2 == NULL) {
        puts("ERROR: pairequ called with null pointer");
        exit(EXIT_FAILURE);
    }
    return p1->time == p2->time && p1->id == p2->id;
}

//Bijective mapping, but values could quickly get higher than max uint32_t value
unsigned int pairhash_cantor(struct pair *p) {
    if(p == NULL) {
        puts("ERROR: pairhash called with null pointer");
        exit(EXIT_FAILURE);
    }
    return ((p->time + p->id + 1)*(p->time + p->id + 2))/2 + p->id;
}

unsigned int pairhash(struct pair *p) {
    if(p == NULL) {
        puts("ERROR: pairhash called with null pointer");
        exit(EXIT_FAILURE);
    }
    unsigned int res = p->time;
    res ^= p->id + 0x9e3779b9 + (res<<6) + (res>>2);
    return res;
}

//TODO Make helper functions to create enveloppes in clean and nice looking way
//TODO Following pointers makes it a lot harder to read the code, try to find some simplification
//         e.g. properly defined macros could help,
//         or sub-functions using only the useful structure fields passed as arguments

static struct amcast_msg *init_amcast_msg(struct groups *groups, unsigned int cluster_size, message_t *cmd);
static void reset_accept_ack_counters(struct amcast_msg *msg, struct groups *groups, unsigned int cluster_size);
static int free_amcast_msg(m_uid_t *mid, struct amcast_msg *msg, void *arg);

static void handle_multicast(struct node *node, xid_t sid, message_t *cmd) {
    //printf("[%u] {%u,%d} We got MULTICAST command from %u!\n", node->id, cmd->mid.time, cmd->mid.id, sid);
    if (node->amcast->status == LEADER) {
        struct amcast_msg *msg = NULL;
        if((msg = htable_lookup(node->amcast->h_msgs, &cmd->mid)) == NULL) {
            msg = init_amcast_msg(node->groups, node->comm->cluster_size, cmd);
            //Run msginit callback
            if(node->amcast->msginit_cb)
                node->amcast->msginit_cb(node, msg, node->amcast->ini_cb_arg);
            htable_insert(node->amcast->h_msgs, &msg->msg.mid, msg);
        }
        if(msg->phase == START) {
            msg->phase = PROPOSED;
            node->amcast->clock++;
            msg->lts[node->comm->groups[node->id]].time = node->amcast->clock;
            msg->lts[node->comm->groups[node->id]].id = node->comm->groups[node->id];
            msg->lballot[node->comm->groups[node->id]] = node->amcast->ballot;
            pqueue_push(node->amcast->pending_lts, msg, &msg->lts[node->comm->groups[node->id]]);
        }
        struct enveloppe rep = {
	    .sid = node->id,
	    .cmd_type = ACCEPT,
	    .cmd.accept = {
	        .mid = cmd->mid,
		.grp = node->comm->groups[node->id],
		.ballot = node->amcast->ballot,
		.lts = msg->lts[node->comm->groups[node->id]],
		.msg = *cmd
	    },
	};
        send_to_destgrps(node, &rep, cmd->destgrps, cmd->destgrps_count);
    }
}

static void handle_accept(struct node *node, xid_t sid, accept_t *cmd) {
    //printf("[%u] {%u,%d} We got ACCEPT command from %u!\n", node->id, cmd->mid.time, cmd->mid.id, sid);
    struct amcast_msg *msg = NULL;
    if((msg = htable_lookup(node->amcast->h_msgs, &cmd->mid)) == NULL) {
        msg = init_amcast_msg(node->groups, node->comm->cluster_size, &cmd->msg);
        //Run msginit callback
        if(node->amcast->msginit_cb)
            node->amcast->msginit_cb(node, msg, node->amcast->ini_cb_arg);
        htable_insert(node->amcast->h_msgs, &msg->msg.mid, msg);
    }
    if ((node->amcast->status == LEADER || node->amcast->status == FOLLOWER)
            && paircmp(&msg->lballot[cmd->grp], &cmd->ballot) <= 0
            && !( cmd->grp == node->comm->groups[node->id]
                  && !(paircmp(&node->amcast->ballot, &cmd->ballot) == 0) )) {
	//TODO Carefully try to see when it's the best time to reset this counter
	//     Probably upon a leader change
	if (msg->accept_groupcount[cmd->grp] == 0) {
            msg->accept_groupcount[cmd->grp] += 1;
            msg->accept_totalcount += 1;
            if(paircmp(&msg->accept_max_lts, &cmd->lts) < 0)
                msg->accept_max_lts = cmd->lts;
	}
    if(node->amcast->status == LEADER
       && paircmp(&msg->lballot[cmd->grp], &default_pair) != 0
       && paircmp(&msg->lballot[cmd->grp], &cmd->ballot) < 0)
        reset_accept_ack_counters(msg, node->groups, node->comm->cluster_size);
    msg->lballot[cmd->grp] = cmd->ballot;
    msg->lts[cmd->grp] = cmd->lts;
    if(msg->accept_totalcount != msg->msg.destgrps_count)
        return;
    msg->phase = ACCEPTED;
    msg->gts = msg->accept_max_lts;
    if(node->amcast->clock < msg->gts.time)
        node->amcast->clock = msg->gts.time;
        struct enveloppe rep = {
	    .sid = node->id,
	    .cmd_type = ACCEPT_ACK,
	    .cmd.accept_ack = {
	        .mid = cmd->mid,
		.grp = node->comm->groups[node->id],
		.gts_last_delivered = node->amcast->gts_last_delivered[node->id],
		.gts = msg->gts
	    },
	};
	memcpy(rep.cmd.accept_ack.ballot, msg->lballot,
			sizeof(p_uid_t) * node->groups->groups_count);
        for(xid_t *grp = msg->msg.destgrps; grp < msg->msg.destgrps + msg->msg.destgrps_count; grp++)
            send_to_peer(node, &rep, msg->lballot[*grp].id);
    }
}

static void handle_reaccept(struct node *node, xid_t sid, reaccept_t *cmd) {
    //printf("[%u] {%u,%d} We got REACCEPT command from %u!\n", node->id, cmd->mid.time, cmd->mid.id, sid);
    //TODO Would be nice to be able to re-use this lookup instead of redoing one in handle_accept()
    struct amcast_msg *msg = NULL;
    if(((msg = htable_lookup(node->amcast->h_msgs, &cmd->mid)) == NULL)
            && (paircmp(&cmd->gts, &node->amcast->gts_last_delivered[node->id]) < 0)) {
        struct enveloppe rep = {
            .sid = node->id,
            .cmd_type = ACCEPT_ACK,
            .cmd.accept_ack = {
                .mid = cmd->mid,
                .grp = node->comm->groups[node->id],
                .gts_last_delivered = node->amcast->gts_last_delivered[node->id],
                .gts = cmd->gts,
            },
        };
        memcpy(rep.cmd.accept_ack.ballot, cmd->ballot, sizeof(p_uid_t) * node->groups->groups_count);
        send_to_peer(node, &rep, sid);
    } else {
        accept_t accept = {
            .mid = cmd->mid,
            .grp = cmd->grp,
            .ballot = cmd->ballot[node->comm->groups[sid]],
            .lts = cmd->gts,
            .msg = msg->msg,
        };
        handle_accept(node, sid, &accept);
    }
}

static void handle_accept_ack(struct node *node, xid_t sid, accept_ack_t *cmd) {
    //printf("[%u] {%u,%d} We got ACCEPT_ACK command from %u!\n", node->id, cmd->mid.time, cmd->mid.id, sid);
    if (node->amcast->status == LEADER) {
        struct amcast_msg *msg = NULL;
        if((msg = htable_lookup(node->amcast->h_msgs, &cmd->mid)) == NULL) {
            //TODO a bit unsafe, must check whether the message is already delivered
            return;
        }
        //Check whether the local ballot and the received one are equal
        //  It seems ACCEPT_ACKS are sometime recevied before gts is initialized
        //  so just update local ballots number if lesser than the received ones
        //  and mark the counters to be reset
        int updated_components = 0;
        for(xid_t i=0; i<node->groups->groups_count; i++)
            switch(paircmp(&msg->lballot[i], &cmd->ballot[i])) {
                case -1:
                    //Reject commands with higher ballot numbers if already locally initialized
                    //  TODO Check if this is important, since it prevents recovery
                    //if(paircmp(&msg->lts[i], &default_pair) != 0)
                    //    return;
                    //Only mark for reset if update from non-null value
                    if(paircmp(&msg->lballot[i], &default_pair) != 0)
                        updated_components++;
                    msg->lballot[i] = cmd->ballot[i];
                    break;
                case 0:
                    break;
                default:
                    return;
            }
        if(updated_components > 0)
            reset_accept_ack_counters(msg, node->groups, node->comm->cluster_size);
        if(msg->accept_ack_counts[sid] < 1) {
            msg->accept_ack_counts[sid] += 1;
            msg->accept_ack_groupcount[cmd->grp] += 1;
        }
        if(msg->accept_ack_groupcount[cmd->grp] >=
                node->groups->node_counts[cmd->grp]/2 + 1
            //Also check if the ACCEPT_ACK from the grp leader was received
            && msg->accept_ack_counts[cmd->ballot[cmd->grp].id] > 0) {
            //TODO Check for the best time to reset this counter
            if(msg->accept_ack_groupready[cmd->grp]++ == 0)
                msg->accept_ack_totalcount += 1;
        }
        //Update latest_gts delivered if sender is in my group
        //  TODO This should not be required: ACCEPT_ACK are not received in order
        if(cmd->grp == node->comm->groups[node->id]
           && paircmp(&node->amcast->gts_last_delivered[sid], &cmd->gts_last_delivered) < 0)
                node->amcast->gts_last_delivered[sid] = cmd->gts_last_delivered;
        if(msg->accept_ack_totalcount !=
			msg->msg.destgrps_count)
            return;
        if(msg->phase != COMMITTED && msg->delivered == FALSE) {
            pqueue_remove(node->amcast->pending_lts,
                          &msg->lts[node->comm->groups[node->id]]);
            pqueue_push(node->amcast->committed_gts, msg, &msg->gts);
        }
        msg->phase = COMMITTED;
        //Compute infimum of gts_last_delivered for my group
        node->amcast->gts_inf_delivered = node->amcast->gts_last_delivered[node->id];
        for(xid_t *nid = node->groups->members[node->comm->groups[node->id]];
                nid < node->groups->members[node->comm->groups[node->id]]
                + node->groups->node_counts[node->comm->groups[node->id]];
                nid++)
            if(paircmp(node->amcast->gts_last_delivered+(*nid), &node->amcast->gts_inf_delivered) < 0)
                node->amcast->gts_inf_delivered = node->amcast->gts_last_delivered[(*nid)];
	//TODO A lot of possible improvements in the delivery pattern
        int try_next = 1;
        while(try_next && pqueue_size(node->amcast->committed_gts) > 0) {
            struct amcast_msg *i_msg = NULL, *j_msg = NULL;
            try_next = 0;
            if((i_msg = pqueue_peek(node->amcast->committed_gts)) == NULL) {
                printf("Failed to peek - %u\n", pqueue_size(node->amcast->committed_gts));
                return;
            }
            if(i_msg->phase == COMMITTED
               && i_msg->delivered == FALSE) {
                j_msg = pqueue_peek(node->amcast->pending_lts);
                if(j_msg != NULL && paircmp(&j_msg->lts[node->comm->groups[node->id]],
                                        &i_msg->gts) < 0
                             && j_msg->phase != COMMITTED) {
                    return;
                }
                if((i_msg = pqueue_pop(node->amcast->committed_gts)) == NULL) {
		    printf("Failed to pop - %u\n", pqueue_size(node->amcast->committed_gts));
                    return;
                }
                try_next = 1;
                struct enveloppe rep = {
	            .sid = node->id,
	            .cmd_type = DELIVER,
	            .cmd.deliver = {
	                .mid = i_msg->msg.mid,
		        .ballot = node->amcast->ballot,
		        .lts = i_msg->lts[node->comm->groups[node->id]],
		        .gts_inf_delivered = node->amcast->gts_inf_delivered,
		        .gts = i_msg->gts
	            },
	        };
                send_to_group(node, &rep, node->comm->groups[node->id]);
                i_msg->delivered = TRUE;
            }
        }
    }
}

static void handle_deliver(struct node *node, xid_t sid, deliver_t *cmd) {
    //printf("[%u] {%u,%d} We got DELIVER command from %u with gts: (%u,%u)!\n",
    //        node->id, cmd->mid.time, cmd->mid.id, sid, cmd->gts.time, cmd->gts.id);
    if ((node->amcast->status == FOLLOWER || node->amcast->status == LEADER)
            && paircmp(&node->amcast->ballot, &cmd->ballot) == 0
            && paircmp(&cmd->gts, &node->amcast->gts_last_delivered[node->id]) > 0) {
        struct amcast_msg *msg = NULL;
        if((msg = htable_lookup(node->amcast->h_msgs, &cmd->mid)) == NULL) {
            puts("ERROR: Could not find this mid in h_msgs");
            exit(EXIT_FAILURE);
        }
        msg->lts[node->comm->groups[node->id]] = cmd->lts;
        msg->gts = cmd->gts;
        msg->phase = COMMITTED;
        if(node->amcast->clock < msg->gts.time)
            node->amcast->clock = msg->gts.time;
        node->amcast->gts_last_delivered[node->id] = msg->gts;
        node->amcast->gts_inf_delivered = cmd->gts_inf_delivered;
        pqueue_push(node->amcast->delivered_gts, msg, &msg->gts);
        if(node->amcast->delivery_cb)
            node->amcast->delivery_cb(node, msg, node->amcast->dev_cb_arg);
        //Free amcast_msg structs delivered by everyone in my group
        while(pqueue_size(node->amcast->delivered_gts) > 0) {
            struct amcast_msg *i_msg = NULL;
            if((i_msg = pqueue_peek(node->amcast->delivered_gts)) == NULL) {
                printf("Failed to peek - %u\n", pqueue_size(node->amcast->delivered_gts));
                return;
            }
            if(paircmp(&(i_msg)->gts, &node->amcast->gts_inf_delivered) > 0)
                break;
            if((i_msg = pqueue_pop(node->amcast->delivered_gts)) == NULL) {
                printf("Failed to pop - %u\n", pqueue_size(node->amcast->delivered_gts));
                return;
            }
            htable_remove(node->amcast->h_msgs, &(i_msg)->msg.mid);
            free_amcast_msg(NULL, i_msg, NULL);
        }
    }
}

static void handle_newleader(struct node *node, xid_t sid, newleader_t *cmd) {
    printf("[%u] We got NEWLEADER command from %u!\n", node->id, sid);
    if(paircmp(&node->amcast->ballot, &cmd->ballot) > 0)
        return;
    node->amcast->status = PREPARE;
    //Reset counters when initializing higher ballot number
    if(sid == node->id && paircmp(&node->amcast->ballot, &cmd->ballot) < 0) {
        node->amcast->newleader_ack_groupcount = 0;
        node->amcast->newleader_sync_ack_groupcount = 1;
        memset(node->amcast->newleader_ack_count, 0, sizeof(unsigned int) * node->comm->cluster_size);
        memset(node->amcast->newleader_sync_ack_count, 0, sizeof(unsigned int) * node->comm->cluster_size);
    }
    node->amcast->ballot = cmd->ballot;
    //TODO Is trimming delivered_gts pqueue here important ?
    struct enveloppe rep = {
        .sid = node->id,
        .cmd_type = NEWLEADER_ACK,
        .cmd.newleader_ack = {
            .ballot = node->amcast->ballot,
            .aballot = node->amcast->aballot,
            .clock = node->amcast->clock,
            .msg_count = htable_size(node->amcast->h_msgs),
        }
    };
    //Limitation due to bad way of sending state arrays
    if(rep.cmd.newleader_ack.msg_count > MAX_MSG_DIFF) {
        printf("[%u] ERROR: can not send %u messages in a single enveloppe %u,%d\n", node->id, rep.cmd.newleader_ack.msg_count, node->amcast->gts_inf_delivered.time, node->amcast->gts_inf_delivered.id);
        return;
    }
    //TODO CHANGETHIS ugly a.f. Have a clean way to append arrays to enveloppe
    int acc = 0;
    void fill_rep(m_uid_t *mid, struct amcast_msg *msg, int *acc) {
        //ADDITION do not bother transmitting PROPOSED messages
        if(msg->phase < ACCEPTED && msg->phase != COMMITTED)
            return;
        rep.cmd.newleader_ack.messages[*acc].msg = msg->msg;
        rep.cmd.newleader_ack.messages[*acc].phase = msg->phase;
        rep.cmd.newleader_ack.messages[*acc].gts = msg->gts;
        memcpy(rep.cmd.newleader_ack.messages[*acc].lballot, msg->lballot, sizeof(p_uid_t) * node->groups->groups_count);
        memcpy(rep.cmd.newleader_ack.messages[*acc].lts, msg->lts, sizeof(g_uid_t) * node->groups->groups_count);
        *acc += 1;
    }
    htable_foreach(node->amcast->h_msgs, (GHFunc) fill_rep, &acc);
    send_to_peer(node, &rep, sid);
}

static void handle_newleader_ack(struct node *node, xid_t sid, newleader_ack_t *cmd) {
    printf("[%u] We got NEWLEADER_ACK command from %u with %u messages!\n", node->id, sid, cmd->msg_count);
    //Reject replies if we already had enough
    if((node->amcast->newleader_ack_groupcount >= node->groups->node_counts[node->comm->groups[node->id]]/2 + 1
            && node->amcast->status == PREPARE)
            || node->amcast->newleader_ack_count[sid] > 0)
        return;
    //Proceed if uninitialised, but reset counters
    if(node->amcast->status != PREPARE && paircmp(&node->amcast->ballot, &cmd->ballot) < 0) {
        node->amcast->ballot = cmd->ballot;
        node->amcast->newleader_ack_groupcount = 0;
        memset(node->amcast->newleader_ack_count, 0, sizeof(unsigned int) * node->comm->cluster_size);
    }
    //Reject replies with wrong ballot
    else if(paircmp(&node->amcast->ballot, &cmd->ballot) != 0)
        return;
    //Keep track of replies and reject if needed
    node->amcast->newleader_ack_groupcount += 1;
    node->amcast->newleader_ack_count[sid] += 1;
    //TODO THOUGHT do we really need to reset all local state ?
    //Forge a new state from replies with high-enough aballot
    //  TODO Find a better way of doing this
    //  TODO Find a way to avoid sending the payload for already known messages
    if(paircmp(&node->amcast->aballot, &cmd->aballot) <= 0) {
        for(int i=0; i<cmd->msg_count; i++) {
            struct amcast_msg *msg = NULL;
            message_t *message = &cmd->messages[i].msg;
            if((msg = htable_lookup(node->amcast->h_msgs, &message->mid)) == NULL) {
                msg = init_amcast_msg(node->groups, node->comm->cluster_size, message);
                //Run msginit callback
                if(node->amcast->msginit_cb)
                    node->amcast->msginit_cb(node, msg, node->amcast->ini_cb_arg);
                htable_insert(node->amcast->h_msgs, &msg->msg.mid, msg);
            }
            //TODO CHANGETHIS depends on enum declaration order
            if(msg->phase < cmd->messages[i].phase) {
                msg->phase = cmd->messages[i].phase;
                msg->gts = cmd->messages[i].gts;
                memcpy(msg->lballot, cmd->messages[i].lballot, sizeof(p_uid_t) * node->groups->groups_count);
                memcpy(msg->lts, cmd->messages[i].lts, sizeof(g_uid_t) * node->groups->groups_count);
            }
            if(msg->phase == COMMITTED)
                pqueue_push(node->amcast->committed_gts, msg, &msg->gts);
            else
                pqueue_push(node->amcast->pending_lts, msg, &msg->lts[node->comm->groups[node->id]]);
            //TODO Is setting lballot to new ballot upon recovery good ?
            msg->lballot[node->comm->groups[node->id]] = cmd->ballot;
        }
        node->amcast->aballot = cmd->ballot;
    }
    //TODO compute max clock incrementally and replace it only at the end
    if(node->amcast->clock < cmd->clock)
        node->amcast->clock = cmd->clock;
    //Send NEWLEADER_SYNC once got a quorum of replies
    if(node->amcast->newleader_ack_groupcount < node->groups->node_counts[node->comm->groups[node->id]]/2 + 1
            || node->amcast->status != PREPARE)
        return;
    struct enveloppe rep = {
        .sid = node->id,
        .cmd_type = NEWLEADER_SYNC,
        .cmd.newleader_sync = {
            .ballot = node->amcast->ballot,
            .clock = node->amcast->clock,
            //TODO send a custom diff, not whole state
            .msg_count = htable_size(node->amcast->h_msgs),
        }
    };
    //Limitation due to bad way of sending state arrays
    if(rep.cmd.newleader_sync.msg_count > MAX_MSG_DIFF) {
        printf("[%u] ERROR: can not send %u messages in a single enveloppe %u,%d\n", node->id, rep.cmd.newleader_sync.msg_count, node->amcast->gts_inf_delivered.time, node->amcast->gts_inf_delivered.id);
        return;
    }
    //TODO CHANGETHIS ugly a.f. Have a clean way to append arrays to enveloppe
    int acc = 0;
    void fill_rep(m_uid_t *mid, struct amcast_msg *msg, int *acc) {
        //ADDITION do not bother transmitting PROPOSED messages
        if(msg->phase != ACCEPTED && msg->phase != COMMITTED)
            return;
        rep.cmd.newleader_sync.messages[*acc].msg = msg->msg;
        rep.cmd.newleader_sync.messages[*acc].phase = msg->phase;
        rep.cmd.newleader_sync.messages[*acc].gts = msg->gts;
        memcpy(rep.cmd.newleader_sync.messages[*acc].lballot, msg->lballot, sizeof(p_uid_t) * node->groups->groups_count);
        memcpy(rep.cmd.newleader_sync.messages[*acc].lts, msg->lts, sizeof(g_uid_t) * node->groups->groups_count);
        *acc += 1;
    }
    htable_foreach(node->amcast->h_msgs, (GHFunc) fill_rep, &acc);
    //TODO CHANGETHIS do not send this to itself (group except me)
    send_to_group(node, &rep, node->comm->groups[node->id]);
}

static void handle_newleader_sync(struct node *node, xid_t sid, newleader_sync_t *cmd) {
    printf("[%u] We got NEWLEADER_SYNC command from %u!\n", node->id, sid);
    //TODO CHANGETHIS Do not let the futur leader send this cmd to itself
    if(node->id == sid)
        return;
    if(node->amcast->status != PREPARE || paircmp(&node->amcast->ballot, &cmd->ballot) != 0)
        return;
    node->amcast->status = FOLLOWER;
    node->amcast->aballot = cmd->ballot;
    //Dump received state into local one
    for(int i=0; i<cmd->msg_count; i++) {
        struct amcast_msg *msg = NULL;
        message_t *message = &cmd->messages[i].msg;
        if((msg = htable_lookup(node->amcast->h_msgs, &message->mid)) == NULL) {
            msg = init_amcast_msg(node->groups, node->comm->cluster_size, message);
            //Run msginit callback
            if(node->amcast->msginit_cb)
                node->amcast->msginit_cb(node, msg, node->amcast->ini_cb_arg);
            htable_insert(node->amcast->h_msgs, &msg->msg.mid, msg);
        }
        msg->phase = cmd->messages[i].phase;
        msg->gts = cmd->messages[i].gts;
        memcpy(msg->lballot, cmd->messages[i].lballot, sizeof(p_uid_t) * node->groups->groups_count);
        memcpy(msg->lts, cmd->messages[i].lts, sizeof(g_uid_t) * node->groups->groups_count);
    }
    struct enveloppe rep = {
        .sid = node->id,
        .cmd_type = NEWLEADER_SYNC_ACK,
        .cmd.newleader_sync_ack = {
            .ballot = cmd->ballot
        }
    };
    send_to_peer(node, &rep, sid);
}

static void handle_newleader_sync_ack(struct node *node, xid_t sid, newleader_sync_ack_t *cmd) {
    printf("[%u] We got NEWLEADER_SYNC_ACK command from %u!\n", node->id, sid);
    if(node->amcast->status != PREPARE || paircmp(&node->amcast->ballot, &cmd->ballot) != 0)
        return;
    //Wait for a quorum of replies
    if(!node->amcast->newleader_sync_ack_count[sid]) {
        node->amcast->newleader_sync_ack_count[sid] += 1;
        node->amcast->newleader_sync_ack_groupcount += 1;
    }
    if(node->amcast->newleader_sync_ack_groupcount < node->groups->node_counts[node->comm->groups[node->id]]/2 + 1)
        return;
    node->amcast->status = LEADER;
    //TODO Check whether computing gts_inf_delivered is useful for recovered messages
    //TODO CHANGETHIS ugly copy-paste of the delivery pattern
    int try_next = 1;
    while(try_next && pqueue_size(node->amcast->committed_gts) > 0) {
        struct amcast_msg *i_msg = NULL, *j_msg = NULL;
        try_next = 0;
        if((i_msg = pqueue_peek(node->amcast->committed_gts)) == NULL) {
            printf("Failed to peek - %u\n", pqueue_size(node->amcast->committed_gts));
            return;
        }
        if(i_msg->phase == COMMITTED
                && i_msg->delivered == FALSE) {
            j_msg = pqueue_peek(node->amcast->pending_lts);
            if(j_msg != NULL && paircmp(&j_msg->lts[node->comm->groups[node->id]],
                    &i_msg->gts) < 0
                    && j_msg->phase != COMMITTED) {
                return;
            }
            if((i_msg = pqueue_pop(node->amcast->committed_gts)) == NULL) {
                printf("Failed to pop - %u\n", pqueue_size(node->amcast->committed_gts));
                return;
            }
            try_next = 1;
            struct enveloppe rep = {
	            .sid = node->id,
	            .cmd_type = DELIVER,
	            .cmd.deliver = {
	                .mid = i_msg->msg.mid,
                    .ballot = node->amcast->ballot,
                    .lts = i_msg->lts[node->comm->groups[node->id]],
                    .gts_inf_delivered = node->amcast->gts_inf_delivered,
                    .gts = i_msg->gts
	            },
	        };
            send_to_group(node, &rep, node->comm->groups[node->id]);
            i_msg->delivered = TRUE;
        }
    }
    //TODO add retry pattern for accepted messages
    int retry_message(g_uid_t *lts, struct amcast_msg *msg, struct node *node) {
        struct enveloppe rep = {
            .sid = node->id,
            .cmd_type = REACCEPT,
            .cmd.reaccept = {
                .mid = msg->msg.mid,
                .grp = node->comm->groups[node->id],
                .gts = msg->gts,
            },
        };
        memcpy(rep.cmd.reaccept.ballot, msg->lballot, sizeof(p_uid_t) * node->groups->groups_count);
        send_to_destgrps(node, &rep, msg->msg.destgrps, msg->msg.destgrps_count);
        return 0;
    }
    pqueue_foreach(node->amcast->pending_lts, (pq_traverse_fun) retry_message, node);
}

void dispatch_amcast_command(struct node *node, struct enveloppe *env) {
    switch(env->cmd_type) {
        case MULTICAST:
            handle_multicast(node, env->sid, &(env->cmd.multicast));
            break;
        case ACCEPT:
            handle_accept(node, env->sid, &(env->cmd.accept));
            break;
        case ACCEPT_ACK:
            handle_accept_ack(node, env->sid, &(env->cmd.accept_ack));
            break;
        case DELIVER:
            handle_deliver(node, env->sid, &(env->cmd.deliver));
            break;
        case REACCEPT:
            handle_reaccept(node, env->sid, &(env->cmd.reaccept));
            break;
        case NEWLEADER:
            handle_newleader(node, env->sid, &(env->cmd.newleader));
            break;
        case NEWLEADER_ACK:
            handle_newleader_ack(node, env->sid, &(env->cmd.newleader_ack));
            break;
        case NEWLEADER_SYNC:
            handle_newleader_sync(node, env->sid, &(env->cmd.newleader_sync));
            break;
	case NEWLEADER_SYNC_ACK:
            handle_newleader_sync_ack(node, env->sid, &(env->cmd.newleader_sync_ack));
            break;
	default:
            printf("[%u] Unhandled command received from %u\n", node->id, env->sid);
            break;
    }
}

static struct amcast_msg *init_amcast_msg(struct groups *groups, unsigned int cluster_size, message_t *cmd) {
    struct amcast_msg *msg = malloc(sizeof(struct amcast_msg));
    msg->phase = START;
    msg->lballot = malloc(sizeof(struct pair) * groups->groups_count);
    msg->lts = malloc(sizeof(struct pair) * groups->groups_count);
    msg->gts = default_pair;
    msg->delivered = FALSE;
    msg->msg = *cmd;
    //EXTRA FIELDS - ACCEPT COUNTERS
    msg->accept_totalcount = 0;
    msg->accept_groupcount = malloc(sizeof(unsigned int) * groups->groups_count);
    msg->accept_max_lts = default_pair;
    //EXTRA FIELDS - ACCEPT_ACK COUNTERS
    msg->groups_count = groups->groups_count;
    msg->accept_ack_totalcount = 0;
    msg->accept_ack_groupready = malloc(sizeof(unsigned int) * groups->groups_count);
    msg->accept_ack_groupcount = malloc(sizeof(unsigned int) * groups->groups_count);
    msg->accept_ack_counts = malloc(sizeof(unsigned int) * cluster_size);
    //Init the several arrays
    memset(msg->accept_groupcount, 0, sizeof(unsigned int) * groups->groups_count);
    memset(msg->accept_ack_counts, 0, sizeof(unsigned int) * cluster_size);
    for(unsigned int *i = groups->node_counts; i<groups->node_counts + groups->groups_count; i++) {
        msg->lballot[i-groups->node_counts] = default_pair;
        msg->lts[i-groups->node_counts] = default_pair;
        msg->accept_ack_groupready[i-groups->node_counts] = 0;
        msg->accept_ack_groupcount[i-groups->node_counts] = 0;
    }
    return msg;
}

struct amcast *amcast_init(msginit_cb_fun msginit_cb, void *ini_cb_arg, delivery_cb_fun delivery_cb, void *dev_cb_arg) {
    struct amcast *amcast = malloc(sizeof(struct amcast));
    amcast->status = INIT;
    amcast->ballot = default_pair;
    amcast->aballot = default_pair;
    amcast->clock = 0;
    amcast->h_msgs = htable_init(pairhash_cantor, pairequ);
    //EXTRA FIELDS (NOT IN SPEC)
    amcast->delivered_gts = pqueue_init((pq_pricmp_fun) paircmp);
    amcast->committed_gts = pqueue_init((pq_pricmp_fun) paircmp);
    amcast->pending_lts = pqueue_init((pq_pricmp_fun) paircmp);
    amcast->gts_inf_delivered = default_pair;
    amcast->msginit_cb = msginit_cb;
    amcast->delivery_cb = delivery_cb;
    amcast->ini_cb_arg = ini_cb_arg;
    amcast->dev_cb_arg = dev_cb_arg;
    return amcast;
}

//Function prototype to be called by htable_foreach()
static int free_amcast_msg(m_uid_t *mid, struct amcast_msg *msg, void *arg) {
    free(msg->lballot);
    free(msg->lts);
    free(msg->accept_groupcount);
    free(msg->accept_ack_groupready);
    free(msg->accept_ack_groupcount);
    free(msg->accept_ack_counts);
    free(msg);
    return 0;
}

int amcast_free(struct amcast *amcast) {
    pqueue_free(amcast->delivered_gts);
    pqueue_free(amcast->committed_gts);
    pqueue_free(amcast->pending_lts);
    free(amcast->gts_last_delivered);
    free(amcast->newleader_ack_count);
    free(amcast->newleader_sync_ack_count);
    htable_foreach(amcast->h_msgs, (GHFunc) free_amcast_msg, NULL);
    htable_free(amcast->h_msgs);
    free(amcast);
    return 0;
}

static void reset_accept_ack_counters(struct amcast_msg *msg, struct groups *groups, unsigned int cluster_size) {
    msg->accept_ack_totalcount = 0;
    memset(msg->accept_ack_groupready, 0, sizeof(unsigned int) * groups->groups_count);
    memset(msg->accept_ack_groupcount, 0, sizeof(unsigned int) * groups->groups_count);
    memset(msg->accept_ack_counts, 0, sizeof(unsigned int) * cluster_size);
}
