/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsgnodeupdater_p.h"
#include "qsgnode.h"
#include "qsgrendernode_p.h"

QT_BEGIN_NAMESPACE

// #define QSG_UPDATER_DEBUG

QSGNodeUpdater::QSGNodeUpdater()
    : m_combined_matrix_stack(64)
    , m_opacity_stack(64)
    , m_current_clip(0)
    , m_force_update(0)
{
    m_opacity_stack.add(1);
}

QSGNodeUpdater::~QSGNodeUpdater()
{
}

void QSGNodeUpdater::updateStates(QSGNode *n)
{
    m_current_clip = 0;
    m_force_update = 0;

    Q_ASSERT(m_opacity_stack.size() == 1); // The one we added in the constructr...
    Q_ASSERT(m_combined_matrix_stack.isEmpty());

    visitNode(n);
}

/*!
    \fn void QSGNodeUpdater::setToplevelOpacity(qreal opacity)

    Sets the toplevel opacity that will be multiplied with the

    The default opacity is 1. Any other value will cause artifacts, and is
    primarily useful for debug purposes.

    The changing the value during an update pass will have undefined results
 */

/*!
    \fn qreal QSGNodeUpdater::toplevelOpacity() const

    Returns the toplevel opacity for the node updater. The default
    value is 1.
 */


/*!
    Returns true if \a node is has something that blocks it in the chain from
    \a node to \a root doing a full state update pass.

    This function does not process dirty states, simply does a simple traversion
    up to the top.

    The function assumes that \a root exists in the parent chain of \a node.
 */

bool QSGNodeUpdater::isNodeBlocked(QSGNode *node, QSGNode *root) const
{
    while (node != root && node != 0) {
        if (node->isSubtreeBlocked())
            return true;
        node = node->parent();
    }
    return false;
}


void QSGNodeUpdater::enterTransformNode(QSGTransformNode *t)
{
#ifdef QSG_UPDATER_DEBUG
    qDebug() << "enter transform:" << t << "force=" << m_force_update;
#endif

    if (!t->matrix().isIdentity()) {
        if (!m_combined_matrix_stack.isEmpty()) {
            t->setCombinedMatrix(*m_combined_matrix_stack.last() * t->matrix());
        } else {
            t->setCombinedMatrix(t->matrix());
        }
        m_combined_matrix_stack.add(&t->combinedMatrix());
    } else {
        if (!m_combined_matrix_stack.isEmpty()) {
            t->setCombinedMatrix(*m_combined_matrix_stack.last());
        } else {
            t->setCombinedMatrix(QMatrix4x4());
        }
    }
}


void QSGNodeUpdater::leaveTransformNode(QSGTransformNode *t)
{
#ifdef QSG_UPDATER_DEBUG
    qDebug() << "leave transform:" << t;
#endif

    if (!t->matrix().isIdentity()) {
        m_combined_matrix_stack.pop_back();
    }

}


void QSGNodeUpdater::enterClipNode(QSGClipNode *c)
{
#ifdef QSG_UPDATER_DEBUG
    qDebug() << "enter clip:" << c;
#endif

    c->m_matrix = m_combined_matrix_stack.isEmpty() ? 0 : m_combined_matrix_stack.last();
    c->m_clip_list = m_current_clip;
    m_current_clip = c;
}


void QSGNodeUpdater::leaveClipNode(QSGClipNode *c)
{
#ifdef QSG_UPDATER_DEBUG
    qDebug() << "leave clip:" << c;
#endif

    m_current_clip = c->m_clip_list;
}


void QSGNodeUpdater::enterGeometryNode(QSGGeometryNode *g)
{
#ifdef QSG_UPDATER_DEBUG
    qDebug() << "enter geometry:" << g;
#endif

    g->m_matrix = m_combined_matrix_stack.isEmpty() ? 0 : m_combined_matrix_stack.last();
    g->m_clip_list = m_current_clip;
    g->setInheritedOpacity(m_opacity_stack.last());
}

void QSGNodeUpdater::leaveGeometryNode(QSGGeometryNode *g)
{
#ifdef QSG_UPDATER_DEBUG
    qDebug() << "leave geometry" << g;
#else
    Q_UNUSED(g)
#endif
}

void QSGNodeUpdater::enterRenderNode(QSGRenderNode *r)
{
#ifdef QSG_UPDATER_DEBUG
    qDebug() << "enter render:" << r;
#endif

    r->m_matrix = m_combined_matrix_stack.isEmpty() ? 0 : m_combined_matrix_stack.last();
    r->m_clip_list = m_current_clip;
    r->setInheritedOpacity(m_opacity_stack.last());
}

void QSGNodeUpdater::leaveRenderNode(QSGRenderNode *r)
{
#ifdef QSG_UPDATER_DEBUG
    qDebug() << "leave render" << r;
#else
    Q_UNUSED(r)
#endif
}

void QSGNodeUpdater::enterOpacityNode(QSGOpacityNode *o)
{
    qreal opacity = m_opacity_stack.last() * o->opacity();
    o->setCombinedOpacity(opacity);
    m_opacity_stack.add(opacity);

#ifdef QSG_UPDATER_DEBUG
    qDebug() << "enter opacity" << o;
#endif
}

void QSGNodeUpdater::leaveOpacityNode(QSGOpacityNode *o)
{
#ifdef QSG_UPDATER_DEBUG
    qDebug() << "leave opacity" << o;
#endif
    if (o->flags() & QSGNode::DirtyOpacity)
        --m_force_update;

    m_opacity_stack.pop_back();
}

void QSGNodeUpdater::visitChildren(QSGNode *n)
{
    for (QSGNode *c = n->firstChild(); c; c = c->nextSibling())
        visitNode(c);
}

void QSGNodeUpdater::visitNode(QSGNode *n)
{
#ifdef QSG_UPDATER_DEBUG
    qDebug() << "enter:" << n;
#endif

    if (!m_force_update)
        return;
    if (n->isSubtreeBlocked())
        return;

    switch (n->type()) {
    case QSGNode::TransformNodeType: {
        QSGTransformNode *t = static_cast<QSGTransformNode *>(n);
        enterTransformNode(t);
        visitChildren(t);
        leaveTransformNode(t);
        break; }
    case QSGNode::GeometryNodeType: {
        QSGGeometryNode *g = static_cast<QSGGeometryNode *>(n);
        enterGeometryNode(g);
        visitChildren(g);
        leaveGeometryNode(g);
        break; }
    case QSGNode::RenderNodeType: {
        QSGRenderNode *r = static_cast<QSGRenderNode *>(n);
        enterRenderNode(r);
        visitChildren(r);
        leaveRenderNode(r);
        break; }
    case QSGNode::ClipNodeType: {
        QSGClipNode *c = static_cast<QSGClipNode *>(n);
        enterClipNode(c);
        visitChildren(c);
        leaveClipNode(c);
        break; }
    case QSGNode::OpacityNodeType: {
        QSGOpacityNode *o = static_cast<QSGOpacityNode *>(n);
        enterOpacityNode(o);
        visitChildren(o);
        leaveOpacityNode(o);
        break; }
    default:
        visitChildren(n);
        break;
    }
}

QT_END_NAMESPACE
