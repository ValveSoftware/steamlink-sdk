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

#ifndef QDECLARATIVEGEOMAPGESTUREAREA_P_H
#define QDECLARATIVEGEOMAPGESTUREAREA_P_H

#include <QtQml/qqml.h>
#include <QTouchEvent>
#include <QObject>
#include <QDebug>
#include <QElapsedTimer>
#include "qgeocoordinate.h"

QT_BEGIN_NAMESPACE

class QGraphicsSceneMouseEvent;
class QDeclarativeGeoMap;
class QTouchEvent;
class QWheelEvent;
class QGeoMap;
class QPropertyAnimation;

class QDeclarativeGeoMapPinchEvent : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QPointF center READ center)
    Q_PROPERTY(qreal angle READ angle)
    Q_PROPERTY(QPointF point1 READ point1)
    Q_PROPERTY(QPointF point2 READ point2)
    Q_PROPERTY(int pointCount READ pointCount)
    Q_PROPERTY(bool accepted READ accepted WRITE setAccepted)

public:
    QDeclarativeGeoMapPinchEvent(const QPointF &center, qreal angle,
                                 const QPointF &point1, const QPointF &point2,
                                 int pointCount = 0, bool accepted = true)
        : QObject(), center_(center), angle_(angle),
          point1_(point1), point2_(point2),
        pointCount_(pointCount), accepted_(accepted) {}
    QDeclarativeGeoMapPinchEvent()
        : QObject(),
          angle_(0.0),
          pointCount_(0),
          accepted_(true) {}

    QPointF center() const { return center_; }
    void setCenter(const QPointF &center) { center_ = center; }
    qreal angle() const { return angle_; }
    void setAngle(qreal angle) { angle_ = angle; }
    QPointF point1() const { return point1_; }
    void setPoint1(const QPointF &p) { point1_ = p; }
    QPointF point2() const { return point2_; }
    void setPoint2(const QPointF &p) { point2_ = p; }
    int pointCount() const { return pointCount_; }
    void setPointCount(int count) { pointCount_ = count; }
    bool accepted() const { return accepted_; }
    void setAccepted(bool a) { accepted_ = a; }

private:
    QPointF center_;
    qreal angle_;
    QPointF point1_;
    QPointF point2_;
    int pointCount_;
    bool accepted_;
};

// tbd: should we have a 'active' / 'moving' boolean attribute when pinch is active?

// class QDeclarativeGeoMapGestureArea: public QObject // supporting pinching, panning, flicking
class QDeclarativeGeoMapGestureArea: public QObject
{
    Q_OBJECT
    Q_ENUMS(ActiveGesture)
    Q_FLAGS(ActiveGestures)

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool pinchEnabled READ pinchEnabled WRITE setPinchEnabled NOTIFY pinchEnabledChanged)
    Q_PROPERTY(bool panEnabled READ panEnabled WRITE setPanEnabled NOTIFY panEnabledChanged)
    Q_PROPERTY(bool isPinchActive READ isPinchActive NOTIFY pinchActiveChanged)
    Q_PROPERTY(bool isPanActive READ isPanActive)
    Q_PROPERTY(ActiveGestures activeGestures READ activeGestures WRITE setActiveGestures NOTIFY activeGesturesChanged)
    Q_PROPERTY(qreal maximumZoomLevelChange READ maximumZoomLevelChange WRITE setMaximumZoomLevelChange NOTIFY maximumZoomLevelChangeChanged)
    Q_PROPERTY(qreal flickDeceleration READ flickDeceleration WRITE setFlickDeceleration NOTIFY flickDecelerationChanged)
public:
    QDeclarativeGeoMapGestureArea(QDeclarativeGeoMap *map, QObject *parent = 0);
    ~QDeclarativeGeoMapGestureArea();

    enum ActiveGesture {
        NoGesture = 0x0000,
        ZoomGesture = 0x0001,
        PanGesture = 0x0002,
        FlickGesture = 0x004
    };
    Q_DECLARE_FLAGS(ActiveGestures, ActiveGesture)

    ActiveGestures activeGestures() const;
    void setActiveGestures(ActiveGestures activeGestures);

    bool isPinchActive() const;
    void setPinchActive(bool active);
    bool isPanActive() const;

    bool enabled() const;
    void setEnabled(bool enabled);

    // backwards compatibility
    bool pinchEnabled() const;
    void setPinchEnabled(bool enabled);
    bool panEnabled() const;
    void setPanEnabled(bool enabled);

    qreal maximumZoomLevelChange() const;
    void setMaximumZoomLevelChange(qreal maxChange);

    qreal flickDeceleration() const;
    void setFlickDeceleration(qreal deceleration);

    void touchEvent(QTouchEvent *event);

    bool wheelEvent(QWheelEvent *event);

    bool mousePressEvent(QMouseEvent *event);
    bool mouseMoveEvent(QMouseEvent *event);
    bool mouseReleaseEvent(QMouseEvent *event);

    bool filterMapChildMouseEvent(QMouseEvent *event);
    bool filterMapChildTouchEvent(QTouchEvent *event);

    void setMinimumZoomLevel(qreal min);
    qreal minimumZoomLevel() const;

    void setMaximumZoomLevel(qreal max);
    qreal maximumZoomLevel() const;

    void setMap(QGeoMap *map);

Q_SIGNALS:
    void pinchActiveChanged();
    void enabledChanged();
    void maximumZoomLevelChangeChanged();
    void activeGesturesChanged();
    void flickDecelerationChanged();

    // backwards compatibility
    void pinchEnabledChanged();
    void panEnabledChanged();

    void pinchStarted(QDeclarativeGeoMapPinchEvent *pinch);
    void pinchUpdated(QDeclarativeGeoMapPinchEvent *pinch);
    void pinchFinished(QDeclarativeGeoMapPinchEvent *pinch);
    void panStarted();
    void panFinished();
    void flickStarted();
    void flickFinished();
    void movementStopped();

private:
    void update();

    // Create general data relating to the touch points
    void touchPointStateMachine();
    void startOneTouchPoint();
    void updateOneTouchPoint();
    void startTwoTouchPoints();
    void updateTwoTouchPoints();

    // All pinch related code, which encompasses zoom
    void pinchStateMachine();
    bool canStartPinch();
    void startPinch();
    void updatePinch();
    void endPinch();

    // Pan related code (regardles of number of touch points),
    // includes the flick based panning after letting go
    void panStateMachine();
    bool canStartPan();
    void updatePan();
    bool tryStartFlick();
    void startFlick(int dx, int dy, int timeMs = 0);
private Q_SLOTS:
    void endFlick();

private:
    void stopPan();
    void clearTouchData();
    void updateVelocityList(const QPointF &pos);

private:
    QGeoMap *map_;
    QDeclarativeGeoMap *declarativeMap_;
    bool enabled_;

    struct Pinch
    {
        Pinch() : enabled(true), startDist(0), lastAngle(0.0) {}

        QDeclarativeGeoMapPinchEvent event;
        bool enabled;
        struct Zoom
        {
            Zoom() : minimum(-1.0), maximum(-1.0), start(0.0), previous(0.0),
                     maximumChange(2.0) {}
            qreal minimum;
            qreal maximum;
            qreal start;
            qreal previous;
            qreal maximumChange;
        } zoom;

        QPointF lastPoint1;
        QPointF lastPoint2;
        qreal startDist;
        qreal lastAngle;
     } pinch_;

    ActiveGestures activeGestures_;

    struct Pan
    {
        qreal maxVelocity_;
        qreal deceleration_;
        QPropertyAnimation *animation_;
        bool enabled_;
    } pan_;

    // these are calculated regardless of gesture or number of touch points
    qreal velocityX_;
    qreal velocityY_;
    QElapsedTimer lastPosTime_;
    QPointF lastPos_;
    QList<QTouchEvent::TouchPoint> touchPoints_;
    QPointF sceneStartPoint1_;

    // only set when two points in contact
    QPointF sceneStartPoint2_;
    QGeoCoordinate startCoord_;
    QGeoCoordinate touchCenterCoord_;
    qreal twoTouchAngle_;
    qreal distanceBetweenTouchPoints_;
    QPointF sceneCenter_;

    // prototype state machine...
    enum TouchPointState
    {
        touchPoints0,
        touchPoints1,
        touchPoints2
    } touchPointState_;

    enum PinchState
    {
        pinchInactive,
        pinchActive
    } pinchState_;

    enum PanState
    {
        panInactive,
        panActive,
        panFlick
    } panState_;
};

QT_END_NAMESPACE
QML_DECLARE_TYPE(QDeclarativeGeoMapGestureArea);

#endif // QDECLARATIVEGEOMAPGESTUREAREA_P_H
