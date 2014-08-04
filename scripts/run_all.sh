#!/bin/bash

set -x

LOG=`mktemp -t log-XXXXXX`

(
    scripts/run_remote.sh triangle_counting theirs twitter_rv org 2>&1
    scripts/run_remote.sh hop_dist theirs twitter_rv org 2>&1
) | tee $LOG

echo "Log is at $LOG"