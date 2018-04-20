#ifndef _CLUSTER_H_
#define _CLUSTER_H_

#include "types.h"

struct cluster_config {
    unsigned int	size;
    unsigned int	groups_count;
    id_t		*id;
    id_t 		*group_membership;
    address_t 		*addresses;
    port_t 		*ports;
};

#endif
