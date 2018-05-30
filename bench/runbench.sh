#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

AMCAST_SSH_USER=anatole
AMCAST_SSH_HOST=localhost

AMCAST_DIR=`dirname ${SCRIPTPATH}`
AMCAST_BIN=${AMCAST_DIR}/bench/node-bench

AMCAST_BENCH_CLUSTER_CONF=${AMCAST_DIR}/bench/cluster.conf
AMCAST_BENCH_NUMBER_OF_GROUPS=5
AMCAST_BENCH_NUMBER_OF_NODES=15
AMCAST_BENCH_NUMBER_OF_CLIENTS=2

run_nodes() {
    NODES_COUNT=$1
    IS_CLIENT=$2

    NODE_IDS=`seq 0 $((${NODES_COUNT} - 1))`

    for id in ${NODE_IDS} ; do
        N_LINE_IN_CONF=$(( 3 + $id + ( $IS_CLIENT * $AMCAST_BENCH_NUMBER_OF_NODES ) ))

        AMCAST_SSH_HOST=`sed -n ${N_LINE_IN_CONF}p ${AMCAST_BENCH_CLUSTER_CONF} | cut -f3`
        AMCAST_DEPLOY="ssh ${AMCAST_SSH_USER}@${AMCAST_SSH_HOST}"

        AMCAST_CMD=( ${AMCAST_DEPLOY} "${AMCAST_BIN} ${id} ${AMCAST_BENCH_NUMBER_OF_NODES} ${AMCAST_BENCH_NUMBER_OF_GROUPS} ${AMCAST_BENCH_NUMBER_OF_CLIENTS} $IS_CLIENT < ${AMCAST_BENCH_CLUSTER_CONF}")
        AMCAST_RETRIEVE_REPORT_CMD=( ${AMCAST_DEPLOY} "mkdir -p ~/log && cp -v /tmp/report* ~/log/ && rm /tmp/report*" )
        "${AMCAST_CMD[@]}" && [ $IS_CLIENT -eq 0 ] && "${AMCAST_RETRIEVE_REPORT_CMD[@]}" &
        AMCAST_FORKED_PIDS[${id}]=$!
    done
}

wait_for_termination() {
    PIDS=$1

    for pid in ${PIDS[@]} ; do
        wait $pid
    done
}

run_nodes $AMCAST_BENCH_NUMBER_OF_NODES 0

sleep 1

run_nodes $AMCAST_BENCH_NUMBER_OF_CLIENTS 1

wait_for_termination $AMCAST_FORKED_PIDS
