/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qv4sparsearray_p.h"
#include "qv4runtime_p.h"
#include "qv4object_p.h"
#include "qv4functionobject_p.h"
#include "qv4scopedvalue_p.h"
#include <stdlib.h>

#ifdef QT_QMAP_DEBUG
# include <qstring.h>
# include <qvector.h>
#endif

using namespace QV4;

const SparseArrayNode *SparseArrayNode::nextNode() const
{
    const SparseArrayNode *n = this;
    if (n->right) {
        n = n->right;
        while (n->left)
            n = n->left;
    } else {
        const SparseArrayNode *y = n->parent();
        while (y && n == y->right) {
            n = y;
            y = n->parent();
        }
        n = y;
    }
    return n;
}

const SparseArrayNode *SparseArrayNode::previousNode() const
{
    const SparseArrayNode *n = this;
    if (n->left) {
        n = n->left;
        while (n->right)
            n = n->right;
    } else {
        const SparseArrayNode *y = n->parent();
        while (y && n == y->left) {
            n = y;
            y = n->parent();
        }
        n = y;
    }
    return n;
}

SparseArrayNode *SparseArrayNode::copy(SparseArray *d) const
{
    SparseArrayNode *n = d->createNode(size_left, 0, false);
    n->value = value;
    n->setColor(color());
    if (left) {
        n->left = left->copy(d);
        n->left->setParent(n);
    } else {
        n->left = 0;
    }
    if (right) {
        n->right = right->copy(d);
        n->right->setParent(n);
    } else {
        n->right = 0;
    }
    return n;
}

/*
     x              y
      \            / \
       y    -->   x   b
      / \          \
     a   b          a
*/
void SparseArray::rotateLeft(SparseArrayNode *x)
{
    SparseArrayNode *&root = header.left;
    SparseArrayNode *y = x->right;
    x->right = y->left;
    if (y->left != 0)
        y->left->setParent(x);
    y->setParent(x->parent());
    if (x == root)
        root = y;
    else if (x == x->parent()->left)
        x->parent()->left = y;
    else
        x->parent()->right = y;
    y->left = x;
    x->setParent(y);
    y->size_left += x->size_left;
}


/*
         x          y
        /          / \
       y    -->   a   x
      / \            /
     a   b          b
*/
void SparseArray::rotateRight(SparseArrayNode *x)
{
    SparseArrayNode *&root = header.left;
    SparseArrayNode *y = x->left;
    x->left = y->right;
    if (y->right != 0)
        y->right->setParent(x);
    y->setParent(x->parent());
    if (x == root)
        root = y;
    else if (x == x->parent()->right)
        x->parent()->right = y;
    else
        x->parent()->left = y;
    y->right = x;
    x->setParent(y);
    x->size_left -= y->size_left;
}


void SparseArray::rebalance(SparseArrayNode *x)
{
    SparseArrayNode *&root = header.left;
    x->setColor(SparseArrayNode::Red);
    while (x != root && x->parent()->color() == SparseArrayNode::Red) {
        if (x->parent() == x->parent()->parent()->left) {
            SparseArrayNode *y = x->parent()->parent()->right;
            if (y && y->color() == SparseArrayNode::Red) {
                x->parent()->setColor(SparseArrayNode::Black);
                y->setColor(SparseArrayNode::Black);
                x->parent()->parent()->setColor(SparseArrayNode::Red);
                x = x->parent()->parent();
            } else {
                if (x == x->parent()->right) {
                    x = x->parent();
                    rotateLeft(x);
                }
                x->parent()->setColor(SparseArrayNode::Black);
                x->parent()->parent()->setColor(SparseArrayNode::Red);
                rotateRight (x->parent()->parent());
            }
        } else {
            SparseArrayNode *y = x->parent()->parent()->left;
            if (y && y->color() == SparseArrayNode::Red) {
                x->parent()->setColor(SparseArrayNode::Black);
                y->setColor(SparseArrayNode::Black);
                x->parent()->parent()->setColor(SparseArrayNode::Red);
                x = x->parent()->parent();
            } else {
                if (x == x->parent()->left) {
                    x = x->parent();
                    rotateRight(x);
                }
                x->parent()->setColor(SparseArrayNode::Black);
                x->parent()->parent()->setColor(SparseArrayNode::Red);
                rotateLeft(x->parent()->parent());
            }
        }
    }
    root->setColor(SparseArrayNode::Black);
}

void SparseArray::deleteNode(SparseArrayNode *z)
{
    SparseArrayNode *&root = header.left;
    SparseArrayNode *y = z;
    SparseArrayNode *x;
    SparseArrayNode *x_parent;
    if (y->left == 0) {
        x = y->right;
        if (y == mostLeftNode) {
            if (x)
                mostLeftNode = x; // It cannot have (left) children due the red black invariant.
            else
                mostLeftNode = y->parent();
        }
    } else if (y->right == 0) {
            x = y->left;
    } else {
        y = y->right;
        while (y->left != 0)
            y = y->left;
        x = y->right;
    }
    if (y != z) {
        // move y into the position of z
        // adjust size_left so the keys are ok.
        z->size_left += y->size_left;
        SparseArrayNode *n = y->parent();
        while (n != z) {
            n->size_left -= y->size_left;
            n = n->parent();
        }
        y->size_left = 0;
        z->value = y->value;

        if (y != z->right) {
            x_parent = y->parent();
            y->parent()->left = x;
        } else {
            x_parent = z;
            z->right = x;
        }
        if (x)
            x->setParent(x_parent);
    } else {
        x_parent = y->parent();
        if (x)
            x->setParent(y->parent());
        if (root == y)
            root = x;
        else if (y->parent()->left == y)
            y->parent()->left = x;
        else
            y->parent()->right = x;
        if (x && x == y->right)
            x->size_left += y->size_left;
        y->size_left = 0;
    }
    if (y->color() != SparseArrayNode::Red) {
        while (x != root && (x == 0 || x->color() == SparseArrayNode::Black)) {
            if (x == x_parent->left) {
                SparseArrayNode *w = x_parent->right;
                if (w->color() == SparseArrayNode::Red) {
                    w->setColor(SparseArrayNode::Black);
                    x_parent->setColor(SparseArrayNode::Red);
                    rotateLeft(x_parent);
                    w = x_parent->right;
                }
                if ((w->left == 0 || w->left->color() == SparseArrayNode::Black) &&
                    (w->right == 0 || w->right->color() == SparseArrayNode::Black)) {
                    w->setColor(SparseArrayNode::Red);
                    x = x_parent;
                    x_parent = x_parent->parent();
                } else {
                    if (w->right == 0 || w->right->color() == SparseArrayNode::Black) {
                        if (w->left)
                            w->left->setColor(SparseArrayNode::Black);
                        w->setColor(SparseArrayNode::Red);
                        rotateRight(w);
                        w = x_parent->right;
                    }
                    w->setColor(x_parent->color());
                    x_parent->setColor(SparseArrayNode::Black);
                    if (w->right)
                        w->right->setColor(SparseArrayNode::Black);
                    rotateLeft(x_parent);
                    break;
                }
            } else {
            SparseArrayNode *w = x_parent->left;
            if (w->color() == SparseArrayNode::Red) {
                w->setColor(SparseArrayNode::Black);
                x_parent->setColor(SparseArrayNode::Red);
                rotateRight(x_parent);
                w = x_parent->left;
            }
            if ((w->right == 0 || w->right->color() == SparseArrayNode::Black) &&
                (w->left == 0 || w->left->color() == SparseArrayNode::Black)) {
                w->setColor(SparseArrayNode::Red);
                x = x_parent;
                x_parent = x_parent->parent();
            } else {
                if (w->left == 0 || w->left->color() == SparseArrayNode::Black) {
                    if (w->right)
                        w->right->setColor(SparseArrayNode::Black);
                    w->setColor(SparseArrayNode::Red);
                    rotateLeft(w);
                    w = x_parent->left;
                }
                w->setColor(x_parent->color());
                x_parent->setColor(SparseArrayNode::Black);
                if (w->left)
                    w->left->setColor(SparseArrayNode::Black);
                rotateRight(x_parent);
                break;
            }
        }
    }
    if (x)
        x->setColor(SparseArrayNode::Black);
    }
    free(y);
    --numEntries;
}

void SparseArray::recalcMostLeftNode()
{
    mostLeftNode = &header;
    while (mostLeftNode->left)
        mostLeftNode = mostLeftNode->left;
}

static inline int qMapAlignmentThreshold()
{
    // malloc on 32-bit platforms should return pointers that are 8-byte
    // aligned or more while on 64-bit platforms they should be 16-byte aligned
    // or more
    return 2 * sizeof(void*);
}

static inline void *qMapAllocate(int alloc, int alignment)
{
    return alignment > qMapAlignmentThreshold()
        ? qMallocAligned(alloc, alignment)
        : ::malloc(alloc);
}

static inline void qMapDeallocate(SparseArrayNode *node, int alignment)
{
    if (alignment > qMapAlignmentThreshold())
        qFreeAligned(node);
    else
        ::free(node);
}

SparseArrayNode *SparseArray::createNode(uint sl, SparseArrayNode *parent, bool left)
{
    SparseArrayNode *node = static_cast<SparseArrayNode *>(qMapAllocate(sizeof(SparseArrayNode), Q_ALIGNOF(SparseArrayNode)));
    Q_CHECK_PTR(node);

    node->p = (quintptr)parent;
    node->left = 0;
    node->right = 0;
    node->size_left = sl;
    node->value = UINT_MAX;
    ++numEntries;

    if (parent) {
        if (left) {
            parent->left = node;
            if (parent == mostLeftNode)
                mostLeftNode = node;
        } else {
            parent->right = node;
        }
        node->setParent(parent);
        rebalance(node);
    }
    return node;
}

void SparseArray::freeTree(SparseArrayNode *root, int alignment)
{
    if (root->left)
        freeTree(root->left, alignment);
    if (root->right)
        freeTree(root->right, alignment);
    qMapDeallocate(root, alignment);
}

SparseArray::SparseArray()
    : numEntries(0)
{
    header.p = 0;
    header.left = 0;
    header.right = 0;
    mostLeftNode = &header;
}

SparseArray::SparseArray(const SparseArray &other)
{
    header.p = 0;
    header.right = 0;
    if (other.header.left) {
        header.left = other.header.left->copy(this);
        header.left->setParent(&header);
        recalcMostLeftNode();
    }
}

SparseArrayNode *SparseArray::insert(uint akey)
{
    SparseArrayNode *n = root();
    SparseArrayNode *y = end();
    bool  left = true;
    uint s = akey;
    while (n) {
        y = n;
        if (s == n->size_left) {
            return n;
        } else if (s < n->size_left) {
            left = true;
            n = n->left;
        } else {
            left = false;
            s -= n->size_left;
            n = n->right;
        }
    }

    return createNode(s, y, left);
}
