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

#include <event2/buffer.h>

#include "types.h"
#include "cluster.h"
#include "node.h"
#include "amcast.h"

#include "mcast.h"
#include "message_mcast.h"

#define CONF_SEPARATOR "\t"
#define LOG_SEPARATOR "\t"
#define NUMBER_OF_MESSAGES 100000
#define NUMBER_OF_TARGETS 2
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
void write_report(struct stats *stats, FILE *stream) {
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

void run_client_node_libevent(struct cluster_config *config, xid_t client_id, struct stats *stats) {
    struct peer {
        unsigned int id;
        unsigned int received;
        struct client *c;
    };
    struct client {
        xid_t id;
        unsigned int nodes_count;
        unsigned int groups_count;
        unsigned int dests_count;
        unsigned int connected;
        unsigned int sent;
        unsigned int received;
        g_uid_t *last_gts;
        struct peer *peers;
        struct stats *stats;
        struct event_base *base;
        struct bufferevent **bev;
        struct enveloppe *ref_value;
        mcast_message *ref_msg;
    } client;
    struct custom_payload {
        m_uid_t mid;
        unsigned int len;
        char val[MAX_PAYLOAD_LEN];
    };
    xid_t get_leader_from_group(xid_t g_id) {
        return g_id * NODES_PER_GROUP + INITIAL_LEADER_IN_GROUP;
    }
    void submit_cb(evutil_socket_t fd, short flags, void *ptr) {
        struct client *c = (struct client *) ptr;
        /* Do some magic with the mcast message */
        /* --> update mid.time */
        c->ref_value->cmd.multicast.mid.time = c->sent++;
        /* --> select circular destgrps */
        xid_t g_dst_id = (c->id + c->sent) % c->groups_count;
        for(int i=0; i<c->ref_value->cmd.multicast.destgrps_count; i++) {
            c->ref_value->cmd.multicast.destgrps[i] = g_dst_id;
            g_dst_id = (g_dst_id + 1) % c->groups_count;
        }
        /* --> update stats struct */
        if( ((stats->delivered + 1) % MEASURE_RESOLUTION) == 0) {
            stats->msg[c->ref_value->cmd.multicast.mid.time] = c->ref_value->cmd.multicast;
            clock_gettime(CLOCK_MONOTONIC, stats->tv_ini + c->ref_value->cmd.multicast.mid.time);
        }

        /* Send it to current known leaders */
        for(int i=0; i<c->ref_value->cmd.multicast.destgrps_count; i++) {
            xid_t peer_id = get_leader_from_group(c->ref_value->cmd.multicast.destgrps[i]);
            if(bufferevent_write(c->bev[peer_id], c->ref_value, sizeof(*c->ref_value)) < 0)
                    printf("[c-%u] Something bad happened (submit)\n", c->id);
        }
    }
    void alt_submit_cb(evutil_socket_t fd, short flags, void *ptr) {
        struct client *c = (struct client *) ptr;
        /* Do some magic with the mcast message */
        /* --> select starting group */
        xid_t g_dst_id = (c->id + c->sent) % c->groups_count;
        /* --> update origin group */
        c->ref_msg->from_group = g_dst_id;
        c->ref_msg->from_node = INITIAL_LEADER_IN_GROUP;
        /* --> update msg uid */
        c->ref_msg->uid = generate_uid(c->ref_msg->from_group, c->ref_msg->from_node, c->sent);
        c->ref_value->cmd.multicast.mid.time = c->sent;
        /* --> embed client mid in payload */
        ((struct custom_payload *) c->ref_msg->value.mcast_value_val)->mid = c->ref_value->cmd.multicast.mid;
        /* --> select circular destgrps */
        for(int i=0; i<c->ref_msg->to_groups_len; i++) {
            c->ref_msg->to_groups[i] = g_dst_id;
            c->ref_value->cmd.multicast.destgrps[i] = g_dst_id;
            g_dst_id = (g_dst_id + 1) % c->groups_count;
        }
        /* --> update stats struct */
        if( ((stats->delivered + 1) % MEASURE_RESOLUTION) == 0) {
            stats->msg[c->ref_value->cmd.multicast.mid.time] = c->ref_value->cmd.multicast;
            clock_gettime(CLOCK_MONOTONIC, stats->tv_ini + c->sent);
        }

        c->sent++;

        /* Send MCAST_CLIENT to leader of first group in dest groups */
        xid_t peer_id = get_leader_from_group(c->ref_msg->from_group);
        send_mcast_message(c->bev[peer_id], c->ref_msg);

        /* Send MCAST_START to all nodes in dest groups */
        /*
        for(int i=0; i<c->ref_msg->to_groups_len; i++) {
            xid_t peer_id = get_leader_from_group(c->ref_msg->to_groups[i]);
            send_mcast_message(c->bev[peer_id], c->ref_msg);
            send_mcast_message(c->bev[peer_id+1], c->ref_msg);
            send_mcast_message(c->bev[peer_id+2], c->ref_msg);
        }
        */
    }
    void read_cb(struct bufferevent *bev, void *ptr) {
        struct peer *p = (struct peer *) ptr;
        struct client *c = p->c;

        /* Do Some STUFFS */
        struct evbuffer *in_buf = bufferevent_get_input(bev);
        while (evbuffer_get_length(in_buf) >= sizeof(struct enveloppe)) {
            struct enveloppe env;
            bufferevent_read(bev, &env, sizeof(struct enveloppe));
            switch(env.cmd_type) {
                case DELIVER:
                    if(env.cmd.deliver.mid.id != c->id) {
                        printf("[c-%u] FAILURE: received deliver ack with wrong c-id %u\n",
                                c->id, env.cmd.deliver.mid.id);
                        exit(EXIT_FAILURE);
                    }
                    /* --> do not re-deliver messages */
                    if(c->last_gts && paircmp(&env.cmd.deliver.gts, c->last_gts) <= 0) {
                        continue;
                    }
                    if(stats->tv_dev[env.cmd.deliver.mid.time].tv_sec != 0
                            && stats->tv_dev[env.cmd.deliver.mid.time].tv_nsec != 0) {
                        printf("[c-%u] FAILURE: received deliver ack with wrong s-mid %u instead of %u from %d\n",
                                c->id, env.cmd.deliver.mid.time, c->ref_value->cmd.multicast.mid.time, p->id);
                        continue;
                    }
                    /* --> update deliver counts */
                    p->received++;
                    c->received++;
                    /* --> update stats struct */
                    if( ((stats->delivered + 1) % MEASURE_RESOLUTION) == 0) {
                        clock_gettime(CLOCK_MONOTONIC, stats->tv_dev + env.cmd.deliver.mid.time);
                        stats->gts[env.cmd.deliver.mid.time] = env.cmd.deliver.gts;
                        c->last_gts = stats->gts + env.cmd.deliver.mid.time;
                        stats->count++;
                    }
                    stats->delivered++;
                    /* MCAST the next message */
                    if(c->sent < NUMBER_OF_MESSAGES)
                        submit_cb(0,0,c);
                    if(c->received >= NUMBER_OF_MESSAGES)
                        kill(getpid(), SIGHUP);
                    break;
                default:
                    break;
            }
        }
    }
    void alt_read_cb(struct bufferevent *bev, void *ptr) {
        struct peer *p = (struct peer *) ptr;
        struct client *c = p->c;
        mcast_message msg;

        /* Do Some STUFFS */
        struct evbuffer *in_buf = bufferevent_get_input(bev);
        while (recv_mcast_message(in_buf, &msg)) {
            m_uid_t mid = ((struct custom_payload *) msg.value.mcast_value_val)->mid;
            if(mid.id != c->id) {
                printf("[c-%u] FAILURE: received deliver ack with wrong c-id %u\n",
                       c->id, mid.id);
                exit(EXIT_FAILURE);
            }
            /* --> do not re-deliver messages */
            if(c->last_gts && msg.timestamp <= c->last_gts.time)
                continue;
            /* --> update deliver counts */
            p->received++;
            c->received++;
            /* --> update stats struct */
            if( ((stats->delivered + 1) % MEASURE_RESOLUTION) == 0) {
                clock_gettime(CLOCK_MONOTONIC, stats->tv_dev + mid.time);
                stats->gts[mid.time].time = msg.timestamp;
                stats->gts[mid.time].id = get_leader_from_group(msg.from_group);
                c->last_gts = stats->gts + mid.time;
                stats->count++;
            }
            stats->delivered++;
            mcast_message_content_free(&msg);
            /* MCAST the next message */
            if(c->sent < NUMBER_OF_MESSAGES)
                alt_submit_cb(0,0,c);
            if(c->received >= NUMBER_OF_MESSAGES)
                kill(getpid(), SIGHUP);
        }
    }
    void event_cb(struct bufferevent *bev, short events, void *ptr) {
        struct peer *p = (struct peer *) ptr;
        struct client *c = p->c;
        if (events & BEV_EVENT_CONNECTED) {
            c->connected++;
            struct enveloppe init_client = { .sid = c->id, .cmd_type = INIT_CLIENT };
            if(bufferevent_write(bev, &init_client, sizeof(init_client)) < 0)
                printf("[c-%u] Something bad happened (init_client)\n", c->id);
        }
        else if (events & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
            c->connected--;
            if(c->received < c->stats->size) {
                printf("[c-%u] Server %i left before all messages were sent: %u sent\n", c->id, p->id, c->sent);
            }
        }
        if(c->connected == c->nodes_count) {
            printf("[c-%u] Connection established to all nodes\n", c->id);
            if(c->sent == 0)
                submit_cb(0,0,c);
        }
    }
    void alt_event_cb(struct bufferevent *bev, short events, void *ptr) {
        struct peer *p = (struct peer *) ptr;
        struct client *c = p->c;
        if (events & BEV_EVENT_CONNECTED) {
            c->connected++;
        }
        else if (events & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
            c->connected--;
            if(c->received < NUMBER_OF_MESSAGES) {
                printf("[c-%u] Server %i left before all messages were sent: %u sent\n", c->id, p->id, c->sent);
            }
        }
        if(c->connected == c->nodes_count) {
            printf("[c-%u] Connection established to all nodes\n", c->id);
            if(c->sent == 0)
                alt_submit_cb(0,0,c);
        }
    }
    void interrupt_cb(evutil_socket_t fd, short flags, void *ptr) {
        struct event *interrupt_ev = (struct event *) ptr;
        struct event_base *base = event_get_base(interrupt_ev);
        event_del(interrupt_ev);
        event_base_loopexit(base, NULL);
    }
    //SET-UP libevent
    memset(&client, 0, sizeof(struct client));
    client.id = client_id;
    client.nodes_count = config->size;
    client.groups_count = config->groups_count;
    client.dests_count = NUMBER_OF_TARGETS;
    client.stats = stats;
    client.base = event_base_new();
    client.bev = calloc(config->size, sizeof(struct bufferevent *));
    client.peers = calloc(config->size, sizeof(struct peer));
    //Start a TCP connection to all nodes
    for(xid_t peer_id=0; peer_id<client.nodes_count; peer_id++) {
        client.peers[peer_id].c = &client;
        client.peers[peer_id].id = peer_id;
        client.bev[peer_id] = bufferevent_socket_new(client.base, -1, BEV_OPT_CLOSE_ON_FREE);
        //TODO CHANGETHIS: Have to edit those 2 lines to switch back to libevamcast
        bufferevent_setcb(client.bev[peer_id], alt_read_cb, NULL, alt_event_cb, client.peers+peer_id);
        //bufferevent_setwatermark(client.bev[peer_id], EV_READ, sizeof(struct enveloppe), 0);
        bufferevent_enable(client.bev[peer_id], EV_READ|EV_WRITE);
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(config->ports[peer_id]),
            .sin_addr.s_addr = inet_addr(config->addresses[peer_id])
        };
        bufferevent_socket_connect(client.bev[peer_id], (struct sockaddr *) &addr, sizeof(addr));
    }
    //Prepare enveloppe template
    struct enveloppe env = {
        .sid = client_id,
        .cmd_type = MULTICAST,
        .cmd.multicast = {
            .mid = {-1, client_id},
            .destgrps_count = client.dests_count,
            .value = {
                .len = strlen("coucou"),
                .val = "coucou"
            }
        },
    };
    client.ref_value = &env;
    //Prepare mcast message template
    mcast_message msg;
    struct custom_payload val;
    msg.type = MCAST_CLIENT;
    msg.timestamp = 0;
    msg.to_groups_len = env.cmd.multicast.destgrps_count;
    val.len = env.cmd.multicast.value.len;
    strcpy(val.val, env.cmd.multicast.value.val);
    msg.value.mcast_value_len = sizeof(struct custom_payload) - MAX_PAYLOAD_LEN + val.len;
    msg.value.mcast_value_val = (char *) &val;
    client.ref_msg = &msg;
    //Set-up eventloop-exit
    struct event *ev_exit = evsignal_new(client.base, SIGHUP, interrupt_cb, event_self_cbarg());
    event_add(ev_exit, NULL);
    //Start client
    event_base_dispatch(client.base);
    //Leaving
    //for(struct peer *peer=peers; peer<peers+config->size; peer++)
    //    printf("[c-%u] Received %u messages from %u\n", client.id, peer->received, peer->id);
    //printf("[c-%u] Leaving with %u message sent\n", client.id, client.sent);
    //Free-up resources
    for(int i=0; i<config->size; i++)
        if(client.bev[i])
            bufferevent_free(client.bev[i]);
    free(client.bev);
    event_free(ev_exit);
    event_base_free(client.base);
    free(client.peers);
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
    int client_count = atoi(argv[4]);
    int is_client = atoi(argv[5]);
    stats->delivered = 0;
    stats->count = 0;
    stats->size = ( NUMBER_OF_MESSAGES / MEASURE_RESOLUTION ) * ( is_client ? 1 : client_count );
    stats->tv_ini = malloc(sizeof(struct timespec) * stats->size);
    stats->tv_dev = malloc(sizeof(struct timespec) * stats->size);
    stats->gts = malloc(sizeof(g_uid_t) * stats->size);
    stats->msg = malloc(sizeof(message_t) * stats->size);
    //CLIENT NODE PATTERN
    if(is_client) {
        run_client_node_libevent(config, node_id, stats);
    } else {
        node = run_amcast_node(config, node_id, stats);
    }
    //Open logfile for editing
    char filename[40];
    sprintf(filename, "/tmp/%s.%d.log", (is_client ? "client" : "node"), node_id);
    if((logfile = fopen(filename, "w")) == NULL) {
        puts("ERROR: Can not open logfile");
        exit(EXIT_FAILURE);
    }

    write_report(stats, logfile);

    //Clean and exit
    fclose(logfile);
    if(!is_client)
        node_free(node);
    free_cluster_config(config);
    free(stats->tv_ini);
    free(stats->tv_dev);
    free(stats->gts);
    free(stats->msg);
    free(stats);
    return EXIT_SUCCESS;
}
