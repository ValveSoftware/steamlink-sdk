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
#include "qquickgeocoordinateanimation_p.h"
#include "qdeclarativegeomap_p.h"
#include "error_messages_p.h"

#include <QtGui/QGuiApplication>
#include <QtGui/qevent.h>
#if QT_CONFIG(wheelevent)
#include <QtGui/QWheelEvent>
#endif
#include <QtGui/QStyleHints>
#include <QtQml/qqmlinfo.h>
#include <QtQuick/QQuickWindow>
#include <QPropertyAnimation>
#include <QDebug>
#include "math.h"
#include <cmath>
#include "qgeomap_p.h"
#include "qdoublevector2d_p.h"
#include "qlocationutils_p.h"
#include <QtGui/QMatrix4x4>


#define QML_MAP_FLICK_DEFAULTMAXVELOCITY 2500
#define QML_MAP_FLICK_MINIMUMDECELERATION 500
#define QML_MAP_FLICK_DEFAULTDECELERATION 2500
#define QML_MAP_FLICK_MAXIMUMDECELERATION 10000

#define QML_MAP_FLICK_VELOCITY_SAMPLE_PERIOD 38
// FlickThreshold determines how far the "mouse" must have moved
// before we perform a flick.
static const int FlickThreshold = 20;
// Really slow flicks can be annoying.
static const qreal MinimumFlickVelocity = 75.0;
// Tolerance for detecting two finger sliding start
static const qreal MaximumParallelPosition = 40.0; // in degrees
// Tolerance for detecting parallel sliding
static const qreal MaximumParallelSlidingAngle = 4.0; // in degrees
// Tolerance for starting rotation
static const qreal MinimumRotationStartingAngle = 15.0; // in degrees
// Tolerance for starting pinch
static const qreal MinimumPinchDelta = 40; // in pixels
// Tolerance for starting tilt when sliding vertical
static const qreal MinimumPanToTiltDelta = 80; // in pixels;

static qreal distanceBetweenTouchPoints(const QPointF &p1, const QPointF &p2)
{
    return QLineF(p1, p2).length();
}

// Returns the new map center after anchoring coordinate to anchorPoint on the screen
// Approach: find the displacement in (wrapped) mercator space, and apply that to the center
static QGeoCoordinate anchorCoordinateToPoint(QGeoMap &map, const QGeoCoordinate &coordinate, const QPointF &anchorPoint)
{
    QDoubleVector2D centerProj = map.geoProjection().geoToWrappedMapProjection(map.cameraData().center());
    QDoubleVector2D coordProj  = map.geoProjection().geoToWrappedMapProjection(coordinate);

    QDoubleVector2D anchorProj = map.geoProjection().itemPositionToWrappedMapProjection(QDoubleVector2D(anchorPoint));
    // Y-clamping done in mercatorToCoord
    return map.geoProjection().wrappedMapProjectionToGeo(centerProj + coordProj - anchorProj);
}

static qreal angleFromPoints(const QPointF &p1, const QPointF &p2)
{
    return QLineF(p1, p2).angle();
}

// Keeps it in +- 180
static qreal touchAngle(const QPointF &p1, const QPointF &p2)
{
    qreal angle = angleFromPoints(p1, p2);
    if (angle > 180)
        angle -= 360;
    return angle;
}

// Deals with angles crossing the +-180 edge, assumes that the delta can't be > 180
static qreal angleDelta(const qreal angle1, const qreal angle2)
{
    qreal delta = angle1 - angle2;
    if (delta > 180.0) // detect crossing angle1 positive, angle2 negative, rotation counterclockwise, difference negative
        delta = angle1 - angle2 - 360.0;
    else if (delta < -180.0) // detect crossing angle1 negative, angle2 positive, rotation clockwise, difference positive
        delta = angle1 - angle2 + 360.0;

    return delta;
}

static bool pointDragged(const QPointF &pOld, const QPointF &pNew)
{
    static const int startDragDistance = qApp->styleHints()->startDragDistance();
    return ( qAbs(pNew.x() - pOld.x()) > startDragDistance
             || qAbs(pNew.y() - pOld.y()) > startDragDistance);
}

static qreal vectorSize(const QPointF &vector)
{
    return std::sqrt(vector.x() * vector.x() + vector.y() * vector.y());
}

// This linearizes the angles around 0, and keep it linear around 180, allowing to differentiate
// touch angles that are supposed to be parallel (0 or 180 depending on what finger goes first)
static qreal touchAngleTilting(const QPointF &p1, const QPointF &p2)
{
    qreal angle = angleFromPoints(p1, p2);
    if (angle > 270)
        angle -= 360;
    return angle;
}

static bool movingParallelVertical(const QPointF &p1old, const QPointF &p1new, const QPointF &p2old, const QPointF &p2new)
{
    if (!pointDragged(p1old, p1new) || !pointDragged(p2old, p2new))
        return false;

    QPointF v1 = p1new - p1old;
    QPointF v2 = p2new - p2old;
    qreal v1v2size = vectorSize(v1 + v2);

    if (v1v2size < vectorSize(v1) || v1v2size < vectorSize(v2)) // going in opposite directions
        return false;

    const qreal newAngle = touchAngleTilting(p1new, p2new);
    const qreal oldAngle = touchAngleTilting(p1old, p2old);
    const qreal angleDiff = angleDelta(newAngle, oldAngle);

    if (qAbs(angleDiff) > MaximumParallelSlidingAngle)
        return false;

    return true;
}

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
    flicking and pinch-to-zoom gesture used on touch displays, as well as two finger rotation
    and two finger parallel vertical sliding to tilt the map.

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

    This read-only property holds whether the pinch gesture is active.
*/

/*!
    \qmlproperty bool QtLocation::MapGestureArea::panActive

    This read-only property holds whether the pan gesture is active.

    \note Change notifications for this property were introduced in Qt 5.5.
*/

/*!
    \qmlproperty bool QtLocation::MapGestureArea::rotationActive

    This read-only property holds whether the two-finger rotation gesture is active.

    \since Qt Location 5.9
*/

/*!
    \qmlproperty bool QtLocation::MapGestureArea::tiltActive

    This read-only property holds whether the two-finger tilt gesture is active.

    \since Qt Location 5.9
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

/*!
    \qmlsignal QtLocation::MapGestureArea::rotationStarted(PinchEvent event)

    This signal is emitted when a two-finger rotation gesture is started.

    The corresponding handler is \c onRotationStarted.

    \sa rotationUpdated, rotationFinished

    \since Qt Location 5.9
*/

/*!
    \qmlsignal QtLocation::MapGestureArea::rotationUpdated(PinchEvent event)

    This signal is emitted as the user's fingers move across the map,
    after the \l rotationStarted signal is emitted.

    The corresponding handler is \c onRotationUpdated.

    \sa rotationStarted, rotationFinished

    \since Qt Location 5.9
*/

/*!
    \qmlsignal QtLocation::MapGestureArea::rotationFinished(PinchEvent event)

    This signal is emitted at the end of a two-finger rotation gesture.

    The corresponding handler is \c onRotationFinished.

    \sa rotationStarted, rotationUpdated

    \since Qt Location 5.9
*/

/*!
    \qmlsignal QtLocation::MapGestureArea::tiltStarted(PinchEvent event)

    This signal is emitted when a two-finger tilt gesture is started.

    The corresponding handler is \c onTiltStarted.

    \sa tiltUpdated, tiltFinished

    \since Qt Location 5.9
*/

/*!
    \qmlsignal QtLocation::MapGestureArea::tiltUpdated(PinchEvent event)

    This signal is emitted as the user's fingers move across the map,
    after the \l tiltStarted signal is emitted.

    The corresponding handler is \c onTiltUpdated.

    \sa tiltStarted, tiltFinished

    \since Qt Location 5.9
*/

/*!
    \qmlsignal QtLocation::MapGestureArea::tiltFinished(PinchEvent event)

    This signal is emitted at the end of a two-finger tilt gesture.

    The corresponding handler is \c onTiltFinished.

    \sa tiltStarted, tiltUpdated

    \since Qt Location 5.9
*/

QQuickGeoMapGestureArea::QQuickGeoMapGestureArea(QDeclarativeGeoMap *map)
    : QQuickItem(map),
      m_map(0),
      m_declarativeMap(map),
      m_enabled(true),
      m_acceptedGestures(PinchGesture | PanGesture | FlickGesture | RotationGesture | TiltGesture),
      m_preventStealing(false)
{
    m_touchPointState = touchPoints0;
    m_pinchState = pinchInactive;
    m_flickState = flickInactive;
    m_rotationState = rotationInactive;
    m_tiltState = tiltInactive;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::setMap(QPointer<QGeoMap> map)
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
    \li MapGestureArea.RotationGesture  - Support the map rotation gesture (value: 0x0008).
    \li MapGestureArea.TiltGesture  - Support the map tilt gesture (value: 0x0010).
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

    if (enabled()) {
        setPanEnabled(acceptedGestures & PanGesture);
        setFlickEnabled(acceptedGestures & FlickGesture);
        setPinchEnabled(acceptedGestures & PinchGesture);
        setRotationEnabled(acceptedGestures & RotationGesture);
        setTiltEnabled(acceptedGestures & TiltGesture);
    }

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
bool QQuickGeoMapGestureArea::isRotationActive() const
{
    return m_rotationState == rotationActive;
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::isTiltActive() const
{
    return m_tiltState == tiltActive;
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
        setRotationEnabled(m_acceptedGestures & RotationGesture);
        setTiltEnabled(m_acceptedGestures & TiltGesture);
    } else {
        setPanEnabled(false);
        setFlickEnabled(false);
        setPinchEnabled(false);
        setRotationEnabled(false);
        setTiltEnabled(false);
    }

    emit enabledChanged();
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::pinchEnabled() const
{
    return m_pinch.m_pinchEnabled;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::setPinchEnabled(bool enabled)
{
    m_pinch.m_pinchEnabled = enabled;
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::rotationEnabled() const
{
    return m_pinch.m_rotationEnabled;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::setRotationEnabled(bool enabled)
{
    m_pinch.m_rotationEnabled = enabled;
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::tiltEnabled() const
{
    return m_pinch.m_tiltEnabled;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::setTiltEnabled(bool enabled)
{
    m_pinch.m_tiltEnabled = enabled;
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::panEnabled() const
{
    return m_flick.m_panEnabled;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::setPanEnabled(bool enabled)
{
    if (enabled == m_flick.m_panEnabled)
        return;
    m_flick.m_panEnabled = enabled;

    // unlike the pinch, the pan existing functionality is to stop immediately
    if (!enabled) {
        stopPan();
        m_flickState = flickInactive;
    }
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::flickEnabled() const
{
    return m_flick.m_flickEnabled;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::setFlickEnabled(bool enabled)
{
    if (enabled == m_flick.m_flickEnabled)
        return;
    m_flick.m_flickEnabled = enabled;
    // unlike the pinch, the flick existing functionality is to stop immediately
    if (!enabled) {
        bool stateActive = (m_flickState != flickInactive);
        stopFlick();
        if (stateActive) {
            if (m_flick.m_panEnabled)
                m_flickState = panActive;
            else
                m_flickState = flickInactive;
        }
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
    // TODO: remove m_zoom.m_minimum and m_maximum and use m_declarativeMap directly instead.
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

#if QT_CONFIG(wheelevent)
void QQuickGeoMapGestureArea::handleWheelEvent(QWheelEvent *event)
{
    if (!m_map)
        return;

    const QGeoCoordinate &wheelGeoPos = m_map->geoProjection().itemPositionToCoordinate(QDoubleVector2D(event->posF()), false);
    const QPointF &preZoomPoint = event->posF();

    const double zoomLevelDelta = event->angleDelta().y() * qreal(0.001);
    // Gesture area should always honor maxZL, but Map might not.
    m_declarativeMap->setZoomLevel(qMin<qreal>(m_declarativeMap->zoomLevel() + zoomLevelDelta, maximumZoomLevel()),
                                   false);
    const QPointF &postZoomPoint = m_map->geoProjection().coordinateToItemPosition(wheelGeoPos, false).toPointF();

    if (preZoomPoint != postZoomPoint) // need to re-anchor the wheel geoPos to the event position
          m_declarativeMap->setCenter(anchorCoordinateToPoint(*m_map, wheelGeoPos, preZoomPoint));

    event->accept();
}
#endif

/*!
    \internal
*/
void QQuickGeoMapGestureArea::clearTouchData()
{
    m_flickVector = QVector2D();
    m_touchPointsCentroid.setX(0);
    m_touchPointsCentroid.setY(0);
    m_touchCenterCoord.setLongitude(0);
    m_touchCenterCoord.setLatitude(0);
    m_startCoord.setLongitude(0);
    m_startCoord.setLatitude(0);
}


/*!
    \internal
*/
void QQuickGeoMapGestureArea::updateFlickParameters(const QPointF &pos)
{
    // Take velocity samples every sufficient period of time, used later to determine the flick
    // duration and speed (when mouse is released).
    qreal elapsed = qreal(m_lastPosTime.elapsed());

    if (elapsed >= QML_MAP_FLICK_VELOCITY_SAMPLE_PERIOD) {
        elapsed /= 1000.;
        qreal vel  = distanceBetweenTouchPoints(pos, m_lastPos) / elapsed;
        m_flickVector = (QVector2D(pos) - QVector2D(m_lastPos)).normalized();
        m_flickVector *= qBound<qreal>(-m_flick.m_maxVelocity, vel, m_flick.m_maxVelocity);

        m_lastPos = pos;
        m_lastPosTime.restart();
    }
}

void QQuickGeoMapGestureArea::setTouchPointState(const QQuickGeoMapGestureArea::TouchPointState state)
{
    m_touchPointState = state;
}

void QQuickGeoMapGestureArea::setFlickState(const QQuickGeoMapGestureArea::FlickState state)
{
    m_flickState = state;
}

void QQuickGeoMapGestureArea::setTiltState(const QQuickGeoMapGestureArea::TiltState state)
{
    m_tiltState = state;
}

void QQuickGeoMapGestureArea::setRotationState(const QQuickGeoMapGestureArea::RotationState state)
{
    m_rotationState = state;
}

void QQuickGeoMapGestureArea::setPinchState(const QQuickGeoMapGestureArea::PinchState state)
{
    m_pinchState = state;
}

/*!
    \internal
*/

bool QQuickGeoMapGestureArea::isActive() const
{
    return isPanActive() || isPinchActive() || isRotationActive() || isTiltActive();
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

    // Parallel state machine for tilt. Tilt goes first as it blocks anything else, when started.
    // But tilting can also only start if nothing else is active.
    if (isTiltActive() || m_pinch.m_tiltEnabled)
        tiltStateMachine();

    // Parallel state machine for pinch
    if (isPinchActive() || m_pinch.m_pinchEnabled)
        pinchStateMachine();

    // Parallel state machine for rotation.
    if (isRotationActive() || m_pinch.m_rotationEnabled)
        rotationStateMachine();

    // Parallel state machine for pan (since you can pan at the same time as pinching)
    // The stopPan function ensures that pan stops immediately when disabled,
    // but the isPanActive() below allows pan continue its current gesture if you disable
    // the whole gesture.
    // Pan goes last because it does reanchoring in updatePan()  which makes the map
    // properly rotate around the touch point centroid.
    if (isPanActive() || m_flick.m_flickEnabled || m_flick.m_panEnabled)
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
            setTouchPointState(touchPoints1);
        } else if (m_allPoints.count() >= 2) {
            clearTouchData();
            startTwoTouchPoints();
            setTouchPointState(touchPoints2);
        }
        break;
    case touchPoints1:
        if (m_allPoints.count() == 0) {
            setTouchPointState(touchPoints0);
        } else if (m_allPoints.count() == 2) {
            m_touchCenterCoord = m_map->geoProjection().itemPositionToCoordinate(QDoubleVector2D(m_touchPointsCentroid), false);
            startTwoTouchPoints();
            setTouchPointState(touchPoints2);
        }
        break;
    case touchPoints2:
        if (m_allPoints.count() == 0) {
            setTouchPointState(touchPoints0);
        } else if (m_allPoints.count() == 1) {
            m_touchCenterCoord = m_map->geoProjection().itemPositionToCoordinate(QDoubleVector2D(m_touchPointsCentroid), false);
            startOneTouchPoint();
            setTouchPointState(touchPoints1);
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
    QGeoCoordinate startCoord = m_map->geoProjection().itemPositionToCoordinate(QDoubleVector2D(m_sceneStartPoint1), false);
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
    m_touchPointsCentroid = mapFromScene(m_allPoints.at(0).scenePos());
    updateFlickParameters(m_touchPointsCentroid);
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
    QGeoCoordinate startCoord = m_map->geoProjection().itemPositionToCoordinate(QDoubleVector2D(startPos), false);
    m_startCoord.setLongitude(m_startCoord.longitude() + startCoord.longitude() -
                             m_touchCenterCoord.longitude());
    m_startCoord.setLatitude(m_startCoord.latitude() + startCoord.latitude() -
                            m_touchCenterCoord.latitude());
    m_twoTouchAngleStart = touchAngle(m_sceneStartPoint1, m_sceneStartPoint2); // Initial angle used for calculating rotation
    m_distanceBetweenTouchPointsStart = distanceBetweenTouchPoints(m_sceneStartPoint1, m_sceneStartPoint2);
    m_twoTouchPointsCentroidStart = (m_sceneStartPoint1 + m_sceneStartPoint2) / 2;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::updateTwoTouchPoints()
{
    QPointF p1 = mapFromScene(m_allPoints.at(0).scenePos());
    QPointF p2 = mapFromScene(m_allPoints.at(1).scenePos());
    m_distanceBetweenTouchPoints = distanceBetweenTouchPoints(p1, p2);
    m_touchPointsCentroid = (p1 + p2) / 2;
    updateFlickParameters(m_touchPointsCentroid);

    m_twoTouchAngle = touchAngle(p1, p2);
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::tiltStateMachine()
{
    TiltState lastState = m_tiltState;
    // Transitions:
    switch (m_tiltState) {
    case tiltInactive:
        if (m_allPoints.count() >= 2) {
            if (!isRotationActive() && !isPinchActive() && canStartTilt()) { // only gesture that can be overridden: pan/flick
                m_declarativeMap->setKeepMouseGrab(true);
                m_declarativeMap->setKeepTouchGrab(true);
                startTilt();
                setTiltState(tiltActive);
            } else {
                setTiltState(tiltInactiveTwoPoints);
            }
        }
        break;
    case tiltInactiveTwoPoints:
        if (m_allPoints.count() <= 1) {
            setTiltState(tiltInactive);
        } else {
            if (!isRotationActive() && !isPinchActive() && canStartTilt()) { // only gesture that can be overridden: pan/flick
                m_declarativeMap->setKeepMouseGrab(true);
                m_declarativeMap->setKeepTouchGrab(true);
                startTilt();
                setTiltState(tiltActive);
            }
        }
        break;
    case tiltActive:
        if (m_allPoints.count() <= 1) {
            setTiltState(tiltInactive);
            m_declarativeMap->setKeepMouseGrab(m_preventStealing);
            m_declarativeMap->setKeepTouchGrab(m_preventStealing);
            endTilt();
        }
        break;
    }
    // This line implements an exclusive state machine, where the transitions and updates don't
    // happen on the same frame
    if (m_tiltState != lastState) {
        emit tiltActiveChanged();
        return;
    }

    // Update
    switch (m_tiltState) {
    case tiltInactive:
    case tiltInactiveTwoPoints:
        break; // do nothing
    case tiltActive:
        updateTilt();
        break;
    }
}

bool validateTouchAngleForTilting(const qreal angle)
{
    return ((qAbs(angle) - 180.0) < MaximumParallelPosition) || (qAbs(angle) < MaximumParallelPosition);
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::canStartTilt()
{
    if (m_allPoints.count() >= 2) {
        QPointF p1 = mapFromScene(m_allPoints.at(0).scenePos());
        QPointF p2 = mapFromScene(m_allPoints.at(1).scenePos());
        if (validateTouchAngleForTilting(m_twoTouchAngle)
                && movingParallelVertical(m_sceneStartPoint1, p1, m_sceneStartPoint2, p2)
                && qAbs(m_twoTouchPointsCentroidStart.y() - m_touchPointsCentroid.y()) > MinimumPanToTiltDelta) {
            m_pinch.m_event.setCenter(mapFromScene(m_touchPointsCentroid));
            m_pinch.m_event.setAngle(m_twoTouchAngle);
            m_pinch.m_event.setPoint1(p1);
            m_pinch.m_event.setPoint2(p2);
            m_pinch.m_event.setPointCount(m_allPoints.count());
            m_pinch.m_event.setAccepted(true);
            emit tiltStarted(&m_pinch.m_event);
            return true;
        }
    }
    return false;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::startTilt()
{
    if (isPanActive()) {
        stopPan();
        setFlickState(flickInactive);
    }

    m_pinch.m_tilt.m_startTouchCentroid = m_touchPointsCentroid;
    m_pinch.m_tilt.m_startTilt = m_declarativeMap->tilt();
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::updateTilt()
{
    // Calculate the new tilt
    qreal verticalDisplacement = (m_touchPointsCentroid - m_pinch.m_tilt.m_startTouchCentroid).y();

    // Approach: 10pixel = 1 degree.
    qreal tilt =  verticalDisplacement / 10.0;
    qreal newTilt = m_pinch.m_tilt.m_startTilt - tilt;
    m_declarativeMap->setTilt(newTilt);

    m_pinch.m_event.setCenter(mapFromScene(m_touchPointsCentroid));
    m_pinch.m_event.setAngle(m_twoTouchAngle);
    m_pinch.m_lastPoint1 = mapFromScene(m_allPoints.at(0).scenePos());
    m_pinch.m_lastPoint2 = mapFromScene(m_allPoints.at(1).scenePos());
    m_pinch.m_event.setPoint1(m_pinch.m_lastPoint1);
    m_pinch.m_event.setPoint2(m_pinch.m_lastPoint2);
    m_pinch.m_event.setPointCount(m_allPoints.count());
    m_pinch.m_event.setAccepted(true);

    emit tiltUpdated(&m_pinch.m_event);
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::endTilt()
{
    QPointF p1 = mapFromScene(m_pinch.m_lastPoint1);
    QPointF p2 = mapFromScene(m_pinch.m_lastPoint2);
    m_pinch.m_event.setCenter((p1 + p2) / 2);
    m_pinch.m_event.setAngle(m_pinch.m_lastAngle);
    m_pinch.m_event.setPoint1(p1);
    m_pinch.m_event.setPoint2(p2);
    m_pinch.m_event.setAccepted(true);
    m_pinch.m_event.setPointCount(0);
    emit tiltFinished(&m_pinch.m_event);
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::rotationStateMachine()
{
    RotationState lastState = m_rotationState;
    // Transitions:
    switch (m_rotationState) {
    case rotationInactive:
        if (m_allPoints.count() >= 2) {
            if (!isTiltActive() && canStartRotation()) {
                m_declarativeMap->setKeepMouseGrab(true);
                m_declarativeMap->setKeepTouchGrab(true);
                startRotation();
                setRotationState(rotationActive);
            } else {
                setRotationState(rotationInactiveTwoPoints);
            }
        }
        break;
    case rotationInactiveTwoPoints:
        if (m_allPoints.count() <= 1) {
            setRotationState(rotationInactive);
        } else {
            if (!isTiltActive() && canStartRotation()) {
                m_declarativeMap->setKeepMouseGrab(true);
                m_declarativeMap->setKeepTouchGrab(true);
                startRotation();
                setRotationState(rotationActive);
            }
        }
        break;
    case rotationActive:
        if (m_allPoints.count() <= 1) {
            setRotationState(rotationInactive);
            m_declarativeMap->setKeepMouseGrab(m_preventStealing);
            m_declarativeMap->setKeepTouchGrab(m_preventStealing);
            endRotation();
        }
        break;
    }
    // This line implements an exclusive state machine, where the transitions and updates don't
    // happen on the same frame
    if (m_rotationState != lastState) {
        emit rotationActiveChanged();
        return;
    }

    // Update
    switch (m_rotationState) {
    case rotationInactive:
    case rotationInactiveTwoPoints:
        break; // do nothing
    case rotationActive:
        updateRotation();
        break;
    }
}

/*!
    \internal
*/
bool QQuickGeoMapGestureArea::canStartRotation()
{
    if (m_allPoints.count() >= 2) {
        QPointF p1 = mapFromScene(m_allPoints.at(0).scenePos());
        QPointF p2 = mapFromScene(m_allPoints.at(1).scenePos());
        if (pointDragged(m_sceneStartPoint1, p1) || pointDragged(m_sceneStartPoint2, p2)) {
            qreal delta = angleDelta(m_twoTouchAngleStart, m_twoTouchAngle);
            if (qAbs(delta) < MinimumRotationStartingAngle) {
                return false;
            }
            m_pinch.m_event.setCenter(mapFromScene(m_touchPointsCentroid));
            m_pinch.m_event.setAngle(m_twoTouchAngle);
            m_pinch.m_event.setPoint1(p1);
            m_pinch.m_event.setPoint2(p2);
            m_pinch.m_event.setPointCount(m_allPoints.count());
            m_pinch.m_event.setAccepted(true);
            emit rotationStarted(&m_pinch.m_event);
            return m_pinch.m_event.accepted();
        }
    }
    return false;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::startRotation()
{
    m_pinch.m_rotation.m_startBearing = m_declarativeMap->bearing();
    m_pinch.m_rotation.m_previousTouchAngle = m_twoTouchAngle;
    m_pinch.m_rotation.m_totalAngle = 0.0;
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::updateRotation()
{
    // Calculate the new bearing
    qreal angle = angleDelta(m_pinch.m_rotation.m_previousTouchAngle, m_twoTouchAngle);
    if (qAbs(angle) < 0.2) // avoiding too many updates
        return;

    m_pinch.m_rotation.m_previousTouchAngle = m_twoTouchAngle;
    m_pinch.m_rotation.m_totalAngle += angle;
    qreal newBearing = m_pinch.m_rotation.m_startBearing - m_pinch.m_rotation.m_totalAngle;
    m_declarativeMap->setBearing(newBearing);

    m_pinch.m_event.setCenter(mapFromScene(m_touchPointsCentroid));
    m_pinch.m_event.setAngle(m_twoTouchAngle);
    m_pinch.m_lastPoint1 = mapFromScene(m_allPoints.at(0).scenePos());
    m_pinch.m_lastPoint2 = mapFromScene(m_allPoints.at(1).scenePos());
    m_pinch.m_event.setPoint1(m_pinch.m_lastPoint1);
    m_pinch.m_event.setPoint2(m_pinch.m_lastPoint2);
    m_pinch.m_event.setPointCount(m_allPoints.count());
    m_pinch.m_event.setAccepted(true);

    emit rotationUpdated(&m_pinch.m_event);
}

/*!
    \internal
*/
void QQuickGeoMapGestureArea::endRotation()
{
    QPointF p1 = mapFromScene(m_pinch.m_lastPoint1);
    QPointF p2 = mapFromScene(m_pinch.m_lastPoint2);
    m_pinch.m_event.setCenter((p1 + p2) / 2);
    m_pinch.m_event.setAngle(m_pinch.m_lastAngle);
    m_pinch.m_event.setPoint1(p1);
    m_pinch.m_event.setPoint2(p2);
    m_pinch.m_event.setAccepted(true);
    m_pinch.m_event.setPointCount(0);
    emit rotationFinished(&m_pinch.m_event);
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
            if (!isTiltActive() && canStartPinch()) {
                m_declarativeMap->setKeepMouseGrab(true);
                m_declarativeMap->setKeepTouchGrab(true);
                startPinch();
                setPinchState(pinchActive);
            } else {
                setPinchState(pinchInactiveTwoPoints);
            }
        }
        break;
    case pinchInactiveTwoPoints:
        if (m_allPoints.count() <= 1) {
            setPinchState(pinchInactive);
        } else {
            if (!isTiltActive() && canStartPinch()) {
                m_declarativeMap->setKeepMouseGrab(true);
                m_declarativeMap->setKeepTouchGrab(true);
                startPinch();
                setPinchState(pinchActive);
            }
        }
        break;
    case pinchActive:
        if (m_allPoints.count() <= 1) { // Once started, pinch goes off only when finger(s) are release
            setPinchState(pinchInactive);
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
    if (m_allPoints.count() >= 2) {
        QPointF p1 = mapFromScene(m_allPoints.at(0).scenePos());
        QPointF p2 = mapFromScene(m_allPoints.at(1).scenePos());
        if (qAbs(m_distanceBetweenTouchPoints - m_distanceBetweenTouchPointsStart) > MinimumPinchDelta) {
            m_pinch.m_event.setCenter(mapFromScene(m_touchPointsCentroid));
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

    m_pinch.m_event.setCenter(mapFromScene(m_touchPointsCentroid));
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
        m_declarativeMap->setZoomLevel(qMin<qreal>(newZoomLevel, maximumZoomLevel()), false);
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
        if (!isTiltActive() && canStartPan()) {
            // Update startCoord_ to ensure smooth start for panning when going over startDragDistance
            QGeoCoordinate newStartCoord = m_map->geoProjection().itemPositionToCoordinate(QDoubleVector2D(m_touchPointsCentroid), false);
            m_startCoord.setLongitude(newStartCoord.longitude());
            m_startCoord.setLatitude(newStartCoord.latitude());
            m_declarativeMap->setKeepMouseGrab(true);
            setFlickState(panActive);
        }
        break;
    case panActive:
        if (m_allPoints.count() == 0) {
            if (!tryStartFlick())
            {
                setFlickState(flickInactive);
                // mark as inactive for use by camera
                if (m_pinchState == pinchInactive && m_rotationState == rotationInactive && m_tiltState == tiltInactive) {
                    m_declarativeMap->setKeepMouseGrab(m_preventStealing);
                    m_map->prefetchData();
                }
                emit panFinished();
            } else {
                setFlickState(flickActive);
                emit panFinished();
                emit flickStarted();
            }
        }
        break;
    case flickActive:
        if (m_allPoints.count() > 0) { // re touched before movement ended
            stopFlick();
            m_declarativeMap->setKeepMouseGrab(true);
            setFlickState(panActive);
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
    QGeoCoordinate animationStartCoordinate = anchorCoordinateToPoint(*m_map, m_startCoord, m_touchPointsCentroid);
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
    qreal flickSpeed = 0.0;
    if (m_lastPosTime.elapsed() < QML_MAP_FLICK_VELOCITY_SAMPLE_PERIOD)
        flickSpeed = m_flickVector.length();

    int flickTime = 0;
    int flickPixels = 0;
    QVector2D flickVector;

    if (qAbs(flickSpeed) > MinimumFlickVelocity
            && distanceBetweenTouchPoints(m_touchPointsCentroid, m_sceneStartPoint1) > FlickThreshold) {
        qreal acceleration = m_flick.m_deceleration;
        if ((flickSpeed > 0.0f) == (m_flick.m_deceleration > 0.0f))
            acceleration = acceleration * -1.0f;
        flickTime = static_cast<int>(-1000 * flickSpeed / acceleration);
        flickPixels = (flickTime * flickSpeed) / 2000.0;
        flickVector = m_flickVector.normalized() * flickPixels;
    }

    if (flickTime > 0) {
        startFlick(flickVector.x(), flickVector.y(), flickTime);
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

    QPointF delta(dx, dy);
    QMatrix4x4 matBearing;
    matBearing.rotate(m_map->cameraData().bearing(), 0, 0, 1);
    delta = matBearing * delta;

    double zoom = pow(2.0, m_declarativeMap->zoomLevel());
    double longitude = animationStartCoordinate.longitude() - (delta.x() / zoom);
    double latitude = animationStartCoordinate.latitude() + (delta.y() / zoom);

    if (delta.x() > 0)
        m_flick.m_animation->setDirection(QQuickGeoCoordinateAnimation::East);
    else
        m_flick.m_animation->setDirection(QQuickGeoCoordinateAnimation::West);

    //keep animation in correct bounds
    animationEndCoordinate.setLongitude(QLocationUtils::wrapLong(longitude));
    animationEndCoordinate.setLatitude(QLocationUtils::clipLat(latitude, QLocationUtils::mercatorMaxLatitude()));

    m_flick.m_animation->setFrom(animationStartCoordinate);
    m_flick.m_animation->setTo(animationEndCoordinate);
    m_flick.m_animation->start();
}

void QQuickGeoMapGestureArea::stopPan()
{
    if (m_flickState == flickActive) {
        stopFlick();
    } else if (m_flickState == panActive) {
        m_flickVector = QVector2D();
        setFlickState(flickInactive);
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
    m_flickVector = QVector2D();
    if (m_flick.m_animation->isRunning())
        m_flick.m_animation->stop();
    else
        handleFlickAnimationStopped();
}

void QQuickGeoMapGestureArea::handleFlickAnimationStopped()
{
    m_declarativeMap->setKeepMouseGrab(m_preventStealing);
    if (m_flickState == flickActive) {
        setFlickState(flickInactive);
        emit flickFinished();
        emit panActiveChanged();
        m_map->prefetchData();
    }
}

QT_END_NAMESPACE
