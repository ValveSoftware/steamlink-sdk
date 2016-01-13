#!/usr/bin/env python
#coding:utf-8
# Author:  mozman
# Created: 08.05.2010
# Copyright (c) 2010-2013 by Manfred Moitzi
# License: MIT-License

from ctrees cimport node_t

cdef extern from "stack.h":
    ctypedef struct node_stack_t:
        pass
    node_stack_t *stack_init(int size)
    void stack_delete(node_stack_t *stack)
    void stack_push(node_stack_t *stack, node_t *node)
    node_t *stack_pop(node_stack_t *stack)
    int stack_is_empty(node_stack_t *stack)
    void stack_reset(node_stack_t *stack)
