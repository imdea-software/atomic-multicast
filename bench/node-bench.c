#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include <error.h>

#include "types.h"
#include "cluster.h"
#include "node.h"
#include "amcast.h"

#define NUMBER_OF_CHILDREN 2
#define CONF_SEPARATOR "\t"
#define NODES_PER_GROUP 3
#define INITIAL_LEADER_IN_GROUP 0

pid_t pids[2];
int pid_idx;

static int tspcmp(struct timespec *tv1, struct timespec *tv2) {
    if(!tv1 || !tv2) {
        puts("Error: un-initialized timespec structs");
        exit(EXIT_FAILURE);
    }
    if(tv1->tv_sec < tv2->tv_sec || ( tv1->tv_sec == tv2->tv_sec && tv1->tv_nsec < tv2->tv_nsec))
        return -1;
    if(tv1->tv_sec > tv2->tv_sec || ( tv1->tv_sec == tv2->tv_sec && tv1->tv_nsec > tv2->tv_nsec))
        return 1;
    return 0;
}


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

//TODO Should find a trick to avoid the ugly conditionals
void read_cluster_config_from_stdin(struct cluster_config *config) {
    char *line = NULL, *token = NULL;
    size_t len = 0;
    ssize_t read;
    int cur_x, cur_y = 0;

    while ((read = getline(&line, &len, stdin)) != -1 && cur_y < config->size) {
        if(line[0] == '#')
            continue;
        cur_x = 0;
        while((token = strtok(line, CONF_SEPARATOR)) != NULL && cur_x < 4) {
            line = NULL;
            switch(cur_x) {
                case 0:
                    config->id[cur_y] = atoi(token);
                    break;
                case 1:
                    config->group_membership[cur_y] = atoi(token);
                    break;
                case 2:
                    config->addresses[cur_y] = token;
                    break;
                case 3:
                    config->ports[cur_y] = atoi(token);
                    break;
                default:
                    printf("Error: bad config file formatting");
                    exit(EXIT_FAILURE);
            }
            cur_x++;
        }
        cur_y++;
    }
}

//TODO This should be part of the lib, nothing to do here
void init_cluster_config(struct cluster_config *config, unsigned int n_nodes, unsigned int n_groups) {
    config->size = n_nodes;
    config->groups_count = n_groups;

    //TODO Do not forget to free those, somewhere ...
    xid_t *ids = malloc(sizeof(xid_t) * n_nodes);
    xid_t *group_memberships = malloc(sizeof(xid_t) * n_nodes);
    address_t *addresses = malloc(sizeof(address_t) * n_nodes);
    port_t *ports = malloc(sizeof(port_t) * n_nodes);

    config->id = ids;
    config->group_membership = group_memberships;
    config->addresses = addresses;
    config->ports = ports;
}
int free_cluster_config(struct cluster_config *config) {
    free(config->id);
    free(config->group_membership);
    free(config->addresses);
    free(config->ports);
    free(config);
    return 0;
}

int main(int argc, char *argv[]) {
    struct cluster_config config;
    xid_t node_id = -1;

    //TODO Add a server/client switch
    if(argc != 4) {
        printf("USAGE: node-bench [node_id] [number_of_nodes] [number_of_groups]\n");
        exit(EXIT_FAILURE);
    }

    //Init node & cluster config
    node_id = atoi(argv[1]);
    init_cluster_config(&config, atoi(argv[2]), atoi(argv[3]));
    read_cluster_config_from_stdin(&config);

    //Let's create some child processes...
    pid_idx = -1;
    for(int i=0; i<NUMBER_OF_CHILDREN; i++) {
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
            //... and kill them when they're done
            for(pid_t *child=pids; child<pids+NUMBER_OF_CHILDREN; child++) {
                kill(*child, SIGHUP);
                waitpid(*child, NULL, 0);
            }
            break;
        //Error
        default:
            printf("Error: wrong process id, should not happen\n");
            exit(EXIT_FAILURE);
    }

    //Clean and exit
    return EXIT_SUCCESS;
}
