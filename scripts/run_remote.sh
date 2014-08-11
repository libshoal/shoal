#!/bin/bash

function error() {
	echo $1
	exit 1
}

function usage() {

    echo "Usage: $0 <program> <implementation> <workload> <prefix>"
    exit 1
}

[[ -n "$4" ]] || usage

WORKLOAD=$3
RESULT_DIR=~/papers/oracle/measurements/

MACHINE=sgs-r815-03
#MACHINE=sgs-r820-01

case "$WORKLOAD" in
    "pagerank")
	SHORT="pr"
	;;
    "triangle_counting")
	SHORT="tc"
	;;
    "hop_dist")
	SHORT="hd"
	;;
    *)
	SHORT=$WORKLOAD
	;;
esac

BASEPREFIX="${MACHINE}_${SHORT}"
PREFIX="${BASEPREFIX}_$4"

# params
# 1) <num_threads> 2) workload 3) implementation
function get_command() {
    next_command=". projects/gm/env.sh;\
	projects/gm/scripts/run.sh $2 $1 $3 $WORKLOAD"
}

TMP=`mktemp`
FTOTAL=`mktemp -t total-XXXXXX`
FCOMP=`mktemp -t comp-XXXXXX`
FINIT=`mktemp -t init-XXXXXX`
echo "Exexcuting on machine [$MACHINE]"
echo "Files are: $FTOTAL $FCOMP $FINIT"

(
    . ~/.bashrc
    echo "x y e" > $FTOTAL
    echo "x y e" > $FCOMP
    echo "x y e" > $FINIT
    for i in {6..3}
    do
	# Add number of threads used as x-value
	echo -n $((2**$i)) >> $FTOTAL
	echo -n $((2**$i)) >> $FCOMP
	echo -n $((2**$i)) >> $FINIT

	get_command $((2**$i)) $1 $2
	echo "Executing command [${next_command}]" 1>&2
	NUM=2 exec_avg ssh $MACHINE "${next_command}" &> $TMP
	echo "Output was:" 1>&2
	cat $TMP 1>&2
	if [[ $2 == "ours" ]]
	then
	    cat $TMP | awk '/^total/ { print $2 }' | skstat.py >> $FTOTAL
	    cat $TMP | awk '/^comp/ { print $2 }'  | skstat.py >> $FCOMP
	    cat $TMP | awk '/^copy/ { print $2 }'  | skstat.py >> $FINIT
	fi

	if [[ $2 == "theirs" ]]
	then
	    cat $TMP | awk -F'=' '/running time/ { print $2 }' | skstat.py >> $FTOTAL
	fi
    done
)
rm $TMP

echo "Files are: $FTOTAL $FCOMP $FINIT"
echo "Suggesting the following copy operations:"

if [[ $2 == "ours" ]]
then
    echo "cp $FTOTAL ${RESULT_DIR}/${PREFIX}_total"
    echo "cp $FCOMP ${RESULT_DIR}/${PREFIX}"
    echo "cp $FINIT ${RESULT_DIR}/${PREFIX}_init"
fi

if [[ $2 == "theirs" ]]
then
    echo "cp $FTOTAL ${RESULT_DIR}/${BASEPREFIX}_org"
fi
