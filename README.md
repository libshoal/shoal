# Shoal runtime library (Usenix ATC'15) 

## Dependencies and download of Shoal

Currently, we support Ubuntu and the Barrelfish OS to run Shoal. 

### Ubuntu / Linux
1. On Ubuntu, install the following dependencies to compile Shoal: ```apt-get install libpapi-dev liblua5.2-dev libpfm4-dev libnuma-dev ```
2. Create a new directory for development with Shoal (```mkdir shoal-base```) and cd into it
3. Acquire Shoal: ```git clone .. libshoal```. You should now have Shoal in ```shoal-base/libshoal```

### Barrelfish
1. Clone the Barrelfish source. ```git clone git://git.barrelfish.org/git/barrelfish```
2. cd into barrelfish/lib and acquire Shoal ```git clone .. shoal```
3. run Hake again.
4. Add ```tests/shl_simple``` to your symbolic_targets


## Streamcluster for Shoal

 1. Download PARSEC 3.0: ```wget http://parsec.cs.princeton.edu/download/3.0/parsec-3.0-core.tar.gz```
 2. Extract it: ```tar -xf parsec-3.0-core.tar.gz```.
 3. Copy the source: ```cp -r parsec-3.0/pkgs/kernels/streamcluster/src streamcluster```. You should now see Streamcluster next to Shoal as ```shoal-base/streamcluster```. Change into the Streamcluster directory.
 4. Apply the patch-file provided with the Shoal distribution: ```patch -p1 <	../libshoal/apps/streamcluster.patch```
 5. Compile Streamcluster: ```make```
 6. Setup environemnt: ```export SHL_PARTITION=0; export SHL_HUGEPAGE=0; export SHL_CPU_AFFINITY=0-$(nproc)```
 7. Execute Streamcluster: ```LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../libshoal/shoal/ ./streamcluster```

