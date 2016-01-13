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

#include "qgeomapcontroller_p.h"

#include "qgeomapdata_p.h"
#include <QtPositioning/private/qgeoprojection_p.h>

#include <QPointF>

QT_BEGIN_NAMESPACE

QGeoMapController::QGeoMapController(QGeoMapData *map)
    : QObject(map),
      map_(map)
{
    oldCameraData_ = map_->cameraData();

    connect(map_,
            SIGNAL(cameraDataChanged(QGeoCameraData)),
            this,
            SLOT(cameraDataChanged(QGeoCameraData)));
}

QGeoMapController::~QGeoMapController() {}

void QGeoMapController::cameraDataChanged(const QGeoCameraData &cameraData)
{
    if (oldCameraData_.center() != cameraData.center())
        emit centerChanged(cameraData.center());

    if (oldCameraData_.bearing() != cameraData.bearing())
        emit bearingChanged(cameraData.bearing());

    if (oldCameraData_.tilt() != cameraData.tilt())
        emit tiltChanged(cameraData.tilt());

    if (oldCameraData_.roll() != cameraData.roll())
        emit rollChanged(cameraData.roll());

    if (oldCameraData_.zoomLevel() != cameraData.zoomLevel())
        emit zoomChanged(cameraData.zoomLevel());

    oldCameraData_ = cameraData;
}

QGeoCoordinate QGeoMapController::center() const
{
    return map_->cameraData().center();
}

void QGeoMapController::setCenter(const QGeoCoordinate &center)
{
    QGeoCameraData cd = map_->cameraData();

    if (center == cd.center())
        return;

    cd.setCenter(center);
    map_->setCameraData(cd);
}

void QGeoMapController::setLatitude(qreal latitude)
{
    QGeoCameraData cd = map_->cameraData();

    if (latitude == cd.center().latitude())
        return;

    QGeoCoordinate coord(latitude, cd.center().longitude(), cd.center().altitude());
    cd.setCenter(coord);
    map_->setCameraData(cd);
}

void QGeoMapController::setLongitude(qreal longitude)
{
    QGeoCameraData cd = map_->cameraData();

    if (longitude == cd.center().longitude())
        return;

    QGeoCoordinate coord(cd.center().latitude(), longitude, cd.center().altitude());
    cd.setCenter(coord);
    map_->setCameraData(cd);
}


void QGeoMapController::setAltitude(qreal altitude)
{
    QGeoCameraData cd = map_->cameraData();

    if (altitude == cd.center().altitude())
        return;

    QGeoCoordinate coord(cd.center().latitude(), cd.center().longitude(), altitude);
    cd.setCenter(coord);
    map_->setCameraData(cd);
}

qreal QGeoMapController::bearing() const
{
    return map_->cameraData().bearing();
}

void QGeoMapController::setBearing(qreal bearing)
{
    QGeoCameraData cd = map_->cameraData();

    if (bearing == cd.bearing())
        return;

    cd.setBearing(bearing);
    map_->setCameraData(cd);
}

qreal QGeoMapController::tilt() const
{
    return map_->cameraData().tilt();
}

void QGeoMapController::setTilt(qreal tilt)
{
    QGeoCameraData cd = map_->cameraData();

    if (tilt == cd.tilt())
        return;

    cd.setTilt(tilt);
    map_->setCameraData(cd);
}

qreal QGeoMapController::roll() const
{
    return map_->cameraData().roll();
}

void QGeoMapController::setRoll(qreal roll)
{
    QGeoCameraData cd = map_->cameraData();

    if (roll == cd.roll())
        return;

    cd.setRoll(roll);
    map_->setCameraData(cd);
}

qreal QGeoMapController::zoom() const
{
    return map_->cameraData().zoomLevel();
}

void QGeoMapController::setZoom(qreal zoom)
{
    QGeoCameraData cd = map_->cameraData();

    if (zoom == cd.zoomLevel())
        return;

    cd.setZoomLevel(zoom);
    map_->setCameraData(cd);
}

void QGeoMapController::pan(qreal dx, qreal dy)
{
    if (dx == 0 && dy == 0)
        return;
    QGeoCameraData cd = map_->cameraData();
    QGeoCoordinate coord = map_->screenPositionToCoordinate(
                                QDoubleVector2D(map_->width() / 2 + dx,
                                        map_->height() / 2 + dy));


    // keep altitude as it was
    coord.setAltitude(cd.center().altitude());
    if (coord.isValid()) {
        cd.setCenter(coord);
        map_->setCameraData(cd);
    }
}

QT_END_NAMESPACE
