#!/bin/sh

command=$1

n_regions=3 #changing this will not magically increase the number of regions
n_nodes=30

topomap_lan="link-1"
topomap_prefix="node-"
topomap="/var/emulab/boot/topomap"
nodes_ip=`cat $topomap | grep $topomap_prefix | sort -V | sed s/^.*$topomap_lan:// | cut -d ' ' -f1`

myhost=`hostname | cut -d '.' -f1`
myip=`cat $topomap | grep ${myhost}, | sed s/^.*${topomap_lan}:// | cut -d ' ' -f1`
interface=`ip a | grep -B2 $myip | head -n 1 | cut -d ':' -f2 | sed s/^\ //g | cut -d "@" -f1`
mypid=$(( `echo $myip | cut -d '.' -f4` - 1 ))
if [ $mypid -lt $n_nodes ] ; then
    myregion=$(( ( $mypid / $n_regions + $mypid ) % $n_regions ))
else
    #myregion=1
    myregion=$(( $mypid % $n_regions ))
fi
case $myregion in
    0)
        lat1=30
	lat2=65
        ;;
    1)
        lat1=30
	lat2=65
        ;;
    2)
        lat1=45
	lat2=65
        ;;
esac

stop() {
    tc qdisc del dev $interface root
}

start() {
    tc qdisc add dev $interface root handle 1: htb

    tc class add dev $interface parent 1: classid 1:1 htb rate 1Gbps
    tc class add dev $interface parent 1: classid 1:2 htb rate 1Gbps
    tc class add dev $interface parent 1: classid 1:3 htb rate 1Gbps

    tc qdisc add dev $interface parent 1:1 sfq
    tc qdisc add dev $interface parent 1:2 netem delay ${lat1}ms 2ms 25% distribution normal
    tc qdisc add dev $interface parent 1:3 netem delay ${lat2}ms 2ms 25% distribution normal
    #tc qdisc add dev $interface parent 1:2 netem delay 75ms

    for ip in $nodes_ip ; do
        pid=$(( `echo $ip | cut -d '.' -f4` - 1 ))
	if [ $pid -lt $n_nodes ] ; then
            region=$(( ( $pid / $n_regions + $pid ) % $n_regions ))
	else
            #region=1
            region=$(( $pid % $n_regions ))
	fi
        signed_distance=$(( $region - $myregion ))
        distance=${signed_distance#-}
        band=$(( $distance + 1 ))
	#[ $myregion -eq $region ] && band=1
	#[ $myregion -ne $region ] && band=2
	#[ $pid -ge $n_nodes ]  && band=1
	#[ $mypid -ge $n_nodes ] && band=1
        tc filter add dev $interface protocol ip parent 1: prio 1 u32 match ip dst $ip flowid 1:$band
        #tc filter add dev $interface protocol ip parent 1:0 prio 1 u32 match ip dst $ip match ip dport 22 0xffff flowid 1:1
    done
    tc filter add dev $interface protocol ip parent 1: prio 1 u32 match ip sport 22 0xffff flowid 1:1
    tc filter add dev $interface protocol ip parent 1: prio 1 u32 match ip dport 22 0xffff flowid 1:1
}

case $command in
    stop)
        stop ;;
    *)
        stop
        start ;;
esac
