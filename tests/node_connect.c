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

#define NUMBER_OF_FAILURES 2

pthread_t pth[NUMBER_OF_NODES];
struct node *nodes[NUMBER_OF_NODES];
int status[NUMBER_OF_NODES];

struct arg {
    xid_t id;
    struct cluster_config *conf;
    pthread_barrier_t *barrier;
};

//NOTE: Also works fine without the barriers.
//      Pthreads were chosen over processes because of the simple
//      synchronization mecanism, but since none is actually needed,
//      let's use proccesses and remove that UGLY node_stop() public primitive
//      and instead send a signal to the process to stop the main event loop
//TODO ReWrite using processes (fork) instead of threads (pthread)
void* node_create_run(void* arg) {
    xid_t id = ((struct arg *) arg)->id;
    struct cluster_config *conf = ((struct arg *) arg)->conf;
    pthread_barrier_t *pth_barrier = ((struct arg *) arg)->barrier;

    pthread_barrier_wait(pth_barrier);
    nodes[id] = node_init(conf, id, NULL, NULL, NULL, NULL);
    status[id] = 1;

    pthread_barrier_wait(pth_barrier);
    node_start(nodes[id]);

    if (nodes[id]->comm->accepted_count != NUMBER_OF_NODES)
        printf("[%u] Failed to connect to the whole cluster"
			" (%u connected peers)\n", id, nodes[id]->comm->accepted_count);

    node_free(nodes[id]);

    status[id] = 0;

    pthread_exit(NULL);
}

//Scenario:
//  Start all the nodes
//  Once they are all connected, kill some random ones
//  After connections were lost, restart the faulty nodes
//  Wait until all the connections successfully re-establish
//  Succesfully exits
int main(int argc, char *argv[]) {
    //Init the RNG
    time_t t;
    srand((unsigned) time(&t));

    //TODO Create some better public config helper function to make it easier
    fill_cluster_config(&conf, NUMBER_OF_NODES, NUMBER_OF_GROUPS, ids, group_memberships, addresses, ports);

    //The plan is to start each node in a separate thread, with an event_base loop in each
    pthread_barrier_t *start_pthb;
    pthread_barrier_t *restart_pthb;
    evthread_use_pthreads();

    start_pthb = malloc(sizeof(pthread_barrier_t));
    if (pthread_barrier_init(start_pthb, NULL, NUMBER_OF_NODES) != 0) {
        printf("Can not init start pthread barrier");
        return EXIT_FAILURE;
    }
    restart_pthb = malloc(sizeof(pthread_barrier_t));
    if (pthread_barrier_init(restart_pthb, NULL, NUMBER_OF_FAILURES) != 0) {
        printf("Can not init restart pthread barrier");
        return EXIT_FAILURE;
    }

    //Let's now create the nodes
    struct arg args[NUMBER_OF_NODES];
    for(int i=0; i<NUMBER_OF_NODES; i++) {
	args[i].id = conf.id[i]; args[i].conf = &conf; args[i].barrier = start_pthb;
        pthread_create(pth+i, NULL, node_create_run, args+i);
    }

    //Let's wait until connections are successful
    int connected;
    do {
        connected = 0;
        for(int i=0; i<NUMBER_OF_NODES; i++) {
	    if (nodes[i])
                if (nodes[i]->comm->accepted_count == NUMBER_OF_NODES)
                    connected++;
        }
    } while(connected != NUMBER_OF_NODES);

    //Let's terminate some random nodes
    for(int i=0; i<NUMBER_OF_FAILURES; ) {
	xid_t id = rand() % NUMBER_OF_NODES;
	if(status[id] != 0) {
            printf("Closing node %u\n", id);
            node_stop(nodes[id]);
            pthread_join(pth[id], NULL);
	    i++;
        }
    }

    //Wait until the nodes are actually dead
    do {
        connected = 0;
        for(int i=0; i<NUMBER_OF_NODES; i++) {
            if(status[i] != 0)
                if(nodes[i]->comm->accepted_count == NUMBER_OF_NODES - NUMBER_OF_FAILURES)
                    connected++;
        }
    } while(connected != NUMBER_OF_NODES - NUMBER_OF_FAILURES);

    //Let's then reconnect them
    for(int i=0; i<NUMBER_OF_NODES; i++) {
	if(status[i] == 0) {
	    args[i].id = conf.id[i]; args[i].conf = &conf; args[i].barrier = restart_pthb;
            pthread_create(pth+i, NULL, node_create_run, args+i);
        }
    }

    //And finally wait until connections are back up
    do {
        connected = 0;
        for(int i=0; i<NUMBER_OF_NODES; i++) {
	    if (status[i] != 0)
                if (nodes[i]->comm->accepted_count == NUMBER_OF_NODES)
                    connected++;
        }
    } while(connected != NUMBER_OF_NODES);

    //Break the event loop for all nodes
    for(int i=0; i<NUMBER_OF_NODES; i++) {
        node_stop(nodes[i]);
    }

    //Wait for the threads to terminate
    for(int i=0; i<NUMBER_OF_NODES; i++) {
        pthread_join(pth[i], NULL);
    }

    puts("The test is finished, if nothing was reported, it means it works!\n");

    //Cleanup and exit
    free(start_pthb);
    free(restart_pthb);
    return EXIT_SUCCESS;
}
