/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSG module of the Qt Toolkit.
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

#include "qquickpincharea_p_p.h"
#include "qquickwindow.h"

#include <QtCore/qmath.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qstylehints.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformnativeinterface.h>
#include <private/qguiapplication_p.h>
#include <QVariant>

#include <float.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype PinchEvent
    \instantiates QQuickPinchEvent
    \inqmlmodule QtQuick
    \ingroup qtquick-input-events
    \brief For specifying information about a pinch event

    \b {The PinchEvent type was added in QtQuick 1.1}

    The \c center, \c startCenter, \c previousCenter properties provide the center position between the two touch points.

    The \c scale and \c previousScale properties provide the scale factor.

    The \c angle, \c previousAngle and \c rotation properties provide the angle between the two points and the amount of rotation.

    The \c point1, \c point2, \c startPoint1, \c startPoint2 properties provide the positions of the touch points.

    The \c accepted property may be set to false in the \c onPinchStarted handler if the gesture should not
    be handled.

    \sa PinchArea
*/

/*!
    \qmlproperty QPointF QtQuick::PinchEvent::center
    \qmlproperty QPointF QtQuick::PinchEvent::startCenter
    \qmlproperty QPointF QtQuick::PinchEvent::previousCenter

    These properties hold the position of the center point between the two touch points.

    \list
    \li \c center is the current center point
    \li \c previousCenter is the center point of the previous event.
    \li \c startCenter is the center point when the gesture began
    \endlist
*/

/*!
    \qmlproperty real QtQuick::PinchEvent::scale
    \qmlproperty real QtQuick::PinchEvent::previousScale

    These properties hold the scale factor determined by the change in distance between the two touch points.

    \list
    \li \c scale is the current scale factor.
    \li \c previousScale is the scale factor of the previous event.
    \endlist

    When a pinch gesture is started, the scale is \c 1.0.
*/

/*!
    \qmlproperty real QtQuick::PinchEvent::angle
    \qmlproperty real QtQuick::PinchEvent::previousAngle
    \qmlproperty real QtQuick::PinchEvent::rotation

    These properties hold the angle between the two touch points.

    \list
    \li \c angle is the current angle between the two points in the range -180 to 180.
    \li \c previousAngle is the angle of the previous event.
    \li \c rotation is the total rotation since the pinch gesture started.
    \endlist

    When a pinch gesture is started, the rotation is \c 0.0.
*/

/*!
    \qmlproperty QPointF QtQuick::PinchEvent::point1
    \qmlproperty QPointF QtQuick::PinchEvent::startPoint1
    \qmlproperty QPointF QtQuick::PinchEvent::point2
    \qmlproperty QPointF QtQuick::PinchEvent::startPoint2

    These properties provide the actual touch points generating the pinch.

    \list
    \li \c point1 and \c point2 hold the current positions of the points.
    \li \c startPoint1 and \c startPoint2 hold the positions of the points when the second point was touched.
    \endlist
*/

/*!
    \qmlproperty bool QtQuick::PinchEvent::accepted

    Setting this property to false in the \c PinchArea::onPinchStarted handler
    will result in no further pinch events being generated, and the gesture
    ignored.
*/

/*!
    \qmlproperty int QtQuick::PinchEvent::pointCount

    Holds the number of points currently touched.  The PinchArea will not react
    until two touch points have initited a gesture, but will remain active until
    all touch points have been released.
*/

QQuickPinch::QQuickPinch()
    : m_target(0), m_minScale(1.0), m_maxScale(1.0)
    , m_minRotation(0.0), m_maxRotation(0.0)
    , m_axis(NoDrag), m_xmin(-FLT_MAX), m_xmax(FLT_MAX)
    , m_ymin(-FLT_MAX), m_ymax(FLT_MAX), m_active(false)
{
}

QQuickPinchAreaPrivate::~QQuickPinchAreaPrivate()
{
    delete pinch;
}

/*!
    \qmltype PinchArea
    \instantiates QQuickPinchArea
    \inqmlmodule QtQuick
    \ingroup qtquick-input
    \inherits Item
    \brief Enables simple pinch gesture handling

    \b {The PinchArea type was added in QtQuick 1.1}

    A PinchArea is an invisible item that is typically used in conjunction with
    a visible item in order to provide pinch gesture handling for that item.

    The \l enabled property is used to enable and disable pinch handling for
    the proxied item. When disabled, the pinch area becomes transparent to
    mouse/touch events.

    PinchArea can be used in two ways:

    \list
    \li setting a \c pinch.target to provide automatic interaction with an item
    \li using the onPinchStarted, onPinchUpdated and onPinchFinished handlers
    \endlist

    Since Qt 5.5, PinchArea can react to native pinch gesture events from the
    operating system if available; otherwise it reacts only to touch events.

    \sa PinchEvent, QNativeGestureEvent, QTouchEvent
*/

/*!
    \qmlsignal QtQuick::PinchArea::pinchStarted()

    This signal is emitted when the pinch area detects that a pinch gesture has
    started: two touch points (fingers) have been detected, and they have moved
    beyond the \l {QStyleHints}{startDragDistance} threshold for the gesture to begin.

    The \l {PinchEvent}{pinch} parameter (not the same as the \l {PinchArea}{pinch}
    property) provides information about the pinch gesture, including the scale,
    center and angle of the pinch. At the time of the \c pinchStarted signal,
    these values are reset to the default values, regardless of the results
    from previous gestures: pinch.scale will be \c 1.0 and pinch.rotation will be \c 0.0.
    As the gesture progresses, \l pinchUpdated will report the deviation from those
    defaults.

    To ignore this gesture set the \c pinch.accepted property to false.  The gesture
    will be canceled and no further events will be sent.

    The corresponding handler is \c onPinchStarted.
*/

/*!
    \qmlsignal QtQuick::PinchArea::pinchUpdated()

    This signal is emitted when the pinch area detects that a pinch gesture has changed.

    The \l {PinchEvent}{pinch} parameter provides information about the pinch
    gesture, including the scale, center and angle of the pinch. These values
    reflect changes only since the beginning of the current gesture, and
    therefore are not limited by the minimum and maximum limits in the
    \l {PinchArea}{pinch} property.

    The corresponding handler is \c onPinchUpdated.
*/

/*!
    \qmlsignal QtQuick::PinchArea::pinchFinished()

    This signal is emitted when the pinch area detects that a pinch gesture has finished.

    The \l {PinchEvent}{pinch} parameter (not the same as the \l {PinchArea}{pinch}
    property) provides information about the pinch gesture, including the
    scale, center and angle of the pinch.

    The corresponding handler is \c onPinchFinished.
*/

/*!
    \qmlsignal QtQuick::PinchArea::smartZoom()
    \since 5.5

    This signal is emitted when the pinch area detects the smart zoom gesture.
    This gesture occurs only on certain operating systems such as \macos.

    The \l {PinchEvent}{pinch} parameter provides information about the pinch
    gesture, including the location where the gesture occurred.  \c pinch.scale
    will be greater than zero when the gesture indicates that the user wishes to
    enter smart zoom, and zero when exiting (even though typically the same gesture
    is used to toggle between the two states).

    The corresponding handler is \c onSmartZoom.
*/


/*!
    \qmlpropertygroup QtQuick::PinchArea::pinch
    \qmlproperty Item QtQuick::PinchArea::pinch.target
    \qmlproperty bool QtQuick::PinchArea::pinch.active
    \qmlproperty real QtQuick::PinchArea::pinch.minimumScale
    \qmlproperty real QtQuick::PinchArea::pinch.maximumScale
    \qmlproperty real QtQuick::PinchArea::pinch.minimumRotation
    \qmlproperty real QtQuick::PinchArea::pinch.maximumRotation
    \qmlproperty enumeration QtQuick::PinchArea::pinch.dragAxis
    \qmlproperty real QtQuick::PinchArea::pinch.minimumX
    \qmlproperty real QtQuick::PinchArea::pinch.maximumX
    \qmlproperty real QtQuick::PinchArea::pinch.minimumY
    \qmlproperty real QtQuick::PinchArea::pinch.maximumY

    \c pinch provides a convenient way to make an item react to pinch gestures.

    \list
    \li \c pinch.target specifies the id of the item to drag.
    \li \c pinch.active specifies if the target item is currently being dragged.
    \li \c pinch.minimumScale and \c pinch.maximumScale limit the range of the Item.scale property, but not the \c PinchEvent \l {PinchEvent}{scale} property.
    \li \c pinch.minimumRotation and \c pinch.maximumRotation limit the range of the Item.rotation property, but not the \c PinchEvent \l {PinchEvent}{rotation} property.
    \li \c pinch.dragAxis specifies whether dragging in not allowed (\c Pinch.NoDrag), can be done horizontally (\c Pinch.XAxis), vertically (\c Pinch.YAxis), or both (\c Pinch.XAndYAxis)
    \li \c pinch.minimum and \c pinch.maximum limit how far the target can be dragged along the corresponding axes.
    \endlist
*/

QQuickPinchArea::QQuickPinchArea(QQuickItem *parent)
  : QQuickItem(*(new QQuickPinchAreaPrivate), parent)
{
    Q_D(QQuickPinchArea);
    d->init();
#ifdef Q_OS_OSX
    setAcceptHoverEvents(true); // needed to enable touch events on mouse hover.
#endif
}

QQuickPinchArea::~QQuickPinchArea()
{
}
/*!
    \qmlproperty bool QtQuick::PinchArea::enabled
    This property holds whether the item accepts pinch gestures.

    This property defaults to true.
*/
bool QQuickPinchArea::isEnabled() const
{
    Q_D(const QQuickPinchArea);
    return d->enabled;
}

void QQuickPinchArea::setEnabled(bool a)
{
    Q_D(QQuickPinchArea);
    if (a != d->enabled) {
        d->enabled = a;
        emit enabledChanged();
    }
}

void QQuickPinchArea::touchEvent(QTouchEvent *event)
{
    Q_D(QQuickPinchArea);
    if (!d->enabled || !isVisible()) {
        QQuickItem::touchEvent(event);
        return;
    }

    // A common non-trivial starting scenario is the user puts down one finger,
    // then that finger remains stationary while putting down a second one.
    // However QQuickWindow will not send TouchUpdates for TouchPoints which
    // were not initially accepted; that would be inefficient and noisy.
    // So even if there is only one touchpoint so far, it's important to accept it
    // in order to get updates later on (and it's accepted by default anyway).
    // If the user puts down one finger, we're waiting for the other finger to drop.
    // Therefore updatePinch() must do the right thing for any combination of
    // points and states that may occur, and there is no reason to ignore any event.
    // One consequence though is that if PinchArea is on top of something else,
    // it's always going to accept the touches, and that means the item underneath
    // will not get them (unless the PA's parent is doing parent filtering,
    // as the Flickable does, for example).
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
        d->touchPoints.clear();
        for (int i = 0; i < event->touchPoints().count(); ++i) {
            if (!(event->touchPoints().at(i).state() & Qt::TouchPointReleased)) {
                d->touchPoints << event->touchPoints().at(i);
            }
        }
        updatePinch();
        break;
    case QEvent::TouchEnd:
        clearPinch();
        break;
    case QEvent::TouchCancel:
        cancelPinch();
        break;
    default:
        QQuickItem::touchEvent(event);
    }
}

void QQuickPinchArea::clearPinch()
{
    Q_D(QQuickPinchArea);

    d->touchPoints.clear();
    if (d->inPinch) {
        d->inPinch = false;
        QPointF pinchCenter = mapFromScene(d->sceneLastCenter);
        QQuickPinchEvent pe(pinchCenter, d->pinchLastScale, d->pinchLastAngle, d->pinchRotation);
        pe.setStartCenter(d->pinchStartCenter);
        pe.setPreviousCenter(pinchCenter);
        pe.setPreviousAngle(d->pinchLastAngle);
        pe.setPreviousScale(d->pinchLastScale);
        pe.setStartPoint1(mapFromScene(d->sceneStartPoint1));
        pe.setStartPoint2(mapFromScene(d->sceneStartPoint2));
        pe.setPoint1(mapFromScene(d->lastPoint1));
        pe.setPoint2(mapFromScene(d->lastPoint2));
        emit pinchFinished(&pe);
        if (d->pinch && d->pinch->target())
            d->pinch->setActive(false);
    }
    d->pinchStartDist = 0;
    d->pinchActivated = false;
    d->initPinch = false;
    d->pinchRejected = false;
    d->stealMouse = false;
    d->id1 = -1;
    QQuickWindow *win = window();
    if (win && win->mouseGrabberItem() == this)
        ungrabMouse();
    setKeepMouseGrab(false);
}

void QQuickPinchArea::cancelPinch()
{
    Q_D(QQuickPinchArea);

    d->touchPoints.clear();
    if (d->inPinch) {
        d->inPinch = false;
        QPointF pinchCenter = mapFromScene(d->sceneLastCenter);
        QQuickPinchEvent pe(d->pinchStartCenter, d->pinchStartScale, d->pinchStartAngle, d->pinchStartRotation);
        pe.setStartCenter(d->pinchStartCenter);
        pe.setPreviousCenter(pinchCenter);
        pe.setPreviousAngle(d->pinchLastAngle);
        pe.setPreviousScale(d->pinchLastScale);
        pe.setStartPoint1(mapFromScene(d->sceneStartPoint1));
        pe.setStartPoint2(mapFromScene(d->sceneStartPoint2));
        pe.setPoint1(pe.startPoint1());
        pe.setPoint2(pe.startPoint2());
        emit pinchFinished(&pe);

        d->pinchLastScale = d->pinchStartScale;
        d->sceneLastCenter = d->sceneStartCenter;
        d->pinchLastAngle = d->pinchStartAngle;
        d->lastPoint1 = pe.startPoint1();
        d->lastPoint2 = pe.startPoint2();
        updatePinchTarget();

        if (d->pinch && d->pinch->target())
            d->pinch->setActive(false);
    }
    d->pinchStartDist = 0;
    d->pinchActivated = false;
    d->initPinch = false;
    d->pinchRejected = false;
    d->stealMouse = false;
    d->id1 = -1;
    QQuickWindow *win = window();
    if (win && win->mouseGrabberItem() == this)
        ungrabMouse();
    setKeepMouseGrab(false);
}

void QQuickPinchArea::updatePinch()
{
    Q_D(QQuickPinchArea);

    QQuickWindow *win = window();

    if (d->touchPoints.count() < 2) {
        setKeepMouseGrab(false);
        QQuickWindow *c = window();
        if (c && c->mouseGrabberItem() == this)
            ungrabMouse();
    }

    if (d->touchPoints.count() == 0) {
        if (d->inPinch) {
            d->inPinch = false;
            QPointF pinchCenter = mapFromScene(d->sceneLastCenter);
            QQuickPinchEvent pe(pinchCenter, d->pinchLastScale, d->pinchLastAngle, d->pinchRotation);
            pe.setStartCenter(d->pinchStartCenter);
            pe.setPreviousCenter(pinchCenter);
            pe.setPreviousAngle(d->pinchLastAngle);
            pe.setPreviousScale(d->pinchLastScale);
            pe.setStartPoint1(mapFromScene(d->sceneStartPoint1));
            pe.setStartPoint2(mapFromScene(d->sceneStartPoint2));
            pe.setPoint1(mapFromScene(d->lastPoint1));
            pe.setPoint2(mapFromScene(d->lastPoint2));
            emit pinchFinished(&pe);
            d->pinchStartDist = 0;
            d->pinchActivated = false;
            if (d->pinch && d->pinch->target())
                d->pinch->setActive(false);
        }
        d->initPinch = false;
        d->pinchRejected = false;
        d->stealMouse = false;
        return;
    }

    QTouchEvent::TouchPoint touchPoint1 = d->touchPoints.at(0);
    QTouchEvent::TouchPoint touchPoint2 = d->touchPoints.at(d->touchPoints. count() >= 2 ? 1 : 0);

    if (touchPoint1.state() == Qt::TouchPointPressed)
        d->sceneStartPoint1 = touchPoint1.scenePos();

    if (touchPoint2.state() == Qt::TouchPointPressed)
        d->sceneStartPoint2 = touchPoint2.scenePos();

    QRectF bounds = clipRect();
    // Pinch is not started unless there are exactly two touch points
    // AND one or more of the points has just now been pressed (wasn't pressed already)
    // AND both points are inside the bounds.
    if (d->touchPoints.count() == 2
            && (touchPoint1.state() & Qt::TouchPointPressed || touchPoint2.state() & Qt::TouchPointPressed) &&
            bounds.contains(touchPoint1.pos()) && bounds.contains(touchPoint2.pos())) {
        d->id1 = touchPoint1.id();
        d->pinchActivated = true;
        d->initPinch = true;

        int touchMouseId = QQuickWindowPrivate::get(win)->touchMouseId;
        if (touchPoint1.id() == touchMouseId || touchPoint2.id() == touchMouseId) {
            if (win && win->mouseGrabberItem() != this) {
                grabMouse();
            }
        }
    }
    if (d->pinchActivated && !d->pinchRejected) {
        const int dragThreshold = QGuiApplication::styleHints()->startDragDistance();
        QPointF p1 = touchPoint1.scenePos();
        QPointF p2 = touchPoint2.scenePos();
        qreal dx = p1.x() - p2.x();
        qreal dy = p1.y() - p2.y();
        qreal dist = qSqrt(dx*dx + dy*dy);
        QPointF sceneCenter = (p1 + p2)/2;
        qreal angle = QLineF(p1, p2).angle();
        if (d->touchPoints.count() == 1) {
            // If we only have one point then just move the center
            if (d->id1 == touchPoint1.id())
                sceneCenter = d->sceneLastCenter + touchPoint1.scenePos() - d->lastPoint1;
            else
                sceneCenter = d->sceneLastCenter + touchPoint2.scenePos() - d->lastPoint2;
            angle = d->pinchLastAngle;
        }
        d->id1 = touchPoint1.id();
        if (angle > 180)
            angle -= 360;
        if (!d->inPinch || d->initPinch) {
            if (d->touchPoints.count() >= 2) {
                if (d->initPinch) {
                    if (!d->inPinch)
                        d->pinchStartDist = dist;
                    d->initPinch = false;
                }
                d->sceneStartCenter = sceneCenter;
                d->sceneLastCenter = sceneCenter;
                d->pinchStartCenter = mapFromScene(sceneCenter);
                d->pinchStartAngle = angle;
                d->pinchLastScale = 1.0;
                d->pinchLastAngle = angle;
                d->pinchRotation = 0.0;
                d->lastPoint1 = p1;
                d->lastPoint2 = p2;
                if (qAbs(dist - d->pinchStartDist) >= dragThreshold ||
                        (pinch()->axis() != QQuickPinch::NoDrag &&
                         (qAbs(p1.x()-d->sceneStartPoint1.x()) >= dragThreshold
                          || qAbs(p1.y()-d->sceneStartPoint1.y()) >= dragThreshold
                          || qAbs(p2.x()-d->sceneStartPoint2.x()) >= dragThreshold
                          || qAbs(p2.y()-d->sceneStartPoint2.y()) >= dragThreshold))) {
                    QQuickPinchEvent pe(d->pinchStartCenter, 1.0, angle, 0.0);
                    d->pinchStartDist = dist;
                    pe.setStartCenter(d->pinchStartCenter);
                    pe.setPreviousCenter(d->pinchStartCenter);
                    pe.setPreviousAngle(d->pinchLastAngle);
                    pe.setPreviousScale(d->pinchLastScale);
                    pe.setStartPoint1(mapFromScene(d->sceneStartPoint1));
                    pe.setStartPoint2(mapFromScene(d->sceneStartPoint2));
                    pe.setPoint1(mapFromScene(d->lastPoint1));
                    pe.setPoint2(mapFromScene(d->lastPoint2));
                    pe.setPointCount(d->touchPoints.count());
                    emit pinchStarted(&pe);
                    if (pe.accepted()) {
                        d->inPinch = true;
                        d->stealMouse = true;
                        if (win && win->mouseGrabberItem() != this)
                            grabMouse();
                        setKeepMouseGrab(true);
                        grabTouchPoints(QVector<int>() << touchPoint1.id() << touchPoint2.id());
                        d->inPinch = true;
                        d->stealMouse = true;
                        if (d->pinch && d->pinch->target()) {
                            d->pinchStartPos = pinch()->target()->position();
                            d->pinchStartScale = d->pinch->target()->scale();
                            d->pinchStartRotation = d->pinch->target()->rotation();
                            d->pinch->setActive(true);
                        }
                    } else {
                        d->pinchRejected = true;
                    }
                }
            }
        } else if (d->pinchStartDist > 0) {
            qreal scale = dist ? dist / d->pinchStartDist : d->pinchLastScale;
            qreal da = d->pinchLastAngle - angle;
            if (da > 180)
                da -= 360;
            else if (da < -180)
                da += 360;
            d->pinchRotation += da;
            QPointF pinchCenter = mapFromScene(sceneCenter);
            QQuickPinchEvent pe(pinchCenter, scale, angle, d->pinchRotation);
            pe.setStartCenter(d->pinchStartCenter);
            pe.setPreviousCenter(mapFromScene(d->sceneLastCenter));
            pe.setPreviousAngle(d->pinchLastAngle);
            pe.setPreviousScale(d->pinchLastScale);
            pe.setStartPoint1(mapFromScene(d->sceneStartPoint1));
            pe.setStartPoint2(mapFromScene(d->sceneStartPoint2));
            pe.setPoint1(touchPoint1.pos());
            pe.setPoint2(touchPoint2.pos());
            pe.setPointCount(d->touchPoints.count());
            d->pinchLastScale = scale;
            d->sceneLastCenter = sceneCenter;
            d->pinchLastAngle = angle;
            d->lastPoint1 = touchPoint1.scenePos();
            d->lastPoint2 = touchPoint2.scenePos();
            emit pinchUpdated(&pe);
            updatePinchTarget();
        }
    }
}

void QQuickPinchArea::updatePinchTarget()
{
    Q_D(QQuickPinchArea);
    if (d->pinch && d->pinch->target()) {
        qreal s = d->pinchStartScale * d->pinchLastScale;
        s = qMin(qMax(pinch()->minimumScale(),s), pinch()->maximumScale());
        pinch()->target()->setScale(s);
        QPointF pos = d->sceneLastCenter - d->sceneStartCenter + d->pinchStartPos;
        if (pinch()->axis() & QQuickPinch::XAxis) {
            qreal x = pos.x();
            if (x < pinch()->xmin())
                x = pinch()->xmin();
            else if (x > pinch()->xmax())
                x = pinch()->xmax();
            pinch()->target()->setX(x);
        }
        if (pinch()->axis() & QQuickPinch::YAxis) {
            qreal y = pos.y();
            if (y < pinch()->ymin())
                y = pinch()->ymin();
            else if (y > pinch()->ymax())
                y = pinch()->ymax();
            pinch()->target()->setY(y);
        }
        if (d->pinchStartRotation >= pinch()->minimumRotation()
                && d->pinchStartRotation <= pinch()->maximumRotation()) {
            qreal r = d->pinchRotation + d->pinchStartRotation;
            r = qMin(qMax(pinch()->minimumRotation(),r), pinch()->maximumRotation());
            pinch()->target()->setRotation(r);
        }
    }
}

bool QQuickPinchArea::childMouseEventFilter(QQuickItem *i, QEvent *e)
{
    Q_D(QQuickPinchArea);
    if (!d->enabled || !isVisible())
        return QQuickItem::childMouseEventFilter(i, e);
    switch (e->type()) {
    case QEvent::TouchBegin:
        clearPinch(); // fall through
    case QEvent::TouchUpdate: {
             QTouchEvent *touch = static_cast<QTouchEvent*>(e);
            d->touchPoints.clear();
            for (int i = 0; i < touch->touchPoints().count(); ++i)
                if (!(touch->touchPoints().at(i).state() & Qt::TouchPointReleased))
                    d->touchPoints << touch->touchPoints().at(i);
            updatePinch();
        }
        e->setAccepted(d->inPinch);
        return d->inPinch;
    case QEvent::TouchEnd:
        clearPinch();
        break;
    default:
        break;
    }

    return QQuickItem::childMouseEventFilter(i, e);
}

void QQuickPinchArea::geometryChanged(const QRectF &newGeometry,
                                            const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}

void QQuickPinchArea::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickItem::itemChange(change, value);
}

bool QQuickPinchArea::event(QEvent *event)
{
    Q_D(QQuickPinchArea);
    if (!d->enabled || !isVisible())
        return QQuickItem::event(event);

    switch (event->type()) {
#ifndef QT_NO_GESTURES
    case QEvent::NativeGesture: {
        QNativeGestureEvent *gesture = static_cast<QNativeGestureEvent *>(event);
        switch (gesture->gestureType()) {
        case Qt::BeginNativeGesture:
            clearPinch(); // probably not necessary; JIC
            d->pinchStartCenter = gesture->localPos();
            d->pinchStartAngle = 0.0;
            d->pinchStartRotation = 0.0;
            d->pinchRotation = 0.0;
            d->pinchStartScale = 1.0;
            d->pinchLastAngle = 0.0;
            d->pinchLastScale = 1.0;
            d->sceneStartPoint1 = gesture->windowPos();
            d->sceneStartPoint2 = gesture->windowPos(); // TODO we never really know
            d->lastPoint1 = gesture->windowPos();
            d->lastPoint2 = gesture->windowPos(); // TODO we never really know
            if (d->pinch && d->pinch->target()) {
                d->pinchStartPos = d->pinch->target()->position();
                d->pinchStartScale = d->pinch->target()->scale();
                d->pinchStartRotation = d->pinch->target()->rotation();
                d->pinch->setActive(true);
            }
            break;
        case Qt::EndNativeGesture:
            clearPinch();
            break;
        case Qt::ZoomNativeGesture: {
            qreal scale = d->pinchLastScale * (1.0 + gesture->value());
            QQuickPinchEvent pe(d->pinchStartCenter, scale, d->pinchLastAngle, 0.0);
            pe.setStartCenter(d->pinchStartCenter);
            pe.setPreviousCenter(d->pinchStartCenter);
            pe.setPreviousAngle(d->pinchLastAngle);
            pe.setPreviousScale(d->pinchLastScale);
            pe.setStartPoint1(mapFromScene(d->sceneStartPoint1));
            pe.setStartPoint2(mapFromScene(d->sceneStartPoint2));
            pe.setPoint1(mapFromScene(d->lastPoint1));
            pe.setPoint2(mapFromScene(d->lastPoint2));
            pe.setPointCount(2);
            d->pinchLastScale = scale;
            if (d->inPinch)
                emit pinchUpdated(&pe);
            else
                emit pinchStarted(&pe);
            d->inPinch = true;
            updatePinchTarget();
        } break;
        case Qt::SmartZoomNativeGesture: {
            if (gesture->value() > 0.0 && d->pinch && d->pinch->target()) {
                d->pinchStartPos = pinch()->target()->position();
                d->pinchStartCenter = mapToItem(pinch()->target()->parentItem(), pinch()->target()->boundingRect().center());
                d->pinchStartScale = d->pinch->target()->scale();
                d->pinchStartRotation = d->pinch->target()->rotation();
                d->pinchLastScale = d->pinchStartScale = d->pinch->target()->scale();
                d->pinchLastAngle = d->pinchStartRotation = d->pinch->target()->rotation();
            }
            QQuickPinchEvent pe(gesture->localPos(), gesture->value(), d->pinchLastAngle, 0.0);
            pe.setStartCenter(gesture->localPos());
            pe.setPreviousCenter(d->pinchStartCenter);
            pe.setPreviousAngle(d->pinchLastAngle);
            pe.setPreviousScale(d->pinchLastScale);
            pe.setStartPoint1(gesture->localPos());
            pe.setStartPoint2(gesture->localPos());
            pe.setPoint1(mapFromScene(gesture->windowPos()));
            pe.setPoint2(mapFromScene(gesture->windowPos()));
            pe.setPointCount(2);
            emit smartZoom(&pe);
        } break;
        case Qt::RotateNativeGesture: {
            qreal angle = d->pinchLastAngle + gesture->value();
            QQuickPinchEvent pe(d->pinchStartCenter, d->pinchLastScale, angle, 0.0);
            pe.setStartCenter(d->pinchStartCenter);
            pe.setPreviousCenter(d->pinchStartCenter);
            pe.setPreviousAngle(d->pinchLastAngle);
            pe.setPreviousScale(d->pinchLastScale);
            pe.setStartPoint1(mapFromScene(d->sceneStartPoint1));
            pe.setStartPoint2(mapFromScene(d->sceneStartPoint2));
            pe.setPoint1(mapFromScene(d->lastPoint1));
            pe.setPoint2(mapFromScene(d->lastPoint2));
            pe.setPointCount(2);
            d->pinchLastAngle = angle;
            if (d->inPinch)
                emit pinchUpdated(&pe);
            else
                emit pinchStarted(&pe);
            d->inPinch = true;
            d->pinchRotation = angle;
            updatePinchTarget();
        } break;
        default:
            return QQuickItem::event(event);
        }
    } break;
#endif // QT_NO_GESTURES
    case QEvent::Wheel:
        event->ignore();
        return false;
    default:
         return QQuickItem::event(event);
    }

    return true;
}

QQuickPinch *QQuickPinchArea::pinch()
{
    Q_D(QQuickPinchArea);
    if (!d->pinch)
        d->pinch = new QQuickPinch;
    return d->pinch;
}

QT_END_NAMESPACE

