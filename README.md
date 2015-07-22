# Shoal runtime library (Usenix ATC'15) 

Shoal is a library that provides an array memory abstraction that
automatically employs replication, partitioning and distribution
according to memory access patterns given in `shl__malloc_array`.

Shoal also uses hardware extensions such as DMA engines (Barrelfish)
and large/hugepage support if present on the machine.

Currently, we provide two types of workloads:

- Streamcluster from the PARSEC benchmark suite, which we manually
  modified to use Shoal (see below)
- pagerank, triangle-count, hop-dist from the Green Marl graph
  language; here, we provide extensions to the Green Marl compiler to
  automatically extract memory access patterns from high-level
  languages and generate a low-level C representation of the Green
  Marl input program that uses Shoal's memory abstraction (available
  on [github](https://github.com/libshoal/Green-Marl))

This is work presented at [Usenix ATC'15](https://www.usenix.org/conference/atc15/technical-session/presentation/kaestle).

The rest of this document describes how to install dependencies and
how to acquire Shoal.

## Dependencies and download of Shoal

Currently, we support Ubuntu and the Barrelfish OS to run Shoal. 

### Ubuntu / Linux
1. On Ubuntu, install the following dependencies to compile Shoal: ```apt-get install libpapi-dev liblua5.2-dev libpfm4-dev libnuma-dev ```
2. Create a new directory for development with Shoal (```mkdir shoal-base```) and cd into it
3. Acquire Shoal: ```git clone git@github.com:libshoal/shoal.git libshoal```. You should now have Shoal in ```shoal-base/libshoal```

### Barrelfish
1. Clone the Barrelfish source. ```git clone git://git.barrelfish.org/git/barrelfish```
2. cd into `barrelfish/lib` and acquire Shoal ```git clone git@github.com:libshoal/shoal.git shoal```
3. run Hake again.
4. Add ```tests/shl_simple``` to your symbolic_targets


## Streamcluster for Shoal

We provide a patch with our modifications to PARSEC's Streamcluster
benchmark as a patch in `apps/`.

 1. Download PARSEC 3.0: ```wget http://parsec.cs.princeton.edu/download/3.0/parsec-3.0-core.tar.gz```
 2. Extract it: ```tar -xf parsec-3.0-core.tar.gz```.
 3. Copy the source: ```cp -r parsec-3.0/pkgs/kernels/streamcluster/src streamcluster```. You should now see Streamcluster next to Shoal as ```shoal-base/streamcluster```. Change into the Streamcluster directory.
 4. Apply the patch-file provided with the Shoal distribution: ```patch -p1 <	../libshoal/apps/streamcluster.patch```
 5. Compile Streamcluster: ```make```
 6. Setup environemnt: ```export SHL_PARTITION=0; export SHL_HUGEPAGE=0; export SHL_CPU_AFFINITY=0-$(nproc)```
 7. Execute Streamcluster: ```LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../libshoal/shoal/ ./streamcluster```

