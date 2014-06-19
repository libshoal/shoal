#!/bin/bash

function error() {
	echo $1
	exit 1
}

function usage() {

    echo "Usage: $0"
    exit 1
}

. env.sh
GOMP_CPU_AFFINITY="0-32" apps/output_cpp/bin/pagerank ../graphs/big.bin 32 | scripts/extract_result.py
