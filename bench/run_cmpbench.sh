#!/bin/bash

for protocol in amcast famcast basecast ; do

tmpdir="/mnt/log"
destdir_base="/mnt/vwan_10ch_${protocol}"

case $protocol in
    basecast|famcast)
        bin="proposer-acceptor node-simple"
        signal="2"
        terminate_wait=25
        timeout=200
        ;;
    amcast)
        bin="node-bench"
        signal="1"
        terminate_wait=5
        timeout=220
        ;;
    *) echo "ERROR: wrong protocol" ; exit 1 ;;
esac

exp_terminate() {
    pids=()
    for i in `seq 31 40` ; do ssh node-$i killall node-bench & pids+=($!) ; done
    for pid in ${pids[*]}; do wait $pid ; done

    pids=()
    for i in `seq 1 30` ; do ssh node-$i killall -s $signal $bin & pids+=($!) ; done
    for pid in ${pids[*]}; do wait $pid ; done
}

exp_run() {
    destgrps=$1
    n_clients=$2

    ret=0

    destdir="${destdir_base}/log.10.30.${destgrps}.${n_clients}.10.24000000.1"
    mkdir -p $destdir
    rm ${tmpdir}/{client,node}.*.log

    /nfs/libamcast/bench/runbench_tmux.sh $destgrps $n_clients $protocol

    sleep $timeout

    exp_terminate
    sleep $terminate_wait

    /nfs/libamcast/bench/killall.sh

    [ `ls ${tmpdir}/client.*.log | wc -l` -ne $n_clients ] && echo "ERROR: some clients have failed" && ret=1
    [ `ls ${tmpdir}/node.*.log | wc -l` -ne 30 ] && echo "ERROR: some nodes have failed" && ls ${tmpdir}/node.*.log && ret=1

    if [ $ret -eq 0 ] ; then
        cat ${tmpdir}/client.*.log > ${destdir}/all.clients.log
        cp ${tmpdir}/node.*.log ${destdir}/
    fi
    tmux kill-session -t AMCAST

    return $ret
}


exp_terminate

echo "Start of data gathering for cmp experiments"
for destgrps in 1 2 3 4 6 8 10 ; do
    [ $destgrps -eq 1 ] && min_clients=2000 && max_clients=24000 && inc_clients=2000
    [ $destgrps -eq 2 ] && min_clients=1000 && max_clients=12000 && inc_clients=1000
    [ $destgrps -eq 3 ] && min_clients=1000 && max_clients=8000 && inc_clients=1000
    [ $destgrps -eq 4 ] && min_clients=1000 && max_clients=6000 && inc_clients=1000
    [ $destgrps -eq 6 ] && min_clients=500 && max_clients=4000 && inc_clients=500
    [ $destgrps -eq 8 ] && min_clients=500 && max_clients=4000 && inc_clients=500
    [ $destgrps -eq 10 ] && min_clients=500 && max_clients=4000 && inc_clients=500
    for n_clients in `echo 1 && seq $min_clients $inc_clients $max_clients` ; do
        while true ; do
            echo "running $protocol exp for $destgrps destination groups and $n_clients clients"
            exp_run $destgrps $n_clients
            [ $? -eq 0 ] && break;
        done
        echo "done with exp for $destgrps destination groups and $n_clients clients"
    done
done

exp_terminate

pids=()
for i in `seq 1 30` ; do ssh node-$i killall 2 node-bench node-simple proposer-acceptor & pids+=($!) ; done
for pid in ${pids[*]}; do wait $pid ; done

tmux kill-session -t AMCAST
rm ${tmpdir}/{client,node}.*.log

done
