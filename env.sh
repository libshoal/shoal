#!/bin/bash

export SHL_CPU_AFFINITY=0-`nproc`
export GOMP_CPU_AFFINITY=0-`nproc`

# http://stackoverflow.com/a/246128
PREFIX="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

export LD_LIBRARY_PATH=$PREFIX/shoal:$PREFIX/contrib/numactl-2.0.9:$PREFIX/contrib/papi-5.3.0/src:$PREFIX/contrib/papi-5.3.0/src/libpfm4/lib:$LD_LIBRARY_PATH
