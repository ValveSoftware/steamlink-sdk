/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

    bool canDrag(const QPointF &movePoint) const;
    void handleMove(const QPointF &point) override;
    void handleRelease(const QPointF &point) override;

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

bool QQuickSwitchPrivate::canDrag(const QPointF &movePoint) const
{
    // don't start dragging the handle unless the initial press was at the indicator,
    // or the drag has reached the indicator area. this prevents unnatural jumps when
    // dragging far outside the indicator.
    const qreal pressPos = positionAt(pressPoint);
    const qreal movePos = positionAt(movePoint);
    return (pressPos >= 0.0 && pressPos <= 1.0) || (movePos >= 0.0 && movePos <= 1.0);
}

void QQuickSwitchPrivate::handleMove(const QPointF &point)
{
    Q_Q(QQuickSwitch);
    QQuickAbstractButtonPrivate::handleMove(point);
    if (q->keepMouseGrab() || q->keepTouchGrab())
        q->setPosition(positionAt(point));
}

void QQuickSwitchPrivate::handleRelease(const QPointF &point)
{
    Q_Q(QQuickSwitch);
    QQuickAbstractButtonPrivate::handleRelease(point);
    q->setKeepMouseGrab(false);
    q->setKeepTouchGrab(false);
}

QQuickSwitch::QQuickSwitch(QQuickItem *parent)
    : QQuickAbstractButton(*(new QQuickSwitchPrivate), parent)
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

void QQuickSwitch::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickSwitch);
    if (!keepMouseGrab()) {
        const QPointF movePoint = event->localPos();
        if (d->canDrag(movePoint))
            setKeepMouseGrab(QQuickWindowPrivate::dragOverThreshold(movePoint.x() - d->pressPoint.x(), Qt::XAxis, event));
    }
    QQuickAbstractButton::mouseMoveEvent(event);
}

#if QT_CONFIG(quicktemplates2_multitouch)
void QQuickSwitch::touchEvent(QTouchEvent *event)
{
    Q_D(QQuickSwitch);
    if (!keepTouchGrab() && event->type() == QEvent::TouchUpdate) {
        for (const QTouchEvent::TouchPoint &point : event->touchPoints()) {
            if (point.id() != d->touchId || point.state() != Qt::TouchPointMoved)
                continue;
            if (d->canDrag(point.pos()))
                setKeepTouchGrab(QQuickWindowPrivate::dragOverThreshold(point.pos().x() - d->pressPoint.x(), Qt::XAxis, &point));
        }
    }
    QQuickAbstractButton::touchEvent(event);
}
#endif

void QQuickSwitch::mirrorChange()
{
    QQuickAbstractButton::mirrorChange();
    emit visualPositionChanged();
}

void QQuickSwitch::nextCheckState()
{
    Q_D(QQuickSwitch);
    if (keepMouseGrab() || keepTouchGrab()) {
        d->toggle(d->position > 0.5);
        // the checked state might not change => force a position update to
        // avoid that the handle is left somewhere in the middle (QTBUG-57944)
        setPosition(d->checked ? 1.0 : 0.0);
    } else {
        QQuickAbstractButton::nextCheckState();
    }
}

void QQuickSwitch::buttonChange(ButtonChange change)
{
    Q_D(QQuickSwitch);
    if (change == ButtonCheckedChange)
        setPosition(d->checked ? 1.0 : 0.0);
    else
        QQuickAbstractButton::buttonChange(change);
}

QT_END_NAMESPACE
