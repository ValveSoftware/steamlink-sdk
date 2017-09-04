#!/usr/bin/env python
#coding:utf-8
# Author:  mozman
# Created: 08.05.2010
# Copyright (c) 2010-2013 by Manfred Moitzi
# License: MIT License

from ctrees cimport node_t
from stack cimport node_stack_t

cdef class cWalker:
    cdef node_t *node
    cdef node_t *root
    cdef node_stack_t *stack

    cdef void set_tree(self, node_t *root)
    cpdef reset(self)
    cpdef push(self)
    cpdef pop(self)
