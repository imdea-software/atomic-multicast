#ifndef _AMCAST_TYPES_H_
#define _AMCAST_TYPES_H_

#include <stdlib.h>

#include "types.h"

typedef struct payload {
    unsigned int	len;
    char		*val;
} payload_t;

typedef struct message {
    m_uid_t 		mid;
    unsigned int 	destgrps_count;
    id_t 		destgrps[10];
    struct payload	value;
} message_t;

#endif
