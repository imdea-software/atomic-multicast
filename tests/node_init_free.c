#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>

#include "node.h"
#include "tests.h"

// Everything is done manually here, quite normal since we want
// some checks in the early stages of the project ; a real use
// case of the lib will be checked when the public interfaces
// are finished and are returning appropriate error codes.
// In other words, this is should not be taken as a usage example!

int main(int argc, char *argv[]) {
    //TODO Create some better public config helper function to make it easier
    fill_cluster_config(&conf, NUMBER_OF_NODES, NUMBER_OF_GROUPS, ids, group_memberships, addresses, ports);

    //Let's now create the nodes
    struct node *nodes[NUMBER_OF_NODES];
    for(int i=0; i<NUMBER_OF_NODES; i++) {
        nodes[i] = node_init(&conf, conf.id[i]);
    }
    //Let's check whether the structures are well populated
    for(int i=0; i<NUMBER_OF_NODES; i++) {
        if (nodes[i]->id != conf.id[i])
	    printf("Failed to set the id of the node %u\n", i);

	if (nodes[i]->comm->cluster_size != NUMBER_OF_NODES)
	    printf("Failed to set the cluster size for the node %u\n", i);

	for(int j=0; j<NUMBER_OF_NODES; j++) {
	    if (nodes[i]->comm->groups[j] != conf.group_membership[j])
	        printf("Failed to set the group membership of node %u for the node %u\n", j, i);

	    if (ntohs(nodes[i]->comm->addrs[j].sin_port) != conf.ports[j])
	        printf("Failed to set the port of node %u for the node %u\n", j, i);

	    if (strcmp(inet_ntoa(nodes[i]->comm->addrs[j].sin_addr), conf.addresses[j]) != 0)
	        printf("Failed to set the ip addr of node %u for the node %u\n", j, i);
	}
    }

    //All checked, now let's free them up before terminating
    for(int i=0; i<NUMBER_OF_NODES; i++) {
        node_free(nodes[i]);
    }

    puts("The test is finished, if nothing was reported, it means it works!\n");
    return EXIT_SUCCESS;
}
