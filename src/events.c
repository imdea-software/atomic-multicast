#include <string.h>

#include "node.h"
#include "events.h"
#include "message.h"

static struct timeval reconnect_timeout = { 1, 0 };

struct cb_arg *set_cb_arg(id_t peer_id, struct node *node) {
    struct cb_arg *arg = malloc(sizeof(struct cb_arg));
    arg->peer_id = peer_id;
    arg->node = node;
    return arg;
}
int retrieve_cb_arg(id_t *peer_id, struct node **node, struct cb_arg *arg) {
    *peer_id = arg->peer_id;
    *node = arg->node;
    //TODO Change this broken design: either store all the cb_arg struct and
    //     alloc/dealloc when a connection is established.
    //     Or change how a peer is represented so this cb_arg struct is not needed.
    //free(arg);
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
    bufferevent_setcb(bev, read_cb, NULL, event_cb, set_cb_arg(peer_id, node));
    bufferevent_enable(bev, EV_READ|EV_WRITE);
    bufferevent_socket_connect(bev, (struct sockaddr *)node->comm->addrs+peer_id,
        sizeof(node->comm->addrs[peer_id]));
    return 0;
}

static int close_connection(struct node *node, id_t peer_id) {
    bufferevent_free(node->comm->bevs[peer_id]);
    node->comm->bevs[peer_id] = NULL;
    return 0;
}

//Called when the status of a connection changes
//TODO Find something useful to do in there
static void event_a_cb(struct bufferevent *bev, short events, void *ptr) {
    struct node *node = NULL; id_t a_id;
    retrieve_cb_arg(&a_id, &node, (struct cb_arg *) ptr);

    if (events & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
        printf("[%u] Connection lost to %u-th accepted\n", node->id, a_id);
	/*
	 *if (node->comm->a_bevs[a_id])
	 *   bufferevent_free(node->comm->a_bevs[a_id]);
         *node->comm->a_bevs[a_id] = NULL;
	 */
	//node->comm->a_size --;
    } else {
        printf("[%u] Event %d not handled", node->id, events);
    }
}

// CALLBACKS IMPLEMENTATION

//Called after accepting a connection, currently, create and
//store a separate bufferevent for the accepted connections
//--> bad design, two TCP connections open for a p2p communication
//--> but very simple and needed (closing the sock upon exit to generate EOF on the other end)
//TODO Since there is no way to tell from whom the accepted connection comes,
//     a protocol extension is needed.
//TODO Instead of creating a new bufferevent, just replace its underlying socket with
//     the one from the accepted connection.
void accept_conn_cb(struct evconnlistener *lev, evutil_socket_t sock,
		struct sockaddr *addr, int len, void *ptr) {
    struct node *node = (struct node *) ptr;
    node->comm->a_size += 1;
    //Dynamically adjust the storage for the buffervents
    if (!node->comm->a_bevs)
        node->comm->a_bevs = malloc(sizeof(struct bufferevent *));
    else
        node->comm->a_bevs =
            realloc(node->comm->a_bevs, sizeof(struct bufferevent *) * node->comm->a_size);
    //Create a new bev & adjust its socket
    node->comm->a_bevs[node->comm->a_size - 1] = bufferevent_socket_new(node->events->base,
		   -1, BEV_OPT_CLOSE_ON_FREE);
    struct bufferevent *bev = node->comm->a_bevs[node->comm->a_size - 1];
    bufferevent_setfd(bev, sock);
    //TODO Do not mess further with the bevs, they already should be correctly set from the connect loop
    bufferevent_setcb(bev, read_cb, NULL, event_a_cb, set_cb_arg(node->comm->a_size, node));
    bufferevent_enable(bev, EV_READ|EV_WRITE);
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
    struct node *node = NULL; id_t peer_id;
    retrieve_cb_arg(&peer_id, &node, (struct cb_arg *) ptr);
    //TODO Implement message reception
    struct enveloppe env;
    read_enveloppe(bev, &env);
    //TODO Have a dedicated cmd_type for receive tests
    write_enveloppe(bev, &env);
    dispatch_message(node, &env);
}

//Called when the status of a connection changes
void event_cb(struct bufferevent *bev, short events, void *ptr) {
    struct node *node = NULL; id_t peer_id;
    retrieve_cb_arg(&peer_id, &node, (struct cb_arg *) ptr);

    if (events & BEV_EVENT_CONNECTED) {
        printf("[%u] Connection established to node %u\n", node->id, peer_id);
        node->comm->accepted_count += 1;
    } else if (events & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
        printf("[%u] Connection lost to node %u\n", node->id, peer_id);
	if (events & BEV_EVENT_EOF)
            node->comm->accepted_count -= 1;
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

void interrupt_cb(evutil_socket_t sock, short flags, void *ptr) {
    struct event *interrupt_ev = (struct event *) ptr;
    struct event_base *base = event_get_base(interrupt_ev);

    event_del(interrupt_ev);
    event_base_loopexit(base, NULL);
}
