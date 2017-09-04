#!/usr/bin/env python
#coding:utf-8
# Author:  mozman
# Purpose: tree walker
# Created: 07.05.2010
# Copyright (c) 2010-2013 by Manfred Moitzi
# License: MIT License

from operator import attrgetter, lt, gt


class Walker(object):
    __slots__ = ['_node', '_stack', '_tree']

    def __init__(self, tree):
        self._tree = tree
        self._node = tree.root
        self._stack = []

    def reset(self):
        self._stack = []
        self._node = self._tree.root

    @property
    def key(self):
        return self._node.key

    @property
    def value(self):
        return self._node.value

    @property
    def item(self):
        return (self._node.key, self._node.value)

    @property
    def is_valid(self):
        return self._node is not None

    def goto(self, key):
        self._node = self._tree.root
        while self._node is not None:
            if key == self._node.key:
                return True
            elif key < self._node.key:
                self.go_left()
            else:
                self.go_right()
        return False

    def push(self):
        self._stack.append(self._node)

    def pop(self):
        self._node = self._stack.pop()

    def stack_is_empty(self):
        return len(self._stack) == 0

    def goto_leaf(self):
        """ get a leaf node """
        while self._node is not None:
            if self.has_left():
                self.go_left()
            elif self.has_right():
                self.go_right()
            else:
                return

    def has_child(self, direction):
        if direction == 0:
            return self._node.left is not None
        else:
            return self._node.right is not None

    def down(self, direction):
        if direction == 0:
            self._node = self._node.left
        else:
            self._node = self._node.right

    def go_left(self):
        self._node = self._node.left

    def go_right(self):
        self._node = self._node.right

    def has_left(self):
        return self._node.left is not None

    def has_right(self):
        return self._node.right is not None

    def _next_item(self, key, left, right, less_than):
        node = self._tree.root
        succ = None
        while node is not None:
            if key == node.key:
                break
            elif less_than(key, node.key):
                if (succ is None) or less_than(node.key, succ.key):
                    succ = node
                node = left(node)
            else:
                node = right(node)

        if node is None:  # stay at dead end
            raise KeyError(str(key))
        # found node of key
        if right(node) is not None:
            # find smallest node of right subtree
            node = right(node)
            while left(node) is not None:
                node = left(node)
            if succ is None:
                succ = node
            elif less_than(node.key, succ.key):
                succ = node
        elif succ is None:  # given key is biggest in tree
            raise KeyError(str(key))
        return (succ.key, succ.value)

    def succ_item(self, key):
        """ Get successor (k,v) pair of key, raises KeyError if key is max key
        or key does not exist.
        """
        return self._next_item(
            key,
            left=attrgetter("left"),
            right=attrgetter("right"),
            less_than=lt,
        )

    def prev_item(self, key):
        """ Get predecessor (k,v) pair of key, raises KeyError if key is min key
        or key does not exist.
        """
        return self._next_item(
            key,
            left=attrgetter("right"),
            right=attrgetter("left"),
            less_than=gt,
        )

    def _iteritems(self, left=attrgetter("left"), right=attrgetter("right")):
        """ optimized forward iterator (reduced method calls) """
        if self._tree.is_empty():
            return
        node = self._tree.root
        stack = self._stack
        go_left = True
        while True:
            if left(node) is not None and go_left:
                stack.append(node)
                node = left(node)
            else:
                yield (node.key, node.value)
                if right(node) is not None:
                    node = right(node)
                    go_left = True
                else:
                    if len(stack) == 0:
                        return  # all done
                    node = stack.pop()
                    go_left = False

    def iter_items_forward(self):
        for item in self._iteritems(
            left=attrgetter("left"),
            right=attrgetter("right"),
        ):
            yield item

    def iter_items_backward(self):
        for item in self._iteritems(
            left=attrgetter("right"),
            right=attrgetter("left"),
        ):
            yield item

    def floor_item(self, key):
        """ Get the element (k,v) pair associated with the greatest key less
        than or equal to the given key, raises KeyError if there is no such key.
        """
        node = self._tree.root
        prev = None
        while node is not None:
            if key == node.key:
                return node.key, node.value
            elif key < node.key:
                node = node.left
            else:
                if (prev is None) or (node.key > prev.key):
                    prev = node
                node = node.right
        # node must be None here
        if prev:
            return prev.key, prev.value
        raise KeyError(str(key))

    def ceiling_item(self, key):
        """ Get the element (k,v) pair associated with the smallest key greater
        than or equal to the given key, raises KeyError if there is no such key.
        """
        node = self._tree.root
        succ = None
        while node is not None:
            if key == node.key:
                return node.key, node.value
            elif key > node.key:
                node = node.right
            else:
                if (succ is None) or (node.key < succ.key):
                    succ = node
                node = node.left
            # node must be None here
        if succ:
            return succ.key, succ.value
        raise KeyError(str(key))

