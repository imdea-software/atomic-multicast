#include <arpa/inet.h>
#include <string.h>

#include "node.h"

static void accept_conn_cb(struct evconnlistener *lev,
		evutil_socket_t sock, struct sockaddr *addr, int len, void *ptr);
static void accept_error_cb(struct evconnlistener *lev, void *ptr);
static void read_cb(struct bufferevent *bev, void *ctx);
static void event_cb(struct bufferevent *bev, short events, void *ctx);

//TODO Do the proper security checks on system calls

struct node_comm *init_node_comm(struct cluster_config *conf) {
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
    comm->addrs = addrs;
    comm->ids = ids;
    comm->groups = groups;
    comm->bevs = bevs;

    return comm;
};

int free_node_comm(struct node_comm *comm) {
    //TODO Every bev should be freed manually with bufferevent_free()
    free(comm->bevs);
    free(comm->groups);
    free(comm->addrs);
    free(comm);
    return 0;
}

struct node_events *init_node_events(struct node_comm *comm, id_t id) {
    struct node_events *events = malloc(sizeof(struct node_events));
    //Create a new event base
    events->base = event_base_new();
    //Listen for incomming connection
    events->lev = evconnlistener_new_bind(events->base, accept_conn_cb, comm,
		    LEV_OPT_CLOSE_ON_EXEC | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
		    -1, (struct sockaddr*) comm->addrs+id, sizeof(comm->addrs[id]));
    evconnlistener_set_error_cb(events->lev, accept_error_cb);
    //Connect to the other nodes of the cluster
    //TODO Create an event that keeps retrying until it succeeds, otherwise, not reliable
    for(int i=0; i<comm->cluster_size; i++) {
        //Create bufferevents for each node
        comm->bevs[i] = bufferevent_socket_new(events->base, -1, BEV_OPT_CLOSE_ON_FREE);
        struct bufferevent *c_bev = comm->bevs[i];
        //TODO Pass the node_id as a callback parameter to identify msg sender
        bufferevent_setcb(c_bev, read_cb, NULL, event_cb, comm->ids+i);
        bufferevent_enable(c_bev, EV_READ|EV_WRITE);
        bufferevent_socket_connect(c_bev, (struct sockaddr *)comm->addrs+i,
	    sizeof(comm->addrs[i]));
    }
    return events;
}

int free_node_events(struct node_events *events) {
    evconnlistener_free(events->lev);
    event_base_free(events->base);
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
    free_node_comm(node->comm);
    free_node_events(node->events);
    free(node);
    return 0;
}


// STATIC FUNCTIONS

// CALLBACKS IMPLEMENTATION

//Called after accepting a connection, currently, get the bufferevent ready to talk
//  Since there is no way to tell from whom the accepted connection comes,
//  a protocol extension is needed
//TODO Store accepted connections from socket & addr to keep track of the current cluster
static void accept_conn_cb(struct evconnlistener *lev, evutil_socket_t sock,
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
static void accept_error_cb(struct evconnlistener *lev, void *ptr) {
    int err = EVUTIL_SOCKET_ERROR();
    fprintf(stderr, "Got an error %d (%s) on the listener. "
                "Shutting down.\n", err, evutil_socket_error_to_string(err));
    event_base_loopexit(evconnlistener_get_base(lev), NULL);
};
