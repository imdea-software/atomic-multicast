#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>

#include "types.h"
#include "cluster.h"

struct node_comm {
    unsigned int	cluster_size;
    id_t 		*groups;
    struct sockaddr_in	*addrs;
};

struct node_events {
    struct event_base	*base;
    struct evconnlistener *lev;
};

struct node {
    id_t		id;
    struct node_comm	*comm;
    struct node_events	*events;
};

struct 	node 	*node_init	(struct cluster_config *conf, id_t id);
int 		node_free	(struct node *node);
