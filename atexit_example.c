#include <stdio.h>
#include <unistd.h>

#include "lib/threads.h"
sem_t sem;

thread_t t1;
thread_t t2;
thread_t t3;
thread_t t4;


void work(void *arg) {
    int i;
    int id;

    id = thread_getid();
    if(id == 3){
        sem_down(&sem);
    }

    for (i=0; i<5; i++) {
        printf("%s id=%d: %d, %p\n", (char *)arg, id, i, &i);
        fflush(stdout);
        // sleep(1);
        thread_yield();
    }

    if(id == 4){
        sem_up(&sem);
        thread_lib_exit();

    }

}

int main(int argc, char *argv[]) {
    /*
      t1 --> t3
      t2 --> t3
             t3 --> t4
     */

    thread_lib_init(2);

    sem_init(&sem,0);

    thread_create(&t4, work, "t4", 0, THREAD_LIST_NONE);
    thread_create(&t3, work, "t3", 0, THREAD_LIST_NONE);
    thread_create(&t2, work, "t2", 0, THREAD_LIST_NONE);
    thread_create(&t1, work, "t1", 0, THREAD_LIST_NONE);

    //thread_inc_dependency(1);
    //thread_yield();

    printf("in main, id = %d\n", thread_getid());

    thread_exit();

    printf("OK\n");

}
