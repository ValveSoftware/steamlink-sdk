/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdeclarativegeomapgesturearea_p.h"
#include "qdeclarativegeomap_p.h"
#include "error_messages.h"

#include <QtGui/QGuiApplication>
#include <QtGui/qevent.h>
#include <QtGui/QWheelEvent>
#include <QtGui/QStyleHints>
#include <QtQml/qqmlinfo.h>
#include <QPropertyAnimation>
#include <QDebug>
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
    \instantiates QDeclarativeGeoMapPinchEvent
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
    \instantiates QDeclarativeGeoMapGestureArea

    \inqmlmodule QtLocation

    \brief The MapGestureArea type provides Map gesture interaction.

    MapGestureArea objects are used as part of a Map, to provide for panning,
    flicking and pinch-to-zoom gesture used on touch displays.

    A MapGestureArea is automatically created with a new Map and available with
    the \l{Map::gesture}{gesture} property. This is the only way
    to create a MapGestureArea, and once created this way cannot be destroyed
    without its parent Map.

    The two most commonly used properties of the MapGestureArea are the \l enabled
    and \l activeGestures properties. Both of these must be set before a
    MapGestureArea will have any effect upon interaction with the Map.
    The \l flickDeceleration property controls how quickly the map pan slows after contact
    is released while panning the map.

    \section2 Performance

    The MapGestureArea, when enabled, must process all incoming touch events in
    order to track the shape and size of the "pinch". The overhead added on
    touch events can be considered constant time.

    \section2 Example Usage

    The following example enables the zoom and pan gestures on the map, but not flicking. So the
    map scrolling will halt immediately on releasing the mouse button / touch.

    \code
    Map {
        gesture.enabled: true
        gesture.activeGestures: MapGestureArea.ZoomGesture | MapGestureArea.PanGesture
    }
    \endcode

    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.0
*/

/*!
    \qmlproperty bool QtLocation::MapGestureArea::enabled

    This property holds whether the gestures are enabled.
    Note: disabling gestures during an active gesture does not have effect on
    the potentially active current gesture.
*/


/*!
    \qmlproperty bool QtLocation::MapGestureArea::panEnabled

    This property holds whether the pan gestures are enabled.
    Note: disabling gestures during an active gesture does not have effect on
    the potentially active current gesture.
*/

/*!
    \qmlproperty bool QtLocation::MapGestureArea::pinchEnabled

    This property holds whether the pinch gestures are enabled.
    Note: disabling gestures during an active gesture does not have effect on
    the potentially active current gesture.
*/

/*!
    \qmlproperty bool QtLocation::MapGestureArea::isPinchActive

    This read-only property holds whether any pinch gesture is active.
*/

/*!
    \qmlproperty bool QtLocation::MapGestureArea::isPanActive

    This read-only property holds whether any pan gesture (panning or flicking) is active.
*/

/*!
    \qmlproperty enumeration QtLocation::MapGestureArea::activeGestures

    This property holds the gestures that will be active. By default
    the zoom, pan and flick gestures are enabled.

    \list
    \li MapGestureArea.NoGesture - Don't support any additional gestures (value: 0x0000).
    \li MapGestureArea.ZoomGesture - Support the map zoom gesture (value: 0x0001).
    \li MapGestureArea.PanGesture  - Support the map pan gesture (value: 0x0002).
    \li MapGestureArea.FlickGesture  - Support the map flick gesture (value: 0x0004).
    \endlist

    \note For the time being, only MapGestureArea.ZoomGesture is supported.
*/

/*!
    \qmlproperty real QtLocation::MapGestureArea::maximumZoomLevelChange

    This property holds the maximum zoom level change per pinch, essentially
    meant to be used for setting the zoom sensitivity.

    It is an indicative measure calculated from the dimensions of the
    map area, roughly corresponding how much zoom level could change with
    maximum pinch zoom. Default value is 2.0, maximum value is 10.0
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

    This signal is emitted when the view begins moving due to user
    interaction. Typically this means that the user is dragging a finger -
    or a mouse with one of more mouse buttons pressed - on the map.

    The corresponding handler is \c onPanStarted.
*/

/*!
    \qmlsignal QtLocation::MapGestureArea::panFinished()

    This signal is emitted when the view stops moving due to user
    interaction.  If a flick was generated, this signal is
    emitted when the flick stops.  If a flick was not
    generated, this signal is emitted when the
    user stops dragging - that is a mouse or touch release.

    The corresponding handler is \c onPanFinished.

*/

/*!
    \qmlsignal QtLocation::MapGestureArea::flickStarted()

    This signal is emitted when the view is flicked.  A flick
    starts from the point that the mouse or touch is released,
    while still in motion.

    The corresponding handler is \c onFlichStarted.
*/

/*!
    \qmlsignal QtLocation::MapGestureArea::flickFinished()

    This signal is emitted when the view stops moving due to a flick.
    The order of panFinished() and flickFinished() is not specified.

    The corresponding handler is \c onFlickFinished.
*/

QDeclarativeGeoMapGestureArea::QDeclarativeGeoMapGestureArea(QDeclarativeGeoMap *map, QObject *parent)
    : QObject(parent),
      declarativeMap_(map),
      enabled_(true),
      activeGestures_(ZoomGesture | PanGesture | FlickGesture)
{
    map_ = 0;
    pan_.enabled_ = true,
    pan_.maxVelocity_ = QML_MAP_FLICK_DEFAULTMAXVELOCITY;
    pan_.deceleration_ = QML_MAP_FLICK_DEFAULTDECELERATION;
    pan_.animation_ = 0;
    touchPointState_ = touchPoints0;
    pinchState_ = pinchInactive;
    panState_ = panInactive;

}
/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::setMap(QGeoMap *map)
{
    if (map_ || !map)
        return;
    map_ = map;
    pan_.animation_ = new QPropertyAnimation(map_->mapController(), "center", this);
    pan_.animation_->setEasingCurve(QEasingCurve(QEasingCurve::OutQuad));
    connect(pan_.animation_, SIGNAL(finished()), this, SLOT(endFlick()));
    connect(this, SIGNAL(movementStopped()),
            map_, SLOT(cameraStopped()));
}

QDeclarativeGeoMapGestureArea::~QDeclarativeGeoMapGestureArea()
{
}

/*!
    \internal
*/
QDeclarativeGeoMapGestureArea::ActiveGestures QDeclarativeGeoMapGestureArea::activeGestures() const
{
    return activeGestures_;
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::setActiveGestures(ActiveGestures activeGestures)
{
    if (activeGestures == activeGestures_)
        return;
    activeGestures_ = activeGestures;
    emit activeGesturesChanged();
}

/*!
    \internal
*/
bool QDeclarativeGeoMapGestureArea::isPinchActive() const
{
    return pinchState_ == pinchActive;
}

/*!
    \internal
*/
bool QDeclarativeGeoMapGestureArea::isPanActive() const
{
    return panState_ == panActive || panState_ == panFlick;
}

/*!
    \internal
*/
bool QDeclarativeGeoMapGestureArea::enabled() const
{
    return enabled_;
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::setEnabled(bool enabled)
{
    if (enabled == enabled_)
        return;
    enabled_ = enabled;
    emit enabledChanged();
}


/*!
    \internal
*/
bool QDeclarativeGeoMapGestureArea::pinchEnabled() const
{
    return pinch_.enabled;
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::setPinchEnabled(bool enabled)
{
    if (enabled == pinch_.enabled)
        return;
    pinch_.enabled = enabled;
    emit pinchEnabledChanged();
}

/*!
    \internal
*/
bool QDeclarativeGeoMapGestureArea::panEnabled() const
{
    return pan_.enabled_;
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::setPanEnabled(bool enabled)
{
    if (enabled == pan_.enabled_)
        return;
    pan_.enabled_ = enabled;
    emit panEnabledChanged();

    // unlike the pinch, the pan existing functionality is to stop immediately
    if (!enabled)
        stopPan();
}

/*!
    \internal
    Used internally to set the minimum zoom level of the gesture area.
    The caller is responsible to only send values that are valid
    for the map plugin. Negative values are ignored.
 */
void QDeclarativeGeoMapGestureArea::setMinimumZoomLevel(qreal min)
{
    if (min >= 0)
        pinch_.zoom.minimum = min;
}

/*!
   \internal
 */
qreal QDeclarativeGeoMapGestureArea::minimumZoomLevel() const
{
    return pinch_.zoom.minimum;
}

/*!
    \internal
    Used internally to set the maximum zoom level of the gesture area.
    The caller is responsible to only send values that are valid
    for the map plugin. Negative values are ignored.
 */
void QDeclarativeGeoMapGestureArea::setMaximumZoomLevel(qreal max)
{
    if (max >= 0)
        pinch_.zoom.maximum = max;
}

/*!
   \internal
 */
qreal QDeclarativeGeoMapGestureArea::maximumZoomLevel() const
{
    return pinch_.zoom.maximum;
}

/*!
    \internal
*/
qreal QDeclarativeGeoMapGestureArea::maximumZoomLevelChange() const
{
    return pinch_.zoom.maximumChange;
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::setMaximumZoomLevelChange(qreal maxChange)
{
    if (maxChange == pinch_.zoom.maximumChange || maxChange < 0.1 || maxChange > 10.0)
        return;
    pinch_.zoom.maximumChange = maxChange;
    emit maximumZoomLevelChangeChanged();
}

/*!
    \internal
*/
qreal QDeclarativeGeoMapGestureArea::flickDeceleration() const
{
    return pan_.deceleration_;
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::setFlickDeceleration(qreal deceleration)
{
    if (deceleration < QML_MAP_FLICK_MINIMUMDECELERATION)
        deceleration = QML_MAP_FLICK_MINIMUMDECELERATION;
    else if (deceleration > QML_MAP_FLICK_MAXIMUMDECELERATION)
        deceleration = QML_MAP_FLICK_MAXIMUMDECELERATION;
    if (deceleration == pan_.deceleration_)
        return;
    pan_.deceleration_ = deceleration;
    emit flickDecelerationChanged();
}

/*!
    \internal
*/
QTouchEvent::TouchPoint makeTouchPointFromMouseEvent(QMouseEvent *event, Qt::TouchPointState state)
{
    // this is only partially filled. But since it is only partially used it works
    // more robust would be to store a list of QPointFs rather than TouchPoints
    QTouchEvent::TouchPoint newPoint;
    newPoint.setPos(event->localPos());
    newPoint.setScenePos(event->windowPos());
    newPoint.setScreenPos(event->screenPos());
    newPoint.setState(state);
    newPoint.setId(0);
    return newPoint;
}

/*!
    \internal
*/
bool QDeclarativeGeoMapGestureArea::mousePressEvent(QMouseEvent *event)
{
    if (!(enabled_ && activeGestures_))
        return false;

    touchPoints_.clear();
    touchPoints_ << makeTouchPointFromMouseEvent(event, Qt::TouchPointPressed);

    update();
    return true;
}

/*!
    \internal
*/
bool QDeclarativeGeoMapGestureArea::mouseMoveEvent(QMouseEvent *event)
{
    if (!(enabled_ && activeGestures_))
        return false;

    touchPoints_.clear();

    touchPoints_ << makeTouchPointFromMouseEvent(event, Qt::TouchPointMoved);
    update();
    return true;
}

/*!
    \internal
*/
bool QDeclarativeGeoMapGestureArea::mouseReleaseEvent(QMouseEvent *)
{
    if (!(enabled_ && activeGestures_))
        return false;

    touchPoints_.clear();
    update();
    return true;
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::touchEvent(QTouchEvent *event)
{
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
        touchPoints_.clear();
        for (int i = 0; i < event->touchPoints().count(); ++i) {
            if (!(event->touchPoints().at(i).state() & Qt::TouchPointReleased)) {
                touchPoints_ << event->touchPoints().at(i);
            }
        }
        update();
        break;
    case QEvent::TouchEnd:
        touchPoints_.clear();
        update();
        break;
    default:
        // no-op
        break;
    }
}

bool QDeclarativeGeoMapGestureArea::wheelEvent(QWheelEvent *event)
{
    declarativeMap_->setZoomLevel(qBound(minimumZoomLevel(), declarativeMap_->zoomLevel() + event->angleDelta().y() * qreal(0.001), maximumZoomLevel()));
    return true;
}

/*!
    \internal
*/
bool QDeclarativeGeoMapGestureArea::filterMapChildMouseEvent(QMouseEvent *event)
{
    bool used = false;
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        used = mousePressEvent(event);
        break;
    case QEvent::MouseButtonRelease:
        used = mouseReleaseEvent(event);
        break;
    case QEvent::MouseMove:
        used = mouseMoveEvent(event);
        break;
    case QEvent::UngrabMouse:
        touchPoints_.clear();
        update();
        break;
    default:
        used = false;
        break;
    }
    return used && (isPanActive() || isPinchActive());
}

/*!
    \internal
*/
bool QDeclarativeGeoMapGestureArea::filterMapChildTouchEvent(QTouchEvent *event)
{
    touchEvent(event);
    return isPanActive() || isPinchActive();
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::clearTouchData()
{
    velocityX_ = 0;
    velocityY_ = 0;
    sceneCenter_.setX(0);
    sceneCenter_.setY(0);
    touchCenterCoord_.setLongitude(0);
    touchCenterCoord_.setLatitude(0);
    startCoord_.setLongitude(0);
    startCoord_.setLatitude(0);
}


/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::updateVelocityList(const QPointF &pos)
{
    // Take velocity samples every sufficient period of time, used later to determine the flick
    // duration and speed (when mouse is released).
    qreal elapsed = qreal(lastPosTime_.elapsed());

    if (elapsed >= QML_MAP_FLICK_VELOCITY_SAMPLE_PERIOD) {
        elapsed /= 1000.;
        int dyFromLastPos = pos.y() - lastPos_.y();
        int dxFromLastPos = pos.x() - lastPos_.x();
        lastPos_ = pos;
        lastPosTime_.restart();
        qreal velX = qreal(dxFromLastPos) / elapsed;
        qreal velY = qreal(dyFromLastPos) / elapsed;
        velocityX_ = qBound<qreal>(-pan_.maxVelocity_, velX, pan_.maxVelocity_);
        velocityY_ = qBound<qreal>(-pan_.maxVelocity_, velY, pan_.maxVelocity_);
    }
}

/*!
    \internal
*/
// simplify the gestures by using a state-machine format (easy to move to a future state machine)
void QDeclarativeGeoMapGestureArea::update()
{
    // First state machine is for the number of touch points
    touchPointStateMachine();

    // Parallel state machine for pinch
    if (isPinchActive() || (enabled_ && pinch_.enabled && (activeGestures_ & (ZoomGesture))))
        pinchStateMachine();

    // Parallel state machine for pan (since you can pan at the same time as pinching)
    // The stopPan function ensures that pan stops immediately when disabled,
    // but the line below allows pan continue its current gesture if you disable
    // the whole gesture (enabled_ flag), this keeps the enabled_ consistent with the pinch
    if (isPanActive() || (enabled_ && pan_.enabled_ && (activeGestures_ & (PanGesture | FlickGesture))))
        panStateMachine();
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::touchPointStateMachine()
{
    // Transitions:
    switch (touchPointState_) {
    case touchPoints0:
        if (touchPoints_.count() == 1) {
            clearTouchData();
            startOneTouchPoint();
            touchPointState_ = touchPoints1;
        } else if (touchPoints_.count() == 2) {
            clearTouchData();
            startTwoTouchPoints();
            touchPointState_ = touchPoints2;
        }
        break;
    case touchPoints1:
        if (touchPoints_.count() == 0) {
            touchPointState_ = touchPoints0;
        } else if (touchPoints_.count() == 2) {
            touchCenterCoord_ = map_->screenPositionToCoordinate(QDoubleVector2D(sceneCenter_), false);
            startTwoTouchPoints();
            touchPointState_ = touchPoints2;
        }
        break;
    case touchPoints2:
        if (touchPoints_.count() == 0) {
            touchPointState_ = touchPoints0;
        } else if (touchPoints_.count() == 1) {
            touchCenterCoord_ = map_->screenPositionToCoordinate(QDoubleVector2D(sceneCenter_), false);
            startOneTouchPoint();
            touchPointState_ = touchPoints1;
        }
        break;
    };

    // Update
    switch (touchPointState_) {
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
void QDeclarativeGeoMapGestureArea::startOneTouchPoint()
{
    sceneStartPoint1_ = declarativeMap_->mapFromScene(touchPoints_.at(0).scenePos());
    lastPos_ = sceneStartPoint1_;
    lastPosTime_.start();
    QGeoCoordinate startCoord = map_->screenPositionToCoordinate(QDoubleVector2D(sceneStartPoint1_), false);
    // ensures a smooth transition for panning
    startCoord_.setLongitude(startCoord_.longitude() + startCoord.longitude() -
                             touchCenterCoord_.longitude());
    startCoord_.setLatitude(startCoord_.latitude() + startCoord.latitude() -
                            touchCenterCoord_.latitude());
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::updateOneTouchPoint()
{
    sceneCenter_ = declarativeMap_->mapFromScene(touchPoints_.at(0).scenePos());
    updateVelocityList(sceneCenter_);
}


/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::startTwoTouchPoints()
{
    sceneStartPoint1_ = declarativeMap_->mapFromScene(touchPoints_.at(0).scenePos());
    sceneStartPoint2_ = declarativeMap_->mapFromScene(touchPoints_.at(1).scenePos());
    QPointF startPos = (sceneStartPoint1_ + sceneStartPoint2_) * 0.5;
    lastPos_ = startPos;
    lastPosTime_.start();
    QGeoCoordinate startCoord = map_->screenPositionToCoordinate(QDoubleVector2D(startPos), false);
    startCoord_.setLongitude(startCoord_.longitude() + startCoord.longitude() -
                             touchCenterCoord_.longitude());
    startCoord_.setLatitude(startCoord_.latitude() + startCoord.latitude() -
                            touchCenterCoord_.latitude());
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::updateTwoTouchPoints()
{
    QPointF p1 = declarativeMap_->mapFromScene(touchPoints_.at(0).scenePos());
    QPointF p2 = declarativeMap_->mapFromScene(touchPoints_.at(1).scenePos());
    qreal dx = p1.x() - p2.x();
    qreal dy = p1.y() - p2.y();
    distanceBetweenTouchPoints_ = sqrt(dx * dx + dy * dy);
    sceneCenter_ = (p1 + p2) / 2;

    updateVelocityList(sceneCenter_);

    twoTouchAngle_ = QLineF(p1, p2).angle();
    if (twoTouchAngle_ > 180)
        twoTouchAngle_ -= 360;
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::setPinchActive(bool active)
{
    if ((active && pinchState_ == pinchActive) || (!active && pinchState_ != pinchActive))
        return;
    pinchState_ = active ? pinchActive : pinchInactive;
    emit pinchActiveChanged();
}


/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::pinchStateMachine()
{
    PinchState lastState = pinchState_;
    // Transitions:
    switch (pinchState_) {
    case pinchInactive:
        if (canStartPinch()) {
            startPinch();
            setPinchActive(true);
        }
        break;
    case pinchActive:
        if (touchPoints_.count() <= 1) {
            endPinch();
            setPinchActive(false);
        }
        break;
    }
    // This line implements an exclusive state machine, where the transitions and updates don't
    // happen on the same frame
    if (pinchState_ != lastState)
         return;
    // Update
    switch (pinchState_) {
    case pinchInactive:
        break; // do nothing
    case pinchActive:
        updatePinch();
        break;
    }
}

/*!
    \internal
*/
bool QDeclarativeGeoMapGestureArea::canStartPinch()
{
    const int startDragDistance = qApp->styleHints()->startDragDistance();

    if (touchPoints_.count() >= 2) {
        QPointF p1 = declarativeMap_->mapFromScene(touchPoints_.at(0).scenePos());
        QPointF p2 = declarativeMap_->mapFromScene(touchPoints_.at(1).scenePos());
        if (qAbs(p1.x()-sceneStartPoint1_.x()) > startDragDistance
         || qAbs(p1.y()-sceneStartPoint1_.y()) > startDragDistance
         || qAbs(p2.x()-sceneStartPoint2_.x()) > startDragDistance
         || qAbs(p2.y()-sceneStartPoint2_.y()) > startDragDistance) {
            pinch_.event.setCenter(declarativeMap_->mapFromScene(sceneCenter_));
            pinch_.event.setAngle(twoTouchAngle_);
            pinch_.event.setPoint1(p1);
            pinch_.event.setPoint2(p2);
            pinch_.event.setPointCount(touchPoints_.count());
            pinch_.event.setAccepted(true);
            emit pinchStarted(&pinch_.event);
            return pinch_.event.accepted();
        }
    }
    return false;
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::startPinch()
{
    pinch_.startDist = distanceBetweenTouchPoints_;
    pinch_.zoom.previous = 1.0;
    pinch_.lastAngle = twoTouchAngle_;

    pinch_.lastPoint1 = declarativeMap_->mapFromScene(touchPoints_.at(0).scenePos());
    pinch_.lastPoint2 = declarativeMap_->mapFromScene(touchPoints_.at(1).scenePos());

    pinch_.zoom.start = declarativeMap_->zoomLevel();
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::updatePinch()
{
    // Calculate the new zoom level if we have distance ( >= 2 touchpoints), otherwise stick with old.
    qreal newZoomLevel = pinch_.zoom.previous;
    if (distanceBetweenTouchPoints_) {
        newZoomLevel =
                // How much further/closer the current touchpoints are (in pixels) compared to pinch start
                ((distanceBetweenTouchPoints_ - pinch_.startDist)  *
                 //  How much one pixel corresponds in units of zoomlevel (and multiply by above delta)
                 (pinch_.zoom.maximumChange / ((declarativeMap_->width() + declarativeMap_->height()) / 2))) +
                // Add to starting zoom level. Sign of (dist-pinchstartdist) takes care of zoom in / out
                pinch_.zoom.start;
    }
    qreal da = pinch_.lastAngle - twoTouchAngle_;
    if (da > 180)
        da -= 360;
    else if (da < -180)
        da += 360;
    pinch_.event.setCenter(declarativeMap_->mapFromScene(sceneCenter_));
    pinch_.event.setAngle(twoTouchAngle_);

    pinch_.lastPoint1 = declarativeMap_->mapFromScene(touchPoints_.at(0).scenePos());
    pinch_.lastPoint2 = declarativeMap_->mapFromScene(touchPoints_.at(1).scenePos());
    pinch_.event.setPoint1(pinch_.lastPoint1);
    pinch_.event.setPoint2(pinch_.lastPoint2);
    pinch_.event.setPointCount(touchPoints_.count());
    pinch_.event.setAccepted(true);

    pinch_.lastAngle = twoTouchAngle_;
    emit pinchUpdated(&pinch_.event);

    if (activeGestures_ & ZoomGesture) {
        // Take maximum and minimumzoomlevel into account
        qreal perPinchMinimumZoomLevel = qMax(pinch_.zoom.start - pinch_.zoom.maximumChange, pinch_.zoom.minimum);
        qreal perPinchMaximumZoomLevel = qMin(pinch_.zoom.start + pinch_.zoom.maximumChange, pinch_.zoom.maximum);
        newZoomLevel = qMin(qMax(perPinchMinimumZoomLevel, newZoomLevel), perPinchMaximumZoomLevel);
        declarativeMap_->setZoomLevel(newZoomLevel);
    }
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::endPinch()
{
    QPointF pinchCenter = declarativeMap_->mapFromScene(sceneCenter_);
    pinch_.event.setCenter(pinchCenter);
    pinch_.event.setAngle(pinch_.lastAngle);
    pinch_.event.setPoint1(declarativeMap_->mapFromScene(pinch_.lastPoint1));
    pinch_.event.setPoint2(declarativeMap_->mapFromScene(pinch_.lastPoint2));
    pinch_.event.setAccepted(true);
    pinch_.event.setPointCount(0);
    emit pinchFinished(&pinch_.event);
    pinch_.startDist = 0;
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::panStateMachine()
{
    PanState lastState = panState_;

    // Transitions
    switch (panState_) {
    case panInactive:
        if (canStartPan()) {
            // Update startCoord_ to ensure smooth start for panning when going over startDragDistance
            QGeoCoordinate newStartCoord = map_->screenPositionToCoordinate(QDoubleVector2D(lastPos_), false);
            startCoord_.setLongitude(newStartCoord.longitude());
            startCoord_.setLatitude(newStartCoord.latitude());
            panState_ = panActive;
        }
        break;
    case panActive:
        if (touchPoints_.count() == 0) {
            panState_ = panFlick;
            if (!tryStartFlick())
            {
                panState_ = panInactive;
                // mark as inactive for use by camera
                if (pinchState_ == pinchInactive) {
                    emit panFinished();
                    emit movementStopped();
                }
            }
        }
        break;
    case panFlick:
        if (touchPoints_.count() > 0) { // re touched before movement ended
            endFlick();
            panState_ = panActive;
        }
        break;
    }
    // Update
    switch (panState_) {
    case panInactive: // do nothing
        break;
    case panActive:
        updatePan();
        // this ensures 'panStarted' occurs after the pan has actually started
        if (lastState != panActive)
            emit panStarted();
        break;
    case panFlick:
        break;
    }
}
/*!
    \internal
*/
bool QDeclarativeGeoMapGestureArea::canStartPan()
{
    if (touchPoints_.count() == 0 || (activeGestures_ & PanGesture) == 0)
        return false;

    // Check if thresholds for normal panning are met.
    // (normal panning vs flicking: flicking will start from mouse release event).
    const int startDragDistance = qApp->styleHints()->startDragDistance();
    QPointF p1 = declarativeMap_->mapFromScene(touchPoints_.at(0).scenePos());
    int dyFromPress = int(p1.y() - sceneStartPoint1_.y());
    int dxFromPress = int(p1.x() - sceneStartPoint1_.x());
    if ((qAbs(dyFromPress) > startDragDistance || qAbs(dxFromPress) > startDragDistance))
        return true;
    return false;
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::updatePan()
{
    QPointF startPoint = map_->coordinateToScreenPosition(startCoord_, false).toPointF();
    int dx = static_cast<int>(sceneCenter_.x() - startPoint.x());
    int dy = static_cast<int>(sceneCenter_.y() - startPoint.y());
    QPointF mapCenterPoint;
    mapCenterPoint.setY(map_->height() / 2.0  - dy);
    mapCenterPoint.setX(map_->width() / 2.0 - dx);
    QGeoCoordinate animationStartCoordinate = map_->screenPositionToCoordinate(QDoubleVector2D(mapCenterPoint), false);
    map_->mapController()->setCenter(animationStartCoordinate);
}

/*!
    \internal
*/
bool QDeclarativeGeoMapGestureArea::tryStartFlick()
{
    if ((activeGestures_ & FlickGesture) == 0)
        return false;
    // if we drag then pause before release we should not cause a flick.
    qreal velocityX = 0.0;
    qreal velocityY = 0.0;
    if (lastPosTime_.elapsed() < QML_MAP_FLICK_VELOCITY_SAMPLE_PERIOD) {
        velocityY = velocityY_;
        velocityX = velocityX_;
    }
    int flickTimeY = 0;
    int flickTimeX = 0;
    int flickPixelsX = 0;
    int flickPixelsY = 0;
    if (qAbs(velocityY) > MinimumFlickVelocity && qAbs(sceneCenter_.y() - sceneStartPoint1_.y()) > FlickThreshold) {
        // calculate Y flick animation values
        qreal acceleration = pan_.deceleration_;
        if ((velocityY > 0.0f) == (pan_.deceleration_ > 0.0f))
            acceleration = acceleration * -1.0f;
        flickTimeY = static_cast<int>(-1000 * velocityY / acceleration);
        flickPixelsY = (flickTimeY * velocityY) / (1000.0 * 2);
    }
    if (qAbs(velocityX) > MinimumFlickVelocity && qAbs(sceneCenter_.x() - sceneStartPoint1_.x()) > FlickThreshold) {
        // calculate X flick animation values
        qreal acceleration = pan_.deceleration_;
        if ((velocityX > 0.0f) == (pan_.deceleration_ > 0.0f))
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
// FIXME:
// - not left right / up down flicking, so if map is rotated, will act unintuitively
void QDeclarativeGeoMapGestureArea::startFlick(int dx, int dy, int timeMs)
{
    if (timeMs < 0)
        return;

    QGeoCoordinate animationStartCoordinate = map_->mapController()->center();

    if (pan_.animation_->state() == QPropertyAnimation::Running)
        pan_.animation_->stop();
    QGeoCoordinate animationEndCoordinate = map_->mapController()->center();
    pan_.animation_->setDuration(timeMs);
    animationEndCoordinate.setLongitude(animationStartCoordinate.longitude() - (dx / pow(2.0, map_->mapController()->zoom())));
    animationEndCoordinate.setLatitude(animationStartCoordinate.latitude() + (dy / pow(2.0, map_->mapController()->zoom())));
    pan_.animation_->setStartValue(QVariant::fromValue(animationStartCoordinate));
    pan_.animation_->setEndValue(QVariant::fromValue(animationEndCoordinate));
    pan_.animation_->start();
    emit flickStarted();
}

void QDeclarativeGeoMapGestureArea::stopPan()
{
    velocityX_ = 0;
    velocityY_ = 0;
    if (panState_ == panFlick) {
        endFlick();
    } else if (panState_ == panActive) {
        emit panFinished();
        emit movementStopped();
    }
    panState_ = panInactive;
}

/*!
    \internal
*/
void QDeclarativeGeoMapGestureArea::endFlick()
{
    emit panFinished();
    if (pan_.animation_->state() == QPropertyAnimation::Running)
        pan_.animation_->stop();
    emit flickFinished();
    panState_ = panInactive;
    emit movementStopped();
}






#include "moc_qdeclarativegeomapgesturearea_p.cpp"

QT_END_NAMESPACE
