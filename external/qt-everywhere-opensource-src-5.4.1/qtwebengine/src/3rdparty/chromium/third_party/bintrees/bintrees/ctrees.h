/*
 * ctrees.h
 *
 *  Author: mozman
 *  Copyright (c) 2010-2013 by Manfred Moitzi
 *  License: MIT-License
 */

#ifndef __CTREES_H
#define __CTREES_H

#include <Python.h>

typedef struct tree_node node_t;

struct tree_node {
	node_t *link[2];
	PyObject *key;
	PyObject *value;
	int xdata;
};

typedef node_t* nodeptr;

/* common binary tree functions */
void ct_delete_tree(node_t *root);
int ct_compare(PyObject *key1, PyObject *key2);
PyObject *ct_get_item(node_t *root, PyObject *key);
node_t *ct_find_node(node_t *root, PyObject *key);
node_t *ct_succ_node(node_t *root, PyObject *key);
node_t *ct_prev_node(node_t *root, PyObject *key);
node_t *ct_max_node(node_t *root);
node_t *ct_min_node(node_t *root);
node_t *ct_floor_node(node_t *root, PyObject *key);
node_t *ct_ceiling_node(node_t *root, PyObject *key);
int ct_index_of(node_t *root, PyObject *key);
node_t *ct_node_at(node_t *root, int index);

/* unbalanced binary tree */
int ct_bintree_insert(node_t **root, PyObject *key, PyObject *value);
int ct_bintree_remove(node_t **root, PyObject *key);

/* avl-tree functions */
int avl_insert(node_t **root, PyObject *key, PyObject *value);
int avl_remove(node_t **root, PyObject *key);

/* rb-tree functions */
int rb_insert(node_t **root, PyObject *key, PyObject *value);
int rb_remove(node_t **root, PyObject *key);

#endif
