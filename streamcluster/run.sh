#!/bin/bash

time ./streamcluster 10 20 32 4096 4096 1000  ../streamcluster/inputs/input_4k.txt output.txt `nproc`
