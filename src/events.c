#include <node.h>
#include <events.h>

int connect_to_node(struct event_base *base, struct node_comm *comm, id_t peer_id) {
    //Create a new bufferevent
    comm->bevs[peer_id] = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
    struct bufferevent *bev = comm->bevs[peer_id];
    //TODO Pass the node_id as a callback parameter to identify msg sender
    bufferevent_setcb(bev, read_cb, NULL, event_cb, comm->ids+peer_id);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
    bufferevent_socket_connect(bev, (struct sockaddr *)comm->addrs+peer_id,
        sizeof(comm->addrs[peer_id]));
    return 0;
}
