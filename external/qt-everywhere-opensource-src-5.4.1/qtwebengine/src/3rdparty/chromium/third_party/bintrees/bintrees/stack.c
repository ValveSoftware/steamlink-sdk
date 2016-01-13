/*
 * stack.c
 *
 *  Author: mozman
 *  Copyright (c) 2010-2013 by Manfred Moitzi
 *  License: MIT-License
 */

#include "ctrees.h"
#include "stack.h"

extern node_stack_t *
stack_init(int size)
{
	node_stack_t *stack;

	stack = PyMem_Malloc(sizeof(node_stack_t));
	stack->stack = PyMem_Malloc(sizeof(node_t *) * size);
	stack->size = size;
	stack->stackptr = 0;
	return stack;
}

extern void
stack_delete(node_stack_t *stack)
{
	PyMem_Free(stack->stack);
	PyMem_Free(stack);
}

extern void
stack_push(node_stack_t *stack, node_t *node)
{
	stack->stack[stack->stackptr++] = node;
	if (stack->stackptr >= stack->size) {
		stack->size *= 2;
		stack->stack = PyMem_Realloc(stack->stack,
				sizeof(node_t *) * stack->size);
	}
}

extern node_t *
stack_pop(node_stack_t *stack)
{
	return (stack->stackptr > 0) ? stack->stack[--stack->stackptr] : NULL;
}

extern int
stack_is_empty(node_stack_t *stack)
{
	return (stack->stackptr == 0);
}

extern void
stack_reset(node_stack_t *stack)
{
	stack->stackptr = 0;
}
