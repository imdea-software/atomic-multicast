#define NUMBER_OF_NODES 6
#define LOCALHOST "127.0.0.1"

#define CLUSTER_ID	{0        , 1        , 2        , 3        , 4        , 5         };
#define CLUSTER_GRP	{0        , 0        , 0        , 1        , 1        , 1         };
#define CLUSTER_ADDR	{LOCALHOST, LOCALHOST, LOCALHOST, LOCALHOST, LOCALHOST, LOCALHOST };
#define CLUSTER_PORTS	{9000     , 9001     , 9002     , 9003     , 9004     , 9005      };

struct cluster_config default_test_conf(void);
