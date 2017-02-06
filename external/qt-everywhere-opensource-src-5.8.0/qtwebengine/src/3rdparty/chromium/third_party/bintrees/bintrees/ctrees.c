/*
 * ctrees.c
 *
 *  Author: mozman
 *  Copyright (c) 2010-2013 by Manfred Moitzi
 *  License: MIT-License
 */

#include "ctrees.h"
#include "stack.h"
#include <Python.h>

#define LEFT 0
#define RIGHT 1
#define KEY(node) (node->key)
#define VALUE(node) (node->value)
#define LEFT_NODE(node) (node->link[LEFT])
#define RIGHT_NODE(node) (node->link[RIGHT])
#define LINK(node, dir) (node->link[dir])
#define XDATA(node) (node->xdata)
#define RED(node) (node->xdata)
#define BALANCE(node) (node->xdata)

static node_t *
ct_new_node(PyObject *key, PyObject *value, int xdata)
{
	node_t *new_node = PyMem_Malloc(sizeof(node_t));
	if (new_node != NULL) {
		KEY(new_node) = key;
		Py_INCREF(key);
		VALUE(new_node) = value;
		Py_INCREF(value);
		LEFT_NODE(new_node) = NULL;
		RIGHT_NODE(new_node) = NULL;
		XDATA(new_node) = xdata;
	}
	return new_node;
}

static void
ct_delete_node(node_t *node)
{
	if (node != NULL) {
		Py_XDECREF(KEY(node));
		Py_XDECREF(VALUE(node));
		LEFT_NODE(node) = NULL;
		RIGHT_NODE(node) = NULL;
		PyMem_Free(node);
	}
}

extern void
ct_delete_tree(node_t *root)
{
	if (root == NULL)
		return;
	if (LEFT_NODE(root) != NULL) {
		ct_delete_tree(LEFT_NODE(root));
	}
	if (RIGHT_NODE(root) != NULL) {
		ct_delete_tree(RIGHT_NODE(root));
	}
	ct_delete_node(root);
}

static void
ct_swap_data(node_t *node1, node_t *node2)
{
	PyObject *tmp;
	tmp = KEY(node1);
	KEY(node1) = KEY(node2);
	KEY(node2) = tmp;
	tmp = VALUE(node1);
	VALUE(node1) = VALUE(node2);
	VALUE(node2) = tmp;
}

int
ct_compare(PyObject *key1, PyObject *key2)
{
	int res;

	res = PyObject_RichCompareBool(key1, key2, Py_LT);
	if (res > 0)
		return -1;
	else if (res < 0) {
		PyErr_SetString(PyExc_TypeError, "invalid type for key");
		return 0;
		}
	/* second compare:
	+1 if key1 > key2
	 0 if not -> equal
	-1 means error, if error, it should happend at the first compare
	*/
	return PyObject_RichCompareBool(key1, key2, Py_GT);
}

extern node_t *
ct_find_node(node_t *root, PyObject *key)
{
	int res;
	while (root != NULL) {
		res = ct_compare(key, KEY(root));
		if (res == 0) /* key found */
			return root;
		else {
			root = LINK(root, (res > 0));
		}
	}
	return NULL; /* key not found */
}

extern PyObject *
ct_get_item(node_t *root, PyObject *key)
{
	node_t *node;
	PyObject *tuple;

	node = ct_find_node(root, key);
	if (node != NULL) {
		tuple = PyTuple_New(2);
		PyTuple_SET_ITEM(tuple, 0, KEY(node));
		PyTuple_SET_ITEM(tuple, 1, VALUE(node));
		return tuple;
	}
	Py_RETURN_NONE;
}

extern node_t *
ct_max_node(node_t *root)
/* get node with largest key */
{
	if (root == NULL)
		return NULL;
	while (RIGHT_NODE(root) != NULL)
		root = RIGHT_NODE(root);
	return root;
}

extern node_t *
ct_min_node(node_t *root)
// get node with smallest key
{
	if (root == NULL)
		return NULL;
	while (LEFT_NODE(root) != NULL)
		root = LEFT_NODE(root);
	return root;
}

extern int
ct_bintree_remove(node_t **rootaddr, PyObject *key)
/* attention: rootaddr is the address of the root pointer */
{
	node_t *node, *parent, *replacement;
	int direction, cmp_res, down_dir;

	node = *rootaddr;

	if (node == NULL)
		return 0; /* root is NULL */
	parent = NULL;
	direction = 0;

	while (1) {
		cmp_res = ct_compare(key, KEY(node));
		if (cmp_res == 0) /* key found, remove node */
		{
			if ((LEFT_NODE(node) != NULL) && (RIGHT_NODE(node) != NULL)) {
				/* find replacement node: smallest key in right-subtree */
				parent = node;
				direction = RIGHT;
				replacement = RIGHT_NODE(node);
				while (LEFT_NODE(replacement) != NULL) {
					parent = replacement;
					direction = LEFT;
					replacement = LEFT_NODE(replacement);
				}
				LINK(parent, direction) = RIGHT_NODE(replacement);
				/* swap places */
				ct_swap_data(node, replacement);
				node = replacement; /* delete replacement node */
			}
			else {
				down_dir = (LEFT_NODE(node) == NULL) ? RIGHT : LEFT;
				if (parent == NULL) /* root */
				{
					*rootaddr = LINK(node, down_dir);
				}
				else {
					LINK(parent, direction) = LINK(node, down_dir);
				}
			}
			ct_delete_node(node);
			return 1; /* remove was success full */
		}
		else {
			direction = (cmp_res < 0) ? LEFT : RIGHT;
			parent = node;
			node = LINK(node, direction);
			if (node == NULL)
				return 0; /* error key not found */
		}
	}
}

extern int
ct_bintree_insert(node_t **rootaddr, PyObject *key, PyObject *value)
/* attention: rootaddr is the address of the root pointer */
{
	node_t *parent, *node;
	int direction, cval;
	node = *rootaddr;
	if (node == NULL) {
		node = ct_new_node(key, value, 0); /* new node is also the root */
		if (node == NULL)
			return -1; /* got no memory */
		*rootaddr = node;
	}
	else {
		direction = LEFT;
		parent = NULL;
		while (1) {
			if (node == NULL) {
				node = ct_new_node(key, value, 0);
				if (node == NULL)
					return -1; /* get no memory */
				LINK(parent, direction) = node;
				return 1;
			}
			cval = ct_compare(key, KEY(node));
			if (cval == 0) {
				/* key exists, replace value object */
				Py_XDECREF(VALUE(node)); /* release old value object */
				VALUE(node) = value; /* set new value object */
				Py_INCREF(value); /* take new value object */
				return 0;
			}
			else {
				parent = node;
				direction = (cval < 0) ? LEFT : RIGHT;
				node = LINK(node, direction);
			}
		}
	}
	return 1;
}

static int
is_red (node_t *node)
{
	return (node != NULL) && (RED(node) == 1);
}

#define rb_new_node(key, value) ct_new_node(key, value, 1)

static node_t *
rb_single(node_t *root, int dir)
{
	node_t *save = root->link[!dir];

	root->link[!dir] = save->link[dir];
	save->link[dir] = root;

	RED(root) = 1;
	RED(save) = 0;
	return save;
}

static node_t *
rb_double(node_t *root, int dir)
{
	root->link[!dir] = rb_single(root->link[!dir], !dir);
	return rb_single(root, dir);
}

#define rb_new_node(key, value) ct_new_node(key, value, 1)

extern int
rb_insert(node_t **rootaddr, PyObject *key, PyObject *value)
{
	node_t *root = *rootaddr;

	if (root == NULL) {
		/*
		 We have an empty tree; attach the
		 new node directly to the root
		 */
		root = rb_new_node(key, value);
		if (root == NULL)
			return -1; // got no memory
	}
	else {
		node_t head; /* False tree root */
		node_t *g, *t; /* Grandparent & parent */
		node_t *p, *q; /* Iterator & parent */
		int dir = 0;
		int last = 0;
		int new_node = 0;

		/* Set up our helpers */
		t = &head;
		g = NULL;
		p = NULL;
		RIGHT_NODE(t) = root;
		LEFT_NODE(t) = NULL;
		q = RIGHT_NODE(t);

		/* Search down the tree for a place to insert */
		for (;;) {
			int cmp_res;
			if (q == NULL) {
				/* Insert a new node at the first null link */
				q = rb_new_node(key, value);
				p->link[dir] = q;
				new_node = 1;
				if (q == NULL)
					return -1; // get no memory
			}
			else if (is_red(q->link[0]) && is_red(q->link[1])) {
				/* Simple red violation: color flip */
				RED(q) = 1;
				RED(q->link[0]) = 0;
				RED(q->link[1]) = 0;
			}

			if (is_red(q) && is_red(p)) {
				/* Hard red violation: rotations necessary */
				int dir2 = (t->link[1] == g);

				if (q == p->link[last])
					t->link[dir2] = rb_single(g, !last);
				else
					t->link[dir2] = rb_double(g, !last);
			}

			/*  Stop working if we inserted a new node. */
			if (new_node)
				break;

			cmp_res = ct_compare(KEY(q), key);
			if (cmp_res == 0) {       /* key exists?              */
				Py_XDECREF(VALUE(q)); /* release old value object */
				VALUE(q) = value;     /* set new value object     */
				Py_INCREF(value);     /* take new value object    */
				return 0;
			}
			last = dir;
			dir = (cmp_res < 0);

			/* Move the helpers down */
			if (g != NULL)
				t = g;

			g = p;
			p = q;
			q = q->link[dir];
		}
		/* Update the root (it may be different) */
		root = head.link[1];
	}

	/* Make the root black for simplified logic */
	RED(root) = 0;
	(*rootaddr) = root;
	return 1;
}

extern int
rb_remove(node_t **rootaddr, PyObject *key)
{
	node_t *root = *rootaddr;

	node_t head = { { NULL } }; /* False tree root */
	node_t *q, *p, *g; /* Helpers */
	node_t *f = NULL; /* Found item */
	int dir = 1;

	if (root == NULL)
		return 0;

	/* Set up our helpers */
	q = &head;
	g = p = NULL;
	RIGHT_NODE(q) = root;

	/*
	 Search and push a red node down
	 to fix red violations as we go
	 */
	while (q->link[dir] != NULL) {
		int last = dir;
		int cmp_res;

		/* Move the helpers down */
		g = p, p = q;
		q = q->link[dir];

		cmp_res =  ct_compare(KEY(q), key);

		dir = cmp_res < 0;

		/*
		 Save the node with matching data and keep
		 going; we'll do removal tasks at the end
		 */
		if (cmp_res == 0)
			f = q;

		/* Push the red node down with rotations and color flips */
		if (!is_red(q) && !is_red(q->link[dir])) {
			if (is_red(q->link[!dir]))
				p = p->link[last] = rb_single(q, dir);
			else if (!is_red(q->link[!dir])) {
				node_t *s = p->link[!last];

				if (s != NULL) {
					if (!is_red(s->link[!last]) &&
						!is_red(s->link[last])) {
						/* Color flip */
						RED(p) = 0;
						RED(s) = 1;
						RED(q) = 1;
					}
					else {
						int dir2 = g->link[1] == p;

						if (is_red(s->link[last]))
							g->link[dir2] = rb_double(p, last);
						else if (is_red(s->link[!last]))
							g->link[dir2] = rb_single(p, last);

						/* Ensure correct coloring */
						RED(q) = RED(g->link[dir2]) = 1;
						RED(g->link[dir2]->link[0]) = 0;
						RED(g->link[dir2]->link[1]) = 0;
					}
				}
			}
		}
	}

	/* Replace and remove the saved node */
	if (f != NULL) {
		ct_swap_data(f, q);
		p->link[p->link[1] == q] = q->link[q->link[0] == NULL];
		ct_delete_node(q);
	}

	/* Update the root (it may be different) */
	root = head.link[1];

	/* Make the root black for simplified logic */
	if (root != NULL)
		RED(root) = 0;
	*rootaddr = root;
	return (f != NULL);
}

#define avl_new_node(key, value) ct_new_node(key, value, 0)
#define height(p) ((p) == NULL ? -1 : (p)->xdata)
#define avl_max(a, b) ((a) > (b) ? (a) : (b))

static node_t *
avl_single(node_t *root, int dir)
{
  node_t *save = root->link[!dir];
	int rlh, rrh, slh;

	/* Rotate */
	root->link[!dir] = save->link[dir];
	save->link[dir] = root;

	/* Update balance factors */
	rlh = height(root->link[0]);
	rrh = height(root->link[1]);
	slh = height(save->link[!dir]);

	BALANCE(root) = avl_max(rlh, rrh) + 1;
	BALANCE(save) = avl_max(slh, BALANCE(root)) + 1;

	return save;
}

static node_t *
avl_double(node_t *root, int dir)
{
	root->link[!dir] = avl_single(root->link[!dir], !dir);
	return avl_single(root, dir);
}

extern int
avl_insert(node_t **rootaddr, PyObject *key, PyObject *value)
{
	node_t *root = *rootaddr;

	if (root == NULL) {
		root = avl_new_node(key, value);
		if (root == NULL)
			return -1; // got no memory
	}
	else {
		node_t *it, *up[32];
		int upd[32], top = 0;
		int done = 0;
		int cmp_res;

		it = root;
		/* Search for an empty link, save the path */
		for (;;) {
			/* Push direction and node onto stack */
			cmp_res = ct_compare(KEY(it), key);
			if (cmp_res == 0) {
				Py_XDECREF(VALUE(it)); // release old value object
				VALUE(it) = value; // set new value object
				Py_INCREF(value); // take new value object
				return 0;
			}
			// upd[top] = it->data < data;
			upd[top] = (cmp_res < 0);
			up[top++] = it;

			if (it->link[upd[top - 1]] == NULL)
				break;
			it = it->link[upd[top - 1]];
		}

		/* Insert a new node at the bottom of the tree */
		it->link[upd[top - 1]] = avl_new_node(key, value);
		if (it->link[upd[top - 1]] == NULL)
			return -1; // got no memory

		/* Walk back up the search path */
		while (--top >= 0 && !done) {
			// int dir = (cmp_res < 0);
			int lh, rh, max;

			cmp_res = ct_compare(KEY(up[top]), key);

			lh = height(up[top]->link[upd[top]]);
			rh = height(up[top]->link[!upd[top]]);

			/* Terminate or rebalance as necessary */
			if (lh - rh == 0)
				done = 1;
			if (lh - rh >= 2) {
				node_t *a = up[top]->link[upd[top]]->link[upd[top]];
				node_t *b = up[top]->link[upd[top]]->link[!upd[top]];

				if (height( a ) >= height( b ))
					up[top] = avl_single(up[top], !upd[top]);
				else
					up[top] = avl_double(up[top], !upd[top]);

				/* Fix parent */
				if (top != 0)
					up[top - 1]->link[upd[top - 1]] = up[top];
				else
					root = up[0];
				done = 1;
			}
			/* Update balance factors */
			lh = height(up[top]->link[upd[top]]);
			rh = height(up[top]->link[!upd[top]]);
			max = avl_max(lh, rh);
			BALANCE(up[top]) = max + 1;
		}
	}
	(*rootaddr) = root;
	return 1;
}

extern int
avl_remove(node_t **rootaddr, PyObject *key)
{
	node_t *root = *rootaddr;
	int cmp_res;

	if (root != NULL) {
		node_t *it, *up[32];
		int upd[32], top = 0;

		it = root;
		for (;;) {
			/* Terminate if not found */
			if (it == NULL)
				return 0;
			cmp_res = ct_compare(KEY(it), key);
			if (cmp_res == 0)
				break;

			/* Push direction and node onto stack */
			upd[top] = (cmp_res < 0);
			up[top++] = it;
			it = it->link[upd[top - 1]];
		}

		/* Remove the node */
		if (it->link[0] == NULL ||
			it->link[1] == NULL) {
			/* Which child is not null? */
			int dir = it->link[0] == NULL;

			/* Fix parent */
			if (top != 0)
				up[top - 1]->link[upd[top - 1]] = it->link[dir];
			else
				root = it->link[dir];

			ct_delete_node(it);
		}
		else {
			/* Find the inorder successor */
			node_t *heir = it->link[1];

			/* Save the path */
			upd[top] = 1;
			up[top++] = it;

			while ( heir->link[0] != NULL ) {
				upd[top] = 0;
				up[top++] = heir;
				heir = heir->link[0];
			}
			/* Swap data */
			ct_swap_data(it, heir);
			/* Unlink successor and fix parent */
			up[top - 1]->link[up[top - 1] == it] = heir->link[1];
			ct_delete_node(heir);
		}

		/* Walk back up the search path */
		while (--top >= 0) {
			int lh = height(up[top]->link[upd[top]]);
			int rh = height(up[top]->link[!upd[top]]);
			int max = avl_max(lh, rh);

			/* Update balance factors */
			BALANCE(up[top]) = max + 1;

			/* Terminate or rebalance as necessary */
			if (lh - rh == -1)
				break;
			if (lh - rh <= -2) {
				node_t *a = up[top]->link[!upd[top]]->link[upd[top]];
				node_t *b = up[top]->link[!upd[top]]->link[!upd[top]];

				if (height(a) <= height(b))
					up[top] = avl_single(up[top], upd[top]);
				else
					up[top] = avl_double(up[top], upd[top]);

				/* Fix parent */
				if (top != 0)
					up[top - 1]->link[upd[top - 1]] = up[top];
				else
					root = up[0];
			}
		}
	}
	(*rootaddr) = root;
	return 1;
}

extern node_t *
ct_succ_node(node_t *root, PyObject *key)
{
	node_t *succ = NULL;
	node_t *node = root;
	int cval;

	while (node != NULL) {
		cval = ct_compare(key, KEY(node));
		if (cval == 0)
			break;
		else if (cval < 0) {
			if ((succ == NULL) ||
				(ct_compare(KEY(node), KEY(succ)) < 0))
				succ = node;
			node = LEFT_NODE(node);
		} else
			node = RIGHT_NODE(node);
	}
	if (node == NULL)
		return NULL;
	/* found node of key */
	if (RIGHT_NODE(node) != NULL) {
		/* find smallest node of right subtree */
		node = RIGHT_NODE(node);
		while (LEFT_NODE(node) != NULL)
			node = LEFT_NODE(node);
		if (succ == NULL)
			succ = node;
		else if (ct_compare(KEY(node), KEY(succ)) < 0)
			succ = node;
	}
	return succ;
}

extern node_t *
ct_prev_node(node_t *root, PyObject *key)
{
	node_t *prev = NULL;
	node_t *node = root;
	int cval;

	while (node != NULL) {
		cval = ct_compare(key, KEY(node));
		if (cval == 0)
			break;
		else if (cval < 0)
			node = LEFT_NODE(node);
		else {
			if ((prev == NULL) || (ct_compare(KEY(node), KEY(prev)) > 0))
				prev = node;
			node = RIGHT_NODE(node);
		}
	}
	if (node == NULL) /* stay at dead end (None) */
		return NULL;
	/* found node of key */
	if (LEFT_NODE(node) != NULL) {
		/* find biggest node of left subtree */
		node = LEFT_NODE(node);
		while (RIGHT_NODE(node) != NULL)
			node = RIGHT_NODE(node);
		if (prev == NULL)
			prev = node;
		else if (ct_compare(KEY(node), KEY(prev)) > 0)
			prev = node;
	}
	return prev;
}

extern node_t *
ct_floor_node(node_t *root, PyObject *key)
{
	node_t *prev = NULL;
	node_t *node = root;
	int cval;

	while (node != NULL) {
		cval = ct_compare(key, KEY(node));
		if (cval == 0)
			return node;
		else if (cval < 0)
			node = LEFT_NODE(node);
		else {
			if ((prev == NULL) || (ct_compare(KEY(node), KEY(prev)) > 0))
				prev = node;
			node = RIGHT_NODE(node);
		}
	}
	return prev;
}

extern node_t *
ct_ceiling_node(node_t *root, PyObject *key)
{
	node_t *succ = NULL;
	node_t *node = root;
	int cval;

	while (node != NULL) {
		cval = ct_compare(key, KEY(node));
		if (cval == 0)
			return node;
		else if (cval < 0) {
			if ((succ == NULL) ||
				(ct_compare(KEY(node), KEY(succ)) < 0))
				succ = node;
			node = LEFT_NODE(node);
		} else
			node = RIGHT_NODE(node);
	}
	return succ;
}

extern int
ct_index_of(node_t *root, PyObject *key)
/*
get index of item <key>, returns -1 if key not found.
*/
{
	node_t *node = root;
	int index = 0;
	int go_down = 1;
	node_stack_t *stack;
	stack = stack_init(32);

	for (;;) {
		if ((LEFT_NODE(node) != NULL) && go_down) {
			stack_push(stack, node);
			node = LEFT_NODE(node);
		}
		else {
			if (ct_compare(KEY(node), key) == 0) {
				stack_delete(stack);
				return index;
			}
			index++;
			if (RIGHT_NODE(node) != NULL) {
				node = RIGHT_NODE(node);
				go_down = 1;
			}
			else {
				if (stack_is_empty(stack)) {
					stack_delete(stack);
					return -1;
				}
				node = stack_pop(stack);
				go_down = 0;
			}
		}
	}
}

extern node_t *
ct_node_at(node_t *root, int index)
{
/*
root -- root node of tree
index -- index of wanted node

return NULL if index out of range
*/
	node_t *node = root;
	int counter = 0;
	int go_down = 1;
	node_stack_t *stack;

	if (index < 0) return NULL;

	stack = stack_init(32);

	for(;;) {
		if ((LEFT_NODE(node) != NULL) && go_down) {
			stack_push(stack, node);
			node = LEFT_NODE(node);
		}
		else {
			if (counter == index) {
				/* reached wanted index */
				stack_delete(stack);
				return node;
			}
			counter++;
			if (RIGHT_NODE(node) != NULL) {
				node = RIGHT_NODE(node);
				go_down = 1;
			}
			else {
				if (stack_is_empty(stack)) { /* index out of range */
					stack_delete(stack);
					return NULL;
                }
				node = stack_pop(stack);
				go_down = 0;
			}
		}
    }
}
