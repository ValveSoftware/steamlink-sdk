Binary Tree Package
===================

Abstract
========

This package provides Binary- RedBlack- and AVL-Trees written in Python and Cython.

This Classes are much slower than the built-in dict class, but all
iterators/generators yielding data in sorted key order.

Source of Algorithms
--------------------

AVL- and RBTree algorithms taken from Julienne Walker: http://eternallyconfuzzled.com/jsw_home.aspx

Trees written in Python (only standard library)
-----------------------------------------------

    - *BinaryTree* -- unbalanced binary tree
    - *AVLTree* -- balanced AVL-Tree
    - *RBTree* -- balanced Red-Black-Tree

Trees written with C-Functions and Cython as wrapper
----------------------------------------------------

    - *FastBinaryTree* -- unbalanced binary tree
    - *FastAVLTree* -- balanced AVL-Tree
    - *FastRBTree* -- balanced Red-Black-Tree

All trees provides the same API, the pickle protocol is supported.

FastXTrees has C-structs as tree-node structure and C-implementation for low level
operations: insert, remove, get_value, max_item, min_item.

Constructor
~~~~~~~~~~~

    * Tree() -> new empty tree;
    * Tree(mapping) -> new tree initialized from a mapping (requires only an items() method)
    * Tree(seq) -> new tree initialized from seq [(k1, v1), (k2, v2), ... (kn, vn)]

Methods
~~~~~~~

    * __contains__(k) -> True if T has a key k, else False, O(log(n))
    * __delitem__(y) <==> del T[y], del[s:e], O(log(n))
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
    * clear() -> None, remove all items from T, O(n)
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
    * foreach(f, [order]) -> visit all nodes of tree (0 = 'inorder', -1 = 'preorder' or +1 = 'postorder') and call f(k, v) for each node, O(n)

slicing by keys
~~~~~~~~~~~~~~~

    * itemslice(s, e) -> generator for (k, v) items of T for s <= key < e, O(n)
    * keyslice(s, e) -> generator for keys of T for s <= key < e, O(n)
    * valueslice(s, e) -> generator for values of T for s <= key < e, O(n)
    * T[s:e] -> TreeSlice object, with keys in range s <= key < e, O(n)
    * del T[s:e] -> remove items by key slicing, for s <= key < e, O(n)

    start/end parameter:

    * if 's' is None or T[:e] TreeSlice/iterator starts with value of min_key();
    * if 'e' is None or T[s:] TreeSlice/iterator ends with value of max_key();
    * T[:] is a TreeSlice which represents the whole tree;

    TreeSlice is a tree wrapper with range check, and contains no references
    to objects, deleting objects in the associated tree also deletes the object
    in the TreeSlice.

    * TreeSlice[k] -> get value for key k, raises KeyError if k not exists in range s:e
    * TreeSlice[s1:e1] -> TreeSlice object, with keys in range s1 <= key < e1
        - new lower bound is max(s, s1)
        - new upper bound is min(e, e1)

    TreeSlice methods:

    * items() -> generator for (k, v) items of T, O(n)
    * keys() -> generator for keys of T, O(n)
    * values() -> generator for values of  T, O(n)
    * __iter__ <==> keys()
    * __repr__ <==> repr(T)
    * __contains__(key)-> True if TreeSlice has a key k, else False, O(log(n))

prev/succ operations
~~~~~~~~~~~~~~~~~~~~

    * prev_item(key) -> get (k, v) pair, where k is predecessor to key, O(log(n))
    * prev_key(key) -> k, get the predecessor of key, O(log(n))
    * succ_item(key) -> get (k,v) pair as a 2-tuple, where k is successor to key, O(log(n))
    * succ_key(key) -> k, get the successor of key, O(log(n))
    * floor_item(key) -> get (k, v) pair, where k is the greatest key less than or equal to key, O(log(n))
    * floor_key(key) -> k, get the greatest key less than or equal to key, O(log(n))
    * ceiling_item(key) -> get (k, v) pair, where k is the smallest key greater than or equal to key, O(log(n))
    * ceiling_key(key) -> k, get the smallest key greater than or equal to key, O(log(n))

Heap methods
~~~~~~~~~~~~

    * max_item() -> get largest (key, value) pair of T, O(log(n))
    * max_key() -> get largest key of T, O(log(n))
    * min_item() -> get smallest (key, value) pair of T, O(log(n))
    * min_key() -> get smallest key of T, O(log(n))
    * pop_min() -> (k, v), remove item with minimum key, O(log(n))
    * pop_max() -> (k, v), remove item with maximum key, O(log(n))
    * nlargest(i[,pop]) -> get list of i largest items (k, v), O(i*log(n))
    * nsmallest(i[,pop]) -> get list of i smallest items (k, v), O(i*log(n))

Set methods (using frozenset)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    * intersection(t1, t2, ...) -> Tree with keys *common* to all trees
    * union(t1, t2, ...) -> Tree with keys from *either* trees
    * difference(t1, t2, ...) -> Tree with keys in T but not any of t1, t2, ...
    * symmetric_difference(t1) -> Tree with keys in either T and t1  but not both
    * issubset(S) -> True if every element in T is in S
    * issuperset(S) -> True if every element in S is in T
    * isdisjoint(S) ->  True if T has a null intersection with S

Classmethods
~~~~~~~~~~~~

    * fromkeys(S[,v]) -> New tree with keys from S and values equal to v.

Performance
===========

Profiling with timeit(): 5000 unique random int keys, time in seconds

========================  =============  ==============  ==============  ==============
unbalanced Trees          CPython 2.7.2  FastBinaryTree  ipy 2.7.0       pypy 1.7.0
========================  =============  ==============  ==============  ==============
build time 100x            7,55           0,60            2,51            0,29
build & delete time 100x  13,34           1,48            4,45            0,47
search 100x all keys       2,86           0,96            0,27            0,06
========================  =============  ==============  ==============  ==============

========================  =============  ==============  ==============  ==============
AVLTrees                  CPython 2.7.2  FastAVLTree     ipy 2.7.0       pypy 1.7.0
========================  =============  ==============  ==============  ==============
build time 100x	          22,66           0,65           10,45            1,29
build & delete time 100x  36,71           1,47           20,89            3,02
search 100x all keys       2,34           0,85            0,89            0,14
========================  =============  ==============  ==============  ==============

========================  =============  ==============  ==============  ==============
RBTrees                   CPython 2.7.2  FastRBTree      ipy 2.7.0       pypy 1.7.0
========================  =============  ==============  ==============  ==============
build time 100x	          14,78           0,65            4,43            0,49
build & delete time 100x  39,34           1,63           12,43            1,32
search 100x all keys       2,32           0,86            0,86            0,13
========================  =============  ==============  ==============  ==============

News
====

Version 1.0.1 February 2013

  * bug fixes
  * refactorings by graingert
  * skip useless tests for pypy
  * new license: MIT License
  * tested with CPython2.7, CPython3.2, CPython3.3, pypy-1.9, pypy-2.0-beta1
  * unified line endings to LF
  * PEP8 refactorings
  * added floor_item/key, ceiling_item/key methods, thanks to Dai Mikurube

Version 1.0.0

  * bug fixes
  * status: 5 - Production/Stable
  * removed useless TreeIterator() class and T.treeiter() method.
  * patch from Max Motovilov to use Visual Studio 2008 for building C-extensions

Version 0.4.0

  * API change!!!
  * full Python 3 support, also for Cython implementations
  * removed user defined compare() function - keys have to be comparable!
  * removed T.has_key(), use 'key in T'
  * keys(), items(), values() generating 'views'
  * removed iterkeys(), itervalues(), iteritems() methods
  * replaced index slicing by key slicing
  * removed index() and item_at()
  * repr() produces a correct representation
  * installs on systems without cython (tested with pypy)
  * new license: GNU Library or Lesser General Public License (LGPL)

Installation
============

from source::

    python setup.py install

Download
========

http://bitbucket.org/mozman/bintrees/downloads

Documentation
=============

this README.txt

bintrees can be found on bitbucket.org at:

http://bitbucket.org/mozman/bintrees
