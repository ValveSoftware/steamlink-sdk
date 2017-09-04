#!/usr/bin/env python
#coding:utf-8
# Author:  mozman
# Purpose: tree walker for cython trees
# Created: 07.05.2010
# Copyright (c) 2010-2013 by Manfred Moitzi
# License: MIT License

DEF MAXSTACK=32

from stack cimport *
from ctrees cimport *

cdef class cWalker:
    def __cinit__(self):
        self.root = NULL
        self.node = NULL
        self.stack = stack_init(MAXSTACK)

    def __dealloc__(self):
        stack_delete(self.stack)

    cdef void set_tree(self, node_t *root):
        self.root = root
        self.reset()

    cpdef reset(self):
        stack_reset(self.stack)
        self.node = self.root

    @property
    def key(self):
        return <object> self.node.key

    @property
    def value(self):
        return <object> self.node.value

    @property
    def item(self):
        return (<object>self.node.key, <object>self.node.value)

    @property
    def is_valid(self):
        return self.node != NULL

    def goto(self, key):
        cdef int cval
        self.node = self.root
        while self.node != NULL:
            cval = ct_compare(key, <object> self.node.key)
            if cval == 0:
                return True
            elif cval < 0:
                self.node = self.node.link[0]
            else:
                self.node = self.node.link[1]
        return False

    cpdef push(self):
        stack_push(self.stack, self.node)

    cpdef pop(self):
        if stack_is_empty(self.stack) != 0:
            raise IndexError('pop(): stack is empty')
        self.node = stack_pop(self.stack)

    def stack_is_empty(self):
        return <bint> stack_is_empty(self.stack)

    def goto_leaf(self):
        """ get a leaf node """
        while self.node != NULL:
            if self.node.link[0] != NULL:
                self.node = self.node.link[0]
            elif self.node.link[1] != NULL:
                self.node = self.node.link[1]
            else:
                return

    def has_child(self, int direction):
        return self.node.link[direction] != NULL

    def down(self, int direction):
        self.node = self.node.link[direction]

    def go_left(self):
        self.node = self.node.link[0]

    def go_right(self):
        self.node = self.node.link[1]

    def has_left(self):
        return self.node.link[0] != NULL

    def has_right(self):
        return self.node.link[1] != NULL

    def succ_item(self, key):
        """ Get successor (k,v) pair of key, raises KeyError if key is max key
        or key does not exist.
        """
        self.node = ct_succ_node(self.root, key)
        if self.node == NULL: # given key is biggest in tree
            raise KeyError(str(key))
        return (<object> self.node.key, <object> self.node.value)

    def prev_item(self, key):
        """ Get predecessor (k,v) pair of key, raises KeyError if key is min key
        or key does not exist.
        """
        self.node = ct_prev_node(self.root, key)
        if self.node == NULL: # given key is smallest in tree
            raise KeyError(str(key))
        return (<object> self.node.key, <object> self.node.value)

    def floor_item(self, key):
        """ Get the element (k,v) pair associated with the greatest key less
        than or equal to the given key, raises KeyError if there is no such key.
        """
        self.node = ct_floor_node(self.root, key)
        if self.node == NULL:  # given key is smaller than min-key in tree
            raise KeyError(str(key))
        return (<object> self.node.key, <object> self.node.value)

    def ceiling_item(self, key):
        """ Get the element (k,v) pair associated with the smallest key greater
        than or equal to the given key, raises KeyError if there is no such key.
        """
        self.node = ct_ceiling_node(self.root, key)
        if self.node == NULL:  # given key is greater than max-key in tree
            raise KeyError(str(key))
        return (<object> self.node.key, <object> self.node.value)
