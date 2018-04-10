#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include "node.h"
#include "message.h"
#include "tests.h"

// Everything is done manually here, quite normal since we want
// some checks in the early stages of the project ; a real use
// case of the lib will be checked when the public interfaces
// are finished and are returning appropriate error codes.
// In other words, this is should not be taken as a usage example!

id_t id;
pid_t pids[NUMBER_OF_NODES];

int envcmp(struct enveloppe *env1, struct enveloppe *env2) {
    int out = 0;
    if (env1->sid != env2->sid)
        out++;
    if (env1->cmd_type != env2->cmd_type)
        out++;
    if (env1->cmd.multicast.mid != env2->cmd.multicast.mid)
        out++;
    if (env1->cmd.multicast.destgrps_count != env2->cmd.multicast.destgrps_count) {
        out++;
        for (int i=0; i<env1->cmd.multicast.destgrps_count; i++) {
            if (env1->cmd.multicast.destgrps[i] != env2->cmd.multicast.destgrps[i])
                out++;
        }
    }
    if (env1->cmd.multicast.value.len != env2->cmd.multicast.value.len) {
        out++;
	if (strcmp(env1->cmd.multicast.value.val, env2->cmd.multicast.value.val) != 0)
            out++;
    }

    return out;
}

//Scenario:
//  Start all the nodes
//  Once they are all connected, send some messages
//  Retrieve and check the integrity of the messages
//  Succesfully exits
int main(int argc, char *argv[]) {
    //TODO Create some better public config helper function to make it easier
    fill_cluster_config(&conf, NUMBER_OF_NODES, ids, group_memberships, addresses, ports);

    //The plan is to start each node in a separate process
    id = -1;
    for(int i=0; i<NUMBER_OF_NODES; i++) {
	if ((pids[i] = fork()) < 0)
            error_at_line(EXIT_FAILURE, pids[i], __FILE__, __LINE__, "fctname");
	if (pids[i] == 0) {
	    id = i;
            break;
        }
    }

    //Let's now create the nodes
    if (id != -1) {
        struct node *n = node_init(&conf, id);
        node_start(n);
        if (n->comm->accepted_count != NUMBER_OF_NODES)
            printf("[%u] Failed to connect to the whole cluster"
	           " (%u connected peers)\n", id, n->comm->accepted_count);
	//TODO Put a barrier here, so that nodes are not being freed until they were all stopped
        node_free(n);
    }
    //Let the main process do some stuffs e.g. be a client
    else {
        //Let's wait until connections are successful
        sleep(2); //No longer possible to inspect nodes, memory is not shared
        //Let's send some messages
        puts("Ready to do stuffs");
        //Let's check the integrity of delivered messages

        //Break the event loop for all nodes
        for(int i=0; i<NUMBER_OF_NODES; i++) {
            kill(pids[i], SIGHUP);
        }

        puts("The test is finished, if nothing was reported, it means it works!\n");
    }

    //Cleanup and exit
    return EXIT_SUCCESS;
}
