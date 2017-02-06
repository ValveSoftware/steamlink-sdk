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

#include "qquickswitch_p.h"
#include "qquickabstractbutton_p_p.h"

#include <QtGui/qstylehints.h>
#include <QtGui/qguiapplication.h>
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQuick/private/qquickevents_p_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Switch
    \inherits AbstractButton
    \instantiates QQuickSwitch
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-buttons
    \brief Switch button that can be toggled on or off.

    \image qtquickcontrols2-switch.gif

    Switch is an option button that can be dragged or toggled on (checked) or
    off (unchecked). Switches are typically used to select between two states.
    For larger sets of options, such as those in a list, consider using
    \l SwitchDelegate instead.

    Switch inherits its API from \l AbstractButton. For instance, the state
    of the switch can be set with the \l {AbstractButton::}{checked} property.

    \code
    ColumnLayout {
        Switch {
            text: qsTr("Wi-Fi")
        }
        Switch {
            text: qsTr("Bluetooth")
        }
    }
    \endcode

    \sa {Customizing Switch}, {Button Controls}
*/

class QQuickSwitchPrivate : public QQuickAbstractButtonPrivate
{
    Q_DECLARE_PUBLIC(QQuickSwitch)

public:
    QQuickSwitchPrivate() : position(0) { }

    qreal positionAt(const QPointF &point) const;

    qreal position;
};

qreal QQuickSwitchPrivate::positionAt(const QPointF &point) const
{
    Q_Q(const QQuickSwitch);
    qreal pos = 0.0;
    if (indicator)
        pos = indicator->mapFromItem(q, point).x() / indicator->width();
    if (q->isMirrored())
        return 1.0 - pos;
    return pos;
}

QQuickSwitch::QQuickSwitch(QQuickItem *parent) :
    QQuickAbstractButton(*(new QQuickSwitchPrivate), parent)
{
    Q_D(QQuickSwitch);
    d->keepPressed = true;
    setCheckable(true);
}

/*!
    \qmlproperty real QtQuick.Controls::Switch::position
    \readonly

    \input includes/qquickswitch.qdocinc position
*/
qreal QQuickSwitch::position() const
{
    Q_D(const QQuickSwitch);
    return d->position;
}

void QQuickSwitch::setPosition(qreal position)
{
    Q_D(QQuickSwitch);
    position = qBound<qreal>(0.0, position, 1.0);
    if (qFuzzyCompare(d->position, position))
        return;

    d->position = position;
    emit positionChanged();
    emit visualPositionChanged();
}

/*!
    \qmlproperty real QtQuick.Controls::Switch::visualPosition
    \readonly

    \input includes/qquickswitch.qdocinc visualPosition
*/
qreal QQuickSwitch::visualPosition() const
{
    Q_D(const QQuickSwitch);
    if (isMirrored())
        return 1.0 - d->position;
    return d->position;
}

void QQuickSwitch::mousePressEvent(QMouseEvent *event)
{
    QQuickAbstractButton::mousePressEvent(event);
}

void QQuickSwitch::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickSwitch);
    QQuickAbstractButton::mouseMoveEvent(event);

    const QPointF movePoint = event->localPos();
    if (!keepMouseGrab()) {
        // don't start dragging the handle unless the initial press was at the indicator,
        // or the drag has reached the indicator area. this prevents unnatural jumps when
        // dragging far outside the indicator.
        const qreal pressPos = d->positionAt(d->pressPoint);
        const qreal movePos = d->positionAt(movePoint);
        if ((pressPos >= 0.0 && pressPos <= 1.0) || (movePos >= 0.0 && movePos <= 1.0))
            setKeepMouseGrab(QQuickWindowPrivate::dragOverThreshold(movePoint.x() - d->pressPoint.x(), Qt::XAxis, event));
    }

    if (keepMouseGrab())
        setPosition(d->positionAt(movePoint));
}

void QQuickSwitch::mouseReleaseEvent(QMouseEvent *event)
{
    QQuickAbstractButton::mouseReleaseEvent(event);
    setKeepMouseGrab(false);
}

void QQuickSwitch::mirrorChange()
{
    QQuickAbstractButton::mirrorChange();
    emit visualPositionChanged();
}

void QQuickSwitch::nextCheckState()
{
    Q_D(QQuickSwitch);
    if (keepMouseGrab()) {
        setChecked(d->position > 0.5);
        // the checked state might not change => force a position update to
        // avoid that the handle is left somewhere in the middle (QTBUG-57944)
        checkStateSet();
    } else {
        QQuickAbstractButton::nextCheckState();
    }
}

void QQuickSwitch::checkStateSet()
{
    Q_D(QQuickSwitch);
    setPosition(d->checked ? 1.0 : 0.0);
}

QT_END_NAMESPACE
