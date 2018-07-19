#include <arpa/inet.h>
#include <string.h>
#include <signal.h>

#include "node.h"
#include "events.h"
#include "amcast.h"


//TODO Do the proper security checks on system calls

static struct groups *init_groups(struct cluster_config *conf) {
    struct groups *g = malloc(sizeof(struct groups));
    g->node_counts = malloc(sizeof(unsigned int) * conf->size);
    g->members = malloc(sizeof(id_t *) * conf->size);
    g->groups_count = conf->groups_count;

    for(int j=0; j<g->groups_count; j++) {
        g->members[j] = malloc(sizeof(id_t) * conf->size);
        g->node_counts[j] = 0;
        for(int i=0; i<conf->size; i++)
            if(conf->group_membership[i] == j) {
                g->node_counts[j] += 1;
                g->members[j][g->node_counts[j] - 1] = conf->id[i];
            }
        g->members[j] = realloc(g->members[j], sizeof(id_t) * g->node_counts[j]);
    }
    g->node_counts = realloc(g->node_counts, sizeof(unsigned int) * g->groups_count);
    g->members = realloc(g->members, sizeof(id_t *) * g->groups_count);
    return g;
}

static int free_groups(struct groups *groups) {
    for(int i=0; i<groups->groups_count; i++)
        free(groups->members[i]);
    free(groups->members);
    free(groups->node_counts);
    free(groups);
    return 0;
}

static struct node_comm *init_node_comm(struct cluster_config *conf) {
    unsigned int size = conf->size;

    struct node_comm *comm = malloc(sizeof(struct node_comm));
    struct sockaddr_in *addrs = malloc(size * sizeof(struct sockaddr_in));
    xid_t *ids = malloc(size * sizeof(xid_t));
    xid_t *groups = malloc(size * sizeof(xid_t));
    //TODO It might be better to realloc when connection is accepted/lost
    struct bufferevent **bevs = malloc(size * sizeof(struct bufferevent *));

    for(int i=0; i<size; i++) {
	xid_t c_id = conf->id[i];
	//Prepare the sockaddr_in structs for each node
	memset(addrs+c_id, 0, sizeof(struct sockaddr_in));
        addrs[c_id].sin_family = AF_INET;
        addrs[c_id].sin_port = htons(conf->ports[c_id]);
	inet_aton(conf->addresses[c_id], &(addrs[c_id].sin_addr));
    }
    //TODO Use the new dedicated group structure
    memcpy(groups, conf->group_membership, size * sizeof(xid_t));
    memcpy(ids, conf->id, size * sizeof(xid_t));

    comm->cluster_size = size;
    comm->accepted_count = 0;
    comm->a_size = 0;
    comm->c_size = 0;
    comm->addrs = addrs;
    comm->ids = ids;
    comm->groups = groups;
    comm->bevs = bevs;
    comm->a_bevs = NULL;
    comm->c_bevs = NULL;

    return comm;
};

static int free_node_comm(struct node_comm *comm) {
    //Every remaining bev have to be freed manually
    //If already freed elsewhere, the pointer was set to NULL
    for(struct bufferevent **bev = comm->bevs; bev< comm->bevs + comm->cluster_size; bev++)
	if(*bev)
	    bufferevent_free(*bev);
    if(comm->a_bevs != NULL) {
        for(struct bufferevent **bev = comm->a_bevs; bev< comm->a_bevs + comm->a_size; bev++)
        if(*bev)
            bufferevent_free(*bev);
        free(comm->a_bevs);
    }
    if(comm->c_bevs != NULL)
        free(comm->c_bevs);
    free(comm->bevs);
    free(comm->ids);
    free(comm->groups);
    free(comm->addrs);
    free(comm);
    return 0;
}

static struct node_events *init_node_events(struct node_comm *comm, xid_t id) {
    struct node_events *events = malloc(sizeof(struct node_events));
    //Create a new event base
    events->base = event_base_new();
    //Create a listener for incomming connections
    events->lev = evconnlistener_new_bind(events->base, NULL, NULL,
		    LEV_OPT_CLOSE_ON_EXEC | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
		    -1, (struct sockaddr*) comm->addrs+id, sizeof(comm->addrs[id]));
    //Create an array of reconnection events
    events->reconnect_evs = malloc(comm->cluster_size * sizeof(struct event *));
    memset(events->reconnect_evs, 0, sizeof(struct event *) * comm->cluster_size);
    events->interrupt_ev = NULL;
    events->termination_ev = NULL;
    //Add ev_cb_arg array
    events->ev_cb_arg_count = comm->cluster_size;
    events->ev_cb_arg = malloc(sizeof(struct cb_arg *) * comm->cluster_size);
    memset(events->ev_cb_arg, 0, sizeof(struct cb_arg *) * events->ev_cb_arg_count);
    return events;
}

static int configure_node_events(struct node *node) {
    //Set the listener callbacks to make it active
    evconnlistener_set_cb(node->events->lev, accept_conn_cb, node);
    evconnlistener_set_error_cb(node->events->lev, accept_error_cb);
    //Connect to the other nodes of the cluster
    //TODO Maybe re-add the event when BEV_EVENT_EOF|ERROR to reconnect when lost
    for(int i=0; i<node->comm->cluster_size; i++) {
        xid_t peer_id = node->comm->ids[i];
        node->events->reconnect_evs[peer_id] = evtimer_new(node->events->base, reconnect_cb,
			set_cb_arg(peer_id, node));
        connect_to_node(node, peer_id);
    }
    //Set-up a signal event to exit the event-loop
    //TODO Add some protection to prevent use in case of multithreaded context
    node->events->interrupt_ev = evsignal_new(node->events->base, SIGINT, interrupt_cb,
                    event_self_cbarg());
    event_add(node->events->interrupt_ev, NULL);
    //Set-up a signal event to gracefully (by starvation) terminate the event-loop
    node->events->termination_ev = evsignal_new(node->events->base, SIGHUP, termination_cb, node);
    event_add(node->events->termination_ev, NULL);
    return 0;
}

static int free_node_events(struct node_events *events) {
    for(struct cb_arg **arg = events->ev_cb_arg; arg < events->ev_cb_arg + events->ev_cb_arg_count; arg++)
        if(*arg != NULL)
            free(*arg);
    free(events->ev_cb_arg);
    if(events->termination_ev)
        event_free(events->termination_ev);
    if(events->interrupt_ev)
        event_free(events->interrupt_ev);
    evconnlistener_free(events->lev);
    event_base_free(events->base);
    free(events->reconnect_evs);
    free(events);
    return 0;
}

struct node *node_init(struct cluster_config *conf, xid_t id, msginit_cb_fun msginit_cb, void *ini_cb_arg, delivery_cb_fun delivery_cb, void *dev_cb_arg) {
    struct node *node = malloc(sizeof(struct node));
    node->id = id;
    node->groups = init_groups(conf);
    node->comm = init_node_comm(conf);
    node->events = init_node_events(node->comm, id);
    node->amcast = amcast_init(msginit_cb, ini_cb_arg, delivery_cb, dev_cb_arg);
    //TODO CHANGETHIS init this amcast field here
    node->amcast->gts_last_delivered = calloc(node->comm->cluster_size, sizeof(g_uid_t));
    node->amcast->newleader_ack_groupcount = 0;
    node->amcast->newleader_ack_count = calloc(node->comm->cluster_size, sizeof(unsigned int));
    node->amcast->newleader_sync_ack_groupcount = 1;
    node->amcast->newleader_sync_ack_count = calloc(node->comm->cluster_size, sizeof(unsigned int));
    return node;
}

int node_free(struct node *node) {
    //TODO Find a better place to free the events
    for(int i=0; i<node->comm->cluster_size; i++)
        if(node->events->reconnect_evs[i])
            event_free(node->events->reconnect_evs[i]);
    free_groups(node->groups);
    free_node_comm(node->comm);
    free_node_events(node->events);
    amcast_free(node->amcast);
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
