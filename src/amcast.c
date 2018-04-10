#include "message.h"
#include "amcast.h"


static void handle_multicast(struct node *node, id_t sid, message_t cmd) {
    printf("[%u] We got MULTICAST command from %u!\n", node->id, sid);
}

static void handle_accept(struct node *node, id_t sid, accept_t cmd) {
    printf("[%u] We got ACCEPT command from %u!\n", node->id, sid);
}

static void handle_accept_ack(struct node *node, id_t sid, accept_ack_t cmd) {
    printf("[%u] We got ACCEPT_ACK command from %u!\n", node->id, sid);
}

static void handle_deliver(struct node *node, id_t sid, deliver_t cmd) {
    printf("[%u] We got DELIVER command from %u!\n", node->id, sid);
}

static void handle_newleader(struct node *node, id_t sid, newleader_t cmd) {
    printf("[%u] We got NEWLEADER command from %u!\n", node->id, sid);
}

static void handle_newleader_ack(struct node *node, id_t sid, newleader_ack_t cmd) {
    printf("[%u] We got NEWLEADER_ACK command from %u!\n", node->id, sid);
}

static void handle_newleader_sync(struct node *node, id_t sid, newleader_sync_t cmd) {
    printf("[%u] We got NEWLEADER_SYNC command from %u!\n", node->id, sid);
}

static void handle_newleader_sync_ack(struct node *node, id_t sid, newleader_sync_ack_t cmd) {
    printf("[%u] We got NEWLEADER_SYNC_ACK command from %u!\n", node->id, sid);
}

void dispatch_amcast_command(struct node *node, struct enveloppe *env) {
    switch(env->cmd_type) {
        case MULTICAST:
            handle_multicast(node, env->sid, env->cmd.multicast);
            break;
        case ACCEPT:
            handle_accept(node, env->sid, env->cmd.accept);
            break;
        case ACCEPT_ACK:
            handle_accept_ack(node, env->sid, env->cmd.accept_ack);
            break;
        case DELIVER:
            handle_deliver(node, env->sid, env->cmd.deliver);
            break;
        case NEWLEADER:
            handle_newleader(node, env->sid, env->cmd.newleader);
            break;
        case NEWLEADER_ACK:
            handle_newleader_ack(node, env->sid, env->cmd.newleader_ack);
            break;
        case NEWLEADER_SYNC:
            handle_newleader_sync(node, env->sid, env->cmd.newleader_sync);
            break;
	case NEWLEADER_SYNC_ACK:
            handle_newleader_sync_ack(node, env->sid, env->cmd.newleader_sync_ack);
            break;
	default:
            printf("[%u] Unhandled command received from %u\n", node->id, env->sid);
            break;
    }
}
