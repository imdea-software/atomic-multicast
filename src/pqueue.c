#include <gmodule.h>
#include <stdio.h>

#include "pqueue.h"

struct pqueue {
    unsigned int	size;
    pq_pri_t		*lowest_pri;
    pq_pricmp_fun	pricmp;
    GTree		*tree;
};



static int update_min(pq_pri_t *pri, pq_val_t *val, pqueue_t *pq) {
     pq_pri_t *prev_lowest = pq->lowest_pri;
     if(pq->pricmp(pri, pq->lowest_pri) > 0)
         pq->lowest_pri = pri;
     return (pq->pricmp(pq->lowest_pri, prev_lowest) != 0) ? 1 : 0;
}

pqueue_t *pqueue_init(pq_pricmp_fun pricmp) {
    pqueue_t *pq = malloc(sizeof(pqueue_t));
    pq->size = 0;
    pq->lowest_pri = NULL;
    pq->pricmp = pricmp;
    pq->tree = g_tree_new((GCompareFunc) pricmp);
    return pq;
}

int pqueue_free(pqueue_t *pq) {
    g_tree_destroy(pq->tree);
    free(pq);
    return 0;
}

int pqueue_push(pqueue_t *pq, pq_val_t *val, pq_pri_t *pri) {
    /*
    if(g_tree_lookup(pq->tree, pri) != NULL)
        return -1;
    */
    if(pq->lowest_pri == NULL || pq->pricmp(pri, pq->lowest_pri) <= 0)
        pq->lowest_pri = pri;
    g_tree_insert(pq->tree, pri, val);
    pq->size += 1;
    return 0;
}

int pqueue_remove(pqueue_t *pq, pq_pri_t *pri) {
    /*
    if(pq->lowest_pri == NULL)
        puts("ERROR: trying to remove from empty pqueue");
    */
    if(pq->pricmp(pri, pq->lowest_pri) == 0) {
        pq_val_t *poped = pqueue_pop(pq);
        return (poped != NULL) ? 1 : 0;
    }
    int ret = g_tree_remove(pq->tree, pri);
    pq->size -= ret;
    return ret;
}

/*
pq_val_t *pqueue_peek(pqueue_t *pq) {
    pq_val_t *val = NULL;
    pq_pri_t *pri = NULL;
    if (pq->lowest_pri != NULL)
        if(!g_tree_lookup_extended(pq->tree, pq->lowest_pri, &pri, &val))
            puts("ERROR: lowest_pri not found in pqueue but pq->lowest_pri not NULL");
    if (pq->lowest_pri != NULL && !(pri != NULL && pq->pricmp(pri, pq->lowest_pri) == 0 && val != NULL)) {
        printf("ERROR: pqueue still has a lowest pri and size %u,"
                " but can not be found in underlying tree of size %u\n", pq->size, g_tree_nnodes(pq->tree));
        //exit(EXIT_FAILURE);
    }
    return val;
}
*/

pq_val_t *pqueue_peek(pqueue_t *pq) {
    return (pq->lowest_pri != NULL) ? g_tree_lookup(pq->tree, pq->lowest_pri) : NULL;
}

pq_pri_t *pqueue_lowest_priority(pqueue_t *pq) {
    return pq->lowest_pri;
}

pq_val_t *pqueue_pop(pqueue_t *pq) {
    /*
    if(pq->lowest_pri == NULL)
        puts("ERROR: trying to pop from empty pqueue");
    */
    pq_val_t *min = pqueue_peek(pq);
    if(!g_tree_remove(pq->tree, pq->lowest_pri))
        min = NULL;
    if(min != NULL) {
        pq->size -= 1;
        if(pq->size == 0) {
            pq->lowest_pri = NULL;
        } else {
        //Because it iterates in order, it stops after the first iteration
            g_tree_foreach(pq->tree, (GTraverseFunc) update_min, (void*) pq);
        }
    }
    return min;
}

unsigned int pqueue_size(pqueue_t *pq) {
    return pq->size;
}

void pqueue_foreach(pqueue_t *pq, pq_traverse_fun cb, void *cb_arg) {
    g_tree_foreach(pq->tree, (GTraverseFunc) cb, cb_arg);
}
