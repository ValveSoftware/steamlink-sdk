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

#include "qquickbutton_p.h"
#include "qquickbutton_p_p.h"

#include <QtGui/qpa/qplatformtheme.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Button
    \inherits AbstractButton
    \instantiates QQuickButton
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-buttons
    \brief Push-button that can be clicked to perform a command or answer a question.

    \image qtquickcontrols2-button.gif

    Button presents a push-button control that can be pushed or clicked by
    the user. Buttons are normally used to perform an action, or to answer
    a question. Typical buttons are \e OK, \e Apply, \e Cancel, \e Close,
    \e Yes, \e No, and \e Help.

    A button emits the signal \l {AbstractButton::}{clicked()} when it is activated by the user.
    Connect to this signal to perform the button's action. Buttons also
    provide the signals \l {AbstractButton::}{canceled()}, \l {AbstractButton::}{doubleClicked()}, \l {AbstractButton::}{pressed()},
    \l {AbstractButton::}{released()} and \l {AbstractButton::}{pressAndHold()} for long presses.

    See the snippet below on how to connect to the button's signals.

    \code
    RowLayout {
        Button {
            text: "Ok"
            onClicked: model.submit()
        }
        Button {
            text: "Cancel"
            onClicked: model.revert()
        }
    }
    \endcode

    \sa {Customizing Button}, {Button Controls}
*/

QQuickButtonPrivate::QQuickButtonPrivate() :
    flat(false), highlighted(false)
{
}

QQuickButton::QQuickButton(QQuickItem *parent) :
    QQuickAbstractButton(*(new QQuickButtonPrivate), parent)
{
}

QQuickButton::QQuickButton(QQuickButtonPrivate &dd, QQuickItem *parent) :
    QQuickAbstractButton(dd, parent)
{
}

/*!
    \qmlproperty bool QtQuick.Controls::Button::autoRepeat

    This property holds whether the button repeats
    \l {AbstractButton::}{pressed()}, \l {AbstractButton::}{released()}
    and \l {AbstractButton::}{clicked()} signals while the button is pressed
    and held down.

    The default value is \c false.
*/

void QQuickButton::autoRepeatChange()
{
    emit autoRepeatChanged();
}

QFont QQuickButton::defaultFont() const
{
    return QQuickControlPrivate::themeFont(QPlatformTheme::PushButtonFont);
}

/*!
    \qmlproperty bool QtQuick.Controls::Button::highlighted

    This property holds whether the button is highlighted.

    \image qtquickcontrols2-button-highlighted.gif

    A button can be highlighted in order to draw the user's attention towards
    it. It has no effect on keyboard interaction.

    The default value is \c false.
*/
bool QQuickButton::isHighlighted() const
{
    Q_D(const QQuickButton);
    return d->highlighted;
}

void QQuickButton::setHighlighted(bool highlighted)
{
    Q_D(QQuickButton);
    if (highlighted == d->highlighted)
        return;

    d->highlighted = highlighted;
    emit highlightedChanged();
}

/*!
    \qmlproperty bool QtQuick.Controls::Button::flat

    This property holds whether the button is flat.

    \image qtquickcontrols2-button-flat.gif

    A flat button typically does not draw a background unless it is pressed or checked.

    The default value is \c false.
*/
bool QQuickButton::isFlat() const
{
    Q_D(const QQuickButton);
    return d->flat;
}

void QQuickButton::setFlat(bool flat)
{
    Q_D(QQuickButton);
    if (flat == d->flat)
        return;

    d->flat = flat;
    emit flatChanged();
}

QT_END_NAMESPACE
