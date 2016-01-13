/*
 * stack.h
 *
 *  Author: mozman
 *  Copyright (c) 2010-2013 by Manfred Moitzi
 *  License: MIT-License
 */

#ifndef STACK_H_
#define STACK_H_

#include "ctrees.h"

typedef struct node_stack node_stack_t;
struct node_stack {
  int stackptr;
  int size;
  node_t **stack;
};

node_stack_t *stack_init(int size);
void stack_delete(node_stack_t *stack);
void stack_push(node_stack_t *stack, node_t *node);
node_t *stack_pop(node_stack_t *stack);
int stack_is_empty(node_stack_t *stack);
void stack_reset(node_stack_t *stack);

#endif /* STACK_H_ */
