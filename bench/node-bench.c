#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include <error.h>

#include "types.h"
#include "cluster.h"
#include "node.h"
#include "amcast.h"

#define NODES_PER_GROUP 3
#define INITIAL_LEADER_IN_GROUP 0

pid_t pids[2];
int pid_idx;


//TODO Find out whether directly computing stats
//     might not be faster than sending the required data
//     for parallel execution.

//TODO Decide whether to compute stats incrementally or to generate a report
//     at the end of the run and then let a script merge and compute the stats from all nodes

//TODO Wait for the node process msg delivery notice
//    Semaphore Producer-Consummer
//    FIFO for data exchange if needed
void update_stats() {
}

//TODO Wait for the stats process to tell the parent when to close
void wait_until_all_messages_delivered() {
}

//TODO Let the stats process know when a new message is delivered
//     and maybe pass some data to it about the message through a FIFO for instance
void delivery_cb(struct node *node, struct amcast_msg *msg) {
}

void run_amcast_node(struct cluster_config *config, xid_t node_id) {
    struct node *n = node_init(config, node_id, &delivery_cb);
    //TODO Do no configure the protocol manually like this
    n->amcast->status = (node_id % NODES_PER_GROUP == INITIAL_LEADER_IN_GROUP) ? LEADER : FOLLOWER;
    n->amcast->ballot.id = n->comm->groups[node_id] * NODES_PER_GROUP;
    node_start(n);
    node_free(n);
}

int main(int argc, char *argv[]) {
    struct cluster_config config = {0};
    xid_t node_id = -1;

//TODO process CLI args here, make those switches :
//    - node id
//    - client or node
//TODO read cluster config from stdin

    //Let's create some child processes...
    pid_idx = -1;
    for(int i=0; i<2; i++) {
        if ((pids[i] = fork()) < 0)
            error_at_line(EXIT_FAILURE, pids[i], __FILE__, __LINE__, "fctname");
        if (pids[i] == 0) {
            pid_idx = i;
            break;
        }
    }

    //... and let them work for you!
    switch(pid_idx) {
        //Node process
        case 0:
            run_amcast_node(&config, node_id);
            break;
        //Stats process
        case 1:
            update_stats();
            break;
        //Parent process
        case -1:
            wait_until_all_messages_delivered();

            kill(pids[0], SIGHUP);
            waitpid(pids[0], NULL, 0);

            kill(pids[1], SIGTERM);
            waitpid(pids[1], NULL, 0);
            break;
        //Error
        default:
            printf("Error: wrong process id, should not happen\n");
            exit(EXIT_FAILURE);
    }

    //Clean and exit
    return EXIT_SUCCESS;
}
