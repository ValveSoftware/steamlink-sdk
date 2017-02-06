/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickmousearea_p.h"
#include "qquickmousearea_p_p.h"
#include "qquickwindow.h"
#include "qquickdrag_p.h"

#include <private/qqmldata_p.h>
#include <private/qsgadaptationlayer_p.h>

#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qevent.h>
#include <QtGui/qstylehints.h>

#include <float.h>

QT_BEGIN_NAMESPACE

DEFINE_BOOL_CONFIG_OPTION(qmlVisualTouchDebugging, QML_VISUAL_TOUCH_DEBUGGING)

Q_DECLARE_LOGGING_CATEGORY(DBG_HOVER_TRACE)

QQuickMouseAreaPrivate::QQuickMouseAreaPrivate()
: enabled(true), scrollGestureEnabled(true), hovered(false), longPress(false),
  moved(false), stealMouse(false), doubleClick(false), preventStealing(false),
  propagateComposedEvents(false), overThreshold(false), pressed(0)
#ifndef QT_NO_DRAGANDDROP
  , drag(0)
#endif
#ifndef QT_NO_CURSOR
  , cursor(0)
#endif
{
}

QQuickMouseAreaPrivate::~QQuickMouseAreaPrivate()
{
#ifndef QT_NO_DRAGANDDROP
    delete drag;
#endif
#ifndef QT_NO_CURSOR
    delete cursor;
#endif
}

void QQuickMouseAreaPrivate::init()
{
    Q_Q(QQuickMouseArea);
    q->setAcceptedMouseButtons(Qt::LeftButton);
    q->setFiltersChildMouseEvents(true);
    if (qmlVisualTouchDebugging()) {
        q->setFlag(QQuickItem::ItemHasContents);
    }
}

void QQuickMouseAreaPrivate::saveEvent(QMouseEvent *event)
{
    lastPos = event->localPos();
    lastScenePos = event->windowPos();
    lastButton = event->button();
    lastButtons = event->buttons();
    lastModifiers = event->modifiers();
}

bool QQuickMouseAreaPrivate::isPressAndHoldConnected()
{
    Q_Q(QQuickMouseArea);
    IS_SIGNAL_CONNECTED(q, QQuickMouseArea, pressAndHold, (QQuickMouseEvent *));
}

bool QQuickMouseAreaPrivate::isDoubleClickConnected()
{
    Q_Q(QQuickMouseArea);
    IS_SIGNAL_CONNECTED(q, QQuickMouseArea, doubleClicked, (QQuickMouseEvent *));
}

bool QQuickMouseAreaPrivate::isClickConnected()
{
    Q_Q(QQuickMouseArea);
    IS_SIGNAL_CONNECTED(q, QQuickMouseArea, clicked, (QQuickMouseEvent *));
}

bool QQuickMouseAreaPrivate::isWheelConnected()
{
    Q_Q(QQuickMouseArea);
    IS_SIGNAL_CONNECTED(q, QQuickMouseArea, wheel, (QQuickWheelEvent *));
}

void QQuickMouseAreaPrivate::propagate(QQuickMouseEvent* event, PropagateType t)
{
    Q_Q(QQuickMouseArea);
    if (!window || !propagateComposedEvents)
        return;
    QPointF scenePos = q->mapToScene(QPointF(event->x(), event->y()));
    propagateHelper(event, window->contentItem(), scenePos, t);
}

bool QQuickMouseAreaPrivate::propagateHelper(QQuickMouseEvent *ev, QQuickItem *item,const QPointF &sp, PropagateType sig)
{
    //Based off of QQuickWindow::deliverInitialMousePressEvent
    //But specific to MouseArea, so doesn't belong in window
    Q_Q(const QQuickMouseArea);
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);

    if (itemPrivate->flags & QQuickItem::ItemClipsChildrenToShape) {
        QPointF p = item->mapFromScene(sp);
        if (!item->contains(p))
            return false;
    }

    QList<QQuickItem *> children = itemPrivate->paintOrderChildItems();
    for (int ii = children.count() - 1; ii >= 0; --ii) {
        QQuickItem *child = children.at(ii);
        if (!child->isVisible() || !child->isEnabled())
            continue;
        if (propagateHelper(ev, child, sp, sig))
            return true;
    }

    QQuickMouseArea* ma = qobject_cast<QQuickMouseArea*>(item);
    if (ma && ma != q && ma->isEnabled() && itemPrivate->acceptedMouseButtons() & ev->button()) {
        switch (sig) {
        case Click:
            if (!ma->d_func()->isClickConnected())
                return false;
            break;
        case DoubleClick:
            if (!ma->d_func()->isDoubleClickConnected())
                return false;
            break;
        case PressAndHold:
            if (!ma->d_func()->isPressAndHoldConnected())
                return false;
            break;
        }
        QPointF p = item->mapFromScene(sp);
        if (item->contains(p)) {
            ev->setX(p.x());
            ev->setY(p.y());
            ev->setAccepted(true);//It is connected, they have to explicitly ignore to let it slide
            switch (sig) {
            case Click: emit ma->clicked(ev); break;
            case DoubleClick: emit ma->doubleClicked(ev); break;
            case PressAndHold: emit ma->pressAndHold(ev); break;
            }
            if (ev->isAccepted())
                return true;
        }
    }
    return false;

}

/*!
    \qmltype MouseArea
    \instantiates QQuickMouseArea
    \inqmlmodule QtQuick
    \ingroup qtquick-input
    \brief Enables simple mouse handling
    \inherits Item

    A MouseArea is an invisible item that is typically used in conjunction with
    a visible item in order to provide mouse handling for that item.
    By effectively acting as a proxy, the logic for mouse handling can be
    contained within a MouseArea item.

    The \l enabled property is used to enable and disable mouse handling for
    the proxied item. When disabled, the mouse area becomes transparent to
    mouse events.

    The \l pressed read-only property indicates whether or not the user is
    holding down a mouse button over the mouse area. This property is often
    used in bindings between properties in a user interface. The containsMouse
    read-only property indicates the presence of the mouse cursor over the
    mouse area but, by default, only when a mouse button is held down; see
    the containsMouse documentation for details.

    Information about the mouse position and button clicks are provided via
    signals for which event handler properties are defined. The most commonly
    used involved handling mouse presses and clicks: onClicked, onDoubleClicked,
    onPressed, onReleased and onPressAndHold. It's also possible to handle mouse
    wheel events via the onWheel signal.

    If a MouseArea overlaps with the area of other MouseArea items, you can choose
    to propagate \c clicked, \c doubleClicked and \c pressAndHold events to these
    other items by setting propagateComposedEvents to true and rejecting events
    that should be propagated. See the propagateComposedEvents documentation for
    details.

    By default, MouseArea items only report mouse clicks and not changes to the
    position of the mouse cursor. Setting the hoverEnabled property ensures that
    handlers defined for onPositionChanged, onEntered and onExited are used and
    that the containsMouse property is updated even when no mouse buttons are
    pressed.

    \section1 Example Usage

    \div {class="float-right"}
    \inlineimage qml-mousearea-snippet.png
    \enddiv

    The following example uses a MouseArea in a \l Rectangle that changes
    the \l Rectangle color to red when clicked:

    \snippet qml/mousearea/mousearea.qml import
    \codeline
    \snippet qml/mousearea/mousearea.qml intro

    \clearfloat
    Many MouseArea signals pass a \l{MouseEvent}{mouse} parameter that contains
    additional information about the mouse event, such as the position, button,
    and any key modifiers.

    Here is an extension of the previous example that produces a different
    color when the area is right clicked:

    \snippet qml/mousearea/mousearea.qml intro-extended

    \sa MouseEvent, {mousearea}{MouseArea example},
    {Important Concepts In Qt Quick - User Input}
*/

/*!
    \qmlsignal QtQuick::MouseArea::entered()

    This signal is emitted when the mouse enters the mouse area.

    By default this signal is only emitted if a button is currently
    pressed. Set \l hoverEnabled to true to emit this signal
    even when no mouse button is pressed.

    \sa hoverEnabled

    The corresponding handler is \c onEntered.
*/

/*!
    \qmlsignal QtQuick::MouseArea::exited()

    This signal is emitted when the mouse exits the mouse area.

    By default this signal is only emitted if a button is currently
    pressed. Set \l hoverEnabled to true to emit this signal
    even when no mouse button is pressed.

    The example below shows a fairly typical relationship between
    two MouseAreas, with \c mouseArea2 on top of \c mouseArea1. Moving the
    mouse into \c mouseArea2 from \c mouseArea1 will cause \c mouseArea1
    to emit the \c exited signal.
    \qml
    Rectangle {
        width: 400; height: 400
        MouseArea {
            id: mouseArea1
            anchors.fill: parent
            hoverEnabled: true
        }
        MouseArea {
            id: mouseArea2
            width: 100; height: 100
            anchors.centerIn: parent
            hoverEnabled: true
        }
    }
    \endqml

    If instead you give the two MouseAreas a parent-child relationship,
    moving the mouse into \c mouseArea2 from \c mouseArea1 will \b not
    cause \c mouseArea1 to emit \c exited. Instead, they will
    both be considered to be simultaneously hovered.

    \sa hoverEnabled

    The corresponding handler is \c onExited.
*/

/*!
    \qmlsignal QtQuick::MouseArea::positionChanged(MouseEvent mouse)

    This signal is emitted when the mouse position changes.

    The \l {MouseEvent}{mouse} parameter provides information about the mouse, including the x and y
    position, and any buttons currently pressed.

    By default this signal is only emitted if a button is currently
    pressed. Set \l hoverEnabled to true to emit this signal
    even when no mouse button is pressed.

    When handling this signal, changing the \l {MouseEvent::}{accepted} property of the \a mouse
    parameter has no effect.

    The corresponding handler is \c onPositionChanged.
*/

/*!
    \qmlsignal QtQuick::MouseArea::clicked(MouseEvent mouse)

    This signal is emitted when there is a click. A click is defined as a press followed by a release,
    both inside the MouseArea (pressing, moving outside the MouseArea, and then moving back inside and
    releasing is also considered a click).

    The \l {MouseEvent}{mouse} parameter provides information about the click, including the x and y
    position of the release of the click, and whether the click was held.

    When handling this signal, changing the \l {MouseEvent::}{accepted} property of the \a mouse
    parameter has no effect.

    The corresponding handler is \c onClicked.
*/

/*!
    \qmlsignal QtQuick::MouseArea::pressed(MouseEvent mouse)

    This signal is emitted when there is a press.
    The \l {MouseEvent}{mouse} parameter provides information about the press, including the x and y
    position and which button was pressed.

    When handling this signal, use the \l {MouseEvent::}{accepted} property of the \a mouse
    parameter to control whether this MouseArea handles the press and all future mouse events until
    release. The default is to accept the event and not allow other MouseAreas beneath this one to
    handle the event.  If \e accepted is set to false, no further events will be sent to this MouseArea
    until the button is next pressed.

    The corresponding handler is \c onPressed.
*/

/*!
    \qmlsignal QtQuick::MouseArea::released(MouseEvent mouse)

    This signal is emitted when there is a release.
    The \l {MouseEvent}{mouse} parameter provides information about the click, including the x and y
    position of the release of the click, and whether the click was held.

    When handling this signal, changing the \l {MouseEvent::}{accepted} property of the \a mouse
    parameter has no effect.

    The corresponding handler is \c onReleased.

    \sa canceled
*/

/*!
    \qmlsignal QtQuick::MouseArea::pressAndHold(MouseEvent mouse)

    This signal is emitted when there is a long press (currently 800ms).
    The \l {MouseEvent}{mouse} parameter provides information about the press, including the x and y
    position of the press, and which button is pressed.

    When handling this signal, changing the \l {MouseEvent::}{accepted} property of the \a mouse
    parameter has no effect.

    The corresponding handler is \c onPressAndHold.
*/

/*!
    \qmlsignal QtQuick::MouseArea::doubleClicked(MouseEvent mouse)

    This signal is emitted when there is a double-click (a press followed by a release followed by a press).
    The \l {MouseEvent}{mouse} parameter provides information about the click, including the x and y
    position of the release of the click, and whether the click was held.

    When handling this signal, if the \l {MouseEvent::}{accepted} property of the \a mouse
    parameter is set to false, the pressed/released/clicked signals will be emitted for the second
    click; otherwise they are suppressed.  The \c accepted property defaults to true.

    The corresponding handler is \c onDoubleClicked.
*/

/*!
    \qmlsignal QtQuick::MouseArea::canceled()

    This signal is emitted when mouse events have been canceled, because another item stole the mouse event handling.

    This signal is for advanced use: it is useful when there is more than one MouseArea
    that is handling input, or when there is a MouseArea inside a \l Flickable. In the latter
    case, if you execute some logic in the \c onPressed signal handler and then start dragging, the
    \l Flickable will steal the mouse handling from the MouseArea. In these cases, to reset
    the logic when the MouseArea has lost the mouse handling to the \l Flickable,
    \c canceled should be handled in addition to \l released.

    The corresponding handler is \c onCanceled.
*/

/*!
    \qmlsignal QtQuick::MouseArea::wheel(WheelEvent wheel)

    This signal is emitted in response to both mouse wheel and trackpad scroll gestures.

    The \l {WheelEvent}{wheel} parameter provides information about the event, including the x and y
    position, any buttons currently pressed, and information about the wheel movement, including
    angleDelta and pixelDelta.

    The corresponding handler is \c onWheel.
*/

QQuickMouseArea::QQuickMouseArea(QQuickItem *parent)
  : QQuickItem(*(new QQuickMouseAreaPrivate), parent)
{
    Q_D(QQuickMouseArea);
    d->init();
#ifndef QT_NO_CURSOR
    // Explcitly call setCursor on QQuickItem since
    // it internally keeps a boolean hasCursor that doesn't
    // get set to true unless you call setCursor
    setCursor(Qt::ArrowCursor);
#endif
}

QQuickMouseArea::~QQuickMouseArea()
{
}

/*!
    \qmlproperty real QtQuick::MouseArea::mouseX
    \qmlproperty real QtQuick::MouseArea::mouseY
    These properties hold the coordinates of the mouse cursor.

    If the hoverEnabled property is false then these properties will only be valid
    while a button is pressed, and will remain valid as long as the button is held
    down even if the mouse is moved outside the area.

    By default, this property is false.

    If hoverEnabled is true then these properties will be valid when:
    \list
        \li no button is pressed, but the mouse is within the MouseArea (containsMouse is true).
        \li a button is pressed and held, even if it has since moved out of the area.
    \endlist

    The coordinates are relative to the MouseArea.
*/
qreal QQuickMouseArea::mouseX() const
{
    Q_D(const QQuickMouseArea);
    return d->lastPos.x();
}

qreal QQuickMouseArea::mouseY() const
{
    Q_D(const QQuickMouseArea);
    return d->lastPos.y();
}

/*!
    \qmlproperty bool QtQuick::MouseArea::enabled
    This property holds whether the item accepts mouse events.

    \note Due to historical reasons, this property is not equivalent to
    Item.enabled. It only affects mouse events, and its effect does not
    propagate to child items.

    By default, this property is true.
*/
bool QQuickMouseArea::isEnabled() const
{
    Q_D(const QQuickMouseArea);
    return d->enabled;
}

void QQuickMouseArea::setEnabled(bool a)
{
    Q_D(QQuickMouseArea);
    if (a != d->enabled) {
        d->enabled = a;
        emit enabledChanged();
    }
}

/*!
    \qmlproperty bool QtQuick::MouseArea::scrollGestureEnabled
    \since 5.5

    This property controls whether this MouseArea responds to scroll gestures
    from non-mouse devices, such as the 2-finger flick gesture on a trackpad.
    If set to false, the \l wheel signal be emitted only when the wheel event
    comes from an actual mouse with a wheel, while scroll gesture events will
    pass through to any other Item that will handle them. For example, the user
    might perform a flick gesture while the cursor is over an item containing a
    MouseArea, intending to interact with a Flickable which is underneath.
    Setting this property to false will allow the PinchArea to handle the mouse
    wheel or the pinch gesture, while the Flickable handles the flick gesture.

    By default, this property is true.
*/
bool QQuickMouseArea::isScrollGestureEnabled() const
{
    Q_D(const QQuickMouseArea);
    return d->scrollGestureEnabled;
}

void QQuickMouseArea::setScrollGestureEnabled(bool e)
{
    Q_D(QQuickMouseArea);
    if (e != d->scrollGestureEnabled) {
        d->scrollGestureEnabled = e;
        emit scrollGestureEnabledChanged();
    }
}

/*!
    \qmlproperty bool QtQuick::MouseArea::preventStealing
    This property holds whether the mouse events may be stolen from this
    MouseArea.

    If a MouseArea is placed within an item that filters child mouse
    events, such as Flickable, the mouse
    events may be stolen from the MouseArea if a gesture is recognized
    by the parent item, e.g. a flick gesture.  If preventStealing is
    set to true, no item will steal the mouse events.

    Note that setting preventStealing to true once an item has started
    stealing events will have no effect until the next press event.

    By default this property is false.
*/
bool QQuickMouseArea::preventStealing() const
{
    Q_D(const QQuickMouseArea);
    return d->preventStealing;
}

void QQuickMouseArea::setPreventStealing(bool prevent)
{
    Q_D(QQuickMouseArea);
    if (prevent != d->preventStealing) {
        d->preventStealing = prevent;
        setKeepMouseGrab(d->preventStealing && d->enabled);
        emit preventStealingChanged();
    }
}


/*!
    \qmlproperty bool QtQuick::MouseArea::propagateComposedEvents
    This property holds whether composed mouse events will automatically propagate to
    other MouseAreas that overlap with this MouseArea but are lower in the visual stacking order.
    By default, this property is false.

    MouseArea contains several composed events: \c clicked, \c doubleClicked and \c pressAndHold.
    These are composed of basic mouse events, like \c pressed, and can be propagated differently
    in comparison to basic events.

    If propagateComposedEvents is set to true, then composed events will be automatically
    propagated to other MouseAreas in the same location in the scene. Each event is propagated
    to the next \l enabled MouseArea beneath it in the stacking order, propagating down this visual
    hierarchy until a MouseArea accepts the event. Unlike \c pressed events, composed events will
    not be automatically accepted if no handler is present.

    For example, below is a yellow \l Rectangle that contains a blue \l Rectangle. The blue
    rectangle is the top-most item in the hierarchy of the visual stacking order; it will
    visually rendered above the yellow rectangle. Since the blue rectangle sets
    propagateComposedEvents to true, and also sets \l MouseEvent::accepted to false for all
    received \c clicked events, any \c clicked events it receives are propagated to the
    MouseArea of the yellow rectangle beneath it.

    \qml
    import QtQuick 2.0

    Rectangle {
        color: "yellow"
        width: 100; height: 100

        MouseArea {
            anchors.fill: parent
            onClicked: console.log("clicked yellow")
        }

        Rectangle {
            color: "blue"
            width: 50; height: 50

            MouseArea {
                anchors.fill: parent
                propagateComposedEvents: true
                onClicked: {
                    console.log("clicked blue")
                    mouse.accepted = false
                }
            }
        }
    }
    \endqml

    Clicking on the blue rectangle will cause the \c onClicked handler of its child MouseArea to
    be invoked; the event will then be propagated to the MouseArea of the yellow rectangle, causing
    its own \c onClicked handler to be invoked.

    This property greatly simplifies the usecase of when you want to have overlapping MouseAreas
    handling the composed events together. For example: if you want one MouseArea to handle \c clicked
    signals and the other to handle \c pressAndHold, or if you want one MouseArea to handle \c clicked most
    of the time, but pass it through when certain conditions are met.
*/
bool QQuickMouseArea::propagateComposedEvents() const
{
    Q_D(const QQuickMouseArea);
    return d->propagateComposedEvents;
}

void QQuickMouseArea::setPropagateComposedEvents(bool prevent)
{
    Q_D(QQuickMouseArea);
    if (prevent != d->propagateComposedEvents) {
        d->propagateComposedEvents = prevent;
        setKeepMouseGrab(d->propagateComposedEvents && d->enabled);
        emit propagateComposedEventsChanged();
    }
}

/*!
    \qmlproperty MouseButtons QtQuick::MouseArea::pressedButtons
    This property holds the mouse buttons currently pressed.

    It contains a bitwise combination of:
    \list
    \li Qt.LeftButton
    \li Qt.RightButton
    \li Qt.MiddleButton
    \endlist

    The code below displays "right" when the right mouse buttons is pressed:

    \snippet qml/mousearea/mousearea.qml mousebuttons

    \note this property only handles buttons specified in \l acceptedButtons.

    \sa acceptedButtons
*/
Qt::MouseButtons QQuickMouseArea::pressedButtons() const
{
    Q_D(const QQuickMouseArea);
    return d->pressed;
}

void QQuickMouseArea::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickMouseArea);
    d->moved = false;
    d->stealMouse = d->preventStealing;
    d->overThreshold = false;
    if (!d->enabled || !(event->button() & acceptedMouseButtons())) {
        QQuickItem::mousePressEvent(event);
    } else {
        d->longPress = false;
        d->saveEvent(event);
#ifndef QT_NO_DRAGANDDROP
        if (d->drag)
            d->drag->setActive(false);
#endif
        setHovered(true);
        d->startScene = event->windowPos();
        d->pressAndHoldTimer.start(QGuiApplication::styleHints()->mousePressAndHoldInterval(), this);
        setKeepMouseGrab(d->stealMouse);
        event->setAccepted(setPressed(event->button(), true, event->source()));
    }
}

void QQuickMouseArea::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickMouseArea);
    if (!d->enabled && !d->pressed) {
        QQuickItem::mouseMoveEvent(event);
        return;
    }

    d->saveEvent(event);

    // ### we should skip this if these signals aren't used
    // ### can GV handle this for us?
    setHovered(contains(d->lastPos));

#ifndef QT_NO_DRAGANDDROP
    if (d->drag && d->drag->target()) {
        if (!d->moved) {
            d->targetStartPos = d->drag->target()->parentItem()
                    ? d->drag->target()->parentItem()->mapToScene(d->drag->target()->position())
                    : d->drag->target()->position();
        }

        QPointF startLocalPos;
        QPointF curLocalPos;
        if (drag()->target()->parentItem()) {
            startLocalPos = drag()->target()->parentItem()->mapFromScene(d->startScene);
            curLocalPos = drag()->target()->parentItem()->mapFromScene(event->windowPos());
        } else {
            startLocalPos = d->startScene;
            curLocalPos = event->windowPos();
        }

        if (keepMouseGrab() && d->stealMouse && d->overThreshold && !d->drag->active())
            d->drag->setActive(true);

        QPointF startPos = d->drag->target()->parentItem()
                ? d->drag->target()->parentItem()->mapFromScene(d->targetStartPos)
                : d->targetStartPos;

        bool dragX = drag()->axis() & QQuickDrag::XAxis;
        bool dragY = drag()->axis() & QQuickDrag::YAxis;

        QPointF dragPos = d->drag->target()->position();
        if (dragX) {
            dragPos.setX(qBound(
                    d->drag->xmin(),
                    startPos.x() + curLocalPos.x() - startLocalPos.x(),
                    d->drag->xmax()));
        }
        if (dragY) {
            dragPos.setY(qBound(
                    d->drag->ymin(),
                    startPos.y() + curLocalPos.y() - startLocalPos.y(),
                    d->drag->ymax()));
        }
        if (d->drag->active())
            d->drag->target()->setPosition(dragPos);

        if (!d->overThreshold && (QQuickWindowPrivate::dragOverThreshold(dragPos.x() - startPos.x(), Qt::XAxis, event, d->drag->threshold())
                                  || QQuickWindowPrivate::dragOverThreshold(dragPos.y() - startPos.y(), Qt::YAxis, event, d->drag->threshold())))
        {
            d->overThreshold = true;
            if (d->drag->smoothed())
                d->startScene = event->windowPos();
        }

        if (!keepMouseGrab() && d->overThreshold) {
            setKeepMouseGrab(true);
            d->stealMouse = true;
        }

        d->moved = true;
    }
#endif

    QQuickMouseEvent &me = d->quickMouseEvent;
    me.reset(d->lastPos.x(), d->lastPos.y(), d->lastButton, d->lastButtons, d->lastModifiers, false, d->longPress);
    me.setSource(event->source());
    emit mouseXChanged(&me);
    me.setPosition(d->lastPos);
    emit mouseYChanged(&me);
    me.setPosition(d->lastPos);
    emit positionChanged(&me);
}

void QQuickMouseArea::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickMouseArea);
    d->stealMouse = false;
    d->overThreshold = false;
    if (!d->enabled && !d->pressed) {
        QQuickItem::mouseReleaseEvent(event);
    } else {
        d->saveEvent(event);
        setPressed(event->button(), false, event->source());
        if (!d->pressed) {
            // no other buttons are pressed
#ifndef QT_NO_DRAGANDDROP
            if (d->drag)
                d->drag->setActive(false);
#endif
            // If we don't accept hover, we need to reset containsMouse.
            if (!acceptHoverEvents())
                setHovered(false);
            QQuickWindow *w = window();
            if (w && w->mouseGrabberItem() == this)
                ungrabMouse();
            setKeepMouseGrab(false);
        }
    }
    d->doubleClick = false;
}

void QQuickMouseArea::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_D(QQuickMouseArea);
    if (d->enabled) {
        d->saveEvent(event);
        QQuickMouseEvent &me = d->quickMouseEvent;
        me.reset(d->lastPos.x(), d->lastPos.y(), d->lastButton, d->lastButtons, d->lastModifiers, true, false);
        me.setSource(event->source());
        me.setAccepted(d->isDoubleClickConnected());
        emit this->doubleClicked(&me);
        if (!me.isAccepted())
            d->propagate(&me, QQuickMouseAreaPrivate::DoubleClick);
        d->doubleClick = d->isDoubleClickConnected() || me.isAccepted();
    }
    QQuickItem::mouseDoubleClickEvent(event);
}

void QQuickMouseArea::hoverEnterEvent(QHoverEvent *event)
{
    Q_D(QQuickMouseArea);
    if (!d->enabled && !d->pressed) {
        QQuickItem::hoverEnterEvent(event);
    } else {
        d->lastPos = event->posF();
        d->lastModifiers = event->modifiers();
        setHovered(true);
        QQuickMouseEvent &me = d->quickMouseEvent;
        me.reset(d->lastPos.x(), d->lastPos.y(), Qt::NoButton, Qt::NoButton, d->lastModifiers, false, false);
        emit mouseXChanged(&me);
        me.setPosition(d->lastPos);
        emit mouseYChanged(&me);
        me.setPosition(d->lastPos);
    }
}

void QQuickMouseArea::hoverMoveEvent(QHoverEvent *event)
{
    Q_D(QQuickMouseArea);
    if (!d->enabled && !d->pressed) {
        QQuickItem::hoverMoveEvent(event);
    } else if (d->lastPos != event->posF()) {
        d->lastPos = event->posF();
        d->lastModifiers = event->modifiers();
        QQuickMouseEvent &me = d->quickMouseEvent;
        me.reset(d->lastPos.x(), d->lastPos.y(), Qt::NoButton, Qt::NoButton, d->lastModifiers, false, false);
        emit mouseXChanged(&me);
        me.setPosition(d->lastPos);
        emit mouseYChanged(&me);
        me.setPosition(d->lastPos);
        emit positionChanged(&me);
    }
}

void QQuickMouseArea::hoverLeaveEvent(QHoverEvent *event)
{
    Q_D(QQuickMouseArea);
    if (!d->enabled && !d->pressed)
        QQuickItem::hoverLeaveEvent(event);
    else
        setHovered(false);
}

#ifndef QT_NO_WHEELEVENT
void QQuickMouseArea::wheelEvent(QWheelEvent *event)
{
    Q_D(QQuickMouseArea);
    if (!d->enabled || (!isScrollGestureEnabled() && event->source() != Qt::MouseEventNotSynthesized)) {
        QQuickItem::wheelEvent(event);
        return;
    }

    QQuickWheelEvent &we = d->quickWheelEvent;
    we.reset(event->posF().x(), event->posF().y(), event->angleDelta(), event->pixelDelta(),
             event->buttons(), event->modifiers(), event->inverted());
    we.setAccepted(d->isWheelConnected());
    emit wheel(&we);
    if (!we.isAccepted())
        QQuickItem::wheelEvent(event);
}
#endif

void QQuickMouseArea::ungrabMouse()
{
    Q_D(QQuickMouseArea);
    if (d->pressed) {
        // if our mouse grab has been removed (probably by Flickable), fix our
        // state
        d->pressed = 0;
        d->stealMouse = false;
        d->doubleClick = false;
        d->overThreshold = false;
        setKeepMouseGrab(false);

#ifndef QT_NO_DRAGANDDROP
        if (d->drag)
            d->drag->setActive(false);
#endif

        emit canceled();
        emit pressedChanged();
        emit containsPressChanged();
        emit pressedButtonsChanged();

        if (d->hovered && !isUnderMouse()) {
            d->hovered = false;
            emit hoveredChanged();
        }
    }
}

void QQuickMouseArea::mouseUngrabEvent()
{
    ungrabMouse();
}

bool QQuickMouseArea::sendMouseEvent(QMouseEvent *event)
{
    Q_D(QQuickMouseArea);
    QPointF localPos = mapFromScene(event->windowPos());

    QQuickWindow *c = window();
    QQuickItem *grabber = c ? c->mouseGrabberItem() : 0;
    bool stealThisEvent = d->stealMouse;
    if ((stealThisEvent || contains(localPos)) && (!grabber || !grabber->keepMouseGrab())) {
        QMouseEvent mouseEvent(event->type(), localPos, event->windowPos(), event->screenPos(),
                               event->button(), event->buttons(), event->modifiers());
        mouseEvent.setAccepted(false);

        switch (event->type()) {
        case QEvent::MouseMove:
            mouseMoveEvent(&mouseEvent);
            break;
        case QEvent::MouseButtonPress:
            mousePressEvent(&mouseEvent);
            break;
        case QEvent::MouseButtonRelease:
            mouseReleaseEvent(&mouseEvent);
            stealThisEvent = d->stealMouse;
            break;
        default:
            break;
        }
        grabber = c ? c->mouseGrabberItem() : 0;
        if (grabber && stealThisEvent && !grabber->keepMouseGrab() && grabber != this)
            grabMouse();

        return stealThisEvent;
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        if (d->pressed) {
            d->pressed &= ~event->button();
            emit pressedButtonsChanged();
            if (!d->pressed) {
                // no other buttons are pressed
                d->stealMouse = false;
                d->overThreshold = false;
                if (c && c->mouseGrabberItem() == this)
                    ungrabMouse();
                emit canceled();
                emit pressedChanged();
                emit containsPressChanged();
                if (d->hovered) {
                    d->hovered = false;
                    emit hoveredChanged();
                }
            }
        }
    }
    return false;
}

bool QQuickMouseArea::childMouseEventFilter(QQuickItem *i, QEvent *e)
{
    Q_D(QQuickMouseArea);
    if (!d->pressed &&
            (!d->enabled || !isVisible()
#ifndef QT_NO_DRAGANDDROP
             || !d->drag || !d->drag->filterChildren()
#endif
            )
       )
        return QQuickItem::childMouseEventFilter(i, e);
    switch (e->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
        return sendMouseEvent(static_cast<QMouseEvent *>(e));
    default:
        break;
    }

    return QQuickItem::childMouseEventFilter(i, e);
}

void QQuickMouseArea::timerEvent(QTimerEvent *event)
{
    Q_D(QQuickMouseArea);
    if (event->timerId() == d->pressAndHoldTimer.timerId()) {
        d->pressAndHoldTimer.stop();
#ifndef QT_NO_DRAGANDDROP
        bool dragged = d->drag && d->drag->active();
#else
        bool dragged = false;
#endif
        if (d->pressed && dragged == false && d->hovered == true) {
            d->longPress = true;
            QQuickMouseEvent &me = d->quickMouseEvent;
            me.reset(d->lastPos.x(), d->lastPos.y(), d->lastButton, d->lastButtons, d->lastModifiers, false, d->longPress);
            me.setSource(Qt::MouseEventSynthesizedByQt);
            me.setAccepted(d->isPressAndHoldConnected());
            emit pressAndHold(&me);
            if (!me.isAccepted())
                d->propagate(&me, QQuickMouseAreaPrivate::PressAndHold);
            if (!me.isAccepted()) // no one handled the long press - allow click
                d->longPress = false;
        }
    }
}

void QQuickMouseArea::windowDeactivateEvent()
{
    ungrabMouse();
    QQuickItem::windowDeactivateEvent();
}

void QQuickMouseArea::geometryChanged(const QRectF &newGeometry,
                                            const QRectF &oldGeometry)
{
    Q_D(QQuickMouseArea);
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    if (d->lastScenePos.isNull)
        d->lastScenePos = mapToScene(d->lastPos);
    else if (newGeometry.x() != oldGeometry.x() || newGeometry.y() != oldGeometry.y())
        d->lastPos = mapFromScene(d->lastScenePos);
}

void QQuickMouseArea::itemChange(ItemChange change, const ItemChangeData &value)
{
    Q_D(QQuickMouseArea);
    switch (change) {
    case ItemVisibleHasChanged:
        if (acceptHoverEvents() && d->hovered != (isVisible() && isUnderMouse())) {
            if (!d->hovered) {
                QPointF cursorPos = QGuiApplicationPrivate::lastCursorPosition;
                d->lastScenePos = d->window->mapFromGlobal(cursorPos.toPoint());
                d->lastPos = mapFromScene(d->lastScenePos);
            }
            setHovered(!d->hovered);
        }
        break;
    default:
        break;
    }

    QQuickItem::itemChange(change, value);
}

/*!
    \qmlproperty bool QtQuick::MouseArea::hoverEnabled
    This property holds whether hover events are handled.

    By default, mouse events are only handled in response to a button event, or when a button is
    pressed.  Hover enables handling of all mouse events even when no mouse button is
    pressed.

    This property affects the containsMouse property and the onEntered, onExited and
    onPositionChanged signals.
*/
bool QQuickMouseArea::hoverEnabled() const
{
    return acceptHoverEvents();
}

void QQuickMouseArea::setHoverEnabled(bool h)
{
    if (h == acceptHoverEvents())
        return;

    setAcceptHoverEvents(h);
    emit hoverEnabledChanged();
}


/*!
    \qmlproperty bool QtQuick::MouseArea::containsMouse
    This property holds whether the mouse is currently inside the mouse area.

    \warning If hoverEnabled is false, containsMouse will only be valid
    when the mouse is pressed while the mouse cursor is inside the MouseArea.
*/
bool QQuickMouseArea::hovered() const
{
    Q_D(const QQuickMouseArea);
    return d->hovered;
}

/*!
    \qmlproperty bool QtQuick::MouseArea::pressed
    This property holds whether any of the \l acceptedButtons are currently pressed.
*/
bool QQuickMouseArea::pressed() const
{
    Q_D(const QQuickMouseArea);
    return d->pressed;
}

/*!
    \qmlproperty bool QtQuick::MouseArea::containsPress
    \since 5.4
    This is a convenience property equivalent to \c {pressed && containsMouse},
    i.e. it holds whether any of the \l acceptedButtons are currently pressed
    and the mouse is currently within the MouseArea.

    This property is particularly useful for highlighting an item while the mouse
    is pressed within its bounds.

    \sa pressed, containsMouse
*/
bool QQuickMouseArea::containsPress() const
{
    Q_D(const QQuickMouseArea);
    return d->pressed && d->hovered;
}

void QQuickMouseArea::setHovered(bool h)
{
    Q_D(QQuickMouseArea);
    if (d->hovered != h) {
        qCDebug(DBG_HOVER_TRACE) << this << d->hovered <<  "->" << h;
        d->hovered = h;
        emit hoveredChanged();
        d->hovered ? emit entered() : emit exited();
        if (d->pressed)
            emit containsPressChanged();
    }
}

/*!
    \qmlproperty Qt::MouseButtons QtQuick::MouseArea::acceptedButtons
    This property holds the mouse buttons that the mouse area reacts to.

    To specify that the MouseArea will react to multiple buttons,
    Qt::MouseButtons flag values are combined using the "|" (or) operator:

    \code
    MouseArea { acceptedButtons: Qt.LeftButton | Qt.RightButton }
    \endcode

    To indicate that all possible mouse buttons are to be accepted,
    the special value 'Qt.AllButtons' may be used:

    \code
    MouseArea { acceptedButtons: Qt.AllButtons }
    \endcode

    The default value is \c Qt.LeftButton.
*/
Qt::MouseButtons QQuickMouseArea::acceptedButtons() const
{
    return acceptedMouseButtons();
}

void QQuickMouseArea::setAcceptedButtons(Qt::MouseButtons buttons)
{
    if (buttons != acceptedMouseButtons()) {
        setAcceptedMouseButtons(buttons);
        emit acceptedButtonsChanged();
    }
}

bool QQuickMouseArea::setPressed(Qt::MouseButton button, bool p, Qt::MouseEventSource source)
{
    Q_D(QQuickMouseArea);

#ifndef QT_NO_DRAGANDDROP
    bool dragged = d->drag && d->drag->active();
#else
    bool dragged = false;
#endif
    bool wasPressed = d->pressed & button;
    bool isclick = wasPressed && p == false && dragged == false && d->hovered == true;
    Qt::MouseButtons oldPressed = d->pressed;

    if (wasPressed != p) {
        QQuickMouseEvent &me = d->quickMouseEvent;
        me.reset(d->lastPos.x(), d->lastPos.y(), d->lastButton, d->lastButtons, d->lastModifiers, isclick, d->longPress);
        me.setSource(source);
        if (p) {
            d->pressed |= button;
            if (!d->doubleClick)
                emit pressed(&me);
            me.setPosition(d->lastPos);
            emit mouseXChanged(&me);
            me.setPosition(d->lastPos);
            emit mouseYChanged(&me);

            if (!me.isAccepted()) {
                d->pressed = Qt::NoButton;
            }

            if (!oldPressed) {
                emit pressedChanged();
                emit containsPressChanged();
            }
            emit pressedButtonsChanged();
        } else {
            d->pressed &= ~button;
            emit released(&me);
            me.setPosition(d->lastPos);
            if (!d->pressed) {
                emit pressedChanged();
                emit containsPressChanged();
            }
            emit pressedButtonsChanged();
            if (isclick && !d->longPress && !d->doubleClick){
                me.setAccepted(d->isClickConnected());
                emit clicked(&me);
                if (!me.isAccepted())
                    d->propagate(&me, QQuickMouseAreaPrivate::Click);
            }
        }

        return me.isAccepted();
    }
    return false;
}


/*!
    \qmlproperty Qt::CursorShape QtQuick::MouseArea::cursorShape
    This property holds the cursor shape for this mouse area.
    Note that on platforms that do not display a mouse cursor this may have
    no effect.

    The available cursor shapes are:
    \list
    \li Qt.ArrowCursor
    \li Qt.UpArrowCursor
    \li Qt.CrossCursor
    \li Qt.WaitCursor
    \li Qt.IBeamCursor
    \li Qt.SizeVerCursor
    \li Qt.SizeHorCursor
    \li Qt.SizeBDiagCursor
    \li Qt.SizeFDiagCursor
    \li Qt.SizeAllCursor
    \li Qt.BlankCursor
    \li Qt.SplitVCursor
    \li Qt.SplitHCursor
    \li Qt.PointingHandCursor
    \li Qt.ForbiddenCursor
    \li Qt.WhatsThisCursor
    \li Qt.BusyCursor
    \li Qt.OpenHandCursor
    \li Qt.ClosedHandCursor
    \li Qt.DragCopyCursor
    \li Qt.DragMoveCursor
    \li Qt.DragLinkCursor
    \endlist

    In order to only set a mouse cursor shape for a region without reacting
    to mouse events set the acceptedButtons to none:

    \code
    MouseArea { cursorShape: Qt.IBeamCursor; acceptedButtons: Qt.NoButton }
    \endcode

    The default value is \c Qt.ArrowCursor.
    \sa Qt::CursorShape
*/

#ifndef QT_NO_CURSOR
Qt::CursorShape QQuickMouseArea::cursorShape() const
{
    return cursor().shape();
}

void QQuickMouseArea::setCursorShape(Qt::CursorShape shape)
{
    if (cursor().shape() == shape)
        return;

    setCursor(shape);

    emit cursorShapeChanged();
}

#endif

/*!
    \qmlpropertygroup QtQuick::MouseArea::drag
    \qmlproperty Item QtQuick::MouseArea::drag.target
    \qmlproperty bool QtQuick::MouseArea::drag.active
    \qmlproperty enumeration QtQuick::MouseArea::drag.axis
    \qmlproperty real QtQuick::MouseArea::drag.minimumX
    \qmlproperty real QtQuick::MouseArea::drag.maximumX
    \qmlproperty real QtQuick::MouseArea::drag.minimumY
    \qmlproperty real QtQuick::MouseArea::drag.maximumY
    \qmlproperty bool QtQuick::MouseArea::drag.filterChildren
    \qmlproperty real QtQuick::MouseArea::drag.threshold

    \c drag provides a convenient way to make an item draggable.

    \list
    \li \c drag.target specifies the id of the item to drag.
    \li \c drag.active specifies if the target item is currently being dragged.
    \li \c drag.axis specifies whether dragging can be done horizontally (\c Drag.XAxis), vertically (\c Drag.YAxis), or both (\c Drag.XAndYAxis)
    \li \c drag.minimum and \c drag.maximum limit how far the target can be dragged along the corresponding axes.
    \endlist

    The following example displays a \l Rectangle that can be dragged along the X-axis. The opacity
    of the rectangle is reduced when it is dragged to the right.

    \snippet qml/mousearea/mousearea.qml drag

    \note Items cannot be dragged if they are anchored for the requested
    \c drag.axis. For example, if \c anchors.left or \c anchors.right was set
    for \c rect in the above example, it cannot be dragged along the X-axis.
    This can be avoided by settng the anchor value to \c undefined in
    an \l {pressed}{onPressed} handler.

    If \c drag.filterChildren is set to true, a drag can override descendant MouseAreas.  This
    enables a parent MouseArea to handle drags, for example, while descendants handle clicks:

    \c drag.threshold determines the threshold in pixels of when the drag operation should
    start. By default this is bound to a platform dependent value. This property was added in
    Qt Quick 2.2.

    If \c drag.smoothed is \c true, the target will be moved only after the drag operation has
    started. If set to \c false, the target will be moved straight to the current mouse position.
    By default, this property is \c true. This property was added in Qt Quick 2.4

    See the \l Drag attached property and \l DropArea if you want to make a drop.

    \snippet qml/mousearea/mouseareadragfilter.qml dragfilter

*/

#ifndef QT_NO_DRAGANDDROP
QQuickDrag *QQuickMouseArea::drag()
{
    Q_D(QQuickMouseArea);
    if (!d->drag)
        d->drag = new QQuickDrag;
    return d->drag;
}
#endif

QSGNode *QQuickMouseArea::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    Q_UNUSED(data);
    Q_D(QQuickMouseArea);

    if (!qmlVisualTouchDebugging())
        return 0;

    QSGInternalRectangleNode *rectangle = static_cast<QSGInternalRectangleNode *>(oldNode);
    if (!rectangle) rectangle = d->sceneGraphContext()->createInternalRectangleNode();

    rectangle->setRect(QRectF(0, 0, width(), height()));
    rectangle->setColor(QColor(255, 0, 0, 50));
    rectangle->update();
    return rectangle;
}

QT_END_NAMESPACE
