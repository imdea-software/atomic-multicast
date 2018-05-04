#ifndef _NODE_H_
#define _NODE_H_

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>

#include "types.h"
#include "cluster.h"
//TODO fix: do not compile when adding this include
//#include "amcast.h"

struct groups {
    unsigned int	groups_count;
    unsigned int	*node_counts;
    id_t		**members;
    //NOT USED
    //id_t		*memberships;
};

struct node_comm {
    unsigned int	cluster_size;
    unsigned int	accepted_count;
    unsigned int	a_size;
    xid_t		*ids;
    xid_t 		*groups;
    struct sockaddr_in	*addrs;
    struct bufferevent  **bevs;
    struct bufferevent	**a_bevs;
};

struct node_events {
    struct event_base	*base;
    struct evconnlistener *lev;
    struct event	*interrupt_ev;
    struct event	**reconnect_evs;
};

struct node {
    xid_t		id;
    struct groups	*groups;
    struct node_comm	*comm;
    struct node_events	*events;
    struct amcast	*amcast;
};

typedef void (*delivery_cb_fun)(struct node *node, m_uid_t mid);

struct 	node 	*node_init	(struct cluster_config *conf, xid_t id);
int 		node_free	(struct node *node);
void		node_start	(struct node *node);
void		node_stop	(struct node *node);

#endif
