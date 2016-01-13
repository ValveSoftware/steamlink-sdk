#!/usr/bin/env python
#coding:utf-8
# Author:  mozman
# Purpose: cython unbalanced binary tree module
# Created: 28.04.2010
# Copyright (c) 2010-2013 by Manfred Moitzi
# License: MIT License

__all__ = ['cRBTree']

from cwalker import cWalker

from cwalker cimport *
from ctrees cimport *

cdef class cRBTree:
    cdef node_t *_root
    cdef int _count

    def __cinit__(self, items=None):
        self._root = NULL
        self._count = 0
        if items:
            self.update(items)

    def __dealloc__(self):
        ct_delete_tree(self._root)

    @property
    def count(self):
        return self._count

    def __getstate__(self):
        return dict(self.items())

    def __setstate__(self, state):
        self.update(state)

    def clear(self):
        ct_delete_tree(self._root)
        self._count = 0
        self._root = NULL

    def get_value(self, key):
        result = <object> ct_get_item(self._root, key)
        if result is None:
            raise KeyError(key)
        else:
            return result[1]

    def get_walker(self):
        cdef cWalker walker
        walker = cWalker()
        walker.set_tree(self._root)
        return walker

    def insert(self, key, value):
        res = rb_insert(&self._root, key, value)
        if res < 0:
            raise MemoryError('Can not allocate memory for node structure.')
        else:
            self._count += res

    def remove(self, key):
        cdef int result
        result =  rb_remove(&self._root, key)
        if result == 0:
            raise KeyError(str(key))
        else:
            self._count -= 1

    def max_item(self):
        """ Get item with max key of tree, raises ValueError if tree is empty. """
        cdef node_t *node
        node = ct_max_node(self._root)
        if node == NULL: # root is None
            raise ValueError("Tree is empty")
        return (<object>node.key, <object>node.value)

    def min_item(self):
        """ Get item with min key of tree, raises ValueError if tree is empty. """
        cdef node_t *node
        node = ct_min_node(self._root)
        if node == NULL: # root is None
            raise ValueError("Tree is empty")
        return (<object>node.key, <object>node.value)
