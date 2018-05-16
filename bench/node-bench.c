#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include <error.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>

#include "types.h"
#include "cluster.h"
#include "node.h"
#include "amcast.h"

#define CONF_SEPARATOR "\t"
#define NUMBER_OF_MESSAGES 100000
#define NODES_PER_GROUP 3
#define INITIAL_LEADER_IN_GROUP 0

struct stats {
    long delivered;
    long size;
    struct timespec tv[NUMBER_OF_MESSAGES];
    struct amcast_msg *msgs[NUMBER_OF_MESSAGES];
} stats;

//TODO write to file the execution log
void write_report(struct stats *stats) {
}

//Record useful info regarding the delivered message
void delivery_cb(struct node *node, struct amcast_msg *msg, void *cb_arg) {
    clock_gettime(CLOCK_MONOTONIC, stats.tv + stats.delivered);
    stats.msgs[stats.delivered] = msg;
    stats.delivered++;
    if(stats.delivered >= NUMBER_OF_MESSAGES)
        kill(getpid(), SIGHUP);
}

void run_amcast_node(struct cluster_config *config, xid_t node_id) {
    struct node *n = node_init(config, node_id, &delivery_cb, NULL);
    //TODO Do no configure the protocol manually like this
    n->amcast->status = (node_id % NODES_PER_GROUP == INITIAL_LEADER_IN_GROUP) ? LEADER : FOLLOWER;
    n->amcast->ballot.id = n->comm->groups[node_id] * NODES_PER_GROUP;
    node_start(n);
    node_free(n);
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
    if(argc != 5) {
        printf("USAGE: node-bench [node_id] [number_of_nodes] [number_of_groups] [isClient?]\n");
        exit(EXIT_FAILURE);
    }

    //Init node & cluster config
    struct cluster_config *config = malloc(sizeof(struct cluster_config));
    xid_t node_id = atoi(argv[1]);
    init_cluster_config(config, atoi(argv[2]), atoi(argv[3]));
    read_cluster_config_from_stdin(config);

    //CLIENT NODE PATTERN
    if(atoi(argv[4])) {
        run_client_node(config, node_id);
        return EXIT_SUCCESS;
    }

    run_amcast_node(config, node_id);

    write_report(&stats);

    //Clean and exit
    free_cluster_config(config);
    return EXIT_SUCCESS;
}
