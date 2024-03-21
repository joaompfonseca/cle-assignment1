# CLE Assignment 1

## Description

The assignment goal was to develop a multithreaded implementation of the two general problems given in the practical classes.

The first problem consisted...

The second problem consisted on developing a sorting algorithm to sort an array of integers in a descending order. The
chosen algorithm was bitonic sort, since it provides good parallel decomposition properties. A distributor thread
coordinates the sorting by assigning the appropriate tasks to each worker thread. A first iteration involves the distributor
equally spliting the array among the workers for each to perform a bitonic sort task. After that is done, the distributor
assigns bitonic merge tasks over pairs of sorted parts of the array to each worker, terminating the threads that are not
necessary anymore. This is repeated for larger and larger subarray sizes, until the whole array is sorted.

**Course:** Large Scale Computing (2023/2024).

## Running the programs

### 1. Multithreaded equal consonants

- ...

#### Usage of the program

```

```

#### Example of usage

```...```

### 2. Multithreaded bitonic sort

- Run `cd prog2` to change to the program's directory.
- Run `make` to compile the program.
- Run `./prog2 REQUIRED OPTIONS` to run the program.

#### Usage of the program

```                                                                                          
REQUIRED:                                                                                                                   
-f --- input file with numbers                                                                                              
OPTIONS:                                                                                                                    
-h --- print this help                                                                                                      
-n --- number of worker threads (default is 2, minimum is 1)
```

#### Example of usage

```./prog2 -f data/datSeq256K.bin -n 8```

## Authors

- João Fonseca, 103154
- Rafael Gonçalves, 102534
