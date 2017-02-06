/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qaccessiblequickview_p.h"

#include <QtGui/qguiapplication.h>

#include <QtQuick/qquickitem.h>
#include <QtQuick/private/qquickitem_p.h>

#include "qaccessiblequickitem_p.h"

#ifndef QT_NO_ACCESSIBILITY

QT_BEGIN_NAMESPACE

QAccessibleQuickWindow::QAccessibleQuickWindow(QQuickWindow *object)
    :QAccessibleObject(object)
{
}

QList<QQuickItem *> QAccessibleQuickWindow::rootItems() const
{
    if (QQuickItem *ci = window()->contentItem())
        return accessibleUnignoredChildren(ci);
    return QList<QQuickItem *>();
}

int QAccessibleQuickWindow::childCount() const
{
    return rootItems().count();
}

QAccessibleInterface *QAccessibleQuickWindow::parent() const
{
    // FIXME: for now we assume to be a top level window...
    return QAccessible::queryAccessibleInterface(qApp);
}

QAccessibleInterface *QAccessibleQuickWindow::child(int index) const
{
    const QList<QQuickItem*> &kids = rootItems();
    if (index >= 0 && index < kids.count())
        return QAccessible::queryAccessibleInterface(kids.at(index));
    return 0;
}

QAccessible::Role QAccessibleQuickWindow::role() const
{
    return QAccessible::Window; // FIXME
}

QAccessible::State QAccessibleQuickWindow::state() const
{
    QAccessible::State st;
    if (window() == QGuiApplication::focusWindow())
        st.active = true;
    if (!window()->isVisible())
        st.invisible = true;
    return st;
}

QRect QAccessibleQuickWindow::rect() const
{
    return QRect(window()->x(), window()->y(), window()->width(), window()->height());
}

QString QAccessibleQuickWindow::text(QAccessible::Text text) const
{
#ifdef Q_ACCESSIBLE_QUICK_ITEM_ENABLE_DEBUG_DESCRIPTION
    if (text == QAccessible::DebugDescription) {
        return QString::fromLatin1(object()->metaObject()->className()) ;
    }
#else
    Q_UNUSED(text)
#endif
    return window()->title();
}

QAccessibleInterface *QAccessibleQuickWindow::childAt(int x, int y) const
{
    Q_ASSERT(window());
    for (int i = childCount() - 1; i >= 0; --i) {
        QAccessibleInterface *childIface = child(i);
        if (childIface && !childIface->state().invisible) {
            if (QAccessibleInterface *iface = childIface->childAt(x, y))
                return iface;
            if (childIface->rect().contains(x, y))
                return childIface;
        }
    }
    return 0;
}

int QAccessibleQuickWindow::indexOfChild(const QAccessibleInterface *iface) const
{
    int i = -1;
    if (iface) {
        const QList<QQuickItem *> &roots = rootItems();
        i = roots.count() - 1;
        while (i >= 0) {
            if (iface->object() == roots.at(i))
                break;
            --i;
        }
    }
    return i;
}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
