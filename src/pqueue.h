#ifndef _PQUEUE_H_
#define _PQUEUE_H_

typedef void pq_pri_t;
typedef void pq_val_t;
typedef int (*pq_pricmp_fun)(pq_pri_t *, pq_pri_t *);
typedef int (*pq_traverse_fun)(pq_pri_t *, pq_val_t *, void *);

struct pqueue;
typedef struct pqueue pqueue_t;

pqueue_t *pqueue_init(pq_pricmp_fun pricmp);
int pqueue_free(pqueue_t *pq);
int pqueue_push(pqueue_t *pq, pq_val_t *val, pq_pri_t *pri);
int pqueue_remove(pqueue_t *pq, pq_pri_t *pri);
pq_pri_t *pqueue_lowest_priority(pqueue_t *pq);
pq_val_t *pqueue_peek(pqueue_t *pq);
pq_val_t *pqueue_pop(pqueue_t *pq);
unsigned int pqueue_size(pqueue_t *pq);
void pqueue_foreach(pqueue_t *pq, pq_traverse_fun cb, void *cb_arg);

#endif
