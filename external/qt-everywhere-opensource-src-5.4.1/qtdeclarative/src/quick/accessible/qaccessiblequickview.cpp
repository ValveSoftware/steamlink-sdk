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

#include "qaccessiblequickview_p.h"

#include <QtGui/qguiapplication.h>

#include <QtQuick/qquickitem.h>
#include <QtQuick/private/qquickitem_p.h>

#include "qaccessiblequickitem_p.h"
#include "qqmlaccessible_p.h"

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
