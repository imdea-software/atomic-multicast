#include <gmodule.h>

#include "pqueue.h"


struct pqueue {
    unsigned int	size;
    pq_pri_t		*lowest_pri;
    pq_pricmp_fun	pricmp;
    GTree		*tree;
};

pqueue_t *pqueue_init(void* pricmp) {
}

int pqueue_free(pqueue_t *pq) {
}

int pqueue_push(pqueue_t *pq, void *val, void* pri) {
}

void *pqueue_peek(pqueue_t *pq) {
}

void *pqueue_pop(pqueue_t *pq) {
}
