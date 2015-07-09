# Dependencies and download of Shoal

1. On Ubuntu, install the following dependencies to compile Shoal: ```apt-get install libpapi-dev liblua5.2-dev libpfm4-dev```
2. Create a new directory for development with Shoal (```mkdir shoal-base```) and cd into it
3. Acquire Shoal: ```git clone .. libshoal```. You should now have Shoal in ```shoal-base/libshoal```

# Streamcluster for Shoal

 1. Download PARSEC 3.0: ```wget http://parsec.cs.princeton.edu/download/3.0/parsec-3.0-core.tar.gz```
 2. Extract it: ```tar -xf parsec-3.0-core.tar.gz```.
 3. Copy the source: ```cp -r parsec-3.0/pkgs/kernels/streamcluster/src streamcluster```. You should now see Streamcluster next to Shoal as ```shoal-base/streamcluster```. Change into the Streamcluster directory.
 4. Apply the patch-file provided with the Shoal distribution: ```patch -p1 <	../libshoal/apps/streamcluster.patch```
 5. Compile Streamcluster: ```make```
 6. Setup environemnt: ```export SHL_PARTITION=0; export SHL_HUGEPAGE=0; export SHL_CPU_AFFINITY=0-$(nproc)```
 7. Execute Streamcluster: ```LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../libshoal/shoal/ ./streamcluster```
