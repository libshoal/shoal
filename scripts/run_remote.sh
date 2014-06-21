#!/bin/bash

function error() {
	echo $1
	exit 1
}

function usage() {

    echo "Usage: $0 <program> <implementation>"
    exit 1
}

[[ -n "$2" ]] || usage

MACHINE=sgs-r815-03

# params
# 1) <num_threads> 2) workload 3) implementation
function get_command() {
    next_command=". projects/gm/env.sh;\
	projects/gm/scripts/run.sh $2 $1 $3"
}

TMP=`mktemp`
FTOTAL=`mktemp`
FCOMP=`mktemp`
echo "Files are: $FTOTAL $FCOMP"

(
    . ~/.bashrc
    echo "x y e" > $FTOTAL
    echo "x y e" > $FCOMP
    for i in {6..0}
    do
	echo -n $((2**$i)) >> $FTOTAL
	echo -n $((2**$i)) >> $FCOMP

	get_command $((2**$i)) $1 $2
	echo "Executing command [${next_command}]" 1>&2
	exec_avg ssh $MACHINE "${next_command}" &> $TMP
	echo "Output was:" 1>&2
	cat $TMP 1>&2
	if [[ $2 == "ours" ]]
	then
	    cat $TMP | awk '/^total/ { print $2 }' | skstat.py >> $FTOTAL
	    cat $TMP | awk '/^comp/ { print $2 }' | skstat.py >> $FCOMP
	fi

	if [[ $2 == "theirs" ]]
	then
	    cat $TMP | awk -F'=' '/running time/ { print $2 }' | skstat.py >> $FTOTAL
	fi
    done
)
rm $TMP

echo "Files are: $FTOTAL $FCOMP"
