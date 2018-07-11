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
#include "amcast.h"

#define NUMBER_OF_MESSAGES 100

// Everything is done manually here, quite normal since we want
// some checks in the early stages of the project ; a real use
// case of the lib will be checked when the public interfaces
// are finished and are returning appropriate error codes.
// In other words, this is should not be taken as a usage example!

xid_t id;
pid_t pids[NUMBER_OF_NODES];

unsigned long delivered;

int msgcmp(message_t *msg1, message_t *msg2) {
    if(msg1 == NULL || msg2 == NULL)
        return 1;
    int out = 0;
    if (msg1->destgrps_count != msg2->destgrps_count) {
        out++;
        for (int i=0; i<msg1->destgrps_count && i<msg2->destgrps_count; i++) {
            if (msg1->destgrps[i] != msg2->destgrps[i])
                out++;
        }
    }
    if (msg1->value.len != msg2->value.len) {
        out++;
	if (strcmp(msg1->value.val, msg2->value.val) != 0)
            out++;
    }

    return out;
}

//TODO Use the delivery callback to check correctness:
//    Do local checks during protocol execution (in this callback),
//    Then write them to parent thread,
//    Finally do global checks
void delivery_cb(struct node *node, struct amcast_msg *msg, void* cb_arg) {
    //printf("[%u] {%u,%d} DELIVERED\n", node->id, msg->msg.mid.time, msg->msg.mid.id);
    //Let's check the integrity of delivered messages
    message_t *rep = (message_t *) cb_arg;
    int ret;
    if ((ret = msgcmp(&msg->msg, rep)) != 0) {
        printf("[%u] {%u, %d} Failed: the copy received is different: %u errors\n",
                node->id, msg->msg.mid.time, msg->msg.mid.id, ret);
	return;
    }
    delivered++;
    if(node->id == 4 && delivered == NUMBER_OF_MESSAGES) {
        struct enveloppe env = {
            .sid = node->id,
            .cmd_type = NEWLEADER,
            .cmd.newleader = {
                .ballot.id = node->id,
                .ballot.time = node->amcast->ballot.time + 1,
            }
        };
        send_to_group(node, &env, node->comm->groups[node->id]);
    }

}

//Scenario:
//  Start all the nodes
//  Once they are all connected, send some messages
//  Retrieve and check the integrity of the messages
//  Succesfully exits
int main(int argc, char *argv[]) {
    //TODO Create some better public config helper function to make it easier
    fill_cluster_config(&conf, NUMBER_OF_NODES, NUMBER_OF_GROUPS, ids, group_memberships, addresses, ports);

    //The plan is to start each node in a separate process
    if(argc<2) {
    id = -1;
    for(int i=0; i<NUMBER_OF_NODES; i++) {
	if ((pids[i] = fork()) < 0)
            error_at_line(EXIT_FAILURE, pids[i], __FILE__, __LINE__, "fctname");
	if (pids[i] == 0) {
	    id = i;
            break;
        }
    }
    } else {
        id = atoi(argv[1]);
    }

    struct enveloppe env = {
        .sid = -1,
        .cmd_type = MULTICAST,
        .cmd.multicast = {
            .mid = {0, 0},
            .destgrps_count = 2,
            .destgrps = {0, 1},
            .value = {
                .len = strlen("coucou"),
                .val = "coucou"
            }
        },
    };

    //Let's now create the nodes
    if (id != -1) {
        struct node *n = node_init(&conf, id, NULL, NULL, delivery_cb, &env.cmd.multicast);
	//Let's give them some AMCAST ROLES and fake proper states
        n->amcast->status = (id == 0 || id == 3) ? LEADER : FOLLOWER;
        n->amcast->ballot.id = (id < 3) ? 0 : 3;

        node_start(n);
        if (n->comm->accepted_count != NUMBER_OF_NODES)
            printf("[%u] Failed to connect to the whole cluster"
	           " (%u connected peers)\n", id, n->comm->accepted_count);
	//TODO Put a barrier here, so that nodes are not being freed until they were all stopped
	if(delivered != NUMBER_OF_MESSAGES)
            printf("[%u] Failed to deliver all messages: %lu delivered \n",
	           id, delivered);
        printf("[%u] Ending with ballot: %u,%d \n", n->id, n->amcast->ballot.time, n->amcast->ballot.id);
        node_free(n);
    }
    //Let the main process do some stuffs e.g. be a client
    else {
        //Let's wait until connections are successful
        sleep(2); //No longer possible to inspect nodes, memory is not shared
        //Connect as a client
	int sock[NUMBER_OF_NODES];
	for(int i=0; i<2; i++) {
            xid_t peer_id = i*3;
            sock[peer_id] = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in addr = {
	        .sin_family = AF_INET,
	        .sin_port = htons(conf.ports[peer_id]),
	        .sin_addr.s_addr = inet_addr(conf.addresses[peer_id])
            };
            connect(sock[peer_id], (struct sockaddr *) &addr, sizeof(addr));
	}
    //Let's send some messages
	for(int j=0; j<NUMBER_OF_MESSAGES; j++) {
            env.cmd.multicast.mid.time = j;
	    for(int i=0; i<2; i++) {
                xid_t peer_id = i*3;
                send(sock[peer_id], &env, sizeof(env), 0);
	    }
	}
        //Close the connections
	for(int i=0; i<2; i++) {
            xid_t peer_id = i*3;
            close(sock[peer_id]);
        }
	sleep(5);
        //Break the event loop for all nodes
        for(int i=0; i<NUMBER_OF_NODES; i++) {
            kill(pids[i], SIGHUP);
        }
        puts("The test is finished, if nothing was reported, it means it works!\n");
    }

    //Cleanup and exit
    return EXIT_SUCCESS;
}
