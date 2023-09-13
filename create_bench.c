#include <stdio.h>
#include "lib/threads.h"

#define SIZE	1024
#define CHUNK	128
double A[SIZE][SIZE], B[SIZE][SIZE], C[SIZE][SIZE];

thread_t _thread[SIZE][SIZE / CHUNK];

union args {
    void *arg;
    int indices[2];
};

void worker_func(void *arg) {
}

void thread_func(void *arg) {
    unsigned long row = (unsigned long)arg;
    unsigned long col;
    union args argument;

    thread_t *self = thread_self();

    int i = 0;

//    printf("Thread %d creating threads\n", thread_getid());
    thread_inc_dependency(SIZE/CHUNK);
    argument.indices[0] = row;
    for(col = 0; col < SIZE; col = col+CHUNK) {
        argument.indices[1] = col;
        thread_create(&_thread[row][i++], worker_func, argument.arg, 0, THREAD_LIST(self));
    }
//    printf("Thread %d done creating threads\n", thread_getid());
    thread_yield();
}

int main (int argc, char *argv[]) {
    thread_t *myself;
    unsigned long i, j;

    thread_t thread[SIZE];

    thread_lib_init(4);
    myself = thread_self();
    for(j = 0; j < 20; j++){
        thread_inc_dependency(SIZE);
        for (i = 0; i < SIZE; i++)
            thread_create(&thread[i], thread_func, (void *)i, 0, THREAD_LIST(myself));

        thread_yield();
    }

    printf("Finished\n");
    thread_lib_exit();

    return(0);
}
