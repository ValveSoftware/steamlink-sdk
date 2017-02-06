#!/usr/bin/env python
#coding:utf-8
# Author:  Mozman
# Purpose: treemixin provides top level functions for binary trees
# Created: 03.05.2010
# Copyright (c) 2010-2013 by Manfred Moitzi
# License: MIT License

from __future__ import absolute_import

from .walker import Walker
from .treeslice import TreeSlice

class TreeMixin(object):
    """
    Abstract-Base-Class for the pure Python Trees: BinaryTree, AVLTree and RBTree
    Mixin-Class for the Cython based Trees: FastBinaryTree, FastAVLTree, FastRBTree

    The TreeMixin Class
    ===================

    T has to implement following properties
    ---------------------------------------

    count -- get node count

    T has to implement following methods
    ------------------------------------

    get_walker(...)
        get a tree walker object, provides methods to traverse the tree see walker.py

    insert(...)
        insert(key, value) <==> T[key] = value, insert key into T

    remove(...)
        remove(key) <==> del T[key], remove key from T

    clear(...)
        T.clear() -> None.  Remove all items from T.

    Methods defined here
    --------------------

    * __contains__(k) -> True if T has a key k, else False, O(log(n))
    * __delitem__(y) <==> del T[y], del T[s:e], O(log(n))
    * __getitem__(y) <==> T[y], T[s:e], O(log(n))
    * __iter__() <==> iter(T)
    * __len__() <==> len(T), O(1)
    * __max__() <==> max(T), get max item (k,v) of T, O(log(n))
    * __min__() <==> min(T), get min item (k,v) of T, O(log(n))
    * __and__(other) <==> T & other, intersection
    * __or__(other) <==> T | other, union
    * __sub__(other) <==> T - other, difference
    * __xor__(other) <==> T ^ other, symmetric_difference
    * __repr__() <==> repr(T)
    * __setitem__(k, v) <==> T[k] = v, O(log(n))
    * clear() -> None, Remove all items from T, , O(n)
    * copy() -> a shallow copy of T, O(n*log(n))
    * discard(k) -> None, remove k from T, if k is present, O(log(n))
    * get(k[,d]) -> T[k] if k in T, else d, O(log(n))
    * is_empty() -> True if len(T) == 0, O(1)
    * items([reverse]) -> generator for (k, v) items of T, O(n)
    * keys([reverse]) -> generator for keys of T, O(n)
    * values([reverse]) -> generator for values of  T, O(n)
    * pop(k[,d]) -> v, remove specified key and return the corresponding value, O(log(n))
    * popitem() -> (k, v), remove and return some (key, value) pair as a 2-tuple, O(log(n))
    * setdefault(k[,d]) -> T.get(k, d), also set T[k]=d if k not in T, O(log(n))
    * update(E) -> None.  Update T from dict/iterable E, O(E*log(n))
    * foreach(f, [order]) -> visit all nodes of tree and call f(k, v) for each node, O(n)

    slicing by keys

    * itemslice(s, e) -> generator for (k, v) items of T for s <= key < e, O(n)
    * keyslice(s, e) -> generator for keys of T for s <= key < e, O(n)
    * valueslice(s, e) -> generator for values of T for s <= key < e, O(n)
    * T[s:e] -> TreeSlice object, with keys in range s <= key < e, O(n)
    * del T[s:e] -> remove items by key slicing, for s <= key < e, O(n)

    if 's' is None or T[:e] TreeSlice/iterator starts with value of min_key()
    if 'e' is None or T[s:] TreeSlice/iterator ends with value of max_key()
    T[:] is a TreeSlice which represents the whole tree.

    TreeSlice is a tree wrapper with range check, and contains no references
    to objects, deleting objects in the associated tree also deletes the object
    in the TreeSlice.

    * TreeSlice[k] -> get value for key k, raises KeyError if k not exists in range s:e
    * TreeSlice[s1:e1] -> TreeSlice object, with keys in range s1 <= key < e1

      * new lower bound is max(s, s1)
      * new upper bound is min(e, e1)

    TreeSlice methods:

    * items() -> generator for (k, v) items of T, O(n)
    * keys() -> generator for keys of T, O(n)
    * values() -> generator for values of  T, O(n)
    * __iter__ <==> keys()
    * __repr__ <==> repr(T)
    * __contains__(key)-> True if TreeSlice has a key k, else False, O(log(n))

    prev/succ operations

    * prev_item(key) -> get (k, v) pair, where k is predecessor to key, O(log(n))
    * prev_key(key) -> k, get the predecessor of key, O(log(n))
    * succ_item(key) -> get (k,v) pair as a 2-tuple, where k is successor to key, O(log(n))
    * succ_key(key) -> k, get the successor of key, O(log(n))
    * floor_item(key) -> get (k, v) pair, where k is the greatest key less than or equal to key, O(log(n))
    * floor_key(key) -> k, get the greatest key less than or equal to key, O(log(n))
    * ceiling_item(key) -> get (k, v) pair, where k is the smallest key greater than or equal to key, O(log(n))
    * ceiling_key(key) -> k, get the smallest key greater than or equal to key, O(log(n))

    Heap methods

    * max_item() -> get largest (key, value) pair of T, O(log(n))
    * max_key() -> get largest key of T, O(log(n))
    * min_item() -> get smallest (key, value) pair of T, O(log(n))
    * min_key() -> get smallest key of T, O(log(n))
    * pop_min() -> (k, v), remove item with minimum key, O(log(n))
    * pop_max() -> (k, v), remove item with maximum key, O(log(n))
    * nlargest(i[,pop]) -> get list of i largest items (k, v), O(i*log(n))
    * nsmallest(i[,pop]) -> get list of i smallest items (k, v), O(i*log(n))

    Set methods (using frozenset)

    * intersection(t1, t2, ...) -> Tree with keys *common* to all trees
    * union(t1, t2, ...) -> Tree with keys from *either* trees
    * difference(t1, t2, ...) -> Tree with keys in T but not any of t1, t2, ...
    * symmetric_difference(t1) -> Tree with keys in either T and t1  but not both
    * issubset(S) -> True if every element in T is in S
    * issuperset(S) -> True if every element in S is in T
    * isdisjoint(S) ->  True if T has a null intersection with S

    Classmethods

    * fromkeys(S[,v]) -> New tree with keys from S and values equal to v.

    """
    def get_walker(self):
        return Walker(self)

    def __repr__(self):
        """ x.__repr__(...) <==> repr(x) """
        tpl = "%s({%s})" % (self.__class__.__name__, '%s')
        return tpl % ", ".join( ("%r: %r" % item for item in self.items()) )

    def copy(self):
        """ T.copy() -> get a shallow copy of T. """
        tree = self.__class__()
        self.foreach(tree.insert, order=-1)
        return tree
    __copy__ = copy

    def __contains__(self, key):
        """ k in T -> True if T has a key k, else False """
        try:
            self.get_value(key)
            return True
        except KeyError:
            return False

    def __len__(self):
        """ x.__len__() <==> len(x) """
        return self.count

    def __min__(self):
        """ x.__min__() <==> min(x) """
        return self.min_item()

    def __max__(self):
        """ x.__max__() <==> max(x) """
        return self.max_item()

    def __and__(self, other):
        """ x.__and__(other) <==> self & other """
        return self.intersection(other)

    def __or__(self, other):
        """ x.__or__(other) <==> self | other """
        return self.union(other)

    def __sub__(self, other):
        """ x.__sub__(other) <==> self - other """
        return self.difference(other)

    def __xor__(self, other):
        """ x.__xor__(other) <==> self ^ other """
        return self.symmetric_difference(other)

    def discard(self, key):
        """ x.discard(k) -> None, remove k from T, if k is present """
        try:
            self.remove(key)
        except KeyError:
            pass

    def __del__(self):
        self.clear()

    def is_empty(self):
        """ x.is_empty() -> False if T contains any items else True"""
        return self.count == 0

    def keys(self, reverse=False):
        """ T.iterkeys([reverse]) -> an iterator over the keys of T, in ascending
        order if reverse is True, iterate in descending order, reverse defaults
        to False
        """
        return ( item[0] for item in self.items(reverse) )
    __iter__ = keys

    def __reversed__(self):
        return self.keys(reverse=True)

    def values(self, reverse=False):
        """ T.values([reverse]) -> an iterator over the values of T, in ascending order
        if reverse is True, iterate in descending order, reverse defaults to False
        """
        return ( item[1] for item in self.items(reverse) )

    def items(self, reverse=False):
        """ T.items([reverse]) -> an iterator over the (key, value) items of T,
        in ascending order if reverse is True, iterate in descending order,
        reverse defaults to False
        """
        if self.is_empty():
            return []

        def default_iterator(node):
            direction = 1 if reverse else 0
            other = 1 - direction
            go_down = True
            while True:
                if node.has_child(direction) and go_down:
                    node.push()
                    node.down(direction)
                else:
                    yield node.item
                    if node.has_child(other):
                        node.down(other)
                        go_down = True
                    else:
                        if node.stack_is_empty():
                            return  # all done
                        node.pop()
                        go_down = False

        treewalker = self.get_walker()
        try:  # specialized iterators
            if reverse:
                return treewalker.iter_items_backward()
            else:
                return treewalker.iter_items_forward()
        except AttributeError:
            return default_iterator(treewalker)

    def __getitem__(self, key):
        """ x.__getitem__(y) <==> x[y] """
        if isinstance(key, slice):
            return TreeSlice(self, key.start, key.stop)
        else:
            return self.get_value(key)

    def __setitem__(self, key, value):
        """ x.__setitem__(i, y) <==> x[i]=y """
        if isinstance(key, slice):
            raise ValueError('setslice is not supported')
        self.insert(key, value)

    def __delitem__(self, key):
        """ x.__delitem__(y) <==> del x[y] """
        if isinstance(key, slice):
            self.delitems(self.keyslice(key.start, key.stop))
        else:
            self.remove(key)

    def delitems(self, keys):
        """ T.delitems(keys) -> remove all items by keys
        """
        # convert generator to a set, because the content of the
        # tree will be modified!
        for key in frozenset(keys):
            self.remove(key)

    def keyslice(self, startkey, endkey):
        """ T.keyslice(startkey, endkey) -> key iterator:
        startkey <= key < endkey.
        """
        return ( item[0] for item in self.itemslice(startkey, endkey) )

    def itemslice(self, startkey, endkey):
        """ T.itemslice(s, e) -> item iterator: s <= key < e.

        if s is None: start with min element -> T[:e]
        if e is None: end with max element -> T[s:]
        T[:] -> all items

        """
        if self.is_empty():
            return

        if startkey is None:
            # no lower bound
            can_go_left = lambda: node.has_left() and visit_left
        else:
            # don't visit subtrees without keys in search range
            can_go_left = lambda: node.key > startkey and node.has_left() and visit_left

        if endkey is None:
            # no upper bound
            can_go_right = lambda: node.has_right()
        else:
            # don't visit subtrees without keys in search range
            can_go_right = lambda: node.key < endkey and node.has_right()

        if (startkey, endkey) == (None, None):
            key_in_range = lambda: True
        elif startkey is None:
            key_in_range = lambda: node.key < endkey
        elif endkey is None:
            key_in_range = lambda: node.key >= startkey
        else:
            key_in_range = lambda: startkey <= node.key < endkey

        node = self.get_walker()
        visit_left = True
        while True:
            if can_go_left():
                node.push()
                node.go_left()
            else:
                if key_in_range():
                    yield node.item
                if can_go_right():
                    node.go_right()
                    visit_left = True
                else:
                    if node.stack_is_empty():
                        return
                    node.pop()
                    # left side is already done
                    visit_left = False

    def valueslice(self, startkey, endkey):
        """ T.valueslice(startkey, endkey) -> value iterator:
        startkey <= key < endkey.
        """
        return ( item[1] for item in self.itemslice(startkey, endkey) )

    def get_value(self, key):
        node = self.root
        while node is not None:
            if key == node.key:
                return node.value
            elif key < node.key:
                node = node.left
            else:
                node = node.right
        raise KeyError(str(key))

    def __getstate__(self):
        return dict(self.items())

    def __setstate__(self, state):
        # note for myself: this is called like __init__, so don't use clear()
        # to remove existing data!
        self._root = None
        self._count = 0
        self.update(state)

    def setdefault(self, key, default=None):
        """ T.setdefault(k[,d]) -> T.get(k,d), also set T[k]=d if k not in T """
        try:
            return self.get_value(key)
        except KeyError:
            self.insert(key, default)
            return default

    def update(self, *args):
        """ T.update(E) -> None. Update T from E : for (k, v) in E: T[k] = v """
        for items in args:
            try:
                generator = items.items()
            except AttributeError:
                generator = iter(items)

            for key, value in generator:
                self.insert(key, value)

    @classmethod
    def fromkeys(cls, iterable, value=None):
        """ T.fromkeys(S[,v]) -> New tree with keys from S and values equal to v.

        v defaults to None.
        """
        tree = cls()
        for key in iterable:
            tree.insert(key, value)
        return tree

    def get(self, key, default=None):
        """ T.get(k[,d]) -> T[k] if k in T, else d.  d defaults to None. """
        try:
            return self.get_value(key)
        except KeyError:
            return default

    def pop(self, key, *args):
        """ T.pop(k[,d]) -> v, remove specified key and return the corresponding value
        If key is not found, d is returned if given, otherwise KeyError is raised
        """
        if len(args) > 1:
            raise TypeError("pop expected at most 2 arguments, got %d" % (1 + len(args)))
        try:
            value = self.get_value(key)
            self.remove(key)
            return value
        except KeyError:
            if len(args) == 0:
                raise
            else:
                return args[0]

    def popitem(self):
        """ T.popitem() -> (k, v), remove and return some (key, value) pair as a
        2-tuple; but raise KeyError if T is empty
        """
        if self.is_empty():
            raise KeyError("popitem(): tree is empty")
        walker = self.get_walker()
        walker.goto_leaf()
        result = walker.item
        self.remove(walker.key)
        return result

    def foreach(self, func, order=0):
        """ Visit all tree nodes and process key, value.

        parm func: function(key, value)
        param int order: inorder = 0, preorder = -1, postorder = +1

        """
        def _traverse():
            if order == -1:
                func(node.key, node.value)
            if node.has_left():
                node.push()
                node.go_left()
                _traverse()
                node.pop()
            if order == 0:
                func(node.key, node.value)
            if node.has_right():
                node.push()
                node.go_right()
                _traverse()
                node.pop()
            if order == +1:
                func(node.key, node.value)

        node = self.get_walker()
        _traverse()

    def min_item(self):
        """ Get item with min key of tree, raises ValueError if tree is empty. """
        if self.count == 0:
            raise ValueError("Tree is empty")
        node = self._root
        while node.left is not None:
            node = node.left
        return (node.key, node.value)

    def pop_min(self):
        """ T.pop_min() -> (k, v), remove item with minimum key, raise ValueError
        if T is empty.
        """
        item = self.min_item()
        self.remove(item[0])
        return item

    def min_key(self):
        """ Get min key of tree, raises ValueError if tree is empty. """
        key, value = self.min_item()
        return key

    def prev_item(self, key):
        """ Get predecessor (k,v) pair of key, raises KeyError if key is min key
        or key does not exist.
        """
        if self.count == 0:
            raise KeyError("Tree is empty")
        walker = self.get_walker()
        return walker.prev_item(key)

    def succ_item(self, key):
        """ Get successor (k,v) pair of key, raises KeyError if key is max key
        or key does not exist.
        """
        if self.count == 0:
            raise KeyError("Tree is empty")
        walker = self.get_walker()
        return walker.succ_item(key)

    def prev_key(self, key):
        """ Get predecessor to key, raises KeyError if key is min key
        or key does not exist.
        """
        key, value = self.prev_item(key)
        return key

    def succ_key(self, key):
        """ Get successor to key, raises KeyError if key is max key
        or key does not exist.
        """
        key, value = self.succ_item(key)
        return key

    def floor_item(self, key):
        """ Get the element (k,v) pair associated with the greatest key less
        than or equal to the given key, raises KeyError if there is no such key.
        """
        if self.count == 0:
            raise KeyError("Tree is empty")
        walker = self.get_walker()
        return walker.floor_item(key)

    def floor_key(self, key):
        """ Get the greatest key less than or equal to the given key, raises
        KeyError if there is no such key.
        """
        key, value = self.floor_item(key)
        return key

    def ceiling_item(self, key):
        """ Get the element (k,v) pair associated with the smallest key greater
        than or equal to the given key, raises KeyError if there is no such key.
        """
        if self.count == 0:
            raise KeyError("Tree is empty")
        walker = self.get_walker()
        return walker.ceiling_item(key)

    def ceiling_key(self, key):
        """ Get the smallest key greater than or equal to the given key, raises
        KeyError if there is no such key.
        """
        key, value = self.ceiling_item(key)
        return key

    def max_item(self):
        """ Get item with max key of tree, raises ValueError if tree is empty. """
        if self.count == 0:
            raise ValueError("Tree is empty")
        node = self._root
        while node.right is not None:
            node = node.right
        return (node.key, node.value)

    def pop_max(self):
        """ T.pop_max() -> (k, v), remove item with maximum key, raise ValueError
        if T is empty.
        """
        item = self.max_item()
        self.remove(item[0])
        return item

    def max_key(self):
        """ Get max key of tree, raises ValueError if tree is empty. """
        key, value = self.max_item()
        return key

    def nsmallest(self, n, pop=False):
        """ T.nsmallest(n) -> get list of n smallest items (k, v).
        If pop is True, remove items from T.
        """
        if pop:
            return [self.pop_min() for _ in range(min(len(self), n))]
        else:
            items = self.items()
            return  [ next(items) for _ in range(min(len(self), n)) ]

    def nlargest(self, n, pop=False):
        """ T.nlargest(n) -> get list of n largest items (k, v).
        If pop is True remove items from T.
        """
        if pop:
            return [self.pop_max() for _ in range(min(len(self), n))]
        else:
            items = self.items(reverse=True)
            return [ next(items) for _ in range(min(len(self), n)) ]

    def intersection(self, *trees):
        """ x.intersection(t1, t2, ...) -> Tree, with keys *common* to all trees
        """
        thiskeys = frozenset(self.keys())
        sets = _build_sets(trees)
        rkeys = thiskeys.intersection(*sets)
        return self.__class__( ((key, self.get(key)) for key in rkeys) )

    def union(self, *trees):
        """ x.union(t1, t2, ...) -> Tree with keys from *either* trees
        """
        thiskeys = frozenset(self.keys())
        rkeys = thiskeys.union(*_build_sets(trees))
        return self.__class__( ((key, self.get(key)) for key in rkeys) )

    def difference(self, *trees):
        """ x.difference(t1, t2, ...) -> Tree with keys in T but not any of t1,
        t2, ...
        """
        thiskeys = frozenset(self.keys())
        rkeys = thiskeys.difference(*_build_sets(trees))
        return self.__class__( ((key, self.get(key)) for key in rkeys) )

    def symmetric_difference(self, tree):
        """ x.symmetric_difference(t1) -> Tree with keys in either T and t1  but
        not both
        """
        thiskeys = frozenset(self.keys())
        rkeys = thiskeys.symmetric_difference(frozenset(tree.keys()))
        return self.__class__( ((key, self.get(key)) for key in rkeys) )

    def issubset(self, tree):
        """ x.issubset(tree) -> True if every element in x is in tree """
        thiskeys = frozenset(self.keys())
        return thiskeys.issubset(frozenset(tree.keys()))

    def issuperset(self, tree):
        """ x.issubset(tree) -> True if every element in tree is in x """
        thiskeys = frozenset(self.keys())
        return thiskeys.issuperset(frozenset(tree.keys()))

    def isdisjoint(self, tree):
        """ x.isdisjoint(S) ->  True if x has a null intersection with tree """
        thiskeys = frozenset(self.keys())
        return thiskeys.isdisjoint(frozenset(tree.keys()))


def _build_sets(trees):
    return [ frozenset(tree.keys()) for tree in trees ]

