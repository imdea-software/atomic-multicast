#!/bin/bash

runall() {
    pids=()
    for i in {1..39} ; do ssh node-$i $@ & pids+=($!) ; done
    for pid in ${pids[*]}; do wait $pid ; done
}

case $1 in
killall)
	runall sudo killall -s $2 node-bench node-simple proposer-acceptor memcheck-amd64- gdb valgrind
	;;
rm)
	runall rm -f /tmp/{node,client}.*.log
	;;
*)
	runall $@
	;;
esac
