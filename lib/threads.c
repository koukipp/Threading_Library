#define _GNU_SOURCE
#include "threads.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <string.h>
#include "atomic.h"
#define SCHED_READY 0
#define SCHED_TERM  1
#define PAGE_NUM 16

/*The number of stacks that can be kept
at our reservoir.*/
#define MAX_SIZE 1000

/*Uncomment this to be able to call
thread_lib_exit() from a user thread that
isnt the main thread or generally to terminate the process
from another thread except the main thread.*/
//#define ATEXIT_HANDLER

/*Two policies can be used for work-stealing.Only one of them
must be defined.*/

/*Max work-stealing policy:Each thread searches for the queue
with the most threads and steals from this queue.*/
//#define MAX_POLICY

/*Neighboor work-stealing policy:Each thread tries to steal from
the first queue that is found to have threads starting from the queue
at our left and ending at the queue at our right.*/
#define NEIGHBOOR_POLICY

/*The old unoptimized policy for reference.*/
//#define NEIGHBOOR_POLICY_OLD

/*The main thread descriptor.*/
static thread_t main_thread;

static native_t *native_threads;
static void *main_stack;
static atomic_t id;

/*Counter used to decide when to terminate (when all
user threads exit).*/
static atomic_t thread_count;

static size_t page_size,stack_size;
static size_t main_stack_size;
static list stack_list;

/*The index of the thread descriptor
in a stack (interpreting the stack as an
array of thread descriptors).*/
static int desc_pos;

static int num_threads;
static int term = 0;

/*Recycling up to MAX_SIZE stacks.*/
void *reserve_memory(){
    void *stack_node;
    stack_node = list_dequeue(&stack_list);
    if(stack_node == NULL){
        posix_memalign(&stack_node,page_size*PAGE_NUM,page_size*PAGE_NUM);

    }
    return stack_node;
}

void free_memory(void * stack){
    if(stack_list.size < MAX_SIZE){
        list_enqueue(&stack_list,(node *)stack);
    }
    else{
        free(stack);
    }
}

int sem_init(sem_t *sem,int val){
    list_init(&sem->wait_q);
    spin_lock_init(&sem->lock);
    sem->val = val;

    return 0;
}

int sem_up(sem_t *sem){
    node *n;
    spin_lock(&sem->lock);
	if(is_empty(&sem->wait_q)){
		sem->val++;
	}
	else{
		n = list_dequeue(&sem->wait_q);
        /*Wake up a thread from the queue and put it the beggining of
        our native thread_queue.*/
        list_push(&thread_self()->native_thread->ready_queue,n);
	}
    spin_unlock(&sem->lock);
	return(0);
}


int sem_down(sem_t *sem){
    thread_t *t;
    spin_lock(&sem->lock);
	if(sem->val > 0){
		sem->val--;
        spin_unlock(&sem->lock);
	}
	else{
        t = thread_self();
        list_enqueue(&sem->wait_q,(node *)t);
        t->native_thread->running_thread = NULL;
        spin_unlock(&sem->lock);
        thread_yield();
	}
	return(0);
}

int sem_destroy(sem_t *sem){
    /*Nothing to cleanup*/
    return 0;
}

int context_create(co_t *co,void(body)(void *),void *arg,thread_t *thr_d,char **stack_p){
	int res;
	void *stack;
	res = getcontext(co);
	stack = reserve_memory();
    if(stack == NULL){
        return -1;
    }
	co->uc_stack.ss_sp = stack;
	co->uc_stack.ss_size = page_size*PAGE_NUM - 16;
	co->uc_stack.ss_flags = 0;
    ((thread_t **)stack)[desc_pos] = thr_d;
	makecontext(co,(void(*)(void))body,1,arg);
    *stack_p = stack;
	return(res);
}

/*Context creation without storing descriptor in stack (used for
scheduling contexts).*/
int context_create_normal(co_t *co,void(body)(void *),void *arg){
	int res;
	void *stack;
	res = getcontext(co);
	res = posix_memalign(&stack,page_size*PAGE_NUM,page_size*PAGE_NUM);
	co->uc_stack.ss_sp = stack;
	co->uc_stack.ss_size = page_size*PAGE_NUM;
	co->uc_stack.ss_flags = 0;
	makecontext(co,(void(*)(void))body,1,arg);
	return(res);
}

/*The scheduler function manages threads that exit and
work stealing in case we dont find a thread in our queue.*/

void scheduler(void *native_thread_map){
    thread_t * prev_thread;
    native_t * native_thread = (native_t *)native_thread_map;
    int i;

    /*The starting point where each thread enters when it context switches
    to the scheduler.*/
    getcontext(&native_thread->scheduler);

    prev_thread = native_thread->running_thread;
    /*Handle terminated threads ,no need to check status
    since if prev_thread isnt null it means it was directly called
    by exit.*/
    if(prev_thread != NULL){
        i=0;
        free_memory(prev_thread->stack);
        thread_t **temp_prev_thread_succesors = prev_thread->successors;
        prev_thread->successors = NULL;
        while(temp_prev_thread_succesors[i]!=NULL){
            if(atomic_sub_and_test(&temp_prev_thread_succesors[i]->deps)){
                list_push(&native_thread->ready_queue,(node *)temp_prev_thread_succesors[i]);
            }
            i++;
        }
        free(temp_prev_thread_succesors);

    }
    native_thread->running_thread = (thread_t *)list_dequeue(&native_thread->ready_queue);

    /*Work stealing , if no thread was found on our queue.*/
    while(native_thread->running_thread == NULL){
        #ifdef MAX_POLICY
            size_t max = 0 , max_i = 0 , temp;
            for(i=0;i<num_threads;i++){
                temp = native_threads[i].ready_queue.size;
                if(temp > max){
                    max = temp;
                    max_i = i;
                }

            }
            transfer_half(&native_threads[max_i].ready_queue,&native_thread->ready_queue);
        #endif

        #ifdef NEIGHBOOR_POLICY
            for(i=(native_thread->idx)+1;i != native_thread->idx;i++){
                if(i == num_threads ){
                    i = 0;
                }
                if(transfer_half(&native_threads[i].ready_queue,&native_thread->ready_queue) 
                || (!native_thread->idx && i == num_threads -1)){
                    break;
                }

            }
        #endif

        #ifdef NEIGHBOOR_POLICY_OLD
            for(i=(native_thread->idx)+1;i != native_thread->idx;i++){
                if(i == num_threads ){
                    i = 0;
                }
                native_thread->running_thread = (thread_t *)list_remove_last(&native_threads[i].ready_queue); 
                if(native_thread->running_thread != NULL 
                || (!native_thread->idx && i == num_threads -1)){
                    break;
                }

            }
        #endif

        #ifndef NEIGHBOOR_POLICY_OLD
            native_thread->running_thread = (thread_t *)list_dequeue(&native_thread->ready_queue);
        #endif

        /*Terminate this native thread when signalled by lib_thread_exit
        (lib_thread_exit has already checked that there are no more other
        user threads that running before setting the flag).*/
        if(term && native_thread->running_thread == NULL){
            pthread_exit(NULL);
        }
    }
    /*Set this native thread as the owner of the user thread
    that is scheduled to run.We always have to do this since we dont
    know if this thread was stolen from other queues.*/
    native_thread->running_thread->native_thread = native_thread;
    setcontext(&native_thread->running_thread->context);
    

}

void thread_wrapper(void *arg){

    void (*body)(void *) = ((wrapper *)arg)->body;
    void *args = ((wrapper *)arg)->args;
    (body)(args);
    thread_self()->status = SCHED_TERM;
    #ifdef ATEXIT_HANDLER
        if(atomic_sub_and_test(&thread_count)){
            /*If all user threads have terminated terminate this
            native thread, and signal other native threads to terminate
            as well (in case nobody ever called thread_lib_exit()).*/
            term = 1;
            pthread_exit(NULL);
        }
    #else
        atomic_add(-1,&thread_count);
    #endif
    setcontext(&thread_self()->native_thread->scheduler);

}

void *native_thread_wrapper(void *arg){

    scheduler(arg);
    return NULL;

}

/*If the ATEXIT_HANDLER is defined
,this will handle the exit of the main thread,
we decrement thr thread count and we remove ourservles
switching to the scheduler.Else if we were the last thread
we just terminate.*/
void main_exit(){
    if (!atomic_sub_and_test(&thread_count)){
        main_thread.status = SCHED_TERM;
        main_thread.native_thread->running_thread = NULL;
        swapcontext(&main_thread.context,&main_thread.native_thread->scheduler);
    }
}


int thread_lib_init(int threads){
    struct rlimit rlim;
    num_threads = threads;
    page_size = getpagesize();
    getrlimit(RLIMIT_STACK,&rlim);
    main_stack_size = rlim.rlim_cur;
    stack_size = page_size*PAGE_NUM;

    /*Calculate the beggining of the main stack.*/
    main_stack = (void *)(((uintptr_t)&rlim) & ~ (uintptr_t)(main_stack_size - 1));
    /*Calculate the position of the thread descriptor
    in a thread stack (except main).*/
    desc_pos = page_size*PAGE_NUM/sizeof(thread_t *) - 1;
    atomic_set(0,&id);
    atomic_set(1,&thread_count);
    pthread_setconcurrency(threads);

    atomic_set(0,&main_thread.deps);
    main_thread.status = SCHED_READY;
    main_thread.successors = NULL;
    atomic_set(id,&main_thread.id);
    main_thread.stack = NULL;
    list_init(&stack_list);

    native_threads = (native_t *)malloc(threads*sizeof(native_t ));
    for(int i=0;i<threads;i++){
	    native_threads[i].scheduler.uc_link = NULL;
        native_threads[i].running_thread = NULL;
        native_threads[i].idx = i;
        list_init(&native_threads[i].ready_queue);
        if(!i){
            native_threads[i].id = pthread_self();
            native_threads[i].running_thread = &main_thread;
            main_thread.native_thread = &native_threads[i];
            context_create_normal(&native_threads[i].scheduler,scheduler,&native_threads[i]);
        }
        else{
            native_threads[i].running_thread = NULL;
        }
    }
    #ifdef ATEXIT_HANDLER
        atexit(main_exit);
    #endif

    for(int i=1;i<threads;i++){
        pthread_create(&native_threads[i].id,NULL,&native_thread_wrapper,&native_threads[i]);
    }



    return 0;
}

thread_t *thread_self(){
    void* bogus;
    /*Black magic.(Wont let me compile unless I cast to uintptr_t)*/
    bogus = (void *)(((uintptr_t)&bogus) & ~ (uintptr_t)(main_stack_size - 1));
    if(bogus == main_stack){
        return &main_thread;
    }

    bogus = (void *)(((uintptr_t)&bogus) & ~ (uintptr_t)(stack_size - 1));

    /*The thread descriptor is stored in the desc_pos index of the stack(interpreted as
    an array)
    .This is the highest address of the stack and thus the begging of the stack
    in the x86 architecure.*/
    return ((thread_t **)bogus)[desc_pos];
}

int thread_getid(){
    return thread_self()->id;
}

/*Yield tries to find a thread in the native thread's queue,
if it fails(in case everything was stolen which would be rare)
it switches to the scheduler.*/
int thread_yield(){
    thread_t *self = thread_self();
    native_t *native_thread = self->native_thread;
    /*The current running thread can be NULL if thread_yield was called
    internally by sem_down(which means that this thread has blocked
    ).In this case we switch to the scheduler.*/
    if(native_thread->running_thread == NULL){
        swapcontext(&self->context,&native_thread->scheduler);
    }
    else{
        if(!atomic_read(&self->deps)){
            list_enqueue(&native_thread->ready_queue,(node *) self);
        }
        native_thread->running_thread = (thread_t *)list_dequeue(&native_thread->ready_queue);
        if(native_thread->running_thread == NULL){
            swapcontext(&self->context,&native_thread->scheduler);
        }
        else{
            native_thread->running_thread->native_thread = native_thread;
            swapcontext(&self->context,&native_thread->running_thread->context);
        }

    }
    return 0;
}
int thread_inc_dependency(int num_deps){
    atomic_add(num_deps,&thread_self()->deps);
    return 0;
}

/*No bookkeeping is necessary for the main thread
, the atexit_handler will do the decrement.*/
void thread_exit(){
    if(thread_self() != &main_thread){
        thread_self()->status = SCHED_TERM;
        #ifdef ATEXIT_HANDLER
            if(atomic_sub_and_test(&thread_count)){
                term = 1;
                pthread_exit(NULL);
            }
        #else
            atomic_add(-1,&thread_count);
        #endif
        swapcontext(&thread_self()->context,&thread_self()->native_thread->scheduler);
    }
    
}
/*Wait for all user threads to terminate (except yourself),
join the other native threads and cleanup.*/
int thread_lib_exit(){
    while (atomic_read(&thread_count) != 1){
        thread_yield();
    }
    term = 1;
    thread_t *self = thread_self();
    for(int i=0; i< num_threads;i++){
        if(self->native_thread != &native_threads[i]){
            pthread_join((pthread_t )native_threads[i].id,NULL);
        }
    }
    free(native_threads[0].scheduler.uc_stack.ss_sp);

    _stack_t *curr;
    while(!is_empty(&stack_list)){
        curr = (_stack_t *)list_dequeue(&stack_list);
        free(curr);
    }
    free(native_threads);
    return 0;
}

int thread_create (
    thread_t *thread_descriptor, 
    void (body)(void *),
    void *arg,
    int deps,
    thread_t **successors) 
{
    int res;
	thread_descriptor->context.uc_link = NULL;
	thread_descriptor->status = SCHED_READY;
    atomic_set(deps,&thread_descriptor->deps);
    thread_descriptor->id = atomic_inc_read(&id);
    thread_descriptor->args.body = body;
    int i=0;
    while(1){
        if(successors[i] == NULL){
            i++;
            break;
        }
        i++;
    }
    thread_descriptor->successors = (thread_t **) malloc(i*sizeof(thread_t *));
    memcpy(thread_descriptor->successors,successors,i*sizeof(thread_t *));
    thread_descriptor->args.args = arg;
    thread_descriptor->native_thread = thread_self()->native_thread;
	res = context_create(&(thread_descriptor->context),(void (*)(void *))thread_wrapper,
    &(thread_descriptor->args),thread_descriptor,&thread_descriptor->stack);
    if(res < 0){
        return(res);
    }
    atomic_add(1,&thread_count);
    if(!atomic_read(&thread_descriptor->deps)){
	    list_enqueue(&thread_descriptor->native_thread->ready_queue,(node *)thread_descriptor);
    }
    return 0;
}


