# Threading_Library:
A threading library written in C, providing similar functionality to pthreads featuring thread
dependecies, stack recycling, and work stealing to balance workloads between multiple threads.

Compatible only with x86 architectures. The threading library files are included in the lib directory.

Made for the Advanced Topics in Systems Software course of University of Thessaly.

## Benchmarks:

### main.c 
> Test basic semaphore functionality.

### matmul.c 
> Multithreaded matrix multiplication benchmark.

### matmul_multilevel.c
> Multilevel multithreaded matrix multiplication benchmark.

### matmul_yield.c 
> Test yield call in matrix multiplication.

### create_bench.c 
> Benchmark memory stack recycling.

### matmul_sem.c 
> Test the functionality of semaphores. Semaphores are used as a mutex
and as a replacement for dependencies.
### atexit_example.c 
> An example to test the thread_lib_exit call from another thread (other than the main/initial thread).

## Instructions:

Compile all benchmarks with:
```
make
```
To enable the atexit_handler functionality (demonstrated mainly in atexit_example.c) compile with:
```
make compile_exec_atexit
```

### Contributors:

Ippokratis Koukoulis

