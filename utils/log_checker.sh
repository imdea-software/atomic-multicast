print_usage () {
    echo -e "log_checker: log_checker [COMMAND] [N_MSGS] [LOG_FILE]\n"
}

if [ ! $# -eq 3 ] ; then print_usage ; exit 0; fi

COMMAND=$1
NUMBER_OF_MSGS=$2
INPUT_FILE=$3

check_deliver () {
    NUMBER_OF_NODES=6
    NUMBER_OF_GROUPS=2

    for msg in `seq 0 $(($NUMBER_OF_MSGS -1))` ; do
        if [ ! `grep $COMMAND $INPUT_FILE | grep "{$msg}" | wc -l` -eq $NUMBER_OF_NODES ] ; then
            echo "FAILURE: msg $msg not delivered by all nodes" ; break
        fi

	gts=`grep $COMMAND $INPUT_FILE | grep "{$msg}" | grep "\[0\]" | cut -d "(" -f2 | cut -d ")" -f1`
	for node in `seq 0 $(($NUMBER_OF_NODES - 1))` ; do
            gts1=`grep $COMMAND $INPUT_FILE | grep "{$msg}" | grep "\[$node\]" | cut -d "(" -f2 | cut -d ")" -f1`
	    if [ "$gts" != "$gts1" ] ; then
                echo "FAILURE: msg $msg delivered with gts $gts1 instead of $gts for node $node"
            fi
        done
    done
}

case $COMMAND in
    DELIVER)
        check_deliver $NUMBER_OF_MSGS $INPUT_FILE
        ;;
    *)
        echo -e "Unsupported Command\n"
	exit 1
        ;;
esac
