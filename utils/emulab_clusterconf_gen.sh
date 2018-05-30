#!/bin/sh

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
AMCAST_DIR=`dirname ${SCRIPTPATH}`
OUTFILE="${AMCAST_DIR}/bench/cluster.conf"
TMPFILE="/tmp/orderedIP"

TOPOMAP="/proj/RDMA-RCU/exp/McastIntercoDeploy/tbdata/topomap"
TOPOMAP_NODE_ID_PREFIX="node-"

NUMBER_OF_NODES_PER_GROUP=3


cat $TOPOMAP | grep $TOPOMAP_NODE_ID_PREFIX | sort -V | cut -d ":" -f2 > $TMPFILE

N_ID=0
G_ID=0
N_PORT=9000
echo -e "#GENERATED_EMULAB_CLUSTER_CONF" > $OUTFILE
echo -e "#N_ID\tG_ID\tIP_ADDR\tIP_LIST_PORT" >> $OUTFILE
while read N_IP ; do
    echo -e "${N_ID}\t${G_ID}\t${N_IP}\t${N_PORT}" >> $OUTFILE
    N_ID=$(( $N_ID + 1 ))
    [ $(( $N_ID % ${NUMBER_OF_NODES_PER_GROUP} )) -eq 0 ] && G_ID=$(( $G_ID + 1 ))
done < $TMPFILE

rm $TMPFILE
