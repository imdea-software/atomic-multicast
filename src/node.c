#include <arpa/inet.h>
#include <string.h>
#include <signal.h>

#include "node.h"
#include "events.h"


//TODO Do the proper security checks on system calls

static struct node_comm *init_node_comm(struct cluster_config *conf) {
    unsigned int size = conf->size;

    struct node_comm *comm = malloc(sizeof(struct node_comm));
    struct sockaddr_in *addrs = malloc(size * sizeof(struct sockaddr_in));
    id_t *ids = malloc(size * sizeof(id_t));
    id_t *groups = malloc(size * sizeof(id_t));
    //TODO It might be better to realloc when connection is accepted/lost
    struct bufferevent **bevs = malloc(size * sizeof(struct bufferevent *));

    for(int i=0; i<size; i++) {
	id_t c_id = conf->id[i];
	//Prepare the sockaddr_in structs for each node
	memset(addrs+c_id, 0, sizeof(struct sockaddr_in));
        addrs[c_id].sin_family = AF_INET;
        addrs[c_id].sin_port = htons(conf->ports[c_id]);
	inet_aton(conf->addresses[c_id], &(addrs[c_id].sin_addr));
    }
    //TODO Create a dedicated group structure
    memcpy(groups, conf->group_membership, size * sizeof(id_t));
    memcpy(ids, conf->id, size * sizeof(id_t));

    comm->cluster_size = size;
    comm->accepted_count = 0;
    comm->a_size = 0;
    comm->addrs = addrs;
    comm->ids = ids;
    comm->groups = groups;
    comm->bevs = bevs;
    comm->a_bevs = NULL;

    return comm;
};

static int free_node_comm(struct node_comm *comm) {
    //Every remaining bev have to be freed manually
    //If already freed elsewhere, the pointer was set to NULL
    for(struct bufferevent **bev = comm->bevs; bev< comm->bevs + comm->cluster_size; bev++)
	if(*bev)
	    bufferevent_free(*bev);
    for(struct bufferevent **bev = comm->a_bevs; bev< comm->a_bevs + comm->a_size; bev++)
	if(*bev)
	    bufferevent_free(*bev);
    free(comm->a_bevs);
    free(comm->bevs);
    free(comm->ids);
    free(comm->groups);
    free(comm->addrs);
    free(comm);
    return 0;
}

static struct node_events *init_node_events(struct node_comm *comm, id_t id) {
    struct node_events *events = malloc(sizeof(struct node_events));
    //Create a new event base
    events->base = event_base_new();
    //Create a listener for incomming connections
    events->lev = evconnlistener_new_bind(events->base, NULL, NULL,
		    LEV_OPT_CLOSE_ON_EXEC | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
		    -1, (struct sockaddr*) comm->addrs+id, sizeof(comm->addrs[id]));
    //Create an array of reconnection events
    events->reconnect_evs = malloc(comm->cluster_size * sizeof(struct event *));
    return events;
}

static int configure_node_events(struct node *node) {
    //Set the listener callbacks to make it active
    evconnlistener_set_cb(node->events->lev, accept_conn_cb, node);
    evconnlistener_set_error_cb(node->events->lev, accept_error_cb);
    //Connect to the other nodes of the cluster
    //TODO Maybe re-add the event when BEV_EVENT_EOF|ERROR to reconnect when lost
    for(int i=0; i<node->comm->cluster_size; i++) {
        id_t peer_id = node->comm->ids[i];
        node->events->reconnect_evs[peer_id] = evtimer_new(node->events->base, reconnect_cb,
			set_cb_arg(peer_id, node));
        connect_to_node(node, peer_id);
    }
    //Set-up a signal event to exit the event-loop
    //TODO Add some protection to prevent use in case of multithreaded context
    node->events->interrupt_ev = evsignal_new(node->events->base, SIGHUP, interrupt_cb,
                    event_self_cbarg());
    event_add(node->events->interrupt_ev, NULL);
    return 0;
}

static int free_node_events(struct node_events *events) {
    event_free(events->interrupt_ev);
    evconnlistener_free(events->lev);
    event_base_free(events->base);
    free(events->reconnect_evs);
    free(events);
    return 0;
}

struct node *node_init(struct cluster_config *conf, id_t id) {
    struct node *node = malloc(sizeof(struct node));
    node->id = id;
    node->comm = init_node_comm(conf);
    node->events = init_node_events(node->comm, id);
    return node;
}

int node_free(struct node *node) {
    //TODO Find a better place to free the events
    for(int i=0; i<node->comm->cluster_size; i++)
        event_free(node->events->reconnect_evs[i]);
    free_node_comm(node->comm);
    free_node_events(node->events);
    free(node);
    return 0;
}

void node_start(struct node *node) {
    configure_node_events(node);
    //event_base_dump_events(node->events->base, stdout);
    event_base_dispatch(node->events->base);
}

void node_stop(struct node *node) {
    event_base_loopexit(node->events->base, NULL);
}
