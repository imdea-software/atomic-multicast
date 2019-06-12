#!/bin/sh

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

AMCAST_TMUX_SESSION="AMCAST"

AMCAST_DIR=`dirname ${SCRIPTPATH}`
AMCAST_BIN="${AMCAST_DIR}/bench/node-bench"
AMCAST_DEPLOY=""

AMCAST_BENCH_CLUSTER_CONF="${AMCAST_DIR}/bench/cluster.conf"
AMCAST_BENCH_NUMBER_OF_GROUPS=5
AMCAST_BENCH_NUMBER_OF_NODES=15
AMCAST_BENCH_NUMBER_OF_CLIENTS=2

run_nodes() {
    NODES_COUNT=$1
    IS_CLIENT=$2

    NODE_IDS=`seq 0 $((${NODES_COUNT} - 1))`
    [ $IS_CLIENT -eq 0 ] && TMUX_WINDOW_NAME_PREFIX=server_ || TMUX_WINDOW_NAME_PREFIX=client_

    for id in ${NODE_IDS} ; do
        N_LINE_IN_CONF=$(( 3 + $id + ( $IS_CLIENT * $AMCAST_BENCH_NUMBER_OF_NODES ) ))

        AMCAST_SSH_HOST=`sed -n ${N_LINE_IN_CONF}p ${AMCAST_BENCH_CLUSTER_CONF} | cut -f3`
        AMCAST_DEPLOY="ssh ${AMCAST_SSH_USER}@${AMCAST_SSH_HOST}"

        tmux new-window -n ${TMUX_WINDOW_NAME_PREFIX}${id}
        tmux send-keys " ${AMCAST_DEPLOY}" Enter
        tmux send-keys " ${AMCAST_BIN} \
            ${id} \
            ${AMCAST_BENCH_NUMBER_OF_NODES} \
            ${AMCAST_BENCH_NUMBER_OF_GROUPS} \
            ${AMCAST_BENCH_NUMBER_OF_CLIENTS} \
            $IS_CLIENT < ${AMCAST_BENCH_CLUSTER_CONF}" Enter
    done
}

run_px_nodes() {
    GROUPS_COUNT=$1

    GROUP_IDS=`seq 0 $((${GROUPS_COUNT} - 1))`
    TMUX_WINDOW_NAME_PREFIX=px_

    for gid in ${GROUP_IDS} ; do
        for nid in `seq 0 2` ; do
            HOST_ID=$(( ( $gid * 3 ) + ${nid} ))

            tmux new-window -n ${TMUX_WINDOW_NAME_PREFIX}${gid}_$nid
            tmux send-keys " ssh node-$(( $HOST_ID + 1))" Enter
            tmux send-keys " export LD_LIBRARY_PATH=/usr/local/lib" Enter
            tmux send-keys " /users/lefort_a/libmcast/build/sample/proposer-acceptor $nid ${AMCAST_DIR}/bench/mcast_conf/paxos-6g3p-group${gid}.conf > /tmp/px${gid}${nid}.log" Enter
        done
    done
}

run_mcast_nodes() {
    GROUPS_COUNT=$1

    GROUP_IDS=`seq 0 $((${GROUPS_COUNT} - 1))`
    TMUX_WINDOW_NAME_PREFIX=n_

    for gid in ${GROUP_IDS} ; do
        for nid in `seq 0 2` ; do
            HOST_ID=$(( ( $gid * 3 ) + ${nid} ))

            tmux new-window -n ${TMUX_WINDOW_NAME_PREFIX}${gid}_$nid
            tmux send-keys " ssh node-$(( $HOST_ID + 1))" Enter
            tmux send-keys " export LD_LIBRARY_PATH=/usr/local/lib" Enter
            tmux send-keys " /users/lefort_a/libmcast/build/sample/node-simple -n $nid -g $gid -c ${AMCAST_DIR}/bench/mcast_conf/mcast-6g3p.conf -s amcast -p ${AMCAST_DIR}/bench/mcast_conf/paxos-6g3p-group${gid}.conf" Enter
        done
    done
}

tmux -2 new-session -d -s $AMCAST_TMUX_SESSION

run_nodes $AMCAST_BENCH_NUMBER_OF_NODES 0

#run_px_nodes $AMCAST_BENCH_NUMBER_OF_GROUPS
#sleep 10
#run_mcast_nodes $AMCAST_BENCH_NUMBER_OF_GROUPS

sleep 10

#run_nodes $AMCAST_BENCH_NUMBER_OF_CLIENTS 1

tmux new-window -n client_0
tmux send-keys " ssh node-19" Enter
tmux send-keys " export LD_LIBRARY_PATH=/usr/local/lib" Enter
for i in `seq 1 $(( $AMCAST_BENCH_NUMBER_OF_CLIENTS / 2))` ; do
    tmux send-keys " (${AMCAST_BIN} ${i} ${AMCAST_BENCH_NUMBER_OF_NODES} ${AMCAST_BENCH_NUMBER_OF_GROUPS} ${AMCAST_BENCH_NUMBER_OF_CLIENTS} 1 < $AMCAST_BENCH_CLUSTER_CONF)> /dev/null &" Enter
    #tmux send-keys " (${AMCAST_BIN} ${i} ${AMCAST_BENCH_NUMBER_OF_NODES} ${AMCAST_BENCH_NUMBER_OF_GROUPS} ${AMCAST_BENCH_NUMBER_OF_CLIENTS} 1 < ${AMCAST_DIR}/bench/mcast_conf/alt_cluster.conf)> /dev/null &" Enter
done

tmux new-window -n client_1
tmux send-keys " ssh node-20" Enter
tmux send-keys " export LD_LIBRARY_PATH=/usr/local/lib" Enter
for i in `seq $(( ( $AMCAST_BENCH_NUMBER_OF_CLIENTS / 2 ) + 1 )) $(( $AMCAST_BENCH_NUMBER_OF_CLIENTS ))` ; do
    tmux send-keys " (${AMCAST_BIN} ${i} ${AMCAST_BENCH_NUMBER_OF_NODES} ${AMCAST_BENCH_NUMBER_OF_GROUPS} ${AMCAST_BENCH_NUMBER_OF_CLIENTS} 1 < $AMCAST_BENCH_CLUSTER_CONF)> /dev/null &" Enter
    #tmux send-keys " (${AMCAST_BIN} ${i} ${AMCAST_BENCH_NUMBER_OF_NODES} ${AMCAST_BENCH_NUMBER_OF_GROUPS} ${AMCAST_BENCH_NUMBER_OF_CLIENTS} 1 < ${AMCAST_DIR}/bench/mcast_conf/alt_cluster.conf)> /dev/null &" Enter
done

#tmux -2 attach-session -t $AMCAST_TMUX_SESSION
