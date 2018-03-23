#define NUMBER_OF_NODES 6
#define LOCALHOST "127.0.0.1"

#define CLUSTER_ID	{0        , 1        , 2        , 3        , 4        , 5         };
#define CLUSTER_GRP	{0        , 0        , 0        , 1        , 1        , 1         };
#define CLUSTER_ADDR	{LOCALHOST, LOCALHOST, LOCALHOST, LOCALHOST, LOCALHOST, LOCALHOST };
#define CLUSTER_PORTS	{9000     , 9001     , 9002     , 9003     , 9004     , 9005      };

//CREATE a default config ready to use

struct cluster_config conf;

id_t id[NUMBER_OF_NODES] = CLUSTER_ID;
id_t group_membership[NUMBER_OF_NODES] = CLUSTER_GRP;
address_t addresses[NUMBER_OF_NODES] = CLUSTER_ADDR;
port_t ports[NUMBER_OF_NODES] = CLUSTER_PORTS;

void fill_cluster_config(struct cluster_config *conf, int size, id_t *ids, id_t *group_membership,
		address_t *addresses, port_t *ports) {
    conf->size = size;
    conf->id = ids;
    conf->group_membership = group_membership;
    conf->addresses = addresses;
    conf->ports = ports;
};
