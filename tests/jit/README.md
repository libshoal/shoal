# JIT

This is an attempt to play with just-in-time compilation.

## Setup on Ubuntu

```
sudo apt-get install clang-3.5 llvm-runtime
```

## LLVM

LLVM seems to be able to do this. Found
[this article](http://stackoverflow.com/questions/3509215/llvm-jit-and-native/18022035\#18022035),
especially [this post](http://stackoverflow.com/a/18022035/491709).

This is how we generate LLVM bytecode:

```
gcc foo.c -shared -o libfoo.so -fPIC
clang -Wall -c -emit-llvm -O3 main.c -o main.bc
```

Execution can then be done like this:

```
clang -Wall -c -emit-llvm -O3 main.c -o - | lli -load=./libfoo.so
```

Note, what we did now (I think) is JIT compile the LLVM bytecode file
and execute it.

## Shoal

What we would have to do for Shoal is to:

 1. execute clang to generate bytecode,
 2. load this bytecode to memory
 3. execute LLVM on it to generate the binary
 4. possibly link it somehow
