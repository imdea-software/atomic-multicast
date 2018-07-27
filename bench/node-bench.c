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
#define LOG_SEPARATOR "\t"
#define NUMBER_OF_MESSAGES 100000
#define NODES_PER_GROUP 3
#define INITIAL_LEADER_IN_GROUP 0
#define MEASURE_RESOLUTION 1 //Only save stats for 1 message out of MEASURE_RESOLUTION

struct stats {
    long delivered;
    long count;
    long size;
    struct timespec *tv_ini;
    struct timespec *tv_dev;
    g_uid_t *gts;
    message_t *msg;
};

//TODO write to file the execution log
void write_report(struct node *node, struct stats *stats, FILE *stream) {
    for(int i=0; i<stats->count; i++) {
        //Retrieve measures & the message's context
        message_t msg = stats->msg[i];
        struct timespec ts_start = stats->tv_ini[i];
        struct timespec ts_end = stats->tv_dev[i];
        g_uid_t gts = stats->gts[i];
        //Write to a string the destination groups
        char *destgrps = malloc(sizeof(char) * (1 + 1 + (12 + 1) * msg.destgrps_count) + 1);
        int idx = 0;
        idx = sprintf(destgrps+idx, "(%d", msg.destgrps[0]);
        for(int i=0; i<msg.destgrps_count - 1; i++)
            idx = sprintf(destgrps+idx, ",%d", msg.destgrps[i]);
        idx = sprintf(destgrps+idx, ",%d)", msg.destgrps[msg.destgrps_count-1]);
        //Write to a file the line corresponding to this message
        fprintf(stream, "(%u,%d)" LOG_SEPARATOR
                        "%lld.%.9ld" LOG_SEPARATOR
                        "%lld.%.9ld" LOG_SEPARATOR
                        "(%u,%d)" LOG_SEPARATOR
                        "%u" LOG_SEPARATOR
                        "%s" LOG_SEPARATOR
                        "%u" LOG_SEPARATOR
                        "%s" "\n",
                        msg.mid.time, msg.mid.id,
                        (long long)ts_start.tv_sec, ts_start.tv_nsec,
                        (long long)ts_end.tv_sec, ts_end.tv_nsec,
                        gts.time, gts.id,
                        msg.destgrps_count,
                        destgrps,
                        msg.value.len,
                        msg.value.val);
        free(destgrps);
    }
}

//Record useful info regarding the initiated message
void msginit_cb(struct node *node, struct amcast_msg *msg, void *cb_arg) {
    struct timespec *tv_ini = malloc(sizeof(struct timespec));
    clock_gettime(CLOCK_MONOTONIC, tv_ini);
    msg->shared_cb_arg = tv_ini;
}

//Record useful info regarding the delivered message
void delivery_cb(struct node *node, struct amcast_msg *msg, void *cb_arg) {
    struct stats *stats = (struct stats *) cb_arg;
    if( ((stats->delivered + 1) % MEASURE_RESOLUTION) == 0) {
        clock_gettime(CLOCK_MONOTONIC, stats->tv_dev + stats->count);
        stats->tv_ini[stats->count] = *((struct timespec *) msg->shared_cb_arg);
        stats->gts[stats->count] = msg->gts;
        stats->msg[stats->count] = msg->msg;
        stats->count++;
    }
    stats->delivered++;
    free(msg->shared_cb_arg);
    if(stats->delivered >= stats->size * MEASURE_RESOLUTION )
        kill(getpid(), SIGHUP);
}

struct node *run_amcast_node(struct cluster_config *config, xid_t node_id, void *dev_cb_arg) {
    struct node *n = node_init(config, node_id, msginit_cb, NULL, &delivery_cb, dev_cb_arg);
    //TODO Do no configure the protocol manually like this
    n->amcast->status = (node_id % NODES_PER_GROUP == INITIAL_LEADER_IN_GROUP) ? LEADER : FOLLOWER;
    n->amcast->ballot.id = n->comm->groups[node_id] * NODES_PER_GROUP;
    node_start(n);
    return(n);
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
        struct enveloppe init = { .sid = client_id, .cmd_type = INIT_CLIENT };
        send(sock[peer_id], &init, sizeof(init), 0);
	}
    //Let's send some messages
    struct enveloppe env = {
	    .sid = client_id,
	    .cmd_type = MULTICAST,
	    .cmd.multicast = {
	        .mid = {-1, client_id},
            .destgrps_count = config->groups_count,
            .value = {
                .len = strlen("coucou"),
                .val = "coucou"
            }
	    },
	};
    for(int i=0; i<env.cmd.multicast.destgrps_count; i++)
        env.cmd.multicast.destgrps[i] = i;
    for(int j=0; j<NUMBER_OF_MESSAGES; j++) {
        env.cmd.multicast.mid.time = j;
	    for(int i=0; i<config->groups_count; i++) {
            xid_t peer_id = i*NODES_PER_GROUP+INITIAL_LEADER_IN_GROUP;
            send(sock[peer_id], &env, sizeof(env), 0);
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
    if(argc != 6) {
        printf("USAGE: node-bench [node_id] [number_of_nodes]"
                "[number_of_groups] [number_of_clients] [isClient?] \n");
        exit(EXIT_FAILURE);
    }
    FILE *logfile;
    struct node *node;
    struct stats *stats = malloc(sizeof(struct stats));

    //Init node & cluster config
    struct cluster_config *config = malloc(sizeof(struct cluster_config));
    xid_t node_id = atoi(argv[1]);
    init_cluster_config(config, atoi(argv[2]), atoi(argv[3]));
    read_cluster_config_from_stdin(config);

    //Get client_count & init stats struct
    stats->delivered = 0;
    stats->count = 0;
    stats->size = ( NUMBER_OF_MESSAGES / MEASURE_RESOLUTION ) * atoi(argv[4]);
    stats->tv_ini = malloc(sizeof(struct timespec) * stats->size);
    stats->tv_dev = malloc(sizeof(struct timespec) * stats->size);
    stats->gts = malloc(sizeof(g_uid_t) * stats->size);
    stats->msg = malloc(sizeof(message_t) * stats->size);
    //CLIENT NODE PATTERN
    if(atoi(argv[5])) {
        run_client_node(config, node_id);
        return EXIT_SUCCESS;
    }

    node = run_amcast_node(config, node_id, stats);

    //Open logfile for editing
    char filename[40];
    sprintf(filename, "/tmp/report.%d.log", node_id);
    if((logfile = fopen(filename, "w")) == NULL) {
        puts("ERROR: Can not open logfile");
        exit(EXIT_FAILURE);
    }

    write_report(node, stats, logfile);

    //Clean and exit
    fclose(logfile);
    node_free(node);
    free_cluster_config(config);
    free(stats->tv_ini);
    free(stats->tv_dev);
    free(stats->gts);
    free(stats->msg);
    free(stats);
    return EXIT_SUCCESS;
}
