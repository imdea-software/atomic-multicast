#include <stdlib.h>
#include <stdio.h>

#include "pqueue.h"
#include "amcast_types.h"

#define NUMBER_OF_MESSAGES 20
#define NUMBER_OF_GROUPS 3


int main() {
    g_uid_t tss[NUMBER_OF_MESSAGES];
    for(int i=0; i<NUMBER_OF_MESSAGES; i++) {
        g_uid_t ts = {i, i % NUMBER_OF_GROUPS};
        tss[i] = ts;
    }

    pqueue_t *pq = pqueue_init((pq_pricmp_fun) paircmp);

    for(int i=0; i<NUMBER_OF_MESSAGES; i++)
        pqueue_push(pq, &(tss[i].time), tss+i);
    
    unsigned int pq_size;
    if((pq_size = pqueue_size(pq)) != NUMBER_OF_MESSAGES)
        printf("FAILED: the pqueue does not hold the right count of elements: "
			"%u instead of %u\n", pq_size, NUMBER_OF_MESSAGES);

    g_uid_t *min = pqueue_peek(pq);
    if(paircmp(tss, min) != 0)
        printf("FAILED: the pqueue min is not at the first position: "
			"%u instead of %u\n", min->time, tss[0].time);
    
    pqueue_remove(pq, tss+NUMBER_OF_MESSAGES-1);
    if(pqueue_size(pq) == NUMBER_OF_MESSAGES)
        printf("FAILED: the pqueue has the same size after removing an element\n");
    
    for(int i=0; i<NUMBER_OF_MESSAGES-1; i++) {
        g_uid_t *front = pqueue_pop(pq);
        if(paircmp(front, tss+i) != 0)
            printf("FAILED: the pqueue %u-th element is out of order ; got %u instead!\n", i, front->time);
    }

    pqueue_free(pq);

    return EXIT_SUCCESS;
}
