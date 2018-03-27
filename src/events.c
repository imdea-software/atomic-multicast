#include <string.h>

#include "node.h"
#include "events.h"

static struct timeval reconnect_timeout = { 0, 0 };

struct cb_arg *set_cb_arg(id_t peer_id, struct node *node) {
    struct cb_arg *arg = malloc(sizeof(struct cb_arg));
    arg->peer_id = peer_id;
    arg->node = node;
    return arg;
}
int retrieve_cb_arg(id_t *peer_id, struct node **node, struct cb_arg *arg) {
    *peer_id = arg->peer_id;
    *node = arg->node;
    free(arg);
    return 0;
}

int connect_to_node(struct node *node, id_t peer_id) {
    event_add(node->events->reconnect_evs[peer_id], &reconnect_timeout);
    return 0;
}

// STATIC FUNCTIONS

static int init_connection(struct node *node, id_t peer_id) {
    //Create a new bufferevent
    node->comm->bevs[peer_id] = bufferevent_socket_new(node->events->base,
		   -1, BEV_OPT_CLOSE_ON_FREE);
    struct bufferevent *bev = node->comm->bevs[peer_id];
    //TODO Pass the node_id as a callback parameter to identify msg sender
    bufferevent_setcb(bev, read_cb, NULL, event_cb, set_cb_arg(peer_id, node));
    bufferevent_enable(bev, EV_READ|EV_WRITE);
    bufferevent_socket_connect(bev, (struct sockaddr *)node->comm->addrs+peer_id,
        sizeof(node->comm->addrs[peer_id]));
    return 0;
}

static int close_connection(struct node *node, id_t peer_id) {
    bufferevent_free(node->comm->bevs[peer_id]);
    return 0;
}

// CALLBACKS IMPLEMENTATION

//Called after accepting a connection, currently, get the bufferevent ready to talk
//  Since there is no way to tell from whom the accepted connection comes,
//  a protocol extension is needed
//TODO Store accepted connections from socket & addr to keep track of the current cluster
void accept_conn_cb(struct evconnlistener *lev, evutil_socket_t sock,
		struct sockaddr *addr, int len, void *ptr) {
    //struct node_comm *comm = ptr;
    //struct event_base *base = evconnlistener_get_base(lev);
    //struct bufferevent *bev = comm->bevs[comm->accepted_count++ - 1];
    //printf("Connection accepted %u\n", comm->accepted_count);
    //Do not mess with the bevs, they already should be correctly set from the connect loop
    //bufferevent_setfd(bev, sock);
    //bufferevent_setcb(bev, read_cb, NULL, event_cb, NULL);
    //bufferevent_enable(bev, EV_READ|EV_WRITE);
}

//Called if an accept() call fails, currently, just ends the event loop
void accept_error_cb(struct evconnlistener *lev, void *ptr) {
    int err = EVUTIL_SOCKET_ERROR();
    fprintf(stderr, "Got an error %d (%s) on the listener. "
                "Shutting down.\n", err, evutil_socket_error_to_string(err));
    event_base_loopexit(evconnlistener_get_base(lev), NULL);
};

//Called whenever data gets into the bufferevents
void read_cb(struct bufferevent *bev, void *ptr) {
    //TODO Implement message reception
    puts("We got mail!\n");
}

//Called when the status of a connection changes
void event_cb(struct bufferevent *bev, short events, void *ptr) {
    struct node *node = NULL; id_t peer_id;
    retrieve_cb_arg(&peer_id, &node, (struct cb_arg *) ptr);

    if (events & BEV_EVENT_CONNECTED) {
        printf("[%u] Connection established to node %u\n", node->id, peer_id);
    } else if (events & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
        printf("[%u] Connection lost to node %u\n", node->id, peer_id);
        close_connection(node, peer_id);
        connect_to_node(node, peer_id);
    } else {
        printf("[%u] Event %d not handled", node->id, events);
    }
}

void reconnect_cb(evutil_socket_t sock, short flags, void *ptr) {
    struct node *node = NULL; id_t peer_id;
    retrieve_cb_arg(&peer_id, &node, (struct cb_arg *) ptr);

    init_connection(node, peer_id);
}
