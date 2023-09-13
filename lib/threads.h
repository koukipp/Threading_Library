#ifndef MYTHREADS_H
#define MYTHREADS_H

#include "list.h"
#include "atomic.h"

#include <ucontext.h>
#define STK_SIZE 8388608
typedef ucontext_t co_t;

typedef struct wrapper {
    void (*body)(void *);
    void *args;
    
} wrapper;

typedef struct thread_descriptor thread_t;

typedef struct native_thread {
    co_t scheduler;
    list ready_queue;
    thread_t *running_thread;
    pthread_t id;
    int idx;

}native_t;

struct thread_descriptor {
    struct thread_descriptor *next, *prev;
    int id;
    char *stack;
    ucontext_t context; /* see man getcontext */
    atomic_t deps;
    int num_successors;
    int status;
    struct thread_descriptor **successors;
    wrapper args;
    native_t *native_thread;
};

typedef struct stack_descriptor {
    struct stack_descriptor *next, *prev;
    char *stack;
}_stack_t;


#define THREAD_LIST_NONE	(thread_t *[]){ NULL }
#define THREAD_LIST(...)	(thread_t *[]){ __VA_ARGS__, NULL }

typedef struct{
    list wait_q;
    int val;
    spin_t lock;
}sem_t;

int thread_lib_init(int threads);
thread_t *thread_self();
int thread_getid();
int thread_create(
    thread_t *thread_descriptor, 
    void (body)(void *),
    void *arg,
    int deps,
    thread_t **successors
);
int thread_yield();
int thread_inc_dependency(int num_deps);
void thread_exit();
int thread_lib_exit();

int sem_init(sem_t *s, int val);
int sem_down(sem_t *s);
int sem_up(sem_t *s);
int sem_destroy(sem_t *s);

#endif
