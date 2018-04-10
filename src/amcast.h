#ifndef _AMCAST_H_
#define _AMCAST_H_

#include "types.h"
#include "node.h"
#include "amcast_types.h"

void dispatch_amcast_command(struct node *node, struct enveloppe *env);

#endif
