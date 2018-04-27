#ifndef _EVENTS_H_
#define _EVENTS_H_

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>

#include "types.h"
#include "node.h"


struct cb_arg {
    xid_t	peer_id;
    struct node	*node;
};

struct cb_arg *set_cb_arg(xid_t peer_id, struct node *node);
int retrieve_cb_arg(xid_t *peer_id, struct node **node, struct cb_arg *arg);

int connect_to_node(struct node *node, xid_t peer_id);

void accept_conn_cb(struct evconnlistener *lev,
		evutil_socket_t sock, struct sockaddr *addr, int len, void *ptr);
void accept_error_cb(struct evconnlistener *lev, void *ptr);
void reconnect_cb(evutil_socket_t sock, short events, void *ptr);
void interrupt_cb(evutil_socket_t sock, short events, void *ptr);
void read_cb(struct bufferevent *bev, void *ptr);
void event_cb(struct bufferevent *bev, short flags, void *ptr);

#endif
