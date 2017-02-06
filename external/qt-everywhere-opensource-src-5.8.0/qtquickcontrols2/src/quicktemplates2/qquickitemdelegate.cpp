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

#include "qquickitemdelegate_p.h"
#include "qquickitemdelegate_p_p.h"
#include "qquickcontrol_p_p.h"

#include <QtGui/qpa/qplatformtheme.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype ItemDelegate
    \inherits AbstractButton
    \instantiates QQuickItemDelegate
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \brief Basic item delegate that can be used in various views and controls.

    \image qtquickcontrols2-itemdelegate.gif

    ItemDelegate presents a standard view item. It can be used as a delegate
    in various views and controls, such as \l ListView and \l ComboBox.

    ItemDelegate inherits its API from AbstractButton. For instance, you can set
    \l {AbstractButton::text}{text}, and react to \l {AbstractButton::clicked}{clicks}
    using the AbstractButton API.

    \snippet qtquickcontrols2-itemdelegate.qml 1

    \sa {Customizing ItemDelegate}, {Delegate Controls}
*/

QQuickItemDelegatePrivate::QQuickItemDelegatePrivate() :
    highlighted(false)
{
}

QQuickItemDelegate::QQuickItemDelegate(QQuickItem *parent) :
    QQuickAbstractButton(*(new QQuickItemDelegatePrivate), parent)
{
    setFocusPolicy(Qt::NoFocus);
}

QQuickItemDelegate::QQuickItemDelegate(QQuickItemDelegatePrivate &dd, QQuickItem *parent) :
    QQuickAbstractButton(dd, parent)
{
    setFocusPolicy(Qt::NoFocus);
}

/*!
    \qmlproperty bool QtQuick.Controls::ItemDelegate::highlighted

    This property holds whether the delegate is highlighted.

    A delegate can be highlighted in order to draw the user's attention towards
    it. It has no effect on keyboard interaction. For example, you can
    highlight the current item in a ListView using the following code:

    \code
    ListView {
        id: listView
        model: 10
        delegate: ItemDelegate {
            text: modelData
            highlighted: ListView.isCurrentItem
            onClicked: listView.currentIndex = index
        }
    }
    \endcode

    The default value is \c false.
*/
bool QQuickItemDelegate::isHighlighted() const
{
    Q_D(const QQuickItemDelegate);
    return d->highlighted;
}

void QQuickItemDelegate::setHighlighted(bool highlighted)
{
    Q_D(QQuickItemDelegate);
    if (highlighted == d->highlighted)
        return;

    d->highlighted = highlighted;
    emit highlightedChanged();
}

QFont QQuickItemDelegate::defaultFont() const
{
    return QQuickControlPrivate::themeFont(QPlatformTheme::ItemViewFont);
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickItemDelegate::accessibleRole() const
{
    return QAccessible::ListItem;
}
#endif

QT_END_NAMESPACE
