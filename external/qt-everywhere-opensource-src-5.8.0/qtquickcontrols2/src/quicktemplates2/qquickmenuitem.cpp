/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickmenuitem_p.h"
#include "qquickabstractbutton_p_p.h"

#include <QtGui/qpa/qplatformtheme.h>
#include <QtQuick/private/qquickevents_p_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype MenuItem
    \inherits AbstractButton
    \instantiates QQuickMenuItem
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-menus
    \brief Presents an item within a Menu.

    MenuItem is a convenience type that implements the AbstractButton API,
    providing a familiar way to respond to menu items being \l triggered, for
    example.

    \code
    Button {
        id: fileButton
        text: "File"
        onClicked: menu.open()

        Menu {
            id: menu

            MenuItem {
                text: "New..."
                onTriggered: document.reset()
            }
            MenuItem {
                text: "Open..."
                onTriggered: openDialog.open()
            }
            MenuItem {
                text: "Save"
                onTriggered: saveDialog.open()
            }
        }
    }
    \endcode

    \sa {Customizing MenuItem}, {Menu Controls}
*/

class QQuickMenuItemPrivate : public QQuickAbstractButtonPrivate
{
    Q_DECLARE_PUBLIC(QQuickMenuItem)

public:
    QQuickMenuItemPrivate();

    bool highlighted;
};

QQuickMenuItemPrivate::QQuickMenuItemPrivate() :
    highlighted(false)
{
}

/*!
    \qmlsignal void QtQuick.Controls::MenuItem::triggered()

    This signal is emitted when the menu item is triggered by the user.
*/

QQuickMenuItem::QQuickMenuItem(QQuickItem *parent) :
    QQuickAbstractButton(*(new QQuickMenuItemPrivate), parent)
{
    connect(this, &QQuickAbstractButton::clicked, this, &QQuickMenuItem::triggered);
}

QFont QQuickMenuItem::defaultFont() const
{
    return QQuickControlPrivate::themeFont(QPlatformTheme::MenuItemFont);
}

/*!
    \qmlproperty bool QtQuick.Controls::MenuItem::highlighted

    This property holds whether the menu item is highlighted.

    A menu item can be highlighted in order to draw the user's attention
    towards it. It has no effect on keyboard interaction.

    The default value is \c false.
*/
bool QQuickMenuItem::isHighlighted() const
{
    Q_D(const QQuickMenuItem);
    return d->highlighted;
}

void QQuickMenuItem::setHighlighted(bool highlighted)
{
    Q_D(QQuickMenuItem);
    if (highlighted == d->highlighted)
        return;

    d->highlighted = highlighted;
    emit highlightedChanged();
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickMenuItem::accessibleRole() const
{
    return QAccessible::MenuItem;
}
#endif

QT_END_NAMESPACE
