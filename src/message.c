#include <event2/bufferevent.h>

#include "node.h"
#include "message.h"

//TODO Properly implement de-serialization of the enveloppe and its content
void read_enveloppe(struct bufferevent *bev, struct enveloppe *env) {
    bufferevent_read(bev, env, sizeof(struct enveloppe));
}

//TODO Properly implement serialization of the enveloppe and its content
void write_enveloppe(struct bufferevent *bev, struct enveloppe *env) {
    bufferevent_write(bev, env, sizeof(struct enveloppe));
}

void send_to_peer(struct node *node, struct enveloppe *env, id_t peer_id) {
    write_enveloppe(node->comm->bevs[peer_id], env);
}

//TODO Add a decent groups structure so that looping over all nodes is not required
void send_to_group(struct node *node, struct enveloppe *env, id_t group_id) {
    for(id_t *grp = node->comm->groups; grp < node->comm->groups + node->comm->cluster_size; grp++)
        if (*grp == group_id)
            send_to_peer(node, env, grp - node->comm->groups);
}

void send_to_destgrps(struct node *node, struct enveloppe *env, id_t *destgrps, unsigned int count) {
    for(id_t *grp = destgrps; grp < destgrps+count; grp++)
        send_to_group(node, env, *grp);
}
