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

#include "qquickrangeslider_p.h"
#include "qquickcontrol_p_p.h"

#include <QtCore/qscopedpointer.h>
#include <QtQuick/private/qquickwindow_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype RangeSlider
    \inherits Control
    \instantiates QQuickRangeSlider
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-input
    \brief Used to select a range of values by sliding two handles along a track.

    \image qtquickcontrols2-rangeslider.gif

    RangeSlider is used to select a range specified by two values, by sliding
    each handle along a track.

    In the example below, custom \l from and \l to values are set, and the
    initial positions of the \l first and \l second handles are set:

    \code
    RangeSlider {
        from: 1
        to: 100
        first.value: 25
        second.value: 75
    }
    \endcode

    The \l {first.position} and \l {second.position} properties are expressed as
    fractions of the control's size, in the range \c {0.0 - 1.0}.
    The \l {first.visualPosition} and \l {second.visualPosition} properties are
    the same, except that they are reversed in a
    \l {Right-to-left User Interfaces}{right-to-left} application.
    The \c visualPosition is useful for positioning the handles when styling
    RangeSlider. In the example above, \l {first.visualPosition} will be \c 0.24
    in a left-to-right application, and \c 0.76 in a right-to-left application.

    \sa {Customizing RangeSlider}, {Input Controls}
*/

class QQuickRangeSliderNodePrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQuickRangeSliderNode)
public:
    QQuickRangeSliderNodePrivate(qreal value, QQuickRangeSlider *slider) :
        value(value),
        isPendingValue(false),
        pendingValue(0),
        position(0),
        handle(nullptr),
        slider(slider),
        pressed(false),
        hovered(false)
    {
    }

    bool isFirst() const;

    void setPosition(qreal position, bool ignoreOtherPosition = false);
    void updatePosition(bool ignoreOtherPosition = false);

    static QQuickRangeSliderNodePrivate *get(QQuickRangeSliderNode *node);

private:
    friend class QQuickRangeSlider;

    qreal value;
    bool isPendingValue;
    qreal pendingValue;
    qreal position;
    QQuickItem *handle;
    QQuickRangeSlider *slider;
    bool pressed;
    bool hovered;
};

bool QQuickRangeSliderNodePrivate::isFirst() const
{
    return this == get(slider->first());
}

void QQuickRangeSliderNodePrivate::setPosition(qreal position, bool ignoreOtherPosition)
{
    Q_Q(QQuickRangeSliderNode);

    const qreal min = isFirst() || ignoreOtherPosition ? 0.0 : qMax<qreal>(0.0, slider->first()->position());
    const qreal max = !isFirst() || ignoreOtherPosition ? 1.0 : qMin<qreal>(1.0, slider->second()->position());
    position = qBound(min, position, max);
    if (!qFuzzyCompare(this->position, position)) {
        this->position = position;
        emit q->positionChanged();
        emit q->visualPositionChanged();
    }
}

void QQuickRangeSliderNodePrivate::updatePosition(bool ignoreOtherPosition)
{
    qreal pos = 0;
    if (!qFuzzyCompare(slider->from(), slider->to()))
        pos = (value - slider->from()) / (slider->to() - slider->from());
    setPosition(pos, ignoreOtherPosition);
}

QQuickRangeSliderNodePrivate *QQuickRangeSliderNodePrivate::get(QQuickRangeSliderNode *node)
{
    return node->d_func();
}

QQuickRangeSliderNode::QQuickRangeSliderNode(qreal value, QQuickRangeSlider *slider) :
    QObject(*(new QQuickRangeSliderNodePrivate(value, slider)), slider)
{
}

QQuickRangeSliderNode::~QQuickRangeSliderNode()
{
}

qreal QQuickRangeSliderNode::value() const
{
    Q_D(const QQuickRangeSliderNode);
    return d->value;
}

void QQuickRangeSliderNode::setValue(qreal value)
{
    Q_D(QQuickRangeSliderNode);
    if (!d->slider->isComponentComplete()) {
        d->pendingValue = value;
        d->isPendingValue = true;
        return;
    }

    // First, restrict the first value to be within to and from.
    const qreal smaller = qMin(d->slider->to(), d->slider->from());
    const qreal larger = qMax(d->slider->to(), d->slider->from());
    value = qBound(smaller, value, larger);

    // Then, ensure that it doesn't go past the other value,
    // a check that depends on whether or not the range is inverted.
    const bool invertedRange = d->slider->from() > d->slider->to();
    if (d->isFirst()) {
        if (invertedRange) {
            if (value < d->slider->second()->value())
                value = d->slider->second()->value();
        } else {
            if (value > d->slider->second()->value())
                value = d->slider->second()->value();
        }
    } else {
        if (invertedRange) {
            if (value > d->slider->first()->value())
                value = d->slider->first()->value();
        } else {
            if (value < d->slider->first()->value())
                value = d->slider->first()->value();
        }
    }

    if (!qFuzzyCompare(d->value, value)) {
        d->value = value;
        d->updatePosition();
        emit valueChanged();
    }
}

qreal QQuickRangeSliderNode::position() const
{
    Q_D(const QQuickRangeSliderNode);
    return d->position;
}

qreal QQuickRangeSliderNode::visualPosition() const
{
    Q_D(const QQuickRangeSliderNode);
    if (d->slider->orientation() == Qt::Vertical || d->slider->isMirrored())
        return 1.0 - d->position;
    return d->position;
}

QQuickItem *QQuickRangeSliderNode::handle() const
{
    Q_D(const QQuickRangeSliderNode);
    return d->handle;
}

void QQuickRangeSliderNode::setHandle(QQuickItem *handle)
{
    Q_D(QQuickRangeSliderNode);
    if (d->handle == handle)
        return;

    QQuickControlPrivate::get(d->slider)->deleteDelegate(d->handle);
    d->handle = handle;
    if (handle) {
        if (!handle->parentItem())
            handle->setParentItem(d->slider);

        QQuickItem *firstHandle = d->slider->first()->handle();
        QQuickItem *secondHandle = d->slider->second()->handle();
        if (firstHandle && secondHandle) {
            // The order of property assignments in QML is undefined,
            // but we need the first handle to be before the second due
            // to focus order constraints, so check for that here.
            const QList<QQuickItem *> childItems = d->slider->childItems();
            const int firstIndex = childItems.indexOf(firstHandle);
            const int secondIndex = childItems.indexOf(secondHandle);
            if (firstIndex != -1 && secondIndex != -1 && firstIndex > secondIndex) {
                firstHandle->stackBefore(secondHandle);
                // Ensure we have some way of knowing which handle is above
                // the other when it comes to mouse presses, and also that
                // they are rendered in the correct order.
                secondHandle->setZ(secondHandle->z() + 1);
            }
        }

        handle->setActiveFocusOnTab(true);
    }
    emit handleChanged();
}

bool QQuickRangeSliderNode::isPressed() const
{
    Q_D(const QQuickRangeSliderNode);
    return d->pressed;
}

void QQuickRangeSliderNode::setPressed(bool pressed)
{
    Q_D(QQuickRangeSliderNode);
    if (d->pressed == pressed)
        return;

    d->pressed = pressed;
    d->slider->setAccessibleProperty("pressed", pressed || d->slider->second()->isPressed());
    emit pressedChanged();
}

bool QQuickRangeSliderNode::isHovered() const
{
    Q_D(const QQuickRangeSliderNode);
    return d->hovered;
}

void QQuickRangeSliderNode::setHovered(bool hovered)
{
    Q_D(QQuickRangeSliderNode);
    if (d->hovered == hovered)
        return;

    d->hovered = hovered;
    emit hoveredChanged();
}

void QQuickRangeSliderNode::increase()
{
    Q_D(QQuickRangeSliderNode);
    qreal step = qFuzzyIsNull(d->slider->stepSize()) ? 0.1 : d->slider->stepSize();
    setValue(d->value + step);
}

void QQuickRangeSliderNode::decrease()
{
    Q_D(QQuickRangeSliderNode);
    qreal step = qFuzzyIsNull(d->slider->stepSize()) ? 0.1 : d->slider->stepSize();
    setValue(d->value - step);
}

static const qreal defaultFrom = 0.0;
static const qreal defaultTo = 1.0;

class QQuickRangeSliderPrivate : public QQuickControlPrivate
{
    Q_DECLARE_PUBLIC(QQuickRangeSlider)

public:
    QQuickRangeSliderPrivate() :
        from(defaultFrom),
        to(defaultTo),
        stepSize(0),
        first(nullptr),
        second(nullptr),
        orientation(Qt::Horizontal),
        snapMode(QQuickRangeSlider::NoSnap)
    {
    }

    void updateHover(const QPointF &pos);

    qreal from;
    qreal to;
    qreal stepSize;
    QQuickRangeSliderNode *first;
    QQuickRangeSliderNode *second;
    QPoint pressPoint;
    Qt::Orientation orientation;
    QQuickRangeSlider::SnapMode snapMode;
};

void QQuickRangeSliderPrivate::updateHover(const QPointF &pos)
{
    Q_Q(QQuickRangeSlider);
    QQuickItem *firstHandle = first->handle();
    QQuickItem *secondHandle = second->handle();
    first->setHovered(firstHandle && firstHandle->isEnabled() && firstHandle->contains(q->mapToItem(firstHandle, pos)));
    second->setHovered(secondHandle && secondHandle->isEnabled() && secondHandle->contains(q->mapToItem(secondHandle, pos)));
}

static qreal valueAt(const QQuickRangeSlider *slider, qreal position)
{
    return slider->from() + (slider->to() - slider->from()) * position;
}

static qreal snapPosition(const QQuickRangeSlider *slider, qreal position)
{
    const qreal range = slider->to() - slider->from();
    if (qFuzzyIsNull(range))
        return position;

    const qreal effectiveStep = slider->stepSize() / range;
    if (qFuzzyIsNull(effectiveStep))
        return position;

    return qRound(position / effectiveStep) * effectiveStep;
}

static qreal positionAt(const QQuickRangeSlider *slider, QQuickItem *handle, const QPoint &point)
{
    if (slider->orientation() == Qt::Horizontal) {
        const qreal hw = handle ? handle->width() : 0;
        const qreal offset = hw / 2;
        const qreal extent = slider->availableWidth() - hw;
        if (!qFuzzyIsNull(extent)) {
            if (slider->isMirrored())
                return (slider->width() - point.x() - slider->rightPadding() - offset) / extent;
            return (point.x() - slider->leftPadding() - offset) / extent;
        }
    } else {
        const qreal hh = handle ? handle->height() : 0;
        const qreal offset = hh / 2;
        const qreal extent = slider->availableHeight() - hh;
        if (!qFuzzyIsNull(extent))
            return (slider->height() - point.y() - slider->bottomPadding() - offset) / extent;
    }
    return 0;
}

QQuickRangeSlider::QQuickRangeSlider(QQuickItem *parent) :
    QQuickControl(*(new QQuickRangeSliderPrivate), parent)
{
    Q_D(QQuickRangeSlider);
    d->first = new QQuickRangeSliderNode(0.0, this);
    d->second = new QQuickRangeSliderNode(1.0, this);

    setAcceptedMouseButtons(Qt::LeftButton);
    setFlag(QQuickItem::ItemIsFocusScope);
}

/*!
    \qmlproperty real QtQuick.Controls::RangeSlider::from

    This property holds the starting value for the range. The default value is \c 0.0.

    \sa to, first.value, second.value
*/
qreal QQuickRangeSlider::from() const
{
    Q_D(const QQuickRangeSlider);
    return d->from;
}

void QQuickRangeSlider::setFrom(qreal from)
{
    Q_D(QQuickRangeSlider);
    if (qFuzzyCompare(d->from, from))
        return;

    d->from = from;
    emit fromChanged();

    if (isComponentComplete()) {
        d->first->setValue(d->first->value());
        d->second->setValue(d->second->value());
    }
}

/*!
    \qmlproperty real QtQuick.Controls::RangeSlider::to

    This property holds the end value for the range. The default value is \c 1.0.

    \sa from, first.value, second.value
*/
qreal QQuickRangeSlider::to() const
{
    Q_D(const QQuickRangeSlider);
    return d->to;
}

void QQuickRangeSlider::setTo(qreal to)
{
    Q_D(QQuickRangeSlider);
    if (qFuzzyCompare(d->to, to))
        return;

    d->to = to;
    emit toChanged();

    if (isComponentComplete()) {
        d->first->setValue(d->first->value());
        d->second->setValue(d->second->value());
    }
}

/*!
    \qmlpropertygroup QtQuick.Controls::RangeSlider::first
    \qmlproperty real QtQuick.Controls::RangeSlider::first.value
    \qmlproperty real QtQuick.Controls::RangeSlider::first.position
    \qmlproperty real QtQuick.Controls::RangeSlider::first.visualPosition
    \qmlproperty Item QtQuick.Controls::RangeSlider::first.handle
    \qmlproperty bool QtQuick.Controls::RangeSlider::first.pressed
    \qmlproperty bool QtQuick.Controls::RangeSlider::first.hovered

    \table
    \header
        \li Property
        \li Description
    \row
        \li value
        \li This property holds the value of the first handle in the range
            \c from - \c to.

            If \l to is greater than \l from, the value of the first handle
            must be greater than the second, and vice versa.

            Unlike \l {first.position}{position}, value is not updated while the
            handle is dragged, but rather when it has been released.

            The default value is \c 0.0.
    \row
        \li handle
        \li This property holds the first handle item.
    \row
        \li visualPosition
        \li This property holds the visual position of the first handle.

            The position is expressed as a fraction of the control's size, in the range
            \c {0.0 - 1.0}. When the control is \l {Control::mirrored}{mirrored}, the
            value is equal to \c {1.0 - position}. This makes the value suitable for
            visualizing the slider, taking right-to-left support into account.
    \row
        \li position
        \li This property holds the logical position of the first handle.

            The position is expressed as a fraction of the control's size, in the range
            \c {0.0 - 1.0}. Unlike \l {first.value}{value}, position is
            continuously updated while the handle is dragged. For visualizing a
            slider, the right-to-left aware
            \l {first.visualPosition}{visualPosition} should be used instead.
    \row
        \li pressed
        \li This property holds whether the first handle is pressed.
    \row
        \li hovered
        \li This property holds whether the first handle is hovered.
            This property was introduced in QtQuick.Controls 2.1.
    \endtable

    \sa first.increase(), first.decrease()
*/
QQuickRangeSliderNode *QQuickRangeSlider::first() const
{
    Q_D(const QQuickRangeSlider);
    return d->first;
}

/*!
    \qmlpropertygroup QtQuick.Controls::RangeSlider::second
    \qmlproperty real QtQuick.Controls::RangeSlider::second.value
    \qmlproperty real QtQuick.Controls::RangeSlider::second.position
    \qmlproperty real QtQuick.Controls::RangeSlider::second.visualPosition
    \qmlproperty Item QtQuick.Controls::RangeSlider::second.handle
    \qmlproperty bool QtQuick.Controls::RangeSlider::second.pressed
    \qmlproperty bool QtQuick.Controls::RangeSlider::second.hovered

    \table
    \header
        \li Property
        \li Description
    \row
        \li value
        \li This property holds the value of the second handle in the range
            \c from - \c to.

            If \l to is greater than \l from, the value of the first handle
            must be greater than the second, and vice versa.

            Unlike \l {second.position}{position}, value is not updated while the
            handle is dragged, but rather when it has been released.

            The default value is \c 0.0.
    \row
        \li handle
        \li This property holds the second handle item.
    \row
        \li visualPosition
        \li This property holds the visual position of the second handle.

            The position is expressed as a fraction of the control's size, in the range
            \c {0.0 - 1.0}. When the control is \l {Control::mirrored}{mirrored}, the
            value is equal to \c {1.0 - position}. This makes the value suitable for
            visualizing the slider, taking right-to-left support into account.
    \row
        \li position
        \li This property holds the logical position of the second handle.

            The position is expressed as a fraction of the control's size, in the range
            \c {0.0 - 1.0}. Unlike \l {second.value}{value}, position is
            continuously updated while the handle is dragged. For visualizing a
            slider, the right-to-left aware
            \l {second.visualPosition}{visualPosition} should be used instead.
    \row
        \li pressed
        \li This property holds whether the second handle is pressed.
    \row
        \li hovered
        \li This property holds whether the second handle is hovered.
            This property was introduced in QtQuick.Controls 2.1.
    \endtable

    \sa second.increase(), second.decrease()
*/
QQuickRangeSliderNode *QQuickRangeSlider::second() const
{
    Q_D(const QQuickRangeSlider);
    return d->second;
}

/*!
    \qmlproperty real QtQuick.Controls::RangeSlider::stepSize

    This property holds the step size. The default value is \c 0.0.

    \sa snapMode, first.increase(), first.decrease()
*/
qreal QQuickRangeSlider::stepSize() const
{
    Q_D(const QQuickRangeSlider);
    return d->stepSize;
}

void QQuickRangeSlider::setStepSize(qreal step)
{
    Q_D(QQuickRangeSlider);
    if (qFuzzyCompare(d->stepSize, step))
        return;

    d->stepSize = step;
    emit stepSizeChanged();
}

/*!
    \qmlproperty enumeration QtQuick.Controls::RangeSlider::snapMode

    This property holds the snap mode.

    Possible values:
    \value RangeSlider.NoSnap The slider does not snap (default).
    \value RangeSlider.SnapAlways The slider snaps while the handle is dragged.
    \value RangeSlider.SnapOnRelease The slider does not snap while being dragged, but only after the handle is released.

    For visual explanations of the various modes, see the
    \l {Slider::}{snapMode} documentation of \l Slider.

    \sa stepSize
*/
QQuickRangeSlider::SnapMode QQuickRangeSlider::snapMode() const
{
    Q_D(const QQuickRangeSlider);
    return d->snapMode;
}

void QQuickRangeSlider::setSnapMode(SnapMode mode)
{
    Q_D(QQuickRangeSlider);
    if (d->snapMode == mode)
        return;

    d->snapMode = mode;
    emit snapModeChanged();
}

/*!
    \qmlproperty enumeration QtQuick.Controls::RangeSlider::orientation

    This property holds the orientation.

    Possible values:
    \value Qt.Horizontal Horizontal (default)
    \value Qt.Vertical Vertical
*/
Qt::Orientation QQuickRangeSlider::orientation() const
{
    Q_D(const QQuickRangeSlider);
    return d->orientation;
}

void QQuickRangeSlider::setOrientation(Qt::Orientation orientation)
{
    Q_D(QQuickRangeSlider);
    if (d->orientation == orientation)
        return;

    d->orientation = orientation;
    emit orientationChanged();
}

/*!
    \qmlmethod void QtQuick.Controls::RangeSlider::setValues(real firstValue, real secondValue)

    Sets \l first.value and \l second.value with the given arguments.

    If \a to is larger than \a from and \a firstValue is larger than
    \a secondValue, \a firstValue will be clamped to \a secondValue.

    If \a from is larger than \a to and \a secondValue is larger than
    \a firstValue, \a secondValue will be clamped to \a firstValue.

    This function may be necessary to set the first and second values
    after the control has been completed, as there is a circular
    dependency between firstValue and secondValue which can cause
    assigned values to be clamped to each other.

    \sa stepSize
*/
void QQuickRangeSlider::setValues(qreal firstValue, qreal secondValue)
{
    Q_D(QQuickRangeSlider);
    // Restrict the values to be within to and from.
    const qreal smaller = qMin(d->to, d->from);
    const qreal larger = qMax(d->to, d->from);
    firstValue = qBound(smaller, firstValue, larger);
    secondValue = qBound(smaller, secondValue, larger);

    if (d->from > d->to) {
        // If the from and to values are reversed, the secondValue
        // might be less than the first value, which is not allowed.
        if (secondValue > firstValue)
            secondValue = firstValue;
    } else {
        // Otherwise, clamp first to second if it's too large.
        if (firstValue > secondValue)
            firstValue = secondValue;
    }

    // Then set both values. If they didn't change, no change signal will be emitted.
    QQuickRangeSliderNodePrivate *firstPrivate = QQuickRangeSliderNodePrivate::get(d->first);
    if (firstValue != firstPrivate->value) {
        firstPrivate->value = firstValue;
        emit d->first->valueChanged();
    }

    QQuickRangeSliderNodePrivate *secondPrivate = QQuickRangeSliderNodePrivate::get(d->second);
    if (secondValue != secondPrivate->value) {
        secondPrivate->value = secondValue;
        emit d->second->valueChanged();
    }

    // After we've set both values, then we can update the positions.
    // If we don't do this last, the positions may be incorrect.
    firstPrivate->updatePosition(true);
    secondPrivate->updatePosition();
}

void QQuickRangeSlider::focusInEvent(QFocusEvent *event)
{
    Q_D(QQuickRangeSlider);
    QQuickControl::focusInEvent(event);

    // The active focus ends up to RangeSlider when using forceActiveFocus()
    // or QML KeyNavigation. We must forward the focus to one of the handles,
    // because RangeSlider handles key events for the focused handle. If
    // neither handle has active focus, RangeSlider doesn't do anything.
    QQuickItem *handle = nextItemInFocusChain();
    // QQuickItem::nextItemInFocusChain() only works as desired with
    // Qt::TabFocusAllControls. otherwise pick the first handle
    if (!handle || handle == this)
        handle = d->first->handle();
    if (handle)
        handle->forceActiveFocus(event->reason());
}

void QQuickRangeSlider::keyPressEvent(QKeyEvent *event)
{
    Q_D(QQuickRangeSlider);
    QQuickControl::keyPressEvent(event);

    QQuickRangeSliderNode *focusNode = d->first->handle()->hasActiveFocus()
        ? d->first : (d->second->handle()->hasActiveFocus() ? d->second : nullptr);
    if (!focusNode)
        return;

    if (d->orientation == Qt::Horizontal) {
        if (event->key() == Qt::Key_Left) {
            focusNode->setPressed(true);
            if (isMirrored())
                focusNode->increase();
            else
                focusNode->decrease();
            event->accept();
        } else if (event->key() == Qt::Key_Right) {
            focusNode->setPressed(true);
            if (isMirrored())
                focusNode->decrease();
            else
                focusNode->increase();
            event->accept();
        }
    } else {
        if (event->key() == Qt::Key_Up) {
            focusNode->setPressed(true);
            focusNode->increase();
            event->accept();
        } else if (event->key() == Qt::Key_Down) {
            focusNode->setPressed(true);
            focusNode->decrease();
            event->accept();
        }
    }
}

void QQuickRangeSlider::hoverEnterEvent(QHoverEvent *event)
{
    Q_D(QQuickRangeSlider);
    QQuickControl::hoverEnterEvent(event);
    d->updateHover(event->posF());
}

void QQuickRangeSlider::hoverMoveEvent(QHoverEvent *event)
{
    Q_D(QQuickRangeSlider);
    QQuickControl::hoverMoveEvent(event);
    d->updateHover(event->posF());
}

void QQuickRangeSlider::hoverLeaveEvent(QHoverEvent *event)
{
    Q_D(QQuickRangeSlider);
    QQuickControl::hoverLeaveEvent(event);
    d->first->setHovered(false);
    d->second->setHovered(false);
}

void QQuickRangeSlider::keyReleaseEvent(QKeyEvent *event)
{
    Q_D(QQuickRangeSlider);
    QQuickControl::keyReleaseEvent(event);
    d->first->setPressed(false);
    d->second->setPressed(false);
}

void QQuickRangeSlider::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickRangeSlider);
    QQuickControl::mousePressEvent(event);
    d->pressPoint = event->pos();

    QQuickItem *firstHandle = d->first->handle();
    QQuickItem *secondHandle = d->second->handle();
    const bool firstHit = firstHandle && firstHandle->contains(mapToItem(firstHandle, d->pressPoint));
    const bool secondHit = secondHandle && secondHandle->contains(mapToItem(secondHandle, d->pressPoint));
    QQuickRangeSliderNode *hitNode = nullptr;
    QQuickRangeSliderNode *otherNode = nullptr;

    if (firstHit && secondHit) {
        // choose highest
        hitNode = firstHandle->z() > secondHandle->z() ? d->first : d->second;
        otherNode = firstHandle->z() > secondHandle->z() ? d->second : d->first;
    } else if (firstHit) {
        hitNode = d->first;
        otherNode = d->second;
    } else if (secondHit) {
        hitNode = d->second;
        otherNode = d->first;
    } else {
        // find the nearest
        const qreal firstDistance = QLineF(firstHandle->boundingRect().center(),
                                           mapToItem(firstHandle, event->pos())).length();
        const qreal secondDistance = QLineF(secondHandle->boundingRect().center(),
                                            mapToItem(secondHandle, event->pos())).length();

        if (qFuzzyCompare(firstDistance, secondDistance)) {
            // same distance => choose the one that can be moved towards the press position
            const bool inverted = d->from > d->to;
            const qreal pos = positionAt(this, firstHandle, event->pos());
            if ((!inverted && pos < d->first->position()) || (inverted && pos > d->first->position())) {
                hitNode = d->first;
                otherNode = d->second;
            } else {
                hitNode = d->second;
                otherNode = d->first;
            }
        } else if (firstDistance < secondDistance) {
            hitNode = d->first;
            otherNode = d->second;
        } else {
            hitNode = d->second;
            otherNode = d->first;
        }
    }

    if (hitNode) {
        hitNode->setPressed(true);
        hitNode->handle()->setZ(1);
    }
    if (otherNode)
        otherNode->handle()->setZ(0);
}

void QQuickRangeSlider::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickRangeSlider);
    QQuickControl::mouseMoveEvent(event);
    if (!keepMouseGrab()) {
        if (d->orientation == Qt::Horizontal)
            setKeepMouseGrab(QQuickWindowPrivate::dragOverThreshold(event->pos().x() - d->pressPoint.x(), Qt::XAxis, event));
        else
            setKeepMouseGrab(QQuickWindowPrivate::dragOverThreshold(event->pos().y() - d->pressPoint.y(), Qt::YAxis, event));
    }
    if (keepMouseGrab()) {
        QQuickRangeSliderNode *pressedNode = d->first->isPressed() ? d->first : (d->second->isPressed() ? d->second : nullptr);
        if (pressedNode) {
            qreal pos = positionAt(this, pressedNode->handle(), event->pos());
            if (d->snapMode == SnapAlways)
                pos = snapPosition(this, pos);
            QQuickRangeSliderNodePrivate::get(pressedNode)->setPosition(pos);
        }
    }
}

void QQuickRangeSlider::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickRangeSlider);
    QQuickControl::mouseReleaseEvent(event);

    d->pressPoint = QPoint();
    if (!keepMouseGrab())
        return;

    QQuickRangeSliderNode *pressedNode = d->first->isPressed() ? d->first : (d->second->isPressed() ? d->second : nullptr);
    if (!pressedNode)
        return;

    qreal pos = positionAt(this, pressedNode->handle(), event->pos());
    if (d->snapMode != NoSnap)
        pos = snapPosition(this, pos);
    qreal val = valueAt(this, pos);
    if (!qFuzzyCompare(val, pressedNode->value()))
        pressedNode->setValue(val);
    else if (d->snapMode != NoSnap)
        QQuickRangeSliderNodePrivate::get(pressedNode)->setPosition(pos);
    setKeepMouseGrab(false);
    pressedNode->setPressed(false);
}

void QQuickRangeSlider::mouseUngrabEvent()
{
    Q_D(QQuickRangeSlider);
    QQuickControl::mouseUngrabEvent();
    d->pressPoint = QPoint();
    d->first->setPressed(false);
    d->second->setPressed(false);
}

void QQuickRangeSlider::mirrorChange()
{
    Q_D(QQuickRangeSlider);
    QQuickControl::mirrorChange();
    emit d->first->visualPositionChanged();
    emit d->second->visualPositionChanged();
}

void QQuickRangeSlider::componentComplete()
{
    Q_D(QQuickRangeSlider);
    QQuickControl::componentComplete();

    QQuickRangeSliderNodePrivate *firstPrivate = QQuickRangeSliderNodePrivate::get(d->first);
    QQuickRangeSliderNodePrivate *secondPrivate = QQuickRangeSliderNodePrivate::get(d->second);

    if (firstPrivate->isPendingValue || secondPrivate->isPendingValue
        || !qFuzzyCompare(d->from, defaultFrom) || !qFuzzyCompare(d->to, defaultTo)) {
        // Properties were set while we were loading. To avoid clamping issues that occur when setting the
        // values of first and second overriding values set by the user, set them all at once at the end.
        // Another reason that we must set these values here is that the from and to values might have made the old range invalid.
        setValues(firstPrivate->isPendingValue ? firstPrivate->pendingValue : firstPrivate->value,
                      secondPrivate->isPendingValue ? secondPrivate->pendingValue : secondPrivate->value);

        firstPrivate->pendingValue = 0;
        firstPrivate->isPendingValue = false;
        secondPrivate->pendingValue = 0;
        secondPrivate->isPendingValue = false;
    } else {
        // If there was no pending data, we must still update the positions,
        // as first.setValue()/second.setValue() won't be called as part of default construction.
        // Don't need to ignore the second position when updating the first position here,
        // as our default values are guaranteed to be valid.
        firstPrivate->updatePosition();
        secondPrivate->updatePosition();
    }
}

/*!
    \qmlmethod void QtQuick.Controls::RangeSlider::first.increase()

    Increases the value of the handle by stepSize, or \c 0.1 if stepSize is not defined.

    \sa first
*/

/*!
    \qmlmethod void QtQuick.Controls::RangeSlider::first.decrease()

    Decreases the value of the handle by stepSize, or \c 0.1 if stepSize is not defined.

    \sa first
*/

/*!
    \qmlmethod void QtQuick.Controls::RangeSlider::second.increase()

    Increases the value of the handle by stepSize, or \c 0.1 if stepSize is not defined.

    \sa second
*/

/*!
    \qmlmethod void QtQuick.Controls::RangeSlider::second.decrease()

    Decreases the value of the handle by stepSize, or \c 0.1 if stepSize is not defined.

    \sa second
*/

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickRangeSlider::accessibleRole() const
{
    return QAccessible::Slider;
}
#endif

QT_END_NAMESPACE
