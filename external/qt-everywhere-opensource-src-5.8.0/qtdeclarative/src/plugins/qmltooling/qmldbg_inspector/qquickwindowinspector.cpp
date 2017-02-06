/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qquickwindowinspector.h"
#include "inspecttool.h"

#include <private/qquickitem_p.h>

QT_BEGIN_NAMESPACE

namespace QmlJSDebugger {

/*
 * Returns the first visible item at the given position, or 0 when no such
 * child exists.
 */
static QQuickItem *itemAt(QQuickItem *item, const QPointF &pos,
                          QQuickItem *overlay)
{
    if (item == overlay)
        return 0;

    if (!item->isVisible() || item->opacity() == 0.0)
        return 0;

    if (item->flags() & QQuickItem::ItemClipsChildrenToShape) {
        if (!QRectF(0, 0, item->width(), item->height()).contains(pos))
            return 0;
    }

    QList<QQuickItem *> children = QQuickItemPrivate::get(item)->paintOrderChildItems();
    for (int i = children.count() - 1; i >= 0; --i) {
        QQuickItem *child = children.at(i);
        if (QQuickItem *betterCandidate = itemAt(child, item->mapToItem(child, pos),
                                                 overlay))
            return betterCandidate;
    }

    if (!(item->flags() & QQuickItem::ItemHasContents))
        return 0;

    if (!QRectF(0, 0, item->width(), item->height()).contains(pos))
        return 0;

    return item;
}

/*
 * Collects all the items at the given position, from top to bottom.
 */
static void collectItemsAt(QQuickItem *item, const QPointF &pos,
                           QQuickItem *overlay, QList<QQuickItem *> &resultList)
{
    if (item == overlay)
        return;

    if (item->flags() & QQuickItem::ItemClipsChildrenToShape) {
        if (!QRectF(0, 0, item->width(), item->height()).contains(pos))
            return;
    }

    QList<QQuickItem *> children = QQuickItemPrivate::get(item)->paintOrderChildItems();
    for (int i = children.count() - 1; i >= 0; --i) {
        QQuickItem *child = children.at(i);
        collectItemsAt(child, item->mapToItem(child, pos), overlay, resultList);
    }

    if (!QRectF(0, 0, item->width(), item->height()).contains(pos))
        return;

    resultList.append(item);
}

QQuickWindowInspector::QQuickWindowInspector(QQuickWindow *quickWindow, QObject *parent) :
    QObject(parent),
    m_overlay(new QQuickItem),
    m_window(quickWindow),
    m_parentWindow(0),
    m_tool(0)
{
    setParentWindow(quickWindow);

    // Try to make sure the overlay is always on top
    m_overlay->setZ(FLT_MAX);

    if (QQuickItem *root = m_window->contentItem())
        m_overlay->setParentItem(root);

    m_window->installEventFilter(this);
}

bool QQuickWindowInspector::eventFilter(QObject *obj, QEvent *event)
{
    if (!m_tool || obj != m_window)
        return QObject::eventFilter(obj, event);

    switch (event->type()) {
    case QEvent::Enter:
        m_tool->enterEvent(event);
        return true;
    case QEvent::Leave:
        m_tool->leaveEvent(event);
        return true;
    case QEvent::MouseButtonPress:
        m_tool->mousePressEvent(static_cast<QMouseEvent*>(event));
        return true;
    case QEvent::MouseMove:
        m_tool->mouseMoveEvent(static_cast<QMouseEvent*>(event));
        return true;
    case QEvent::MouseButtonRelease:
        return true;
    case QEvent::KeyPress:
        m_tool->keyPressEvent(static_cast<QKeyEvent*>(event));
        return true;
    case QEvent::KeyRelease:
        return true;
    case QEvent::MouseButtonDblClick:
        m_tool->mouseDoubleClickEvent(static_cast<QMouseEvent*>(event));
        return true;
#ifndef QT_NO_WHEELEVENT
    case QEvent::Wheel:
        return true;
#endif
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
        m_tool->touchEvent(static_cast<QTouchEvent*>(event));
        return true;
    default:
        break;
    }

    return QObject::eventFilter(obj, event);
}

void QQuickWindowInspector::setShowAppOnTop(bool appOnTop)
{
    if (!m_parentWindow)
        return;

    Qt::WindowFlags flags = m_parentWindow->flags();
    Qt::WindowFlags newFlags = appOnTop ? (flags | Qt::WindowStaysOnTopHint) :
                                          (flags & ~Qt::WindowStaysOnTopHint);
    if (newFlags != flags)
        m_parentWindow->setFlags(newFlags);
}

bool QQuickWindowInspector::isEnabled() const
{
    return m_tool != 0;
}

void QQuickWindowInspector::setEnabled(bool enabled)
{
    if (enabled) {
        m_tool = new InspectTool(this, m_window);
    } else {
        delete m_tool;
        m_tool = 0;
    }
}

QQuickWindow *QQuickWindowInspector::quickWindow() const
{
    return m_window;
}

void QQuickWindowInspector::setParentWindow(QWindow *parentWindow)
{
    if (parentWindow) {
        while (QWindow *w = parentWindow->parent())
            parentWindow = w;
    }

    m_parentWindow = parentWindow;
}

QList<QQuickItem *> QQuickWindowInspector::itemsAt(const QPointF &pos) const
{
    QList<QQuickItem *> resultList;
    QQuickItem *root = m_window->contentItem();
    collectItemsAt(root, root->mapFromScene(pos), m_overlay,
                   resultList);
    return resultList;
}

QQuickItem *QQuickWindowInspector::topVisibleItemAt(const QPointF &pos) const
{
    QQuickItem *root = m_window->contentItem();
    return itemAt(root, root->mapFromScene(pos), m_overlay);
}


} // namespace QmlJSDebugger

QT_END_NAMESPACE
