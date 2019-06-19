#!/bin/sh

command=$1

n_regions=3 #changing this will not magically increase the number of regions

topomap_lan="big-lan"
topomap_prefix="node-"
topomap="/var/emulab/boot/topomap"
nodes_ip=`cat $topomap | grep $topomap_prefix | sort -V | sed s/^.*$topomap_lan:// | cut -d ' ' -f1`

myhost=`hostname | cut -d '.' -f1`
myip=`cat $topomap | grep ${myhost}, | sed s/^.*${topomap_lan}:// | cut -d ' ' -f1`
interface=`ip a | grep -B2 $myip | head -n 1 | cut -d ':' -f2 | sed s/^\ //g`
mypid=$(( `echo $myip | cut -d '.' -f4` - 2 ))
if [ $mypid -lt 18 ] ; then
    myregion=$(( ( $mypid / $n_regions + $mypid ) % $n_regions ))
else
    #myregion=1
    myregion=$(( $mypid % $n_regions ))
fi

stop() {
    tc qdisc del dev $interface root
}

start() {
    tc qdisc add dev $interface root handle 1: prio

    tc qdisc add dev $interface parent 1:1 handle 2: netem delay 0ms
    tc qdisc add dev $interface parent 1:2 handle 3: netem delay 0ms
    tc qdisc add dev $interface parent 1:3 handle 4: netem delay 75ms 5ms 25% distribution normal

    for ip in $nodes_ip ; do
        pid=$(( `echo $ip | cut -d '.' -f4` - 2 ))
	if [ $pid -lt 18 ] ; then
            region=$(( ( $pid / $n_regions + $pid ) % $n_regions ))
	else
            #region=1
            region=$(( $pid % $n_regions ))
	fi
        signed_distance=$(( $region - $myregion ))
        distance=${signed_distance#-}
        band=$(( $distance + 1 ))
	[ $myregion -eq $region ] && band=1
	[ $myregion -ne $region ] && band=3
	[ $pid -ge 18 ]  && band=2
	[ $mypid -ge 18 ] && band=2
        tc filter add dev $interface protocol ip parent 1:0 prio 1 u32 match ip dst $ip flowid 1:$band
        #tc filter add dev $interface protocol ip parent 1:0 prio 1 u32 match ip dst $ip match ip dport 22 0xffff flowid 1:1
    done
}

case $command in
    stop)
        stop ;;
    *)
        stop
        start ;;
esac
