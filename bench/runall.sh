#!/bin/bash

runall() {
    pids=()
    for i in {1..40} ; do ssh node-$i $@ & pids+=($!) ; done
    for pid in ${pids[*]}; do wait $pid ; done
}

case $1 in
killall)
	runall sudo killall -s $2 node-bench node-simple proposer-acceptor
	;;
rm)
	runall rm -f /tmp/{node,client}.*.log
	;;
*)
	runall $@
	;;
esac
