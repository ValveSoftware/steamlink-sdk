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

#include "qquickbusyindicator_p.h"
#include "qquickcontrol_p_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype BusyIndicator
    \inherits Control
    \instantiates QQuickBusyIndicator
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-indicators
    \brief Indicates background activity, for example, while content is being loaded.

    \image qtquickcontrols2-busyindicator.gif

    The busy indicator should be used to indicate activity while content is
    being loaded or the UI is blocked waiting for a resource to become available.

    The following snippet shows how to use the BusyIndicator:

    \qml
    BusyIndicator {
        running: image.status === Image.Loading
    }
    \endqml

    BusyIndicator is similar to an indeterminate \l ProgressBar. Both can be
    used to indicate background activity. The main difference is visual, and
    that ProgressBar can also present a concrete amount of progress (when it
    can be determined). Due to the visual difference, busy indicators and
    indeterminate progress bars fit different places in user interfaces.
    Typical places for a busy indicator:
    \list
    \li in the corner of a \l ToolBar
    \li as an overlay on top of a \l Page
    \li on the side of an \l ItemDelegate
    \endlist

    \sa {Customizing BusyIndicator}, {Indicator Controls}, ProgressBar
*/

class QQuickBusyIndicatorPrivate : public QQuickControlPrivate
{
public:
    QQuickBusyIndicatorPrivate() : running(true) { }

    bool running;
};

QQuickBusyIndicator::QQuickBusyIndicator(QQuickItem *parent) :
    QQuickControl(*(new QQuickBusyIndicatorPrivate), parent)
{
}

/*!
    \qmlproperty bool QtQuick.Controls::BusyIndicator::running

    This property holds whether the busy indicator is currently indicating
    activity.

    \note The indicator is only visible when this property is set to \c true.

    The default value is \c true.
*/
bool QQuickBusyIndicator::isRunning() const
{
    Q_D(const QQuickBusyIndicator);
    return d->running;
}

void QQuickBusyIndicator::setRunning(bool running)
{
    Q_D(QQuickBusyIndicator);
    if (d->running == running)
        return;

    d->running = running;
    emit runningChanged();
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickBusyIndicator::accessibleRole() const
{
    return QAccessible::Indicator;
}
#endif

QT_END_NAMESPACE
