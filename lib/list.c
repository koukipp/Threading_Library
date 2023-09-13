#include "list.h"
#include <stdio.h>

node *node_create () {
    node *p = (node*)malloc(sizeof(node));
    p->next = NULL;
    p->prev = NULL;
    return p;
}

void node_free (node *p) {
    free(p);
}

list *list_create () {
    list *l = (list*)malloc(sizeof(list));
    l->head = NULL;
    l->tail = NULL;
    l->size = 0;
    return l;
}

void list_init (list *l) {
    spin_lock_init(&l->lock);
    l->head = NULL;
    l->tail = NULL;
    l->size = 0;
}

void list_free (list *l) {
    free(l);
}

node *list_remove (list *l, node *p) {
    if (l->size == 1) {
        l->head = NULL;
        l->tail = NULL;
    } 
    else {
        if (l->tail == p) {
            l->tail = p->prev;
            l->tail->next = NULL;
        }
        else if (l->head == p) {
            l->head = p->next;
            l->head->prev = NULL;
        }
        else {
            p->prev->next = p->next;
            p->next->prev = p->prev;
        }
    }

    l->size--;
    return p;
}


/*Remove the head of the queue-list.*/
node *list_dequeue (list *l) {
    spin_lock(&l->lock);
    node *n= NULL;
    if (l->size != 0) {
        n = list_remove(l, l->head);
    }
    spin_unlock(&l->lock);
    return n;
}

/*Remove the tail of the queue-list.*/
node *list_remove_last (list *l) {
    spin_lock(&l->lock);
    node *n= NULL;
    if (l->size != 0) {
        n = list_remove(l, l->tail);
    }
    spin_unlock(&l->lock);
    return n;
}
/*We dont lock both queues at the same time since
that could lead to a deadlock.We remove the 
nodes at the end of the src queue , and add them at 
the end of the dst queue.*/
int transfer_half (list *src,list *dst) {
    int i;
    spin_lock(&src->lock);
    node *n1= NULL,*old = NULL;
    if (src->size != 0) {
        n1 = src->tail;

        for(i=1;i<(src->size)/2;i++){
            n1 = n1->prev;
        }
        if(n1->prev == NULL){
            src->head = NULL;
        }
        else{
            n1->prev->next = NULL;
        }
        old = src->tail;
        src->tail = n1->prev;
        src->size -= i;
        spin_unlock(&src->lock);
        spin_lock(&dst->lock);
        if(dst->tail != NULL){
            dst->tail->next = n1;
        }
        n1->prev = dst->tail;
        dst->tail = old;
        if(!dst->size){
            dst->head = n1;
        }
        dst->size += i;
    }
    else{
        spin_unlock(&src->lock);
        return 0;
    }
    spin_unlock(&dst->lock);
    return 1;
}

int is_empty(list *l){
    spin_lock(&l->lock);
    if (l->size == 0) {
        spin_unlock(&l->lock);
        return 1;
    }
    else {
        spin_unlock(&l->lock);
        return 0;
    }
}
/*Add node at the end of the queue-list.*/
void list_enqueue (list *l, node *p) {
    spin_lock(&l->lock);
    if (l->size == 0) {
        l->head = p;
        l->tail = p;
        p->prev = NULL;
	    p->next = NULL;
    } 
    else {
        p->next = NULL;
        p->prev = l->tail;
        l->tail->next = p;
        l->tail = p;
        
    }

    l->size++;
    spin_unlock(&l->lock);
}

/*Add node at the beggining of the queue-list.*/
void list_push (list *l, node *p) {
    spin_lock(&l->lock);
    if (l->size == 0) {
        l->head = p;
        l->tail = p;
        p->prev = NULL;
	    p->next = NULL;
    } 
    else {
        p->next = l->head;
        p->prev = NULL;
        l->head->prev = p;
        l->head = p;
        
    }

    l->size++;
    spin_unlock(&l->lock);
}

