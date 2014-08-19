#!/bin/bash

#set -x

PROG=pagerank
OPTS=

txtred='\e[0;31m' # Red
txtgrn='\e[0;32m' # Green
txtrst='\e[0m'    # Text Reset

(
	for PROG in "pagerank" "triangle_counting" "hop_dist"; do
		for OPTS in "" "-d" "-d -r" "-d -r -p" "-d -r -p -h"; do
			TMP=`mktemp /tmp/tmp-run_all-XXXXXX`
			scripts/run.sh $OPTS $PROG 32 ours soc-LiveJournal1 &> $TMP
			RC=$?
			if [[ $RC -eq 0 ]]; then
				RCS=$txtgrn"ok  "$txtrst
			else
				RCS=$txtred"fail"$txtrst
			fi
			echo -e "Return code [$RCS] for [$PROG] with [$OPTS] was [$RC] ... log at [$TMP]"
		done
	done
)
