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

#include "qquicktabbutton_p.h"
#include "qquickcontrol_p_p.h"

#include <QtGui/qpa/qplatformtheme.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype TabButton
    \inherits AbstractButton
    \instantiates QQuickTabButton
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-navigation
    \brief Button with a look suitable for a TabBar.

    \image qtquickcontrols2-tabbutton.png

    TabButton is used in conjunction with a \l TabBar.

    \snippet qtquickcontrols2-tabbutton.qml 1

    TabButton inherits its API from AbstractButton. For instance, you can set
    \l {AbstractButton::text}{text}, and react to \l {AbstractButton::clicked}{clicks}
    using the AbstractButton API.

    \sa TabBar, {Customizing TabButton}, {Button Controls}, {Navigation Controls}
*/

QQuickTabButton::QQuickTabButton(QQuickItem *parent) :
    QQuickAbstractButton(parent)
{
    setCheckable(true);
    setAutoExclusive(true);
}

QFont QQuickTabButton::defaultFont() const
{
    return QQuickControlPrivate::themeFont(QPlatformTheme::TabButtonFont);
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickTabButton::accessibleRole() const
{
    return QAccessible::PageTab;
}
#endif

QT_END_NAMESPACE
