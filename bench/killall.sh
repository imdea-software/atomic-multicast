#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

AMCAST_DIR=`dirname ${SCRIPTPATH}`
AMCAST_BIN=${AMCAST_DIR}/bench/node-bench

AMCAST_BENCH_CLUSTER_CONF=${AMCAST_DIR}/bench/cluster.conf
AMCAST_BENCH_NUMBER_OF_GROUPS=6
AMCAST_BENCH_NUMBER_OF_NODES=18
AMCAST_BENCH_NUMBER_OF_CLIENTS_NODES=3
AMCAST_STORE_HOST=$(( $AMCAST_BENCH_NUMBER_OF_NODES + $AMCAST_BENCH_NUMBER_OF_CLIENTS_NODES ))

PIDS=()

kill_nodes() {
    NODES_COUNT=$1
    IS_CLIENT=$2

    NODE_IDS=`seq 0 $((${NODES_COUNT} - 1))`

    for id in ${NODE_IDS} ; do
	AMCAST_SSH_USER="lefort_a"
	AMCAST_SSH_HOST=`sed -n $(( 3 + $id + ( $IS_CLIENT * $AMCAST_BENCH_NUMBER_OF_NODES )  ))p ${AMCAST_BENCH_CLUSTER_CONF} | cut -f3`
        AMCAST_DEPLOY="ssh ${AMCAST_SSH_USER}@${AMCAST_SSH_HOST}"
        [ $IS_CLIENT -eq 0 ] && FILENAME=node || FILENAME=client
        #AMCAST_CMD=( ${AMCAST_DEPLOY} "killall -s 1 ${AMCAST_BIN}")
        #AMCAST_CMD=( ${AMCAST_DEPLOY} "mv /tmp/${FILENAME}.* /proj/RDMA-RCU/tmp/mcast/log/")
        AMCAST_CMD=( ${AMCAST_DEPLOY} "rsync -az /tmp/${FILENAME}.* node-$AMCAST_STORE_HOST:/mnt/log/ && rm /tmp/${FILENAME}.*")
        "${AMCAST_CMD[@]}" &
        PIDS+=($!)
    done
}

kill_nodes $AMCAST_BENCH_NUMBER_OF_NODES 0

kill_nodes $AMCAST_BENCH_NUMBER_OF_CLIENTS_NODES 1

for pid in ${PIDS[*]}; do wait $pid ; done
