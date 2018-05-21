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

unsigned int pairhash(struct pair *p) {
    if(p == NULL) {
        puts("ERROR: pairhash called with null pointer");
        exit(EXIT_FAILURE);
    }
    return ((p->time + p->id + 1)*(p->time + p->id + 2))/2 + p->id;
}

//TODO Make helper functions to create enveloppes in clean and nice looking way
//TODO Following pointers makes it a lot harder to read the code, try to find some simplification
//         e.g. properly defined macros could help,
//         or sub-functions using only the useful structure fields passed as arguments

static struct amcast_msg *init_amcast_msg(struct groups *groups, unsigned int cluster_size, message_t *cmd);
static void reset_accept_ack_counters(struct amcast_msg *msg, struct groups *groups, unsigned int cluster_size);

static void handle_multicast(struct node *node, xid_t sid, message_t *cmd) {
    printf("[%u] {%u,%d} We got MULTICAST command from %u!\n", node->id, cmd->mid.time, cmd->mid.id, sid);
    if (node->amcast->status == LEADER) {
        struct amcast_msg *msg = NULL;
        if((msg = htable_lookup(node->amcast->h_msgs, &cmd->mid)) == NULL) {
            msg = init_amcast_msg(node->groups, node->comm->cluster_size, cmd);
            htable_insert(node->amcast->h_msgs, &cmd->mid, msg);
        }
        if(msg->phase == START) {
            msg->phase = PROPOSED;
            node->amcast->clock++;
            msg->lts[node->comm->groups[node->id]].time = node->amcast->clock;
            msg->lts[node->comm->groups[node->id]].id = node->comm->groups[node->id];
            pqueue_push(node->amcast->pending_lts, &msg, &msg->lts[node->comm->groups[node->id]]);
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
    printf("[%u] {%u,%d} We got ACCEPT command from %u!\n", node->id, cmd->mid.time, cmd->mid.id, sid);
    struct amcast_msg *msg = NULL;
    if((msg = htable_lookup(node->amcast->h_msgs, &cmd->mid)) == NULL) {
        msg = init_amcast_msg(node->groups, node->comm->cluster_size, &cmd->msg);
        htable_insert(node->amcast->h_msgs, &cmd->mid, msg);
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
                   && cmd->grp == node->comm->groups[node->id]
                   && paircmp(&msg->lts[node->comm->groups[node->id]],
                              &cmd->lts) < 0) {
            pqueue_remove(node->amcast->pending_lts,
                   &msg->lts[node->comm->groups[node->id]]);
            pqueue_push(node->amcast->pending_lts, &msg, &cmd->lts);
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
		.gts = msg->gts
	    },
	};
	memcpy(rep.cmd.accept_ack.ballot, msg->lballot,
			sizeof(p_uid_t) * node->groups->groups_count);
        //send_to_destgrps(node, &rep, node->amcast->msgs[cmd->mid]->msg.destgrps,
        //                 node->amcast->msgs[cmd->mid]->msg.destgrps_count);
        send_to_peer(node, &rep, 0);
        send_to_peer(node, &rep, 3);
    }
}

static void handle_accept_ack(struct node *node, xid_t sid, accept_ack_t *cmd) {
    printf("[%u] {%u,%d} We got ACCEPT_ACK command from %u!\n", node->id, cmd->mid.time, cmd->mid.id, sid);
    if (node->amcast->status == LEADER) {
        struct amcast_msg *msg = NULL;
        if((msg = htable_lookup(node->amcast->h_msgs, &cmd->mid)) == NULL) {
            puts("ERROR: Could not find this mid in h_msgs");
            exit(EXIT_FAILURE);
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
                    if(paircmp(&msg->lts[i], &default_pair) != 0)
                        return;
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
        if(msg->accept_ack_totalcount !=
			msg->msg.destgrps_count)
            return;
        if(msg->phase != COMMITTED) {
            pqueue_remove(node->amcast->pending_lts,
                          &msg->lts[node->comm->groups[node->id]]);
            pqueue_push(node->amcast->committed_gts, &msg, &msg->gts);
        }
        msg->phase = COMMITTED;
	//TODO A lot of possible improvements in the delivery pattern
        int try_next = 1;
        while(try_next && pqueue_size(node->amcast->committed_gts) > 0) {
            struct amcast_msg **i_msg = NULL, **j_msg = NULL;
            try_next = 0;
            if((i_msg = pqueue_peek(node->amcast->committed_gts)) == NULL) {
                printf("Failed to peek - %u\n", pqueue_size(node->amcast->committed_gts));
                return;
            }
            if((*i_msg)->phase == COMMITTED
               && (*i_msg)->delivered == FALSE) {
                j_msg = pqueue_peek(node->amcast->pending_lts);
                if(j_msg != NULL && paircmp(&(*j_msg)->lts[node->comm->groups[node->id]],
                                        &(*i_msg)->gts) < 0
                             && (*j_msg)->phase != COMMITTED) {
                    return;
                }
                if((i_msg = pqueue_pop(node->amcast->committed_gts)) == NULL) {
		    printf("Failed to pop - %u\n", pqueue_size(node->amcast->committed_gts));
                    return;
                }
                try_next = 1;
                (*i_msg)->delivered = TRUE;
                if(node->amcast->delivery_cb)
                    node->amcast->delivery_cb(node, (*i_msg)->msg.mid);
                struct enveloppe rep = {
	            .sid = node->id,
	            .cmd_type = DELIVER,
	            .cmd.deliver = {
	                .mid = (*i_msg)->msg.mid,
		        .ballot = node->amcast->ballot,
		        .lts = (*i_msg)->lts[node->comm->groups[node->id]],
		        .gts = (*i_msg)->gts
	            },
	        };
                send_to_group(node, &rep, node->comm->groups[node->id]);
            }
        }
    }
}

static void handle_deliver(struct node *node, xid_t sid, deliver_t *cmd) {
    printf("[%u] {%u,%d} We got DELIVER command from %u with gts: (%u,%u)!\n",
            node->id, cmd->mid.time, cmd->mid.id, sid, cmd->gts.time, cmd->gts.id);
    struct amcast_msg *msg = NULL;
    if((msg = htable_lookup(node->amcast->h_msgs, &cmd->mid)) == NULL) {
        puts("ERROR: Could not find this mid in h_msgs");
        exit(EXIT_FAILURE);
    }
    if (node->amcast->status == FOLLOWER
            && paircmp(&node->amcast->ballot, &cmd->ballot) == 0
            && msg->delivered == FALSE) {
        msg->lts[node->comm->groups[node->id]] = cmd->lts;
        msg->gts = cmd->gts;
        if(node->amcast->clock < msg->gts.time)
            node->amcast->clock = msg->gts.time;
        msg->delivered = TRUE;
        if(node->amcast->delivery_cb)
            node->amcast->delivery_cb(node, cmd->mid);
    }
}

static void handle_newleader(struct node *node, xid_t sid, newleader_t *cmd) {
    printf("[%u] We got NEWLEADER command from %u!\n", node->id, sid);
}

static void handle_newleader_ack(struct node *node, xid_t sid, newleader_ack_t *cmd) {
    printf("[%u] We got NEWLEADER_ACK command from %u!\n", node->id, sid);
}

static void handle_newleader_sync(struct node *node, xid_t sid, newleader_sync_t *cmd) {
    printf("[%u] We got NEWLEADER_SYNC command from %u!\n", node->id, sid);
}

static void handle_newleader_sync_ack(struct node *node, xid_t sid, newleader_sync_ack_t *cmd) {
    printf("[%u] We got NEWLEADER_SYNC_ACK command from %u!\n", node->id, sid);
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

struct amcast *amcast_init(delivery_cb_fun delivery_cb) {
    struct amcast *amcast = malloc(sizeof(struct amcast));
    amcast->status = INIT;
    amcast->ballot = default_pair;
    amcast->aballot = default_pair;
    amcast->clock = 0;
    amcast->h_msgs = htable_init(pairhash, pairequ);
    //EXTRA FIELDS (NOT IN SPEC)
    amcast->committed_gts = pqueue_init((pq_pricmp_fun) paircmp);
    amcast->pending_lts = pqueue_init((pq_pricmp_fun) paircmp);
    amcast->delivery_cb = delivery_cb;
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
    pqueue_free(amcast->committed_gts);
    pqueue_free(amcast->pending_lts);
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
