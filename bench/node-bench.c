#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include <error.h>
#include <arpa/inet.h>

#include "types.h"
#include "cluster.h"
#include "node.h"
#include "amcast.h"

#define NUMBER_OF_MESSAGES 100000
#define NUMBER_OF_CHILDREN 2
#define CONF_SEPARATOR "\t"
#define NODES_PER_GROUP 3
#define INITIAL_LEADER_IN_GROUP 0

pid_t pids[2];
int pid_idx;

struct stats {
    long delivered;
    double msg_per_sec;
    struct timespec avg_latency;
    struct timespec min_latency;
    struct timespec max_latency;
    struct timespec last_tv;
    struct timespec first_tv;
} stats;

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
static void tspdiff(struct timespec *end, struct timespec *start, struct timespec *diff) {
    if(!end || !start || !diff) {
        puts("Error: un-initialized timespec structs");
        exit(EXIT_FAILURE);
    }
    diff->tv_sec = end->tv_sec - start->tv_sec;
    diff->tv_nsec = end->tv_nsec - start->tv_nsec;
    if(diff->tv_nsec < 0) {
        diff->tv_sec -= 1;
        diff->tv_nsec = 1e9 + diff->tv_nsec;
    }
}


//TODO Find out whether directly computing stats
//     might not be faster than sending the required data
//     for parallel execution.

//TODO Decide whether to compute stats incrementally or to generate a report
//     at the end of the run and then let a script merge and compute the stats from all nodes

//TODO Wait for the node process msg delivery notice
//    Semaphore Producer-Consummer
//    FIFO for data exchange if needed
void update_stats(struct stats *stats) {
    struct timespec tv, latency;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    tspdiff(&tv, &stats->last_tv, &latency);
    if(stats->delivered++ == 0) {
        stats->first_tv = tv;
        stats->last_tv = tv;
        stats->min_latency = latency;
        //stats->max_latency = latency;
        //stats->avg_latency = latency;
        return;
    }

    if(tspcmp(&latency, &stats->min_latency) < 0)
        stats->min_latency = latency;
    if(tspcmp(&latency, &stats->max_latency) > 0)
        stats->max_latency = latency;
    stats->avg_latency.tv_sec =
        stats->avg_latency.tv_sec + ((latency.tv_sec - stats->avg_latency.tv_sec) / (stats->delivered - 1));
    stats->avg_latency.tv_nsec =
        stats->avg_latency.tv_nsec + ((latency.tv_nsec - stats->avg_latency.tv_nsec) / (stats->delivered - 1));
    stats->last_tv = tv;
}

void report_stats(struct stats *stats) {
    struct timespec complete_duration;
    tspdiff(&stats->last_tv, &stats->first_tv, &complete_duration);
    stats->msg_per_sec = stats->delivered / (complete_duration.tv_sec + complete_duration.tv_nsec * 1e-9);

    printf("AverageLatency=%fms MinLatency=%fms MaxLatency=%fms "
            "MsgPerSec=%fmsg/sec Duration=%lfsec "
            "TotalMsgReceived=%ldmsg\n",
        stats->avg_latency.tv_sec * 1e3 + stats->avg_latency.tv_nsec * 1e-6,
        stats->min_latency.tv_sec * 1e3 + stats->min_latency.tv_nsec * 1e-6,
        stats->max_latency.tv_sec * 1e3 + stats->max_latency.tv_nsec * 1e-6,
        stats->msg_per_sec,
        complete_duration.tv_sec + complete_duration.tv_nsec * 1e-9,
        stats->delivered);
}

//TODO Wait for the stats process to tell the parent when to close
void wait_until_all_messages_delivered() {
}

//TODO Let the stats process know when a new message is delivered
//     and maybe pass some data to it about the message through a FIFO for instance
void delivery_cb(struct node *node, struct amcast_msg *msg) {
    update_stats(&stats);
}

void run_amcast_node(struct cluster_config *config, xid_t node_id) {
    struct node *n = node_init(config, node_id, &delivery_cb);
    //TODO Do no configure the protocol manually like this
    n->amcast->status = (node_id % NODES_PER_GROUP == INITIAL_LEADER_IN_GROUP) ? LEADER : FOLLOWER;
    n->amcast->ballot.id = n->comm->groups[node_id] * NODES_PER_GROUP;
    node_start(n);
    node_free(n);
    report_stats(&stats);
}

void run_client_node(struct cluster_config *config, xid_t client_id) {
    //Connect with TCP to group LEADERS
    int *sock = malloc(sizeof(int) * config->size);
    for(int i=0; i<config->groups_count; i++) {
        xid_t peer_id = i*NODES_PER_GROUP+INITIAL_LEADER_IN_GROUP;
        sock[peer_id] = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(config->ports[peer_id]),
            .sin_addr.s_addr = inet_addr(config->addresses[peer_id])
        };
        connect(sock[peer_id], (struct sockaddr *) &addr, sizeof(addr));
	}
    //Let's send some messages
    struct enveloppe env = {
	    .sid = client_id,
	    .cmd_type = MULTICAST,
	    .cmd.multicast = {
	        .mid = {-1, client_id},
            .destgrps_count = config->groups_count,
            .destgrps = {0, 1},
            .value = {
                .len = sizeof("coucou"),
                .val = "coucou"
            }
	    },
	};
    for(int j=0; j<NUMBER_OF_MESSAGES; j++) {
        env.cmd.multicast.mid.time = j;
	    for(int i=0; i<config->groups_count; i++) {
            xid_t peer_id = i*NODES_PER_GROUP+INITIAL_LEADER_IN_GROUP;
	        struct enveloppe rep;
            send(sock[peer_id], &env, sizeof(env), 0);
            recv(sock[peer_id], &rep, sizeof(rep), 0);
	    }
	}
    //Close the connections
	for(int i=0; i<config->groups_count; i++) {
            xid_t peer_id = i*NODES_PER_GROUP+INITIAL_LEADER_IN_GROUP;
            close(sock[peer_id]);
    }
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

    memset(&stats, 0, sizeof(struct stats));

    if(argc != 5) {
        printf("USAGE: node-bench [node_id] [number_of_nodes] [number_of_groups] [isClient?]\n");
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
            if(!atoi(argv[4]))
                run_amcast_node(&config, node_id);
            if(atoi(argv[4]))
                run_client_node(&config, -1);
            break;
        //Stats process
        case 1:
            //update_stats(&stats);
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
