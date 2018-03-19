#include <arpa/inet.h>
#include <string.h>

#include "node.h"

//TODO Do the proper security checks on system calls

struct node_comm *init_node_comm(struct cluster_config *conf) {
    unsigned int size = conf->size;

    struct node_comm *comm = malloc(sizeof(struct node_comm));
    struct sockaddr_in *addr = malloc(size * sizeof(struct sockaddr_in));
    id_t *groups = malloc(size * sizeof(id_t));

    for(int i=0; i<size; i++) {
	id_t c_id = *(conf->id+i);
	struct sockaddr_in *c_addr = addr+c_id;
	address_t *conf_addr = conf->addresses+c_id;
	port_t conf_port = *(conf->ports+c_id);

	memset(c_addr, 0, sizeof(struct sockaddr_in));
        c_addr->sin_family = AF_INET;
        c_addr->sin_port = htons(conf_port);
	inet_aton(*conf_addr, &(c_addr->sin_addr));
    }
    //TODO Create a dedicated group structure
    memcpy(groups, conf->group_membership, size * sizeof(id_t));

    comm->c_size = size;
    comm->addr = addr;
    comm->groups = groups;

    return comm;
};

int free_node_comm(struct node_comm *comm) {
    free(comm->groups);
    free(comm->addr);
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
