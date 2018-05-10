#include <string.h>

#include "types.h"
#include "node.h"
#include "message.h"
#include "amcast_types.h"
#include "amcast.h"
#include "pqueue.h"


int paircmp(struct pair *p1, struct pair *p2) {
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

int midequ(m_uid_t *m1, m_uid_t *m2) {
    if(m1 == NULL || m2 == NULL)
        exit(EXIT_FAILURE);
    return *m1 == *m2;
}

//TODO Make helper functions to create enveloppes in clean and nice looking way
//TODO Following pointers makes it a lot harder to read the code, try to find some simplification
//         e.g. properly defined macros could help,
//         or sub-functions using only the useful structure fields passed as arguments

static struct amcast_msg *init_amcast_msg(struct groups *groups, unsigned int cluster_size, message_t *cmd);

static void handle_multicast(struct node *node, xid_t sid, message_t *cmd) {
    printf("[%u] {%u} We got MULTICAST command from %u!\n", node->id, cmd->mid, sid);
    if (node->amcast->status == LEADER) {
        if(node->amcast->msgs_count >= node->amcast->msgs_size) {
            node->amcast->msgs_size *= 2;
            node->amcast->msgs = realloc(node->amcast->msgs,
                            sizeof(struct amcast_msg *) * node->amcast->msgs_size);
        }
        if(cmd->mid >= node->amcast->msgs_count) {
            node->amcast->msgs_count++;
            node->amcast->msgs[cmd->mid] = init_amcast_msg(node->groups, node->comm->cluster_size, cmd);
        }
        struct amcast_msg *msg = node->amcast->msgs[cmd->mid];
        if(msg->phase == START) {
            msg->phase = PROPOSED;
            node->amcast->clock++;
            msg->lts[node->comm->groups[node->id]].time = node->amcast->clock;
            msg->lts[node->comm->groups[node->id]].id = node->comm->groups[node->id];
            pqueue_push(node->amcast->pending_lts, &cmd->mid,
			    &msg->lts[node->comm->groups[node->id]]);
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
    printf("[%u] {%u} We got ACCEPT command from %u!\n", node->id, cmd->mid, sid);
    if(node->amcast->msgs_count >= node->amcast->msgs_size) {
        node->amcast->msgs_size *= 2;
        node->amcast->msgs = realloc(node->amcast->msgs,
                        sizeof(struct amcast_msg *) * node->amcast->msgs_size);
    }
    if(cmd->mid >= node->amcast->msgs_count) {
        node->amcast->msgs_count++;
        node->amcast->msgs[cmd->mid] = init_amcast_msg(node->groups, node->comm->cluster_size, &cmd->msg);
    }
    struct amcast_msg *msg = node->amcast->msgs[cmd->mid];
    if ((node->amcast->status == LEADER || node->amcast->status == FOLLOWER)
            && paircmp(&msg->lballot[cmd->grp], &cmd->ballot) <= 0
            && !( cmd->grp == node->comm->groups[node->id]
                  && !(paircmp(&node->amcast->ballot, &cmd->ballot) == 0) )) {
	//TODO Carefully try to see when it's the best time to reset this counter
	//     Probably upon a leader change
	if (paircmp(&msg->lballot[cmd->grp], &default_pair) == 0) {
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
            pqueue_push(node->amcast->pending_lts, &cmd->mid, &cmd->lts);
        }
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
    printf("[%u] {%u} We got ACCEPT_ACK command from %u!\n", node->id, cmd->mid, sid);
    if (node->amcast->status == LEADER) {
        struct amcast_msg *msg = node->amcast->msgs[cmd->mid];
        //TODO Cache messages instead of resending to yourself
        //It seems ACCEPT_ACKS are sometime recevied before gts is initialized
        if(paircmp(&msg->gts, &default_pair) == 0) {
            printf("[%d] {%u} Re-sending ACCEPT_ACK command from %d!\n", node->id, cmd->mid, sid);
            struct enveloppe retry = { .sid = sid, .cmd_type = ACCEPT_ACK, .cmd.accept_ack = *cmd };
            send_to_peer(node, &retry, node->id);
            return;
        }
        //Check whether the local ballot and the received one are equal
        for(xid_t i=0; i<node->groups->groups_count; i++)
            if(paircmp(&msg->lballot[i], &cmd->ballot[i]) != 0)
                return;
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
            pqueue_push(node->amcast->committed_gts, &cmd->mid, &msg->gts);
        }
        msg->phase = COMMITTED;
	//TODO A lot of possible improvements in the delivery pattern
        int try_next = 1;
        while(try_next && pqueue_size(node->amcast->committed_gts) > 0) {
            try_next = 0;
            m_uid_t *i;
            if((i = pqueue_peek(node->amcast->committed_gts)) == NULL) {
                printf("Failed to peek - %u\n", pqueue_size(node->amcast->committed_gts));
                return;
            }
            if(node->amcast->msgs[*i]->phase == COMMITTED
               && node->amcast->msgs[*i]->delivered == FALSE) {
                m_uid_t *j = pqueue_peek(node->amcast->pending_lts);
                if(j != NULL && paircmp(&node->amcast->msgs[*j]->lts[node->comm->groups[node->id]],
                                        &node->amcast->msgs[*i]->gts) < 0
                             && node->amcast->msgs[*j]->phase != COMMITTED) {
                    return;
                }
                if((i = pqueue_pop(node->amcast->committed_gts)) == NULL) {
		    printf("Failed to pop - %u\n", pqueue_size(node->amcast->committed_gts));
                    return;
                }
                try_next = 1;
                node->amcast->msgs[*i]->delivered = TRUE;
                if(node->amcast->delivery_cb)
                    node->amcast->delivery_cb(node, *i);
                struct enveloppe rep = {
	            .sid = node->id,
	            .cmd_type = DELIVER,
	            .cmd.deliver = {
	                .mid = *i,
		        .ballot = node->amcast->ballot,
		        .lts = node->amcast->msgs[*i]->lts[node->comm->groups[node->id]],
		        .gts = node->amcast->msgs[*i]->gts
	            },
	        };
                send_to_group(node, &rep, node->comm->groups[node->id]);
                //RESET counter variables
                memset(msg->accept_ack_counts,
                            0, sizeof(unsigned int) * node->comm->cluster_size);
                for(int i=0; i<node->groups->groups_count; i++) {
                    msg->accept_ack_groupready[i] = 0;
                    msg->accept_ack_groupcount[i] = 0;
	        }
            }
        }
    }
}

static void handle_deliver(struct node *node, xid_t sid, deliver_t *cmd) {
    printf("[%u] {%u} We got DELIVER command from %u with gts: (%u,%u)!\n", node->id, cmd->mid, sid, cmd->gts.time, cmd->gts.id);
    struct amcast_msg *msg = node->amcast->msgs[cmd->mid];
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
    msg->accept_max_lts = default_pair;
    //EXTRA FIELDS - ACCEPT_ACK COUNTERS
    msg->groups_count = groups->groups_count;
    msg->accept_ack_totalcount = 0;
    msg->accept_ack_groupready = malloc(sizeof(unsigned int) * groups->groups_count);
    msg->accept_ack_groupcount = malloc(sizeof(unsigned int) * groups->groups_count);
    msg->accept_ack_counts = malloc(sizeof(unsigned int) * cluster_size);
    //Init the several arrays
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
    amcast->msgs_count = 0;
    amcast->msgs_size = MSGS_DEFAULT_SIZE;
    amcast->msgs = malloc(sizeof(struct amcast_msg *) * MSGS_DEFAULT_SIZE);
    memset(amcast->msgs, 0, sizeof(struct amcast_msg *) * MSGS_DEFAULT_SIZE);
    //EXTRA FIELDS (NOT IN SPEC)
    amcast->committed_gts = pqueue_init((pq_pricmp_fun) paircmp);
    amcast->pending_lts = pqueue_init((pq_pricmp_fun) paircmp);
    amcast->delivery_cb = delivery_cb;
    return amcast;
}

static int free_amcast_msg(struct amcast_msg *msg) {
    free(msg->lballot);
    free(msg->lts);
    free(msg->accept_ack_groupready);
    free(msg->accept_ack_groupcount);
    free(msg->accept_ack_counts);
    free(msg);
    return 0;
}

int amcast_free(struct amcast *amcast) {
    pqueue_free(amcast->committed_gts);
    pqueue_free(amcast->pending_lts);
    for(struct amcast_msg **msg = amcast->msgs; msg < amcast->msgs + amcast->msgs_count; msg++)
        if(*msg)
            free_amcast_msg(*msg);
    free(amcast);
    return 0;
}
