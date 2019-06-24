#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "node.h"
#include "message.h"
#include "amcast.h"


//TODO Add some non-multicast specific message types, e.g. discovery protocol
void dispatch_message(struct node *node, struct enveloppe *env) {
    switch(env->cmd_type) {
        default:
            dispatch_amcast_command(node, env);
            break;
    }
}

//TODO Properly implement de-serialization of the enveloppe and its content
int read_enveloppe(struct bufferevent *bev, struct enveloppe *env) {
    struct evbuffer *ev_in = bufferevent_get_input(bev);
    size_t msg_size = sizeof(struct enveloppe);

    evbuffer_copyout(ev_in, env, msg_size);
    switch(env->cmd_type) {
        case NEWLEADER_ACK:
            msg_size += env->cmd.newleader_ack.msg_count * sizeof(msgstate_t);
            if(evbuffer_get_length(ev_in) < msg_size) {
                bufferevent_setwatermark(bev, EV_READ, msg_size, 0);
                return 0;
            }
            bufferevent_setwatermark(bev, EV_READ, sizeof(struct enveloppe), 0);
            evbuffer_drain(ev_in, sizeof(struct enveloppe));
            //TODO Avoid copying data out of in_evbuffer
            //TODO memleak here, this is never freed
            env->cmd.newleader_ack.messages = malloc(env->cmd.newleader_ack.msg_count * sizeof(msgstate_t));
            evbuffer_remove(ev_in, env->cmd.newleader_ack.messages, env->cmd.newleader_ack.msg_count * sizeof(msgstate_t));
            break;
        case NEWLEADER_SYNC:
            msg_size += env->cmd.newleader_sync.msg_count * sizeof(msgstate_t);
            if(evbuffer_get_length(ev_in) < msg_size) {
                bufferevent_setwatermark(bev, EV_READ, msg_size, 0);
                return 0;
            }
            bufferevent_setwatermark(bev, EV_READ, sizeof(struct enveloppe), 0);
            evbuffer_drain(ev_in, sizeof(struct enveloppe));
            //TODO Avoid copying data out of in_evbuffer
            //TODO memleak here, this is never freed
            env->cmd.newleader_sync.messages = malloc(env->cmd.newleader_sync.msg_count * sizeof(msgstate_t));
            evbuffer_remove(ev_in, env->cmd.newleader_sync.messages, env->cmd.newleader_sync.msg_count * sizeof(msgstate_t));
            break;
        default:
            evbuffer_drain(ev_in, msg_size);
            break;
    }
    return 1;
}

static void cleanup(const void *data, size_t len, void *arg) {
    free((void *) data);
}

//TODO Properly implement serialization of the enveloppe and its content
void write_enveloppe(struct bufferevent *bev, struct enveloppe *env) {
    struct evbuffer *ev_out = bufferevent_get_output(bev);
    evbuffer_add(ev_out, env, sizeof(struct enveloppe));
    switch(env->cmd_type) {
        case NEWLEADER_ACK:
            evbuffer_add_reference(ev_out, env->cmd.newleader_ack.messages,
                    env->cmd.newleader_ack.msg_count * sizeof(msgstate_t), cleanup, NULL);
            break;
        case NEWLEADER_SYNC:
            evbuffer_add_reference(ev_out, env->cmd.newleader_sync.messages,
                    env->cmd.newleader_sync.msg_count * sizeof(msgstate_t), cleanup, NULL);
            break;
        default:
            break;
    }
}

void send_to_peer(struct node *node, struct enveloppe *env, xid_t peer_id) {
    struct bufferevent *bev;
    if( peer_id < node->comm->cluster_size && (bev = node->comm->bevs[peer_id]) != NULL
            && (bufferevent_get_enabled(bev) & EV_WRITE))
        write_enveloppe(bev, env);
}

void send_to_client(struct node *node, struct enveloppe *env, xid_t client_id) {
    xid_t peer_id = node->comm->cluster_size * 2 + client_id;
    struct bufferevent *bev;
    if( peer_id < node->comm->bevs_size && (bev = node->comm->bevs[peer_id]) != NULL
            && (bufferevent_get_enabled(bev) & EV_WRITE))
        write_enveloppe(bev, env);
}

//TODO Add a decent groups structure so that looping over all nodes is not required
void send_to_group(struct node *node, struct enveloppe *env, xid_t group_id) {
    for(xid_t *peer = node->groups->members[group_id];
            peer < node->groups->members[group_id] + node->groups->node_counts[group_id]; peer++)
        send_to_peer(node, env, *peer);
}

void send_to_group_except_me(struct node *node, struct enveloppe *env, xid_t group_id) {
    for(xid_t *peer = node->groups->members[group_id];
            peer < node->groups->members[group_id] + node->groups->node_counts[group_id]; peer++)
        if(*peer != node->id)
            send_to_peer(node, env, *peer);
}

void send_to_destgrps(struct node *node, struct enveloppe *env, xid_t *destgrps, unsigned int count) {
    for(xid_t *grp = destgrps; grp < destgrps+count; grp++)
        send_to_group(node, env, *grp);
}
