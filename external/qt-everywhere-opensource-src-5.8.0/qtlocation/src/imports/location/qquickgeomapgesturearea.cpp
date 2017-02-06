/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "qquickgeomapgesturearea_p.h"
#include "qdeclarativegeomap_p.h"
#include "error_messages.h"

#include <QtGui/QGuiApplication>
#include <QtGui/qevent.h>
#include <QtGui/QWheelEvent>
#include <QtGui/QStyleHints>
#include <QtQml/qqmlinfo.h>
#include <QtQuick/QQuickWindow>
#include <QPropertyAnimation>
#include <QDebug>
#include <QtPositioning/private/qgeoprojection_p.h>
#include "math.h"
#include "qgeomap_p.h"
#include "qdoublevector2d_p.h"

#define QML_MAP_FLICK_DEFAULTMAXVELOCITY 2500
#define QML_MAP_FLICK_MINIMUMDECELERATION 500
#define QML_MAP_FLICK_DEFAULTDECELERATION 2500
#define QML_MAP_FLICK_MAXIMUMDECELERATION 10000

#define QML_MAP_FLICK_VELOCITY_SAMPLE_PERIOD 50
// FlickThreshold determines how far the "mouse" must have moved
// before we perform a flick.
static const int FlickThreshold = 20;
// Really slow flicks can be annoying.
const qreal MinimumFlickVelocity = 75.0;

QT_BEGIN_NAMESPACE


/*!
    \qmltype MapPinchEvent
    \instantiates QGeoMapPinchEvent
    \inqmlmodule QtLocation

    \brief MapPinchEvent type provides basic information about pinch event.

    MapPinchEvent type provides basic information about pinch event. They are
    present in handlers of MapPinch (for example pinchStarted/pinchUpdated). Events are only
    guaranteed to be valid for the duration of the handler.

    Except for the \l accepted property, all properties are read-only.

    \section2 Example Usage

    The following example enables the pinch gesture on a map and reacts to the
    finished event.

    \code
    Map {
        id: map
        gesture.enabled: true
        gesture.onPinchFinished:{
            var coordinate1 = map.toCoordinate(gesture.point1)
            var coordinate2 = map.toCoordinate(gesture.point2)
            console.log("Pinch started at:")
            console.log("        Points (" + gesture.point1.x + ", " + gesture.point1.y + ") - (" + gesture.point2.x + ", " + gesture.point2.y + ")")
            console.log("   Coordinates (" + coordinate1.latitude + ", " + coordinate1.longitude + ") - (" + coordinate2.latitude + ", " + coordinate2.longitude + ")")
        }
    }
    \endcode

    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.0
*/

/*!
    \qmlproperty QPoint QtLocation::MapPinchEvent::center

    This read-only property holds the current center point.
*/

/*!
    \qmlproperty real QtLocation::MapPinchEvent::angle

    This read-only property holds the current angle between the two points in
    the range -180 to 180. Positive values for the angles mean counter-clockwise
    while negative values mean the clockwise direction. Zero degrees is at the
    3 o'clock position.
*/

/*!
    \qmlproperty QPoint QtLocation::MapPinchEvent::point1
    \qmlproperty QPoint QtLocation::MapPinchEvent::point2

    These read-only properties hold the actual touch points generating the pinch.
    The points are not in any particular order.
*/

/*!
    \qmlproperty int QtLocation::MapPinchEvent::pointCount

    This read-only property holds the number of points currently touched.
    The MapPinch will not react until two touch points have initiated a gesture,
    but will remain active until all touch points have been released.
*/

/*!
    \qmlproperty bool QtLocation::MapPinchEvent::accepted

    Setting this property to false in the \c MapPinch::onPinchStarted handler
    will result in no further pinch events being generated, and the gesture
    ignored.
*/

/*!
    \qmltype MapGestureArea
    \instantiates QQuickGeoMapGestureArea

    \inqmlmodule QtLocation

    \brief The MapGestureArea type provides Map gesture interaction.

    MapGestureArea objects are used as part of a Map, to provide for panning,
    flicking and pinch-to-zoom gesture used on touch displays.

    A MapGestureArea is automatically created with a new Map and available with
    the \l{Map::gesture}{gesture} property. This is the only way
    to create a MapGestureArea, and once created this way cannot be destroyed
    without its parent Map.

    The two most commonly used properties of the MapGestureArea are the \l enabled
    and \l acceptedGestures properties. Both of these must be set before a
    MapGestureArea will have any effect upon interaction with the Map.
    The \l flickDeceleration property controls how quickly the map pan slows after contact
    is released while panning the map.

    \section2 Performance

    The MapGestureArea, when enabled, must process all incoming touch events in
    order to track the shape and size of the "pinch". The overhead added on
    touch events can be considered constant time.

    \section2 Example Usage

    The following example enables the pinch and pan gestures on the map, but not flicking. So the
    map scrolling will halt immediately on releasing the mouse button / touch.

    \code
    Map {
        gesture.enabled: true
        gesture.acceptedGestures: MapGestureArea.PinchGesture | MapGestureArea.PanGesture
    }
    \endcode

    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.0
*/

/*!
    \qmlproperty bool QtLocation::MapGestureArea::enabled

    This property holds whether the gestures are enabled.
*/

/*!
    \qmlproperty bool QtLocation::MapGestureArea::pinchActive

    This read-only property holds whether pinch gesture is active.
*/

/*!
    \qmlproperty bool QtLocation::MapGestureArea::panActive

    This read-only property holds whether pan gesture is active.

    \note Change notifications for this property were introduced in Qt 5.5.
*/

/*!
    \qmlproperty real QtLocation::MapGestureArea::maximumZoomLevelChange

    This property holds the maximum zoom level change per pinch, essentially
    meant to be used for setting the zoom sensitivity.

    It is an indicative measure calculated from the dimensions of the
    map area, roughly corresponding how much zoom level could change with
    maximum pinch zoom. Default value is 4.0, maximum value is 10.0
*/

/*!
    \qmlproperty real MapGestureArea::flickDeceleration

    This property holds the rate at which a flick will decelerate.

    The default value is 2500.
*/

/*!
    \qmlsignal QtLocation::MapGestureArea::pinchStarted(PinchEvent event)

    This signal is emitted when a pinch gesture is started.

    The corresponding handler is \c onPinchStarted.

    \sa pinchUpdated, pinchFinished
*/

/*!
    \qmlsignal QtLocation::MapGestureArea::pinchUpdated(PinchEvent event)

    This signal is emitted as the user's fingers move across the map,
    after the \l pinchStarted signal is emitted.

    The corresponding handler is \c onPinchUpdated.

    \sa pinchStarted, pinchFinished
*/

/*!
    \qmlsignal QtLocation::MapGestureArea::pinchFinished(PinchEvent event)

    This signal is emitted at the end of a pinch gesture.

    The corresponding handler is \c onPinchFinished.

    \sa pinchStarted, pinchUpdated
*/

/*!
    \qmlsignal QtLocation::MapGestureArea::panStarted()

    This signal is emitted when the map begins to move due to user
    interaction. Typically this means that the user is dragging a finger -
    or a mouse with one of more mouse buttons pressed - on the map.

    The corresponding handler is \c onPanStarted.
*/

/*!
    \qmlsignal QtLocation::MapGestureArea::panFinished()

    This signal is emitted when the map stops moving due to user
    interaction.  If a flick was generated, this signal is
    emitted before flick starts. If a flick was not
    generated, this signal is emitted when the
    user stops dragging - that is a mouse or touch release.

    The corresponding handler is \c onPanFinished.

*/

/*!
    \qmlsignal QtLocation::MapGestureArea::flickStarted()

    This signal is emitted when the map is flicked.  A flick
    starts from the point where the mouse or touch was released,
    while still in motion.

    The corresponding handler is \c onFlichStarted.
*/

/*!
    \qmlsignal QtLocation::MapGestureArea::flickFinished()

    This signal is emitted when the map stops moving due to a flick.

    The corresponding handler is \c onFlickFinished.
*/

QQuickGeoMapGestureArea::QQuickGeoMapGestureArea(QDeclarativeGeoMap *map)
    : QQuickItem(map),
      m_map(0),
      m_declarativeMap(map),
      m_enabled(true),
      m_acceptedGestures(PinchGesture | PanGesture | FlickGesture),
      m_preventStealing(false),
      m_panEnabled(true)
{
    m_flick.m_enabled = true,
    m_flick.m_maxVelocity = QML_MAP_FLICK_DEFAULTMAXVELOCITY;
    m_flick.m_deceleration = QML_MAP_FLICK_DEFAULTDECELERATION;
    m_flick.m_animation = 0;
    m_touchPointState = touchPoints0;
    m_pinchState = pinchInactive;
    m_flickState = flickInactive;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::setMap(QGeoMap *map)
{
    if (m_map || !map)
        return;

    m_map = map;
    m_flick.m_animation = new QQuickGeoCoordinateAnimation(this);
    m_flick.m_animation->setTargetObject(m_declarativeMap);
    m_flick.m_animation->setProperty(QStringLiteral("center"));
    m_flick.m_animation->setEasing(QEasingCurve(QEasingCurve::OutQuad));
    connect(m_flick.m_animation, &QQuickAbstractAnimation::stopped, this, &QQuickGeoMapGestureArea::handleFlickAnimationStopped);
}

/*!
    \qmlproperty bool QtQuick::MapGestureArea::preventStealing
    This property holds whether the mouse events may be stolen from this
    MapGestureArea.

    If a Map is placed within an item that filters child mouse
    and touch events, such as Flickable, the mouse and touch events
    may be stolen from the MapGestureArea if a gesture is recognized
    by the parent item, e.g. a flick gesture.  If preventStealing is
    set to true, no item will steal the mouse and touch events.

    Note that setting preventStealing to true once an item has started
    stealing events will have no effect until the next press event.

    By default this property is false.
*/

bool QQuickGeoMapGestureArea::preventStealing() const
{
    return m_preventStealing;
}

void QQuickGeoMapGestureArea::setPreventStealing(bool prevent)
{
    if (prevent != m_preventStealing) {
        m_preventStealing = prevent;
        m_declarativeMap->setKeepMouseGrab(m_preventStealing && m_enabled);
        m_declarativeMap->setKeepTouchGrab(m_preventStealing && m_enabled);
        emit preventStealingChanged();
    }
}

QQuickGeoMapGestureArea::~QQuickGeoMapGestureArea()
{
}

/*!
    \qmlproperty enumeration QtLocation::MapGestureArea::acceptedGestures

    This property holds the gestures that will be active. By default
    the zoom, pan and flick gestures are enabled.

    \list
    \li MapGestureArea.NoGesture - Don't support any additional gestures (value: 0x0000).
    \li MapGestureArea.PinchGesture - Support the map pinch gesture (value: 0x0001).
    \li MapGestureArea.PanGesture  - Support the map pan gesture (value: 0x0002).
    \li MapGestureArea.FlickGesture  - Support the map flick gesture (value: 0x0004).
    \endlist
*/

QQuickGeoMapGestureArea::AcceptedGestures QQuickGeoMapGestureArea::acceptedGestures() const
{
    return m_acceptedGestures;
}


void QQuickGeoMapGestureArea::setAcceptedGestures(AcceptedGestures acceptedGestures)
{
    if (acceptedGestures == m_acceptedGestures)
        return;
    m_acceptedGestures = acceptedGestures;

    setPanEnabled(acceptedGestures & PanGesture);
    setFlickEnabled(acceptedGestures & FlickGesture);
    setPinchEnabled(acceptedGestures & PinchGesture);

    emit acceptedGesturesChanged();
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::isPinchActive() const
{
    return m_pinchState == pinchActive;
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::isPanActive() const
{
    return m_flickState == panActive || m_flickState == flickActive;
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::enabled() const
{
    return m_enabled;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;

    if (enabled) {
        setPanEnabled(m_acceptedGestures & PanGesture);
        setFlickEnabled(m_acceptedGestures & FlickGesture);
        setPinchEnabled(m_acceptedGestures & PinchGesture);
    } else {
        setPanEnabled(false);
        setFlickEnabled(false);
        setPinchEnabled(false);
    }

    emit enabledChanged();
}


/*!
    \internal
*/
bool QQuickGeoMapGestureArea::pinchEnabled() const
{
    return m_pinch.m_enabled;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::setPinchEnabled(bool enabled)
{
    if (enabled == m_pinch.m_enabled)
        return;
    m_pinch.m_enabled = enabled;
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::panEnabled() const
{
    return m_panEnabled;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::setPanEnabled(bool enabled)
{
    if (enabled == m_flick.m_enabled)
        return;
    m_panEnabled = enabled;

    // unlike the pinch, the pan existing functionality is to stop immediately
    if (!enabled)
        stopPan();
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::flickEnabled() const
{
    return m_flick.m_enabled;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::setFlickEnabled(bool enabled)
{
    if (enabled == m_flick.m_enabled)
        return;
    m_flick.m_enabled = enabled;
    // unlike the pinch, the flick existing functionality is to stop immediately
    if (!enabled) {
         stopFlick();
    }
}

/*!
    \internal
    Used internally to set the minimum zoom level of the gesture area.
    The caller is responsible to only send values that are valid
    for the map plugin. Negative values are ignored.
 */
void QQuickGeoMapGestureArea::setMinimumZoomLevel(qreal min)
{
    if (min >= 0)
        m_pinch.m_zoom.m_minimum = min;
}

/*!
   \internal
 */
qreal QQuickGeoMapGestureArea::minimumZoomLevel() const
{
    return m_pinch.m_zoom.m_minimum;
}

/*!
    \internal
    Used internally to set the maximum zoom level of the gesture area.
    The caller is responsible to only send values that are valid
    for the map plugin. Negative values are ignored.
 */
void QQuickGeoMapGestureArea::setMaximumZoomLevel(qreal max)
{
    if (max >= 0)
        m_pinch.m_zoom.m_maximum = max;
}

/*!
   \internal
 */
qreal QQuickGeoMapGestureArea::maximumZoomLevel() const
{
    return m_pinch.m_zoom.m_maximum;
}

/*!
    \internal
*/
qreal QQuickGeoMapGestureArea::maximumZoomLevelChange() const
{
    return m_pinch.m_zoom.maximumChange;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::setMaximumZoomLevelChange(qreal maxChange)
{
    if (maxChange == m_pinch.m_zoom.maximumChange || maxChange < 0.1 || maxChange > 10.0)
        return;
    m_pinch.m_zoom.maximumChange = maxChange;
    emit maximumZoomLevelChangeChanged();
}

/*!
    \internal
*/
qreal QQuickGeoMapGestureArea::flickDeceleration() const
{
    return m_flick.m_deceleration;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::setFlickDeceleration(qreal deceleration)
{
    if (deceleration < QML_MAP_FLICK_MINIMUMDECELERATION)
        deceleration = QML_MAP_FLICK_MINIMUMDECELERATION;
    else if (deceleration > QML_MAP_FLICK_MAXIMUMDECELERATION)
        deceleration = QML_MAP_FLICK_MAXIMUMDECELERATION;
    if (deceleration == m_flick.m_deceleration)
        return;
    m_flick.m_deceleration = deceleration;
    emit flickDecelerationChanged();
}

/*!
    \internal
*/
QTouchEvent::TouchPoint* createTouchPointFromMouseEvent(QMouseEvent *event, Qt::TouchPointState state)
{
    // this is only partially filled. But since it is only partially used it works
    // more robust would be to store a list of QPointFs rather than TouchPoints
    QTouchEvent::TouchPoint* newPoint = new QTouchEvent::TouchPoint();
    newPoint->setPos(event->localPos());
    newPoint->setScenePos(event->windowPos());
    newPoint->setScreenPos(event->screenPos());
    newPoint->setState(state);
    newPoint->setId(0);
    return newPoint;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::handleMousePressEvent(QMouseEvent *event)
{
    m_mousePoint.reset(createTouchPointFromMouseEvent(event, Qt::TouchPointPressed));
    if (m_touchPoints.isEmpty()) update();
    event->accept();
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::handleMouseMoveEvent(QMouseEvent *event)
{
    m_mousePoint.reset(createTouchPointFromMouseEvent(event, Qt::TouchPointMoved));
    if (m_touchPoints.isEmpty()) update();
    event->accept();
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::handleMouseReleaseEvent(QMouseEvent *event)
{
    if (!m_mousePoint.isNull()) {
        //this looks super ugly , however is required in case we do not get synthesized MouseReleaseEvent
        //and we reset the point already in handleTouchUngrabEvent
        m_mousePoint.reset(createTouchPointFromMouseEvent(event, Qt::TouchPointReleased));
        if (m_touchPoints.isEmpty()) update();
    }
    event->accept();
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::handleMouseUngrabEvent()
{

    if (m_touchPoints.isEmpty() && !m_mousePoint.isNull()) {
        m_mousePoint.reset();
        update();
    } else {
        m_mousePoint.reset();
    }
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::handleTouchUngrabEvent()
{
        m_touchPoints.clear();
        //this is needed since in some cases mouse release is not delivered
        //(second touch point breaks mouse synthesized events)
        m_mousePoint.reset();
        update();
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::handleTouchEvent(QTouchEvent *event)
{
    m_touchPoints.clear();
    m_mousePoint.reset();

    for (int i = 0; i < event->touchPoints().count(); ++i) {
        auto point = event->touchPoints().at(i);
        if (point.state() != Qt::TouchPointReleased)
            m_touchPoints << point;
    }
    if (event->touchPoints().count() >= 2)
        event->accept();
    else
        event->ignore();
    update();
}

void QQuickGeoMapGestureArea::handleWheelEvent(QWheelEvent *event)
{
    if (!m_map)
        return;

    QGeoCoordinate wheelGeoPos = m_map->itemPositionToCoordinate(QDoubleVector2D(event->posF()), false);
    QPointF preZoomPoint = m_map->coordinateToItemPosition(wheelGeoPos, false).toPointF();

    double zoomLevelDelta = event->angleDelta().y() * qreal(0.001);
    m_declarativeMap->setZoomLevel(m_declarativeMap->zoomLevel() + zoomLevelDelta);
    QPointF postZoomPoint = m_map->coordinateToItemPosition(wheelGeoPos, false).toPointF();

    if (preZoomPoint != postZoomPoint)
    {
        qreal dx = postZoomPoint.x() - preZoomPoint.x();
        qreal dy = postZoomPoint.y() - preZoomPoint.y();
        QPointF mapCenterPoint(m_map->viewportWidth() / 2.0 + dx, m_map->viewportHeight() / 2.0  + dy);

        QGeoCoordinate mapCenterCoordinate = m_map->itemPositionToCoordinate(QDoubleVector2D(mapCenterPoint), false);
        m_declarativeMap->setCenter(mapCenterCoordinate);
    }
    event->accept();
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::clearTouchData()
{
    m_velocityX = 0;
    m_velocityY = 0;
    m_sceneCenter.setX(0);
    m_sceneCenter.setY(0);
    m_touchCenterCoord.setLongitude(0);
    m_touchCenterCoord.setLatitude(0);
    m_startCoord.setLongitude(0);
    m_startCoord.setLatitude(0);
}


/*!
    \internal
*/
void QQuickGeoMapGestureArea::updateVelocityList(const QPointF &pos)
{
    // Take velocity samples every sufficient period of time, used later to determine the flick
    // duration and speed (when mouse is released).
    qreal elapsed = qreal(m_lastPosTime.elapsed());

    if (elapsed >= QML_MAP_FLICK_VELOCITY_SAMPLE_PERIOD) {
        elapsed /= 1000.;
        int dyFromLastPos = pos.y() - m_lastPos.y();
        int dxFromLastPos = pos.x() - m_lastPos.x();
        m_lastPos = pos;
        m_lastPosTime.restart();
        qreal velX = qreal(dxFromLastPos) / elapsed;
        qreal velY = qreal(dyFromLastPos) / elapsed;
        m_velocityX = qBound<qreal>(-m_flick.m_maxVelocity, velX, m_flick.m_maxVelocity);
        m_velocityY = qBound<qreal>(-m_flick.m_maxVelocity, velY, m_flick.m_maxVelocity);
    }
}

/*!
    \internal
*/

bool QQuickGeoMapGestureArea::isActive() const
{
    return isPanActive() || isPinchActive();
}

/*!
    \internal
*/
// simplify the gestures by using a state-machine format (easy to move to a future state machine)
void QQuickGeoMapGestureArea::update()
{
    if (!m_map)
        return;

    // First state machine is for the number of touch points

    //combine touch with mouse event
    m_allPoints.clear();
    m_allPoints << m_touchPoints;
    if (m_allPoints.isEmpty() && !m_mousePoint.isNull())
        m_allPoints << *m_mousePoint.data();

    touchPointStateMachine();

    // Parallel state machine for pinch
    if (isPinchActive() || (m_enabled && m_pinch.m_enabled && (m_acceptedGestures & (PinchGesture))))
        pinchStateMachine();

    // Parallel state machine for pan (since you can pan at the same time as pinching)
    // The stopPan function ensures that pan stops immediately when disabled,
    // but the line below allows pan continue its current gesture if you disable
    // the whole gesture (enabled_ flag), this keeps the enabled_ consistent with the pinch
    if (isPanActive() || (m_enabled && m_flick.m_enabled && (m_acceptedGestures & (PanGesture | FlickGesture))))
        panStateMachine();
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::touchPointStateMachine()
{
    // Transitions:
    switch (m_touchPointState) {
    case touchPoints0:
        if (m_allPoints.count() == 1) {
            clearTouchData();
            startOneTouchPoint();
            m_touchPointState = touchPoints1;
        } else if (m_allPoints.count() >= 2) {
            clearTouchData();
            startTwoTouchPoints();
            m_touchPointState = touchPoints2;
        }
        break;
    case touchPoints1:
        if (m_allPoints.count() == 0) {
            m_touchPointState = touchPoints0;
        } else if (m_allPoints.count() == 2) {
            m_touchCenterCoord = m_map->itemPositionToCoordinate(QDoubleVector2D(m_sceneCenter), false);
            startTwoTouchPoints();
            m_touchPointState = touchPoints2;
        }
        break;
    case touchPoints2:
        if (m_allPoints.count() == 0) {
            m_touchPointState = touchPoints0;
        } else if (m_allPoints.count() == 1) {
            m_touchCenterCoord = m_map->itemPositionToCoordinate(QDoubleVector2D(m_sceneCenter), false);
            startOneTouchPoint();
            m_touchPointState = touchPoints1;
        }
        break;
    };

    // Update
    switch (m_touchPointState) {
    case touchPoints0:
        break; // do nothing if no touch points down
    case touchPoints1:
        updateOneTouchPoint();
        break;
    case touchPoints2:
        updateTwoTouchPoints();
        break;
    }
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::startOneTouchPoint()
{
    m_sceneStartPoint1 = mapFromScene(m_allPoints.at(0).scenePos());
    m_lastPos = m_sceneStartPoint1;
    m_lastPosTime.start();
    QGeoCoordinate startCoord = m_map->itemPositionToCoordinate(QDoubleVector2D(m_sceneStartPoint1), false);
    // ensures a smooth transition for panning
    m_startCoord.setLongitude(m_startCoord.longitude() + startCoord.longitude() -
                             m_touchCenterCoord.longitude());
    m_startCoord.setLatitude(m_startCoord.latitude() + startCoord.latitude() -
                            m_touchCenterCoord.latitude());
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::updateOneTouchPoint()
{
    m_sceneCenter = mapFromScene(m_allPoints.at(0).scenePos());
    updateVelocityList(m_sceneCenter);
}


/*!
    \internal
*/
void QQuickGeoMapGestureArea::startTwoTouchPoints()
{
    m_sceneStartPoint1 = mapFromScene(m_allPoints.at(0).scenePos());
    m_sceneStartPoint2 = mapFromScene(m_allPoints.at(1).scenePos());
    QPointF startPos = (m_sceneStartPoint1 + m_sceneStartPoint2) * 0.5;
    m_lastPos = startPos;
    m_lastPosTime.start();
    QGeoCoordinate startCoord = m_map->itemPositionToCoordinate(QDoubleVector2D(startPos), false);
    m_startCoord.setLongitude(m_startCoord.longitude() + startCoord.longitude() -
                             m_touchCenterCoord.longitude());
    m_startCoord.setLatitude(m_startCoord.latitude() + startCoord.latitude() -
                            m_touchCenterCoord.latitude());
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::updateTwoTouchPoints()
{
    QPointF p1 = mapFromScene(m_allPoints.at(0).scenePos());
    QPointF p2 = mapFromScene(m_allPoints.at(1).scenePos());
    qreal dx = p1.x() - p2.x();
    qreal dy = p1.y() - p2.y();
    m_distanceBetweenTouchPoints = sqrt(dx * dx + dy * dy);
    m_sceneCenter = (p1 + p2) / 2;
    updateVelocityList(m_sceneCenter);

    m_twoTouchAngle = QLineF(p1, p2).angle();
    if (m_twoTouchAngle > 180)
        m_twoTouchAngle -= 360;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::pinchStateMachine()
{
    PinchState lastState = m_pinchState;
    // Transitions:
    switch (m_pinchState) {
    case pinchInactive:
        if (m_allPoints.count() >= 2) {
            if (canStartPinch()) {
                m_declarativeMap->setKeepMouseGrab(true);
                m_declarativeMap->setKeepTouchGrab(true);
                startPinch();
                m_pinchState = pinchActive;
            } else {
                m_pinchState = pinchInactiveTwoPoints;
            }
        }
        break;
    case pinchInactiveTwoPoints:
        if (m_allPoints.count() <= 1) {
            m_pinchState = pinchInactive;
        } else {
            if (canStartPinch()) {
                m_declarativeMap->setKeepMouseGrab(true);
                m_declarativeMap->setKeepTouchGrab(true);
                startPinch();
                m_pinchState = pinchActive;
            }
        }
        break;
    case pinchActive:
        if (m_allPoints.count() <= 1) {
            m_pinchState = pinchInactive;
            m_declarativeMap->setKeepMouseGrab(m_preventStealing);
            m_declarativeMap->setKeepTouchGrab(m_preventStealing);
            endPinch();
        }
        break;
    }
    // This line implements an exclusive state machine, where the transitions and updates don't
    // happen on the same frame
    if (m_pinchState != lastState) {
        emit pinchActiveChanged();
        return;
    }

    // Update
    switch (m_pinchState) {
    case pinchInactive:
    case pinchInactiveTwoPoints:
        break; // do nothing
    case pinchActive:
        updatePinch();
        break;
    }
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::canStartPinch()
{
    const int startDragDistance = qApp->styleHints()->startDragDistance();

    if (m_allPoints.count() >= 2) {
        QPointF p1 = mapFromScene(m_allPoints.at(0).scenePos());
        QPointF p2 = mapFromScene(m_allPoints.at(1).scenePos());
        if (qAbs(p1.x()-m_sceneStartPoint1.x()) > startDragDistance
         || qAbs(p1.y()-m_sceneStartPoint1.y()) > startDragDistance
         || qAbs(p2.x()-m_sceneStartPoint2.x()) > startDragDistance
         || qAbs(p2.y()-m_sceneStartPoint2.y()) > startDragDistance) {
            m_pinch.m_event.setCenter(mapFromScene(m_sceneCenter));
            m_pinch.m_event.setAngle(m_twoTouchAngle);
            m_pinch.m_event.setPoint1(p1);
            m_pinch.m_event.setPoint2(p2);
            m_pinch.m_event.setPointCount(m_allPoints.count());
            m_pinch.m_event.setAccepted(true);
            emit pinchStarted(&m_pinch.m_event);
            return m_pinch.m_event.accepted();
        }
    }
    return false;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::startPinch()
{
    m_pinch.m_startDist = m_distanceBetweenTouchPoints;
    m_pinch.m_zoom.m_previous = m_declarativeMap->zoomLevel();
    m_pinch.m_lastAngle = m_twoTouchAngle;

    m_pinch.m_lastPoint1 = mapFromScene(m_allPoints.at(0).scenePos());
    m_pinch.m_lastPoint2 = mapFromScene(m_allPoints.at(1).scenePos());

    m_pinch.m_zoom.m_start = m_declarativeMap->zoomLevel();
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::updatePinch()
{
    // Calculate the new zoom level if we have distance ( >= 2 touchpoints), otherwise stick with old.
    qreal newZoomLevel = m_pinch.m_zoom.m_previous;
    if (m_distanceBetweenTouchPoints) {
        newZoomLevel =
                // How much further/closer the current touchpoints are (in pixels) compared to pinch start
                ((m_distanceBetweenTouchPoints - m_pinch.m_startDist)  *
                 //  How much one pixel corresponds in units of zoomlevel (and multiply by above delta)
                 (m_pinch.m_zoom.maximumChange / ((width() + height()) / 2))) +
                // Add to starting zoom level. Sign of (dist-pinchstartdist) takes care of zoom in / out
                m_pinch.m_zoom.m_start;
    }
    m_pinch.m_event.setCenter(mapFromScene(m_sceneCenter));
    m_pinch.m_event.setAngle(m_twoTouchAngle);

    m_pinch.m_lastPoint1 = mapFromScene(m_allPoints.at(0).scenePos());
    m_pinch.m_lastPoint2 = mapFromScene(m_allPoints.at(1).scenePos());
    m_pinch.m_event.setPoint1(m_pinch.m_lastPoint1);
    m_pinch.m_event.setPoint2(m_pinch.m_lastPoint2);
    m_pinch.m_event.setPointCount(m_allPoints.count());
    m_pinch.m_event.setAccepted(true);

    m_pinch.m_lastAngle = m_twoTouchAngle;
    emit pinchUpdated(&m_pinch.m_event);

    if (m_acceptedGestures & PinchGesture) {
        // Take maximum and minimumzoomlevel into account
        qreal perPinchMinimumZoomLevel = qMax(m_pinch.m_zoom.m_start - m_pinch.m_zoom.maximumChange, m_pinch.m_zoom.m_minimum);
        qreal perPinchMaximumZoomLevel = qMin(m_pinch.m_zoom.m_start + m_pinch.m_zoom.maximumChange, m_pinch.m_zoom.m_maximum);
        newZoomLevel = qMin(qMax(perPinchMinimumZoomLevel, newZoomLevel), perPinchMaximumZoomLevel);
        m_declarativeMap->setZoomLevel(newZoomLevel);
        m_pinch.m_zoom.m_previous = newZoomLevel;
    }
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::endPinch()
{
    QPointF p1 = mapFromScene(m_pinch.m_lastPoint1);
    QPointF p2 = mapFromScene(m_pinch.m_lastPoint2);
    m_pinch.m_event.setCenter((p1 + p2) / 2);
    m_pinch.m_event.setAngle(m_pinch.m_lastAngle);
    m_pinch.m_event.setPoint1(p1);
    m_pinch.m_event.setPoint2(p2);
    m_pinch.m_event.setAccepted(true);
    m_pinch.m_event.setPointCount(0);
    emit pinchFinished(&m_pinch.m_event);
    m_pinch.m_startDist = 0;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::panStateMachine()
{
    FlickState lastState = m_flickState;

    // Transitions
    switch (m_flickState) {
    case flickInactive:
        if (canStartPan()) {
            // Update startCoord_ to ensure smooth start for panning when going over startDragDistance
            QGeoCoordinate newStartCoord = m_map->itemPositionToCoordinate(QDoubleVector2D(m_sceneCenter), false);
            m_startCoord.setLongitude(newStartCoord.longitude());
            m_startCoord.setLatitude(newStartCoord.latitude());
            m_declarativeMap->setKeepMouseGrab(true);
            m_flickState = panActive;
        }
        break;
    case panActive:
        if (m_allPoints.count() == 0) {
            if (!tryStartFlick())
            {
                m_flickState = flickInactive;
                // mark as inactive for use by camera
                if (m_pinchState == pinchInactive) {
                    m_declarativeMap->setKeepMouseGrab(m_preventStealing);
                    m_map->prefetchData();
                }
                emit panFinished();
            } else {
                m_flickState = flickActive;
                emit panFinished();
                emit flickStarted();
            }
        }
        break;
    case flickActive:
        if (m_allPoints.count() > 0) { // re touched before movement ended
            stopFlick();
            m_declarativeMap->setKeepMouseGrab(true);
            m_flickState = panActive;
        }
        break;
    }

    if (m_flickState != lastState)
        emit panActiveChanged();

    // Update
    switch (m_flickState) {
    case flickInactive: // do nothing
        break;
    case panActive:
        updatePan();
        // this ensures 'panStarted' occurs after the pan has actually started
        if (lastState != panActive)
            emit panStarted();
        break;
    case flickActive:
        break;
    }
}
/*!
    \internal
*/
bool QQuickGeoMapGestureArea::canStartPan()
{
    if (m_allPoints.count() == 0 || (m_acceptedGestures & PanGesture) == 0)
        return false;

    // Check if thresholds for normal panning are met.
    // (normal panning vs flicking: flicking will start from mouse release event).
    const int startDragDistance = qApp->styleHints()->startDragDistance() * 2;
    QPointF p1 = mapFromScene(m_allPoints.at(0).scenePos());
    int dyFromPress = int(p1.y() - m_sceneStartPoint1.y());
    int dxFromPress = int(p1.x() - m_sceneStartPoint1.x());
    if ((qAbs(dyFromPress) >= startDragDistance || qAbs(dxFromPress) >= startDragDistance))
        return true;
    return false;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::updatePan()
{
    QPointF startPoint = m_map->coordinateToItemPosition(m_startCoord, false).toPointF();
    int dx = static_cast<int>(m_sceneCenter.x() - startPoint.x());
    int dy = static_cast<int>(m_sceneCenter.y() - startPoint.y());
    QPointF mapCenterPoint;
    mapCenterPoint.setY(m_map->viewportHeight() / 2.0  - dy);
    mapCenterPoint.setX(m_map->viewportWidth() / 2.0 - dx);
    QGeoCoordinate animationStartCoordinate = m_map->itemPositionToCoordinate(QDoubleVector2D(mapCenterPoint), false);
    m_declarativeMap->setCenter(animationStartCoordinate);
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::tryStartFlick()
{
    if ((m_acceptedGestures & FlickGesture) == 0)
        return false;
    // if we drag then pause before release we should not cause a flick.
    qreal velocityX = 0.0;
    qreal velocityY = 0.0;
    if (m_lastPosTime.elapsed() < QML_MAP_FLICK_VELOCITY_SAMPLE_PERIOD) {
        velocityY = m_velocityY;
        velocityX = m_velocityX;
    }
    int flickTimeY = 0;
    int flickTimeX = 0;
    int flickPixelsX = 0;
    int flickPixelsY = 0;
    if (qAbs(velocityY) > MinimumFlickVelocity && qAbs(m_sceneCenter.y() - m_sceneStartPoint1.y()) > FlickThreshold) {
        // calculate Y flick animation values
        qreal acceleration = m_flick.m_deceleration;
        if ((velocityY > 0.0f) == (m_flick.m_deceleration > 0.0f))
            acceleration = acceleration * -1.0f;
        flickTimeY = static_cast<int>(-1000 * velocityY / acceleration);
        flickPixelsY = (flickTimeY * velocityY) / (1000.0 * 2);
    }
    if (qAbs(velocityX) > MinimumFlickVelocity && qAbs(m_sceneCenter.x() - m_sceneStartPoint1.x()) > FlickThreshold) {
        // calculate X flick animation values
        qreal acceleration = m_flick.m_deceleration;
        if ((velocityX > 0.0f) == (m_flick.m_deceleration > 0.0f))
            acceleration = acceleration * -1.0f;
        flickTimeX = static_cast<int>(-1000 * velocityX / acceleration);
        flickPixelsX = (flickTimeX * velocityX) / (1000.0 * 2);
    }
    int flickTime = qMax(flickTimeY, flickTimeX);
    if (flickTime > 0) {
        startFlick(flickPixelsX, flickPixelsY, flickTime);
        return true;
    }
    return false;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::startFlick(int dx, int dy, int timeMs)
{
    if (!m_flick.m_animation)
        return;
    if (timeMs < 0)
        return;

    QGeoCoordinate animationStartCoordinate = m_declarativeMap->center();

    if (m_flick.m_animation->isRunning())
        m_flick.m_animation->stop();
    QGeoCoordinate animationEndCoordinate = m_declarativeMap->center();
    m_flick.m_animation->setDuration(timeMs);

    double zoom = pow(2.0, m_declarativeMap->zoomLevel());
    double longitude = animationStartCoordinate.longitude() - (dx / zoom);
    double latitude = animationStartCoordinate.latitude() + (dy / zoom);

    if (dx > 0)
        m_flick.m_animation->setDirection(QQuickGeoCoordinateAnimation::East);
    else
        m_flick.m_animation->setDirection(QQuickGeoCoordinateAnimation::West);

    //keep animation in correct bounds
    if (latitude > 85.05113)
        latitude = 85.05113;
    else if (latitude < -85.05113)
        latitude = -85.05113;

    if (longitude > 180)
        longitude = longitude - 360;
    else if (longitude < -180)
        longitude = longitude + 360;

    animationEndCoordinate.setLongitude(longitude);
    animationEndCoordinate.setLatitude(latitude);

    m_flick.m_animation->setFrom(animationStartCoordinate);
    m_flick.m_animation->setTo(animationEndCoordinate);
    m_flick.m_animation->start();
}

void QQuickGeoMapGestureArea::stopPan()
{
    if (m_flickState == flickActive) {
        stopFlick();
    } else if (m_flickState == panActive) {
        m_velocityX = 0;
        m_velocityY = 0;
        m_flickState = flickInactive;
        m_declarativeMap->setKeepMouseGrab(m_preventStealing);
        emit panFinished();
        emit panActiveChanged();
        m_map->prefetchData();
    }
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::stopFlick()
{
    if (!m_flick.m_animation)
        return;
    m_velocityX = 0;
    m_velocityY = 0;
    if (m_flick.m_animation->isRunning())
        m_flick.m_animation->stop();
    else
        handleFlickAnimationStopped();
}

void QQuickGeoMapGestureArea::handleFlickAnimationStopped()
{
    m_declarativeMap->setKeepMouseGrab(m_preventStealing);
    if (m_flickState == flickActive) {
        m_flickState = flickInactive;
        emit flickFinished();
        emit panActiveChanged();
        m_map->prefetchData();
    }
}

#include "moc_qquickgeomapgesturearea_p.cpp"

QT_END_NAMESPACE
