#!/bin/sh

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

AMCAST_TMUX_SESSION="AMCAST"

AMCAST_DIR=`dirname ${SCRIPTPATH}`
AMCAST_BIN="${AMCAST_DIR}/bench/node-bench"
AMCAST_DEPLOY=""

AMCAST_BENCH_CLUSTER_CONF="${AMCAST_DIR}/bench/cluster.conf"
AMCAST_BENCH_NUMBER_OF_GROUPS=10
AMCAST_BENCH_NUMBER_OF_NODES=30
AMCAST_BENCH_NUMBER_OF_CLIENTS=$2
AMCAST_BENCH_NUMBER_OF_DESTGROUPS=$1
PROTO=$3

run_nodes() {
    NODES_COUNT=$1
    IS_CLIENT=$2

    NODE_IDS=`seq 0 $((${NODES_COUNT} - 1))`
    [ $IS_CLIENT -eq 0 ] && TMUX_WINDOW_NAME_PREFIX=server_ || TMUX_WINDOW_NAME_PREFIX=client_

    for id in ${NODE_IDS} ; do
        N_LINE_IN_CONF=$(( 3 + $id + ( $IS_CLIENT * $AMCAST_BENCH_NUMBER_OF_NODES ) ))
        gid=`sed -n ${N_LINE_IN_CONF}p ${AMCAST_BENCH_CLUSTER_CONF} | cut -f2`

	AMCAST_SSH_USER="anatole"
        AMCAST_SSH_HOST=`sed -n ${N_LINE_IN_CONF}p ${AMCAST_BENCH_CLUSTER_CONF} | cut -f3`
        AMCAST_DEPLOY="ssh ${AMCAST_SSH_USER}@${AMCAST_SSH_HOST}"

        tmux new-window -n ${TMUX_WINDOW_NAME_PREFIX}${id}
        tmux send-keys " ${AMCAST_DEPLOY}" Enter
        tmux send-keys " export LD_LIBRARY_PATH=/usr/local/lib" Enter
        tmux send-keys " nice -n 20 ${AMCAST_BIN} \
            ${id} \
            ${gid} \
            ${AMCAST_BENCH_NUMBER_OF_NODES} \
            ${AMCAST_BENCH_NUMBER_OF_GROUPS} \
            ${AMCAST_BENCH_NUMBER_OF_CLIENTS} \
            ${AMCAST_BENCH_NUMBER_OF_DESTGROUPS} \
            $IS_CLIENT amcast < ${AMCAST_BENCH_CLUSTER_CONF}" Enter #&& echo success > /tmp/node.${id}.log" Enter
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
            tmux send-keys " nice -n 20 /home/anatole/libmcast/build/sample/proposer-acceptor $nid ${AMCAST_DIR}/bench/mcast_conf/paxos-6g3p-group${gid}.conf > /tmp/px${gid}${nid}.log" Enter
        done
    done
}

run_mcast_nodes() {
    GROUPS_COUNT=$1

    GROUP_IDS=`seq 0 $((${GROUPS_COUNT} - 1))`
    TMUX_WINDOW_NAME_PREFIX=n_

    [ $PROTO = "basecast" ] && PROTO_OPT=amcast
    [ $PROTO = "famcast" ] && PROTO_OPT=famcast

    for gid in ${GROUP_IDS} ; do
        for nid in `seq 0 2` ; do
            HOST_ID=$(( ( $gid * 3 ) + ${nid} ))

            tmux new-window -n ${TMUX_WINDOW_NAME_PREFIX}${gid}_$nid
            tmux send-keys " ssh node-$(( $HOST_ID + 1))" Enter
            tmux send-keys " export LD_LIBRARY_PATH=/usr/local/lib" Enter
            tmux send-keys " nice -n 20 /home/anatole/libmcast/build/sample/node-simple -n $nid -g $gid -c ${AMCAST_DIR}/bench/mcast_conf/mcast-6g3p.conf -s $PROTO_OPT -p ${AMCAST_DIR}/bench/mcast_conf/paxos-6g3p-group${gid}.conf && echo success > /tmp/node.${HOST_ID}.log " Enter
        done
    done
}

tmux -2 new-session -d -s $AMCAST_TMUX_SESSION

if [ $PROTO = "amcast" ] ; then
    run_nodes $AMCAST_BENCH_NUMBER_OF_NODES 0
elif [ $PROTO = "basecast" ] || [ $PROTO = "famcast" ] ; then
    run_px_nodes $AMCAST_BENCH_NUMBER_OF_GROUPS
    sleep 10
    run_mcast_nodes $AMCAST_BENCH_NUMBER_OF_GROUPS
fi

sleep 10

start_cid=0
end_cid=0
client_hosts=9
#client_hosts=$AMCAST_BENCH_NUMBER_OF_GROUPS
for gid in `seq 1 $client_hosts` ; do
    chid=$(( $AMCAST_BENCH_NUMBER_OF_NODES + $gid ))
    start_cid=$(( $end_cid + 1 ))
    end_cid=$(( ( $AMCAST_BENCH_NUMBER_OF_CLIENTS * $gid + $client_hosts - 1 ) / $client_hosts ))
    n_clients=$(( $end_cid - ( $start_cid - 1 ) ))
    [ $n_clients -lt 1 ] && continue
    [ $end_cid -gt $AMCAST_BENCH_NUMBER_OF_CLIENTS ] && break
    tmux new-window -n client_$gid
    tmux send-keys " ssh node-$chid" Enter
    tmux send-keys " export LD_LIBRARY_PATH=/usr/local/lib" Enter
    tmux send-keys " nice -n 20 ${AMCAST_BIN} $start_cid $(( $gid - 1)) ${AMCAST_BENCH_NUMBER_OF_NODES} ${AMCAST_BENCH_NUMBER_OF_GROUPS} ${client_hosts} ${AMCAST_BENCH_NUMBER_OF_DESTGROUPS} $n_clients $PROTO < $AMCAST_BENCH_CLUSTER_CONF" Enter
done

exit

#tmux -2 attach-session -t $AMCAST_TMUX_SESSION
