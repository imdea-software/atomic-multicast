#include "types.h"
#include "node.h"
#include "message.h"
#include "amcast_types.h"
#include "amcast.h"


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

//TODO Make helper functions to create enveloppes in clean and nice looking way
//TODO Following pointers makes it a lot harder to read the code, try to find some simplification
//         e.g. properly defined macros could help,
//         or sub-functions using only the useful structure fields passed as arguments

static struct amcast_msg *init_amcast_msg(unsigned int groups_count, message_t *cmd);

static void handle_multicast(struct node *node, xid_t sid, message_t *cmd) {
    printf("[%u] We got MULTICAST command from %u!\n", node->id, sid);
    if (node->amcast->status == LEADER) {
	if(node->amcast->msgs_count == 0 || node->amcast->msgs_count == cmd->mid) {
	//if(!node->amcast->msgs+cmd->mid) {
            node->amcast->msgs_count++;
            node->amcast->msgs = realloc(node->amcast->msgs,
                            sizeof(struct amcast_msg *) * node->amcast->msgs_count);
	    //TODO Have a proper group structure to avoid manually counting groups
	    int groups_count = 0;
	    for(int i=0; i<node->comm->cluster_size; i++)
                if (node->comm->groups[i] >= groups_count)
                    groups_count++;
            node->amcast->msgs[cmd->mid] = init_amcast_msg(groups_count, cmd);
	}
        if(node->amcast->msgs[cmd->mid]->phase == START) {
            node->amcast->msgs[cmd->mid]->phase = PROPOSED;
            node->amcast->clock++;
	    //TODO Properly implement the uid_t type (only a placeholder now)
            node->amcast->msgs[cmd->mid]->lts = node->amcast->clock;
        }
        struct enveloppe rep = {
	    .sid = node->id,
	    .cmd_type = ACCEPT,
	    .cmd.accept = {
	        .mid = cmd->mid,
		.grp = node->comm->groups[node->id],
		.ballot = node->amcast->ballot,
		.lts = node->amcast->msgs[cmd->mid]->lts,
		.msg = *cmd
	    },
	};
        send_to_destgrps(node, &rep, cmd->destgrps, cmd->destgrps_count);
    }
}

static void handle_accept(struct node *node, xid_t sid, accept_t *cmd) {
    printf("[%u] We got ACCEPT command from %u!\n", node->id, sid);
    if(node->amcast->msgs_count == 0 || node->amcast->msgs_count == cmd->mid) {
    //if(!node->amcast->msgs+cmd->mid) {
        node->amcast->msgs_count++;
        node->amcast->msgs = realloc(node->amcast->msgs,
                        sizeof(struct amcast_msg *) * node->amcast->msgs_count);
	//TODO Have a proper group structure to avoid manually counting groups
	int groups_count = 0;
	for(int i=0; i<node->comm->cluster_size; i++)
            if (node->comm->groups[i] >= groups_count)
                groups_count++;
        node->amcast->msgs[cmd->mid] = init_amcast_msg(groups_count, &cmd->msg);
    }
    if ((node->amcast->status == LEADER || node->amcast->status == FOLLOWER)
            && node->amcast->msgs[cmd->mid]->proposals[cmd->grp]->ballot <= cmd->ballot
            && !( cmd->grp == node->comm->groups[node->id]
                  && !(node->amcast->ballot == cmd->ballot) )) {
        if (node->amcast->msgs[cmd->mid]->proposals[cmd->grp]->status != UNDEF) {
	    for(xid_t *grp = node->amcast->msgs[cmd->mid]->msg.destgrps;
                grp < node->amcast->msgs[cmd->mid]->msg.destgrps + node->amcast->msgs[cmd->mid]->msg.destgrps_count;
		grp++)
                if(node->amcast->msgs[cmd->mid]->proposals[*grp]->status == CONFIRMED)
                    node->amcast->msgs[cmd->mid]->proposals[*grp]->status = RECEIVED;
        }
	node->amcast->msgs[cmd->mid]->proposals[cmd->grp]->status = RECEIVED;
	node->amcast->msgs[cmd->mid]->proposals[cmd->grp]->ballot = cmd->ballot;
	node->amcast->msgs[cmd->mid]->proposals[cmd->grp]->lts = cmd->lts;
	for(xid_t *grp = node->amcast->msgs[cmd->mid]->msg.destgrps;
            grp < node->amcast->msgs[cmd->mid]->msg.destgrps + node->amcast->msgs[cmd->mid]->msg.destgrps_count;
	    grp++)
            if(node->amcast->msgs[cmd->mid]->proposals[*grp]->status != RECEIVED)
	        return;
        if(node->amcast->msgs[cmd->mid]->phase != COMMITTED) {
            node->amcast->msgs[cmd->mid]->phase = ACCEPTED;
            node->amcast->msgs[cmd->mid]->lts =
		    node->amcast->msgs[cmd->mid]->proposals[node->comm->groups[node->id]]->lts;
	    for(xid_t *grp = node->amcast->msgs[cmd->mid]->msg.destgrps;
                grp < node->amcast->msgs[cmd->mid]->msg.destgrps + node->amcast->msgs[cmd->mid]->msg.destgrps_count;
	        grp++)
                if(node->amcast->msgs[cmd->mid]->gts < node->amcast->msgs[cmd->mid]->proposals[*grp]->lts)
                    node->amcast->msgs[cmd->mid]->gts = node->amcast->msgs[cmd->mid]->proposals[*grp]->lts;
            if(node->amcast->clock < node->amcast->msgs[cmd->mid]->gts)
                node->amcast->clock = node->amcast->msgs[cmd->mid]->gts;
        }
        struct enveloppe rep = {
	    .sid = node->id,
	    .cmd_type = ACCEPT_ACK,
	    .cmd.accept_ack = {
	        .mid = cmd->mid,
		.grp = node->comm->groups[node->id],
		.ballot = cmd->ballot,
		.gts = node->amcast->msgs[cmd->mid]->gts
	    },
	};
        //send_to_destgrps(node, &rep, node->amcast->msgs[cmd->mid]->msg.destgrps,
        //                 node->amcast->msgs[cmd->mid]->msg.destgrps_count);
        send_to_peer(node, &rep, 0);
        send_to_peer(node, &rep, 3);
    }
}

static void handle_accept_ack(struct node *node, xid_t sid, accept_ack_t *cmd) {
    printf("[%u] We got ACCEPT_ACK command from %u!\n", node->id, sid);
    if (node->amcast->status == LEADER) {
        //TODO Not too sure about the entry condition
        static int accept_acks_per_group_count[256];
        static int accept_acks_per_node_count[256];
        //static int accept_acks_per_group_count[node->amcast->msgs[cmd->mid]->msg.destgrps_count];
        //static int accept_acks_per_node_count[node->comm->cluster_size];
        if(node->amcast->msgs[cmd->mid]->proposals[cmd->grp]->ballot == cmd->ballot
                && node->amcast->msgs[cmd->mid]->gts == cmd->gts
                && accept_acks_per_node_count[sid] < 1) {
            accept_acks_per_node_count[sid] += 1;
            accept_acks_per_group_count[cmd->grp] += 1;
        }
        //TODO Have a proper group structure to avoid manually counting nodes in group
        int nodes_in_group_count = 0;
        for(int i=0; i<node->comm->cluster_size; i++)
            if (node->comm->groups[i] == cmd->grp)
                nodes_in_group_count++;
        //TODO Also check if the ACCEPT_ACK from the grp leader was received
        if(accept_acks_per_group_count[cmd->grp] >= nodes_in_group_count/2 + 1) {
            node->amcast->msgs[cmd->mid]->proposals[cmd->grp]->status = CONFIRMED;
        }
	for(xid_t *grp = node->amcast->msgs[cmd->mid]->msg.destgrps;
                grp < node->amcast->msgs[cmd->mid]->msg.destgrps + node->amcast->msgs[cmd->mid]->msg.destgrps_count;
	        grp++)
            if(node->amcast->msgs[cmd->mid]->proposals[*grp]->status != CONFIRMED)
                return;
        node->amcast->msgs[cmd->mid]->phase = COMMITTED;
	//TODO Do not rebuild on every call the gts-ordered set of messages
	//This really is INEFFICIENT
	int gts_order[node->amcast->msgs_count];
	for(int i=0; i<node->amcast->msgs_count; i++)
            gts_order[i] = 1;
	for(int i=0; i<node->amcast->msgs_count; i++) {
	    int lowest = 0;
	    for(int j=0; j<node->amcast->msgs_count; j++) {
		    /*
                if(!node->amcast->msgs+j) {
                    printf("[%u] Message at index %u does not exists, "
				    "only %u were received "
				    "and current mid is: %u\n",
				    node->id, j, node->amcast->msgs_count,
				    //node->amcast->msgs[node->amcast->msgs_count - 1]->msg.mid);
				    cmd->mid);
		    return;
                }
		*/
                if(node->amcast->msgs[j]->gts <=
				node->amcast->msgs[lowest]->gts) {
                    for(int k=0; k<node->amcast->msgs_count; k++) {
                        if(gts_order[k] != j)
                            lowest = j;
                    }
                }
	    }
	    gts_order[i] = lowest;
        }
	//TODO A lot of possible improvements in the delivery pattern
	for(int *i = gts_order; i<gts_order + node->amcast->msgs_count; i++) {
            if(node->amcast->msgs[*i]->phase == COMMITTED
               && node->amcast->msgs[*i]->delivered == FALSE) {
	        for(int j=0; j<node->amcast->msgs_count; j++) {
                    if(node->amcast->msgs[j]->lts < node->amcast->msgs[*i]->gts
                       && node->amcast->msgs[j]->phase != COMMITTED)
                    return;
                }
                node->amcast->msgs[*i]->delivered = TRUE;
                //TODO Invok some deliver callback
                struct enveloppe rep = {
	            .sid = node->id,
	            .cmd_type = DELIVER,
	            .cmd.deliver = {
	                .mid = *i,
		        .ballot = node->amcast->ballot,
		        .lts = node->amcast->msgs[*i]->lts,
		        .gts = node->amcast->msgs[*i]->gts
	            },
	        };
                send_to_group(node, &rep, node->comm->groups[node->id]);
                for(int i=0; i<256; i++) {
                    accept_acks_per_group_count[i] = 0;
                    accept_acks_per_node_count[i] = 0;
	        }
            }
        }
    }
}

static void handle_deliver(struct node *node, xid_t sid, deliver_t *cmd) {
    printf("[%u] We got DELIVER command from %u for message %u!\n", node->id, sid, cmd->mid);
    if (node->amcast->status == FOLLOWER
            && node->amcast->ballot == cmd->ballot
            && node->amcast->msgs[cmd->mid]->delivered == FALSE) {
        node->amcast->msgs[cmd->mid]->lts = cmd->lts;
        node->amcast->msgs[cmd->mid]->gts = cmd->gts;
        if(node->amcast->clock < node->amcast->msgs[cmd->mid]->gts)
            node->amcast->clock = node->amcast->msgs[cmd->mid]->gts;
        node->amcast->msgs[cmd->mid]->delivered = TRUE;
	//TODO Invok some deliver callback
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

static struct amcast_msg_proposal *init_amcast_msg_proposal() {
    struct amcast_msg_proposal *prop = malloc(sizeof(struct amcast_msg_proposal));
    prop->ballot = -1;
    prop->status = UNDEF;
    prop->lts = -1;
    return prop;
}

static struct amcast_msg *init_amcast_msg(unsigned int groups_count, message_t *cmd) {
    struct amcast_msg *msg = malloc(sizeof(struct amcast_msg));
    msg->phase = START;
    msg->lts = -1;
    msg->gts = -1;
    msg->delivered = FALSE;
    msg->msg = *cmd;
    msg->proposals_count = groups_count;
    msg->proposals = malloc(sizeof(struct amcast_msg_proposals *) * groups_count);
    for(int i=0; i<groups_count; i++)
        msg->proposals[i] = init_amcast_msg_proposal();
    return msg;
}

struct amcast *amcast_init() {
    struct amcast *amcast = malloc(sizeof(struct amcast));
    amcast->status = INIT;
    amcast->ballot = -1;
    amcast->aballot = -1;
    amcast->clock = 0;
    amcast->msgs_count = 0;
    amcast->msgs = NULL;
    return amcast;
}

static int free_amcast_msg_proposal(struct amcast_msg_proposal *prop) {
    free(prop);
    return 0;
}

static int free_amcast_msg(struct amcast_msg *msg) {
    struct amcast_msg_proposal **prop;
    for(prop = msg->proposals; prop < msg->proposals + msg->proposals_count; prop++)
        if(*prop)
            free_amcast_msg_proposal(*prop);
    free(msg);
    return 0;
}

int amcast_free(struct amcast *amcast) {
    for(struct amcast_msg **msg = amcast->msgs; msg < amcast->msgs + amcast->msgs_count; msg++)
        if(*msg)
            free_amcast_msg(*msg);
    free(amcast);
    return 0;
}
