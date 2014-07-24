#!/bin/bash

#time ./streamcluster 10 20 32 4096 4096 1000  ../streamcluster/inputs/input_4k.txt output.txt `nproc`
./streamcluster 10 20 128 0 200000 5000 /mnt/scratch/skaestle/inputs/sc_input_native output.txt `nproc`
