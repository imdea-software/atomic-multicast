#include <event2/bufferevent.h>

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
void read_enveloppe(struct bufferevent *bev, struct enveloppe *env) {
    bufferevent_read(bev, env, sizeof(struct enveloppe));
}

//TODO Properly implement serialization of the enveloppe and its content
void write_enveloppe(struct bufferevent *bev, struct enveloppe *env) {
    bufferevent_write(bev, env, sizeof(struct enveloppe));
}

void send_to_peer(struct node *node, struct enveloppe *env, xid_t peer_id) {
    write_enveloppe(node->comm->bevs[peer_id], env);
}

void send_to_client(struct node *node, struct enveloppe *env, xid_t client_id) {
    xid_t peer_id = node->comm->cluster_size * 2 + client_id;
    struct bufferevent *bev;
    if( peer_id < node->comm->bevs_size && (bev = node->comm->bevs[peer_id]) != NULL )
        write_enveloppe(bev, env);
}

//TODO Add a decent groups structure so that looping over all nodes is not required
void send_to_group(struct node *node, struct enveloppe *env, xid_t group_id) {
    for(xid_t *peer = node->groups->members[group_id];
            peer < node->groups->members[group_id] + node->groups->node_counts[group_id]; peer++)
        send_to_peer(node, env, *peer);
}

void send_to_destgrps(struct node *node, struct enveloppe *env, xid_t *destgrps, unsigned int count) {
    for(xid_t *grp = destgrps; grp < destgrps+count; grp++)
        send_to_group(node, env, *grp);
}
