#include "types.h"

struct cluster_config {
    unsigned int	size;
    id_t		*id;
    id_t 		*group_membership;
    address_t 		*addresses;
    port_t 		*ports;
};
