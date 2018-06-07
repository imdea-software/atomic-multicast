#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

AMCAST_DIR=`dirname ${SCRIPTPATH}`
AMCAST_BIN=${AMCAST_DIR}/bench/node-bench

AMCAST_BENCH_CLUSTER_CONF=${AMCAST_DIR}/bench/cluster.conf
AMCAST_BENCH_NUMBER_OF_GROUPS=4
AMCAST_BENCH_NUMBER_OF_NODES=12
AMCAST_BENCH_NUMBER_OF_CLIENTS=3

kill_nodes() {
    NODES_COUNT=$1
    IS_CLIENT=$2

    NODE_IDS=`seq 0 $((${NODES_COUNT} - 1))`

    for id in ${NODE_IDS} ; do
	AMCAST_SSH_USER="lefort_a"
	AMCAST_SSH_HOST=`sed -n $(( 3 + $id + ( $IS_CLIENT * $AMCAST_BENCH_NUMBER_OF_NODES )  ))p ${AMCAST_BENCH_CLUSTER_CONF} | cut -f3`
        AMCAST_DEPLOY="ssh ${AMCAST_SSH_USER}@${AMCAST_SSH_HOST}"
        AMCAST_CMD=( ${AMCAST_DEPLOY} "rm /tmp/report.* ; killall -s 1 ${AMCAST_BIN}")
        "${AMCAST_CMD[@]}"
    done
}

kill_nodes $AMCAST_BENCH_NUMBER_OF_NODES 0

kill_nodes $AMCAST_BENCH_NUMBER_OF_CLIENTS 1
