/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include "qquickmenubar_p.h"

#include <private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QtGui/qpa/qplatformmenu.h>
#include <qquickwindow.h>

QT_BEGIN_NAMESPACE


/*!
  \class QQuickMenuBar1
  \internal
 */

/*!
  \qmltype MenuBarPrivate
  \instantiates QQuickMenuBar1
  \internal
  \inqmlmodule QtQuick.Controls
 */

/*!
    \qmlproperty list<Menu> MenuBar::menus
    \default

    The list of menus in the menubar.

    \sa Menu
*/

QQuickMenuBar1::QQuickMenuBar1(QObject *parent)
    : QObject(parent), m_platformMenuBar(0), m_contentItem(0), m_parentWindow(0)
{
}

QQuickMenuBar1::~QQuickMenuBar1()
{
    if (isNative())
        setNativeNoNotify(false);
}

QQmlListProperty<QQuickMenu1> QQuickMenuBar1::menus()
{
    return QQmlListProperty<QQuickMenu1>(this, 0, &QQuickMenuBar1::append_menu, &QQuickMenuBar1::count_menu, &QQuickMenuBar1::at_menu, 0);
}

bool QQuickMenuBar1::isNative() const
{
    return m_platformMenuBar != 0;
}

void QQuickMenuBar1::setNative(bool native)
{
    bool wasNative = isNative();
    setNativeNoNotify(native);
    if (isNative() != wasNative)
        emit nativeChanged();
}

void QQuickMenuBar1::setNativeNoNotify(bool native)
{
    // QTBUG-51372
    if (QGuiApplication::platformName() == QStringLiteral("xcb"))
        return;

    if (native) {
        if (!m_platformMenuBar) {
            m_platformMenuBar = QGuiApplicationPrivate::platformTheme()->createPlatformMenuBar();
            if (m_platformMenuBar) {
                m_platformMenuBar->handleReparent(m_parentWindow);
                for (QQuickMenu1 *menu : qAsConst(m_menus))
                    m_platformMenuBar->insertMenu(menu->platformMenu(), 0 /* append */);
            }
        }
    } else {
        if (m_platformMenuBar) {
            for (QQuickMenu1 *menu : qAsConst(m_menus))
                m_platformMenuBar->removeMenu(menu->platformMenu());
        }
        delete m_platformMenuBar;
        m_platformMenuBar = 0;
    }
}

void QQuickMenuBar1::setContentItem(QQuickItem *item)
{
    if (item != m_contentItem) {
        m_contentItem = item;
        emit contentItemChanged();
    }
}

void QQuickMenuBar1::setParentWindow(QQuickWindow *newParentWindow)
{
    if (newParentWindow != m_parentWindow) {
        m_parentWindow = newParentWindow;
        if (m_platformMenuBar)
            m_platformMenuBar->handleReparent(m_parentWindow);
    }
}

void QQuickMenuBar1::append_menu(QQmlListProperty<QQuickMenu1> *list, QQuickMenu1 *menu)
{
    if (QQuickMenuBar1 *menuBar = qobject_cast<QQuickMenuBar1 *>(list->object)) {
        menu->setParent(menuBar);
        menuBar->m_menus.append(menu);

        if (menuBar->m_platformMenuBar)
            menuBar->m_platformMenuBar->insertMenu(menu->platformMenu(), 0 /* append */);

        emit menuBar->menusChanged();
    }
}

int QQuickMenuBar1::count_menu(QQmlListProperty<QQuickMenu1> *list)
{
    if (QQuickMenuBar1 *menuBar = qobject_cast<QQuickMenuBar1 *>(list->object))
        return menuBar->m_menus.size();
    return 0;
}

QQuickMenu1 *QQuickMenuBar1::at_menu(QQmlListProperty<QQuickMenu1> *list, int index)
{
    QQuickMenuBar1 *menuBar = qobject_cast<QQuickMenuBar1 *>(list->object);
    if (menuBar &&  0 <= index && index < menuBar->m_menus.size())
        return menuBar->m_menus[index];
    return 0;
}

QT_END_NAMESPACE
