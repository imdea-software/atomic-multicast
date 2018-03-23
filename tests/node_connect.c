#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <event2/thread.h>
#include <pthread.h>
#include <unistd.h>

#include "node.h"
#include "tests.h"

// Everything is done manually here, quite normal since we want
// some checks in the early stages of the project ; a real use
// case of the lib will be checked when the public interfaces
// are finished and are returning appropriate error codes.
// In other words, this is should not be taken as a usage example!

pthread_barrier_t *pth_barrier;

struct arg {
    id_t id;
    struct cluster_config *conf;
};

void* node_create_run(void* arg) {
    id_t id = ((struct arg *) arg)->id;
    struct cluster_config *conf = ((struct arg *) arg)->conf;

    pthread_barrier_wait(pth_barrier);
    struct node *n = node_init(conf, id);

    pthread_barrier_wait(pth_barrier);
    node_start(n);

    pthread_barrier_wait(pth_barrier);
    node_free(n);

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    //TODO Create some better public config helper function to make it easier
    fill_cluster_config(&conf, NUMBER_OF_NODES, id, group_membership, addresses, ports);

    //The plan is to start each node in a separate thread, with an event_base loop in each
    pthread_t pth[NUMBER_OF_NODES];
    evthread_use_pthreads();

    pth_barrier = malloc(sizeof(pthread_barrier_t));
    if (pthread_barrier_init(pth_barrier, NULL, NUMBER_OF_NODES) != 0) {
        printf("Can not init pthread barrier");
        return EXIT_FAILURE;
    }

    //Let's now create the nodes
    struct arg args[NUMBER_OF_NODES];
    for(int i=0; i<NUMBER_OF_NODES; i++) {
	args[i].id = conf.id[i]; args[i].conf = &conf;
        pthread_create(pth+i, NULL, node_create_run, args+i);
    }

    //If needed, let the main thread do some stuffs

    //Wait for the threads to terminate
    for(int i=0; i<NUMBER_OF_NODES; i++) {
        pthread_join(pth[i], NULL);
    }

    puts("The test is finished, if nothing was reported, it means it works!\n");
    return EXIT_SUCCESS;
}
