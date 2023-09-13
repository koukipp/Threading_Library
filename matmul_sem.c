#include <stdio.h>
#include "lib/threads.h"

#define SIZE	1024
double A[SIZE][SIZE], B[SIZE][SIZE], C[SIZE][SIZE];
int thread_count_num=0;
int thread_count;
sem_t sem,sem2;

void thread_func(void *arg) {
    unsigned long row = (unsigned long)arg;
    unsigned long col, k;

    for(col = 0; col < SIZE; col++) {
        C[row][col] = 0;
        for (k = 0; k < SIZE; k++)
            C[row][col] += A[row][k]*B[k][col];
    }
    sem_down(&sem2);
    thread_count_num++;
    sem_up(&sem2);

    sem_up(&sem);
}


void init_arrays() {
    unsigned long i, j;
    for (i = 0; i < SIZE; i++)
        for (j = 0; j < SIZE; j++) {
            A[i][j] = B[i][j] = (i == j);
        }
}

int main (int argc, char *argv[]) {
    unsigned long i, j;
    int depend=0;

    thread_t thread[SIZE];

    init_arrays();
    thread_lib_init(4);
    sem_init(&sem,0);
    sem_init(&sem2,1);
    for (i = 0; i < SIZE; i++)
        thread_create(&thread[i], thread_func, (void *)i, 0, THREAD_LIST_NONE);
    
    /*Dependecies are done by using a semaphore.This checks that the 
    general property for the semaphores for a rendez-vous works.*/
    while(depend < SIZE){
        sem_down(&sem);
        depend++;
    }

    /*Each thread increases a count by 1
    at the end this count must be equal to SIZE if the
    semaphore worked as intended (as a mutex).*/
    if(thread_count_num == SIZE){
        printf("Success\n");
    }
    else{
        printf("Failed %d\n",thread_count_num);
    }

    printf("Finished\n");
    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++) {
            if (i != j) {
                if (C[i][j] != 0.0)
                    printf("Error (%lu, %lu)\n", i, j);
            }
            else if (C[i][j] != 1.0)
                printf("ErrorB (%lu, %lu)\n", i, j);
        }
    }
    /*We do thread_lib_exit at the end so that we can
    be sure that the waits were done by the semaphore.*/
    thread_lib_exit();
    return(0);
}
