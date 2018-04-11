#include "message.h"
#include "amcast.h"


static void handle_multicast(struct node *node, id_t sid, message_t *cmd) {
    printf("[%u] We got MULTICAST command from %u!\n", node->id, sid);
}

static void handle_accept(struct node *node, id_t sid, accept_t *cmd) {
    printf("[%u] We got ACCEPT command from %u!\n", node->id, sid);
}

static void handle_accept_ack(struct node *node, id_t sid, accept_ack_t *cmd) {
    printf("[%u] We got ACCEPT_ACK command from %u!\n", node->id, sid);
}

static void handle_deliver(struct node *node, id_t sid, deliver_t *cmd) {
    printf("[%u] We got DELIVER command from %u!\n", node->id, sid);
}

static void handle_newleader(struct node *node, id_t sid, newleader_t *cmd) {
    printf("[%u] We got NEWLEADER command from %u!\n", node->id, sid);
}

static void handle_newleader_ack(struct node *node, id_t sid, newleader_ack_t *cmd) {
    printf("[%u] We got NEWLEADER_ACK command from %u!\n", node->id, sid);
}

static void handle_newleader_sync(struct node *node, id_t sid, newleader_sync_t *cmd) {
    printf("[%u] We got NEWLEADER_SYNC command from %u!\n", node->id, sid);
}

static void handle_newleader_sync_ack(struct node *node, id_t sid, newleader_sync_ack_t *cmd) {
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

static struct amcast_msg *init_amcast_msg(unsigned int groups_count) {
    struct amcast_msg *msg = malloc(sizeof(struct amcast_msg));
    msg->phase = START;
    msg->lts = -1;
    msg->gts = -1;
    msg->delivered = FALSE;
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
