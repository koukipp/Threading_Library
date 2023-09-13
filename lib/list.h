#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include "atomic.h"

typedef struct node {
  struct node *next;
  struct node *prev;
} node;

node *node_create ();
void node_free (node *p);

typedef struct list {
  node *head;
  node *tail;
  size_t size;
  spin_t lock;
} list;


list *list_create ();
void list_free (list *l);
node *list_remove (list *l, node *p);
int is_empty(list *l);
node *list_dequeue (list *l);
node *list_remove_last (list *l);
void list_enqueue (list *l, node *p);
void list_push (list *l, node *p);
void list_init (list *l);
int transfer_half(list *src,list *dst);

#endif
