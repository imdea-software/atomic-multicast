#include <string.h>
#include <event2/buffer.h>

#include "node.h"
#include "events.h"
#include "message.h"
#include "amcast.h"

#define __extend_array(base_ptr, size_ptr, elem_size, new_index) do { \
    unsigned int __nsize = *(size_ptr) * 2; \
    (base_ptr) = realloc((base_ptr), (elem_size) * (((new_index) < __nsize) ? \
             __nsize : (__nsize = (new_index) + 1))); \
    memset((base_ptr) + *(size_ptr), 0, (__nsize - *(size_ptr)) * (elem_size)); \
    *(size_ptr) = __nsize; \
} while(0)

static struct timeval reconnect_timeout = { 1, 0 };

struct cb_arg *set_cb_arg(xid_t peer_id, struct node *node) {
    if(node->events->ev_cb_arg[peer_id] == NULL) {
	node->events->ev_cb_arg[peer_id] = malloc(sizeof(struct cb_arg));
	node->events->ev_cb_arg[peer_id]->peer_id = peer_id;
	node->events->ev_cb_arg[peer_id]->node = node;
    }
    return node->events->ev_cb_arg[peer_id];
}
int retrieve_cb_arg(xid_t *peer_id, struct node **node, struct cb_arg *arg) {
    *peer_id = arg->peer_id;
    *node = arg->node;
    return 0;
}

int connect_to_node(struct node *node, xid_t peer_id) {
    event_add(node->events->reconnect_evs[peer_id], &reconnect_timeout);
    return 0;
}

// STATIC FUNCTIONS

static int init_connection(struct node *node, xid_t peer_id) {
    struct bufferevent *bev = bufferevent_socket_new(node->events->base,
		   -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, read_cb, NULL, event_cb, set_cb_arg(peer_id, node));
    bufferevent_setwatermark(bev, EV_READ, sizeof(struct enveloppe), 0);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
    bufferevent_socket_connect(bev, (struct sockaddr *)node->comm->addrs+peer_id,
        sizeof(node->comm->addrs[peer_id]));
    node->comm->bevs[peer_id] = bev;
    return 0;
}

static int close_connection(struct node *node, xid_t peer_id) {
    struct bufferevent **bev = node->comm->bevs+peer_id;
    if(*bev) {
        if(evbuffer_get_length(bufferevent_get_input(*bev)))
            read_cb(*bev, set_cb_arg(peer_id, node));
        if(evbuffer_get_length(bufferevent_get_output(*bev))) {
            bufferevent_setcb(*bev, NULL, close_cb, event_cb, set_cb_arg(peer_id, node));
            bufferevent_disable(*bev, EV_READ);
        } else
            close_cb(*bev, NULL);
	*bev = NULL;
    }
    return 0;
}

//Called when the status of a connection changes
static void event_a_cb(struct bufferevent *bev, short events, void *ptr) {
    struct node *node = NULL; xid_t peer_id;
    retrieve_cb_arg(&peer_id, &node, (struct cb_arg *) ptr);

    if (events & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
        printf("[%u] Connection lost to %u-th accepted\n", node->id, peer_id);
        node->comm->accepted_count--;
        close_connection(node, peer_id);
    } else {
        printf("[%u] Event %d not handled", node->id, events);
    }
}

void read_a_cb(struct bufferevent *bev, void *ptr) {
    struct node *node = (struct node *) ptr;
    xid_t peer_id = -1;

    struct enveloppe env;
    read_enveloppe(bev, &env);
    switch(env.cmd_type) {
        case INIT_CLIENT:
            peer_id = node->comm->cluster_size * 2 + env.sid;
            if(peer_id >= node->comm->bevs_size)
                __extend_array(node->comm->bevs, &node->comm->bevs_size,
                        sizeof(struct bufferevent *), peer_id);
            if(peer_id >= node->events->ev_cb_arg_count)
                __extend_array(node->events->ev_cb_arg, &node->events->ev_cb_arg_count,
                        sizeof(struct cb_arg *), peer_id);
            bufferevent_enable(bev, EV_WRITE);
            break;
        case INIT_NODE:
            peer_id = node->comm->cluster_size + env.sid;
            break;
        default:
            break;
    }
    if(peer_id >= 0) {
        node->comm->bevs[peer_id] = bev;
        node->comm->accepted_count++;
        bufferevent_setcb(bev, read_cb, NULL, event_a_cb, set_cb_arg(peer_id, node));
        if(node->comm->connected_count < node->comm->cluster_size)
            bufferevent_disable(bev, EV_READ);
        else
            bufferevent_trigger(bev, EV_READ, 0);
    }
}

// CALLBACKS IMPLEMENTATION

//Called after accepting a connection
//  TODO It seems that using 2 TCP connections for p2p is expected in libevent.
void accept_conn_cb(struct evconnlistener *lev, evutil_socket_t sock,
		struct sockaddr *addr, int len, void *ptr) {
    struct node *node = (struct node *) ptr;

    struct bufferevent *bev = bufferevent_socket_new(node->events->base, sock, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, read_a_cb, NULL, event_a_cb, node);
    bufferevent_setwatermark(bev, EV_READ, sizeof(struct enveloppe), 0);
    bufferevent_enable(bev, EV_READ);
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
    struct node *node = NULL; xid_t peer_id;
    retrieve_cb_arg(&peer_id, &node, (struct cb_arg *) ptr);
    //TODO Change read_enveloppe() implem so that looping over it
    //     doesn't cause bufferevent_read() to be called several times
    struct evbuffer *in_buf = bufferevent_get_input(bev);
    while (evbuffer_get_length(in_buf) >= sizeof(struct enveloppe)) {
        struct enveloppe env;
        read_enveloppe(bev, &env);
        switch(env.cmd_type) {
            case TESTREPLY:
                write_enveloppe(bev, &env);
                break;
            default:
                dispatch_message(node, &env);
                break;
        }
    }
}

//Called when the status of a connection changes
void event_cb(struct bufferevent *bev, short events, void *ptr) {
    struct node *node = NULL; xid_t peer_id;
    retrieve_cb_arg(&peer_id, &node, (struct cb_arg *) ptr);

    if (events & BEV_EVENT_CONNECTED) {
        printf("[%u] Connection established to node %u\n", node->id, peer_id);
        node->comm->connected_count++;
        struct enveloppe init = { .sid = node->id, .cmd_type = INIT_NODE };
        write_enveloppe(bev, &init);
    } else if (events & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
        printf("[%u] Connection lost to node %u\n", node->id, peer_id);
        node->comm->connected_count--;
        close_connection(node, peer_id);
        //Start recover routine
        failure_cb(0, 0, ptr);
        //event_base_once(node->events->base, -1, EV_TIMEOUT, failure_cb, ptr, &reconnect_timeout);
        //TODO Have nodes tell each other when they exit normally
        //     so we can have a smarter reconnect pattern
        //connect_to_node(node, peer_id);
    } else {
        printf("[%u] Event %d not handled", node->id, events);
    }
    if(node->comm->connected_count == node->comm->cluster_size
            && node->comm->accepted_count > 0) {
        struct bufferevent **bev = node->comm->bevs + node->comm->cluster_size;
        unsigned int checked = 0;
        while(bev < node->comm->bevs + node->comm->bevs_size
                && checked < node->comm->accepted_count) {
            if(*bev) {
                bufferevent_enable(*bev,
                        (!(bufferevent_get_enabled(*bev) & EV_READ)) ? EV_READ : 0);
                bufferevent_trigger(*bev, EV_READ, 0);
                checked++;
            }
            bev++;
        }
    }
}

//Called after last write of a connection
void close_cb(struct bufferevent *bev, void *ptr) {
    bufferevent_free(bev);
}

void failure_cb(evutil_socket_t sock, short flags, void *ptr) {
    struct node *node = NULL; xid_t peer_id;
    retrieve_cb_arg(&peer_id, &node, (struct cb_arg *) ptr);

    //run recover routine after node failed to reconnect for too long
    if(node->comm->groups[peer_id] == node->comm->groups[node->id])
        if(!node->comm->bevs[peer_id])
            amcast_recover(node, peer_id);
}

void reconnect_cb(evutil_socket_t sock, short flags, void *ptr) {
    struct node *node = NULL; xid_t peer_id;
    retrieve_cb_arg(&peer_id, &node, (struct cb_arg *) ptr);

    init_connection(node, peer_id);
}

void interrupt_cb(evutil_socket_t sock, short flags, void *ptr) {
    struct event *interrupt_ev = (struct event *) ptr;
    struct event_base *base = event_get_base(interrupt_ev);

    event_del(interrupt_ev);
    event_base_loopexit(base, NULL);
}

void termination_cb(evutil_socket_t sock, short flags, void *ptr) {
    struct node *node = (struct node *) ptr;

    event_del(node->events->interrupt_ev);
    event_del(node->events->termination_ev);
    evconnlistener_disable(node->events->lev);
    for(int peer_id=0; peer_id< node->comm->cluster_size; peer_id++)
        close_connection(node, peer_id);
}
