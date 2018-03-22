#include <arpa/inet.h>
#include <string.h>

#include "node.h"

//TODO Do the proper security checks on system calls

struct node_comm *init_node_comm(struct cluster_config *conf) {
    unsigned int size = conf->size;

    struct node_comm *comm = malloc(sizeof(struct node_comm));
    struct sockaddr_in *addrs = malloc(size * sizeof(struct sockaddr_in));
    id_t *groups = malloc(size * sizeof(id_t));

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

    comm->cluster_size = size;
    comm->addrs = addrs;
    comm->groups = groups;

    return comm;
};

int free_node_comm(struct node_comm *comm) {
    free(comm->groups);
    free(comm->addrs);
    free(comm);
    return 0;
}

struct node_events *init_node_events() {
    struct node_events *events = malloc(sizeof(struct node_events));
    //TODO Actually init the structure
    return events;
}

int free_node_events(struct node_events *events) {
    free(events);
    //TODO Update this once the structure is well intialized
    return 0;
}

struct node *node_init(struct cluster_config *conf, id_t id) {
    struct node *node = malloc(sizeof(struct node));
    struct node_comm *comm = init_node_comm(conf);
    struct node_events *events = init_node_events();

    node->id = id;
    node->comm = comm;
    node->events = events;

    return node;
}

int node_free(struct node *node) {
    free_node_comm(node->comm);
    free_node_events(node->events);
    free(node);
    return 0;
}
