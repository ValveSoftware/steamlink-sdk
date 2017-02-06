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

#include "qquickevents_p_p.h"
#include <QtGui/private/qguiapplication_p.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickwindow_p.h>
#include <private/qdebug_p.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcPointerEvents, "qt.quick.pointer.events")
Q_LOGGING_CATEGORY(lcPointerGrab, "qt.quick.pointer.grab")

/*!
    \qmltype KeyEvent
    \instantiates QQuickKeyEvent
    \inqmlmodule QtQuick
    \ingroup qtquick-input-events

    \brief Provides information about a key event

    For example, the following changes the Item's state property when the Enter
    key is pressed:
    \qml
Item {
    focus: true
    Keys.onPressed: { if (event.key == Qt.Key_Enter) state = 'ShowDetails'; }
}
    \endqml
*/

/*!
    \qmlproperty int QtQuick::KeyEvent::key

    This property holds the code of the key that was pressed or released.

    See \l {Qt::Key}{Qt.Key} for the list of keyboard codes. These codes are
    independent of the underlying window system. Note that this
    function does not distinguish between capital and non-capital
    letters; use the \l {KeyEvent::}{text} property for this purpose.

    A value of either 0 or \l {Qt::Key_unknown}{Qt.Key_Unknown} means that the event is not
    the result of a known key; for example, it may be the result of
    a compose sequence, a keyboard macro, or due to key event
    compression.
*/

/*!
    \qmlproperty string QtQuick::KeyEvent::text

    This property holds the Unicode text that the key generated.
    The text returned can be an empty string in cases where modifier keys,
    such as Shift, Control, Alt, and Meta, are being pressed or released.
    In such cases \c key will contain a valid value
*/

/*!
    \qmlproperty bool QtQuick::KeyEvent::isAutoRepeat

    This property holds whether this event comes from an auto-repeating key.
*/

/*!
    \qmlproperty quint32 QtQuick::KeyEvent::nativeScanCode

    This property contains the native scan code of the key that was pressed. It is
    passed through from QKeyEvent unchanged.

    \sa QKeyEvent::nativeScanCode()
*/

/*!
    \qmlproperty int QtQuick::KeyEvent::count

    This property holds the number of keys involved in this event. If \l KeyEvent::text
    is not empty, this is simply the length of the string.
*/

/*!
    \qmlproperty bool QtQuick::KeyEvent::accepted

    Setting \a accepted to true prevents the key event from being
    propagated to the item's parent.

    Generally, if the item acts on the key event then it should be accepted
    so that ancestor items do not also respond to the same event.
*/

/*!
    \qmlproperty int QtQuick::KeyEvent::modifiers

    This property holds the keyboard modifier flags that existed immediately
    before the event occurred.

    It contains a bitwise combination of:
    \list
    \li Qt.NoModifier - No modifier key is pressed.
    \li Qt.ShiftModifier - A Shift key on the keyboard is pressed.
    \li Qt.ControlModifier - A Ctrl key on the keyboard is pressed.
    \li Qt.AltModifier - An Alt key on the keyboard is pressed.
    \li Qt.MetaModifier - A Meta key on the keyboard is pressed.
    \li Qt.KeypadModifier - A keypad button is pressed.
    \endlist

    For example, to react to a Shift key + Enter key combination:
    \qml
    Item {
        focus: true
        Keys.onPressed: {
            if ((event.key == Qt.Key_Enter) && (event.modifiers & Qt.ShiftModifier))
                doSomething();
        }
    }
    \endqml
*/

/*!
    \qmlmethod bool QtQuick::KeyEvent::matches(StandardKey key)
    \since 5.2

    Returns \c true if the key event matches the given standard \a key; otherwise returns \c false.

    \qml
    Item {
        focus: true
        Keys.onPressed: {
            if (event.matches(StandardKey.Undo))
                myModel.undo();
            else if (event.matches(StandardKey.Redo))
                myModel.redo();
        }
    }
    \endqml

    \sa QKeySequence::StandardKey
*/

/*!
    \qmltype MouseEvent
    \instantiates QQuickMouseEvent
    \inqmlmodule QtQuick
    \ingroup qtquick-input-events

    \brief Provides information about a mouse event

    The position of the mouse can be found via the \l x and \l y properties.
    The button that caused the event is available via the \l button property.

    \sa MouseArea
*/

/*!
    \internal
    \class QQuickMouseEvent
*/

/*!
    \qmlproperty int QtQuick::MouseEvent::x
    \qmlproperty int QtQuick::MouseEvent::y

    These properties hold the coordinates of the position supplied by the mouse event.
*/


/*!
    \qmlproperty bool QtQuick::MouseEvent::accepted

    Setting \a accepted to true prevents the mouse event from being
    propagated to items below this item.

    Generally, if the item acts on the mouse event then it should be accepted
    so that items lower in the stacking order do not also respond to the same event.
*/

/*!
    \qmlproperty enumeration QtQuick::MouseEvent::button

    This property holds the button that caused the event.  It can be one of:
    \list
    \li Qt.LeftButton
    \li Qt.RightButton
    \li Qt.MiddleButton
    \endlist
*/

/*!
    \qmlproperty bool QtQuick::MouseEvent::wasHeld

    This property is true if the mouse button has been held pressed longer the
    threshold (800ms).
*/

/*!
    \qmlproperty int QtQuick::MouseEvent::buttons

    This property holds the mouse buttons pressed when the event was generated.
    For mouse move events, this is all buttons that are pressed down. For mouse
    press and double click events this includes the button that caused the event.
    For mouse release events this excludes the button that caused the event.

    It contains a bitwise combination of:
    \list
    \li Qt.LeftButton
    \li Qt.RightButton
    \li Qt.MiddleButton
    \endlist
*/

/*!
    \qmlproperty int QtQuick::MouseEvent::modifiers

    This property holds the keyboard modifier flags that existed immediately
    before the event occurred.

    It contains a bitwise combination of:
    \list
    \li Qt.NoModifier - No modifier key is pressed.
    \li Qt.ShiftModifier - A Shift key on the keyboard is pressed.
    \li Qt.ControlModifier - A Ctrl key on the keyboard is pressed.
    \li Qt.AltModifier - An Alt key on the keyboard is pressed.
    \li Qt.MetaModifier - A Meta key on the keyboard is pressed.
    \li Qt.KeypadModifier - A keypad button is pressed.
    \endlist

    For example, to react to a Shift key + Left mouse button click:
    \qml
    MouseArea {
        onClicked: {
            if ((mouse.button == Qt.LeftButton) && (mouse.modifiers & Qt.ShiftModifier))
                doSomething();
        }
    }
    \endqml
*/

/*!
    \qmlproperty int QtQuick::MouseEvent::source
    \since 5.7

    This property holds the source of the mouse event.

    The mouse event source can be used to distinguish between genuine and
    artificial mouse events. When using other pointing devices such as
    touchscreens and graphics tablets, if the application does not make use of
    the actual touch or tablet events, mouse events may be synthesized by the
    operating system or by Qt itself.

    The value can be one of:

    \value Qt.MouseEventNotSynthesized The most common value. On platforms where
    such information is available, this value indicates that the event
    represents a genuine mouse event from the system.

    \value Qt.MouseEventSynthesizedBySystem Indicates that the mouse event was
    synthesized from a touch or tablet event by the platform.

    \value Qt.MouseEventSynthesizedByQt Indicates that the mouse event was
    synthesized from an unhandled touch or tablet event by Qt.

    \value Qt.MouseEventSynthesizedByApplication Indicates that the mouse event
    was synthesized by the application. This allows distinguishing
    application-generated mouse events from the ones that are coming from the
    system or are synthesized by Qt.

    For example, to react only to events which come from an actual mouse:
    \qml
    MouseArea {
        onPressed: if (mouse.source !== Qt.MouseEventNotSynthesized) {
            mouse.accepted = false
        }

        onClicked: doSomething()
    }
    \endqml

    If the handler for the press event rejects the event, it will be propagated
    further, and then another Item underneath can handle synthesized events
    from touchscreens. For example, if a Flickable is used underneath (and the
    MouseArea is not a child of the Flickable), it can be useful for the
    MouseArea to handle genuine mouse events in one way, while allowing touch
    events to fall through to the Flickable underneath, so that the ability to
    flick on a touchscreen is retained. In that case the ability to drag the
    Flickable via mouse would be lost, but it does not prevent Flickable from
    receiving mouse wheel events.
*/

/*!
    \qmltype WheelEvent
    \instantiates QQuickWheelEvent
    \inqmlmodule QtQuick
    \ingroup qtquick-input-events
    \brief Provides information about a mouse wheel event

    The position of the mouse can be found via the \l x and \l y properties.

    \sa MouseArea
*/

/*!
    \internal
    \class QQuickWheelEvent
*/

/*!
    \qmlproperty int QtQuick::WheelEvent::x
    \qmlproperty int QtQuick::WheelEvent::y

    These properties hold the coordinates of the position supplied by the wheel event.
*/

/*!
    \qmlproperty bool QtQuick::WheelEvent::accepted

    Setting \a accepted to true prevents the wheel event from being
    propagated to items below this item.

    Generally, if the item acts on the wheel event then it should be accepted
    so that items lower in the stacking order do not also respond to the same event.
*/

/*!
    \qmlproperty int QtQuick::WheelEvent::buttons

    This property holds the mouse buttons pressed when the wheel event was generated.

    It contains a bitwise combination of:
    \list
    \li Qt.LeftButton
    \li Qt.RightButton
    \li Qt.MiddleButton
    \endlist
*/

/*!
    \qmlproperty point QtQuick::WheelEvent::angleDelta

    This property holds the distance that the wheel is rotated in wheel degrees.
    The x and y cordinate of this property holds the delta in horizontal and
    vertical orientation.

    A positive value indicates that the wheel was rotated up/right;
    a negative value indicates that the wheel was rotated down/left.

    Most mouse types work in steps of 15 degrees, in which case the delta value is a
    multiple of 120; i.e., 120 units * 1/8 = 15 degrees.
*/

/*!
    \qmlproperty point QtQuick::WheelEvent::pixelDelta

    This property holds the delta in screen pixels and is available in platforms that
    have high-resolution trackpads, such as \macos.
    The x and y cordinate of this property holds the delta in horizontal and
    vertical orientation. The value should be used directly to scroll content on screen.

    For platforms without high-resolution trackpad support, pixelDelta will always be (0,0),
    and angleDelta should be used instead.
*/

/*!
    \qmlproperty int QtQuick::WheelEvent::modifiers

    This property holds the keyboard modifier flags that existed immediately
    before the event occurred.

    It contains a bitwise combination of:
    \list
    \li Qt.NoModifier - No modifier key is pressed.
    \li Qt.ShiftModifier - A Shift key on the keyboard is pressed.
    \li Qt.ControlModifier - A Ctrl key on the keyboard is pressed.
    \li Qt.AltModifier - An Alt key on the keyboard is pressed.
    \li Qt.MetaModifier - A Meta key on the keyboard is pressed.
    \li Qt.KeypadModifier - A keypad button is pressed.
    \endlist

    For example, to react to a Control key pressed during the wheel event:
    \qml
    MouseArea {
        onWheel: {
            if (wheel.modifiers & Qt.ControlModifier) {
                adjustZoom(wheel.angleDelta.y / 120);
            }
        }
    }
    \endqml
*/

/*!
    \qmlproperty int QtQuick::WheelEvent::inverted

    Returns whether the delta values delivered with the event are inverted.

    Normally, a vertical wheel will produce a WheelEvent with positive delta
    values if the top of the wheel is rotating away from the hand operating it.
    Similarly, a horizontal wheel movement will produce a QWheelEvent with
    positive delta values if the top of the wheel is moved to the left.

    However, on some platforms this is configurable, so that the same
    operations described above will produce negative delta values (but with the
    same magnitude). For instance, in a QML component (such as a tumbler or a
    slider) where it is appropriate to synchronize the movement or rotation of
    an item with the direction of the wheel, regardless of the system settings,
    the wheel event handler can use the inverted property to decide whether to
    negate the angleDelta or pixelDelta values.

    \note Many platforms provide no such information. On such platforms
    \l inverted always returns false.
*/

typedef QHash<QTouchDevice *, QQuickPointerDevice *> PointerDeviceForTouchDeviceHash;
Q_GLOBAL_STATIC(PointerDeviceForTouchDeviceHash, g_touchDevices)

Q_GLOBAL_STATIC_WITH_ARGS(QQuickPointerDevice, g_genericMouseDevice,
                            (QQuickPointerDevice::Mouse,
                             QQuickPointerDevice::GenericPointer,
                             QQuickPointerDevice::Position | QQuickPointerDevice::Scroll | QQuickPointerDevice::Hover,
                             1, 3, QLatin1String("core pointer"), 0))

typedef QHash<qint64, QQuickPointerDevice *> PointerDeviceForDeviceIdHash;
Q_GLOBAL_STATIC(PointerDeviceForDeviceIdHash, g_tabletDevices)

QQuickPointerDevice *QQuickPointerDevice::touchDevice(QTouchDevice *d)
{
    if (g_touchDevices->contains(d))
        return g_touchDevices->value(d);

    QQuickPointerDevice::DeviceType type = QQuickPointerDevice::TouchScreen;
    QString name;
    int maximumTouchPoints = 10;
    QQuickPointerDevice::Capabilities caps = QQuickPointerDevice::Capabilities(QTouchDevice::Position);
    if (d) {
        QQuickPointerDevice::Capabilities caps =
            static_cast<QQuickPointerDevice::Capabilities>(static_cast<int>(d->capabilities()) & 0x0F);
        if (d->type() == QTouchDevice::TouchPad) {
            type = QQuickPointerDevice::TouchPad;
            caps |= QQuickPointerDevice::Scroll;
        }
        name = d->name();
        maximumTouchPoints = d->maximumTouchPoints();
    } else {
        qWarning() << "QQuickWindowPrivate::touchDevice: creating touch device from nullptr device in QTouchEvent";
    }

    QQuickPointerDevice *dev = new QQuickPointerDevice(type, QQuickPointerDevice::Finger,
        caps, maximumTouchPoints, 0, name, 0);
    g_touchDevices->insert(d, dev);
    return dev;
}

QList<QQuickPointerDevice*> QQuickPointerDevice::touchDevices()
{
    return g_touchDevices->values();
}

QQuickPointerDevice *QQuickPointerDevice::genericMouseDevice()
{
    return g_genericMouseDevice;
}

QQuickPointerDevice *QQuickPointerDevice::tabletDevice(qint64 id)
{
    auto it = g_tabletDevices->find(id);
    if (it != g_tabletDevices->end())
        return it.value();

    // ### Figure out how to populate the tablet devices
    return nullptr;
}

void QQuickEventPoint::reset(Qt::TouchPointState state, QPointF scenePos, quint64 pointId, ulong timestamp)
{
    m_scenePos = scenePos;
    m_pointId = pointId;
    m_valid = true;
    m_accept = false;
    m_state = static_cast<QQuickEventPoint::State>(state);
    m_timestamp = timestamp;
    if (state == Qt::TouchPointPressed)
        m_pressTimestamp = timestamp;
    // TODO calculate velocity
}

QQuickItem *QQuickEventPoint::grabber() const
{
    return m_grabber.data();
}

void QQuickEventPoint::setGrabber(QQuickItem *grabber)
{
    if (Q_UNLIKELY(lcPointerGrab().isDebugEnabled()) && m_grabber.data() != grabber) {
        auto device = static_cast<const QQuickPointerEvent *>(parent())->device();
        static const QMetaEnum stateMetaEnum = metaObject()->enumerator(metaObject()->indexOfEnumerator("State"));
        QString deviceName = (device ? device->name() : QLatin1String("null device"));
        deviceName.resize(16, ' '); // shorten, and align in case of sequential output
        qCDebug(lcPointerGrab) << deviceName << "point" << hex << m_pointId << stateMetaEnum.valueToKey(state())
                               << ": grab" << m_grabber << "->" << grabber;
    }
    m_grabber = QPointer<QQuickItem>(grabber);
}

void QQuickEventPoint::setAccepted(bool accepted)
{
    if (m_accept != accepted) {
        qCDebug(lcPointerEvents) << this << m_accept << "->" << accepted;
        m_accept = accepted;
    }
}

QQuickEventTouchPoint::QQuickEventTouchPoint(QQuickPointerTouchEvent *parent)
    : QQuickEventPoint(parent), m_rotation(0), m_pressure(0)
{}

void QQuickEventTouchPoint::reset(const QTouchEvent::TouchPoint &tp, ulong timestamp)
{
    QQuickEventPoint::reset(tp.state(), tp.scenePos(), tp.id(), timestamp);
    m_rotation = tp.rotation();
    m_pressure = tp.pressure();
    m_uniqueId = tp.uniqueId();
}

/*!
    \internal
    \class QQuickPointerEvent

    QQuickPointerEvent is used as a long-lived object to store data related to
    an event from a pointing device, such as a mouse, touch or tablet event,
    during event delivery. It also provides properties which may be used later
    to expose the event to QML, the same as is done with QQuickMouseEvent,
    QQuickTouchPoint, QQuickKeyEvent, etc. Since only one event can be
    delivered at a time, this class is effectively a singleton.  We don't worry
    about the QObject overhead because the instances are long-lived: we don't
    dynamically create and destroy objects of this type for each event.
*/

QQuickPointerEvent::~QQuickPointerEvent()
{}

QQuickPointerEvent *QQuickPointerMouseEvent::reset(QEvent *event)
{
    auto ev = static_cast<QMouseEvent*>(event);
    m_event = ev;
    if (!event)
        return this;

    m_device = QQuickPointerDevice::genericMouseDevice();
    m_button = ev->button();
    m_pressedButtons = ev->buttons();
    Qt::TouchPointState state = Qt::TouchPointStationary;
    switch (ev->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
        state = Qt::TouchPointPressed;
        break;
    case QEvent::MouseButtonRelease:
        state = Qt::TouchPointReleased;
        break;
    case QEvent::MouseMove:
        state = Qt::TouchPointMoved;
        break;
    default:
        break;
    }
    m_mousePoint->reset(state, ev->windowPos(), 0, ev->timestamp());  // mouse is 0
    return this;
}

QQuickPointerEvent *QQuickPointerTouchEvent::reset(QEvent *event)
{
    auto ev = static_cast<QTouchEvent*>(event);
    m_event = ev;
    if (!event)
        return this;

    m_device = QQuickPointerDevice::touchDevice(ev->device());
    m_button = Qt::NoButton;
    m_pressedButtons = Qt::NoButton;

    const QList<QTouchEvent::TouchPoint> &tps = ev->touchPoints();
    int newPointCount = tps.count();
    m_touchPoints.reserve(newPointCount);

    for (int i = m_touchPoints.size(); i < newPointCount; ++i)
        m_touchPoints.insert(i, new QQuickEventTouchPoint(this));

    // Make sure the grabbers are right from one event to the next
    QVector<QQuickItem*> grabbers;
    // Copy all grabbers, because the order of points might have changed in the event.
    // The ID is all that we can rely on (release might remove the first point etc).
    for (int i = 0; i < newPointCount; ++i) {
        QQuickItem *grabber = nullptr;
        if (auto point = pointById(tps.at(i).id()))
            grabber = point->grabber();
        grabbers.append(grabber);
    }

    for (int i = 0; i < newPointCount; ++i) {
        auto point = m_touchPoints.at(i);
        point->reset(tps.at(i), ev->timestamp());
        if (point->state() == QQuickEventPoint::Pressed) {
            if (grabbers.at(i))
                qWarning() << "TouchPointPressed without previous release event" << point;
            point->setGrabber(nullptr);
        } else {
            point->setGrabber(grabbers.at(i));
        }
    }
    m_pointCount = newPointCount;
    return this;
}

QQuickEventPoint *QQuickPointerMouseEvent::point(int i) const {
    if (i == 0)
        return m_mousePoint;
    return nullptr;
}

QQuickEventPoint *QQuickPointerTouchEvent::point(int i) const {
    if (i >= 0 && i < m_pointCount)
        return m_touchPoints.at(i);
    return nullptr;
}

QQuickEventPoint::QQuickEventPoint(QQuickPointerEvent *parent)
  : QObject(parent), m_pointId(0), m_grabber(nullptr), m_timestamp(0), m_pressTimestamp(0),
    m_state(QQuickEventPoint::Released), m_valid(false), m_accept(false)
{
    Q_UNUSED(m_reserved);
}

QQuickPointerEvent *QQuickEventPoint::pointerEvent() const
{
    return static_cast<QQuickPointerEvent *>(parent());
}

bool QQuickPointerMouseEvent::allPointsAccepted() const {
    return m_mousePoint->isAccepted();
}

QMouseEvent *QQuickPointerMouseEvent::asMouseEvent(const QPointF &localPos) const
{
    auto event = static_cast<QMouseEvent *>(m_event);
    event->setLocalPos(localPos);
    return event;
}

QVector<QQuickItem *> QQuickPointerMouseEvent::grabbers() const
{
    QVector<QQuickItem *> result;
    if (QQuickItem *grabber = m_mousePoint->grabber())
        result << grabber;
    return result;
}

void QQuickPointerMouseEvent::clearGrabbers() const {
    m_mousePoint->setGrabber(nullptr);
}

bool QQuickPointerMouseEvent::isPressEvent() const
{
    auto me = static_cast<QMouseEvent*>(m_event);
    return ((me->type() == QEvent::MouseButtonPress || me->type() == QEvent::MouseButtonDblClick) &&
            (me->buttons() & me->button()) == me->buttons());
}

bool QQuickPointerTouchEvent::allPointsAccepted() const {
    for (int i = 0; i < m_pointCount; ++i) {
        if (!m_touchPoints.at(i)->isAccepted())
            return false;
    }
    return true;
}

QVector<QQuickItem *> QQuickPointerTouchEvent::grabbers() const
{
    QVector<QQuickItem *> result;
    for (int i = 0; i < m_pointCount; ++i) {
        auto point = m_touchPoints.at(i);
        if (QQuickItem *grabber = point->grabber()) {
            if (!result.contains(grabber))
                result << grabber;
        }
    }
    return result;
}

void QQuickPointerTouchEvent::clearGrabbers() const {
    for (auto point: m_touchPoints)
        point->setGrabber(nullptr);
}

bool QQuickPointerTouchEvent::isPressEvent() const
{
    return static_cast<QTouchEvent*>(m_event)->touchPointStates() & Qt::TouchPointPressed;
}

QVector<QPointF> QQuickPointerEvent::unacceptedPressedPointScenePositions() const
{
    QVector<QPointF> points;
    for (int i = 0; i < pointCount(); ++i) {
        if (!point(i)->isAccepted() && point(i)->state() == QQuickEventPoint::Pressed)
            points << point(i)->scenePos();
    }
    return points;
}

/*!
    \internal
    Populate the reusable synth-mouse event from one touchpoint.
    It's required that isTouchEvent() be true when this is called.
    If the touchpoint cannot be found, this returns nullptr.
    Ownership of the event is NOT transferred to the caller.
*/
QMouseEvent *QQuickPointerTouchEvent::syntheticMouseEvent(int pointID, QQuickItem *relativeTo) const {
    const QTouchEvent::TouchPoint *p = touchPointById(pointID);
    if (!p)
        return nullptr;
    QEvent::Type type;
    Qt::MouseButton buttons = Qt::LeftButton;
    switch (p->state()) {
    case Qt::TouchPointPressed:
        type = QEvent::MouseButtonPress;
        break;
    case Qt::TouchPointMoved:
    case Qt::TouchPointStationary:
        type = QEvent::MouseMove;
        break;
    case Qt::TouchPointReleased:
        type = QEvent::MouseButtonRelease;
        buttons = Qt::NoButton;
        break;
    default:
        Q_ASSERT(false);
        return nullptr;
    }
    m_synthMouseEvent = QMouseEvent(type, relativeTo->mapFromScene(p->scenePos()),
        p->scenePos(), p->screenPos(), Qt::LeftButton, buttons, m_event->modifiers());
    m_synthMouseEvent.setAccepted(true);
    m_synthMouseEvent.setTimestamp(m_event->timestamp());
    // In the future we will try to always have valid velocity in every QQuickEventPoint.
    // QQuickFlickablePrivate::handleMouseMoveEvent() checks for QTouchDevice::Velocity
    // and if it is set, then it does not need to do its own velocity calculations.
    // That's probably the only usecase for this, so far.  Some day Flickable should handle
    // pointer events, and then passing touchpoint velocity via QMouseEvent will be obsolete.
    // Conveniently (by design), QTouchDevice::Velocity == QQuickPointerDevice.Velocity
    // so that we don't need to convert m_device->capabilities().
    if (m_device)
        QGuiApplicationPrivate::setMouseEventCapsAndVelocity(&m_synthMouseEvent, m_device->capabilities(), p->velocity());
    QGuiApplicationPrivate::setMouseEventSource(&m_synthMouseEvent, Qt::MouseEventSynthesizedByQt);
    return &m_synthMouseEvent;
}

/*!
    \internal
    Returns a pointer to the QQuickEventPoint which has the \a pointId as
    \l {QQuickEventPoint::pointId}{pointId}.
    Returns nullptr if there is no point with that ID.

    \fn QQuickPointerEvent::pointById(quint64 pointId) const
*/
QQuickEventPoint *QQuickPointerMouseEvent::pointById(quint64 pointId) const {
    if (m_mousePoint && pointId == m_mousePoint->pointId())
        return m_mousePoint;
    return nullptr;
}

QQuickEventPoint *QQuickPointerTouchEvent::pointById(quint64 pointId) const {
    auto it = std::find_if(m_touchPoints.constBegin(), m_touchPoints.constEnd(),
        [pointId](const QQuickEventTouchPoint *tp) { return tp->pointId() == pointId; } );
    if (it != m_touchPoints.constEnd())
        return *it;
    return nullptr;
}


/*!
    \internal
    Returns a pointer to the original TouchPoint which has the same
    \l {QTouchEvent::TouchPoint::id}{id} as \a pointId, if the original event is a
    QTouchEvent, and if that point is found. Otherwise, returns nullptr.
*/
const QTouchEvent::TouchPoint *QQuickPointerTouchEvent::touchPointById(int pointId) const {
    const QTouchEvent *ev = asTouchEvent();
    if (!ev)
        return nullptr;
    const QList<QTouchEvent::TouchPoint> &tps = ev->touchPoints();
    auto it = std::find_if(tps.constBegin(), tps.constEnd(),
        [pointId](QTouchEvent::TouchPoint const& tp) { return tp.id() == pointId; } );
    // return the pointer to the actual TP in QTouchEvent::_touchPoints
    return (it == tps.constEnd() ? nullptr : it.operator->());
}

/*!
    \internal
    Make a new QTouchEvent, giving it a subset of the original touch points.

    Returns a nullptr if all points are stationary or there are no points inside the item.
*/
QTouchEvent *QQuickPointerTouchEvent::touchEventForItem(QQuickItem *item, bool isFiltering) const
{
    QList<QTouchEvent::TouchPoint> touchPoints;
    Qt::TouchPointStates eventStates;
    // TODO maybe add QQuickItem::mapVector2DFromScene(QVector2D) to avoid needing QQuickItemPrivate here
    // Or else just document that velocity is always scene-relative and is not scaled and rotated with the item
    // but that would require changing tst_qquickwindow::touchEvent_velocity(): it expects transformed velocity

    QMatrix4x4 transformMatrix(QQuickItemPrivate::get(item)->windowToItemTransform());
    for (int i = 0; i < m_pointCount; ++i) {
        auto p = m_touchPoints.at(i);
        if (p->isAccepted())
            continue;
        bool isGrabber = p->grabber() == item;
        bool isPressInside = p->state() == QQuickEventPoint::Pressed && item->contains(item->mapFromScene(p->scenePos()));
        if (!(isGrabber || isPressInside || isFiltering))
            continue;

        const QTouchEvent::TouchPoint *tp = touchPointById(p->pointId());
        if (tp) {
            eventStates |= tp->state();
            QTouchEvent::TouchPoint tpCopy = *tp;
            tpCopy.setPos(item->mapFromScene(tpCopy.scenePos()));
            tpCopy.setLastPos(item->mapFromScene(tpCopy.lastScenePos()));
            tpCopy.setStartPos(item->mapFromScene(tpCopy.startScenePos()));
            tpCopy.setRect(item->mapRectFromScene(tpCopy.sceneRect()));
            tpCopy.setVelocity(transformMatrix.mapVector(tpCopy.velocity()).toVector2D());
            touchPoints << tpCopy;
        }
    }

    if (eventStates == Qt::TouchPointStationary || touchPoints.isEmpty())
        return nullptr;

    // if all points have the same state, set the event type accordingly
    const QTouchEvent &event = *asTouchEvent();
    QEvent::Type eventType = event.type();
    switch (eventStates) {
    case Qt::TouchPointPressed:
        eventType = QEvent::TouchBegin;
        break;
    case Qt::TouchPointReleased:
        eventType = QEvent::TouchEnd;
        break;
    default:
        eventType = QEvent::TouchUpdate;
        break;
    }

    QTouchEvent *touchEvent = new QTouchEvent(eventType);
    touchEvent->setWindow(event.window());
    touchEvent->setTarget(item);
    touchEvent->setDevice(event.device());
    touchEvent->setModifiers(event.modifiers());
    touchEvent->setTouchPoints(touchPoints);
    touchEvent->setTouchPointStates(eventStates);
    touchEvent->setTimestamp(event.timestamp());
    touchEvent->accept();
    return touchEvent;
}

QTouchEvent *QQuickPointerTouchEvent::asTouchEvent() const
{
    return static_cast<QTouchEvent *>(m_event);
}

#ifndef QT_NO_DEBUG_STREAM

Q_QUICK_PRIVATE_EXPORT QDebug operator<<(QDebug dbg, const QQuickPointerDevice *dev) {
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    if (!dev) {
        dbg << "QQuickPointerDevice(0)";
        return dbg;
    }
    dbg << "QQuickPointerDevice("<< dev->name() << ' ';
    QtDebugUtils::formatQEnum(dbg, dev->type());
    dbg << ' ';
    QtDebugUtils::formatQEnum(dbg, dev->pointerType());
    dbg << " caps:";
    QtDebugUtils::formatQFlags(dbg, dev->capabilities());
    if (dev->type() == QQuickPointerDevice::TouchScreen ||
            dev->type() == QQuickPointerDevice::TouchPad)
        dbg << " maxTouchPoints:" << dev->maximumTouchPoints();
    else
        dbg << " buttonCount:" << dev->buttonCount();
    dbg << ')';
    return dbg;
}

Q_QUICK_PRIVATE_EXPORT QDebug operator<<(QDebug dbg, const QQuickPointerEvent *event) {
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "QQuickPointerEvent(dev:";
    QtDebugUtils::formatQEnum(dbg, event->device()->type());
    if (event->buttons() != Qt::NoButton) {
        dbg << " buttons:";
        QtDebugUtils::formatQEnum(dbg, event->buttons());
    }
    dbg << " [";
    int c = event->pointCount();
    for (int i = 0; i < c; ++i)
        dbg << event->point(i) << ' ';
    dbg << "])";
    return dbg;
}

Q_QUICK_PRIVATE_EXPORT QDebug operator<<(QDebug dbg, const QQuickEventPoint *event) {
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "QQuickEventPoint(valid:" << event->isValid() << " accepted:" << event->isAccepted()
        << " state:";
    QtDebugUtils::formatQEnum(dbg, event->state());
    dbg << " scenePos:" << event->scenePos() << " id:" << event->pointId()
        << " timeHeld:" << event->timeHeld() << ')';
    return dbg;
}

#endif

QT_END_NAMESPACE
