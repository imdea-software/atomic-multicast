clean() {
	rm /tmp/{client,node}.*.log
	killall node-bench memcheck-amd64- gdb
	tmux list-session | grep AMCAST && tmux kill-session -t AMCAST
}

successes=0
while true ; do
	clean

	/home/anatole/Documents/libamcast/bench/runbench_tmux.sh
	sleep 48
	t_node=`ls /tmp/node.*.log | wc -l`
	t_client=`ls /tmp/client.*.log | wc -l`
	m_clients=`cut -f1 /tmp/client.*.log | wc -l`
	mu_clients=`cut -f1 /tmp/client.*.log | uniq | wc -l`
	gts_clients=`cut -f4 /tmp/client.*.log | wc -l`
	gtsu_clients=`cut -f4 /tmp/client.*.log | uniq | wc -l`
	if [ $t_node -eq 18 ] && [ $t_client -eq 10 ] && [ $m_clients -eq $mu_clients ] && [ $gts_clients -eq $gtsu_clients ] ; then
		clean
		sleep 2
		successes=$(( successes + 1 ))
		echo CONTINUING after $successes
		continue
	else
		echo FAILURE after $successes - exiting\
			t_node $t_node - t_client $t_client\
			m_clients $m_clients - mu_clients $mu_clients\
			gts_clients $gts_clients - gtsu_clients $gtsu_clients
		exit 1
	fi
done
