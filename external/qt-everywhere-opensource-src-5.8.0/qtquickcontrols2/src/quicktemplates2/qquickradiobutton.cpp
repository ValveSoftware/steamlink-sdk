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

#include "qquickradiobutton_p.h"
#include "qquickcontrol_p_p.h"

#include <QtGui/qpa/qplatformtheme.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype RadioButton
    \inherits AbstractButton
    \instantiates QQuickRadioButton
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-buttons
    \brief Exclusive radio button that can be toggled on or off.

    \image qtquickcontrols2-radiobutton.gif

    RadioButton presents an option button that can be toggled on (checked) or
    off (unchecked). Radio buttons are typically used to select one option
    from a set of options.

    RadioButton inherits its API from \l AbstractButton. For instance,
    you can set \l {AbstractButton::text}{text} and react to
    \l {AbstractButton::clicked}{clicks} using the AbstractButton API.
    The state of the radio button can be set with the
    \l {AbstractButton::}{checked} property.

    Radio buttons are \l {AbstractButton::autoExclusive}{auto-exclusive}
    by default. Only one button can be checked at any time amongst radio
    buttons that belong to the same parent item; checking another button
    automatically unchecks the previously checked one. For radio buttons
    that do not share a common parent, ButtonGroup can be used to manage
    exclusivity.

    \l RadioDelegate is similar to RadioButton, except that it is typically
    used in views.

    \code
    ColumnLayout {
        RadioButton {
            checked: true
            text: qsTr("First")
        }
        RadioButton {
            text: qsTr("Second")
        }
        RadioButton {
            text: qsTr("Third")
        }
    }
    \endcode

    \sa ButtonGroup, {Customizing RadioButton}, {Button Controls}, RadioDelegate
*/

QQuickRadioButton::QQuickRadioButton(QQuickItem *parent) :
    QQuickAbstractButton(parent)
{
    setCheckable(true);
    setAutoExclusive(true);
}

QFont QQuickRadioButton::defaultFont() const
{
    return QQuickControlPrivate::themeFont(QPlatformTheme::RadioButtonFont);
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickRadioButton::accessibleRole() const
{
    return QAccessible::RadioButton;
}
#endif

QT_END_NAMESPACE
