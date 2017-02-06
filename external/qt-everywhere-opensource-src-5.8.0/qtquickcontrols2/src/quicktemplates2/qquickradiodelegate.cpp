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

#include "qquickradiodelegate_p.h"
#include "qquickabstractbutton_p_p.h"

#include <QtGui/qpa/qplatformtheme.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype RadioDelegate
    \inherits ItemDelegate
    \instantiates QQuickRadioDelegate
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-delegates
    \brief Exclusive item delegate with a radio indicator that can be toggled on or off.

    \image qtquickcontrols2-radiodelegate.gif

    RadioDelegate presents an item delegate that can be toggled on (checked) or
    off (unchecked). Radio delegates are typically used to select one option
    from a set of options.

    RadioDelegate inherits its API from \l ItemDelegate, which is inherited
    from AbstractButton. For instance, you can set \l {AbstractButton::text}{text},
    and react to \l {AbstractButton::clicked}{clicks} using the AbstractButton
    API. The state of the radio delegate can be set with the
    \l {AbstractButton::}{checked} property.

    Radio delegates are \l {AbstractButton::autoExclusive}{auto-exclusive}
    by default. Only one delegate can be checked at any time amongst radio
    delegates that belong to the same parent item; checking another delegate
    automatically unchecks the previously checked one. For radio delegates
    that do not share a common parent, ButtonGroup can be used to manage
    exclusivity.

    \l RadioButton is similar to RadioDelegate, except that it is typically
    not used in views, but rather when there are only a few options, and often
    with the requirement that each button is uniquely identifiable.

    \code
    ButtonGroup {
        id: buttonGroup
    }

    ListView {
        model: ["Option 1", "Option 2", "Option 3"]
        delegate: RadioDelegate {
            text: modelData
            checked: index == 0
            ButtonGroup.group: buttonGroup
        }
    }
    \endcode

    \sa {Customizing RadioDelegate}, {Delegate Controls}, RadioButton
*/

QQuickRadioDelegate::QQuickRadioDelegate(QQuickItem *parent) :
    QQuickItemDelegate(parent)
{
    setCheckable(true);
    setAutoExclusive(true);
}

QFont QQuickRadioDelegate::defaultFont() const
{
    return QQuickControlPrivate::themeFont(QPlatformTheme::ListViewFont);
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickRadioDelegate::accessibleRole() const
{
    return QAccessible::RadioButton;
}
#endif

QT_END_NAMESPACE
