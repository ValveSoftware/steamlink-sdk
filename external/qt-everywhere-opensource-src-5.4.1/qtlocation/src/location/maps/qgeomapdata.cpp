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
#include "qgeomapdata_p.h"
#include "qgeomapdata_p_p.h"
#include "qgeomap_p.h"

#include "qgeotilecache_p.h"
#include "qgeotilespec_p.h"
#include "qgeocameracapabilities_p.h"
#include "qgeomapcontroller_p.h"

#include "qgeocameratiles_p.h"
#include "qgeotilerequestmanager_p.h"
#include "qgeomapscene_p.h"

#include "qgeomappingmanager_p.h"


#include <QtPositioning/private/qgeoprojection_p.h>
#include <QtPositioning/private/qdoublevector2d_p.h>
#include <QtPositioning/private/qdoublevector3d_p.h>

#include <QMutex>
#include <QMap>

#include <qnumeric.h>

#include <cmath>

QT_BEGIN_NAMESPACE

QGeoMapData::QGeoMapData(QGeoMappingManagerEngine *engine, QObject *parent)
    : QObject(parent),
      d_ptr(new QGeoMapDataPrivate(engine, this)) {}

QGeoMapData::~QGeoMapData()
{
    delete d_ptr;
}

QGeoMapController *QGeoMapData::mapController()
{
    Q_D(QGeoMapData);
    return d->mapController();
}

void QGeoMapData::resize(int width, int height)
{
    Q_D(QGeoMapData);
    d->resize(width, height);

    // always emit this signal to trigger items to redraw
    emit cameraDataChanged(d->cameraData());
}

int QGeoMapData::width() const
{
    Q_D(const QGeoMapData);
    return d->width();
}

int QGeoMapData::height() const
{
    Q_D(const QGeoMapData);
    return d->height();
}

void QGeoMapData::setCameraData(const QGeoCameraData &cameraData)
{
    Q_D(QGeoMapData);
    if (cameraData == d->cameraData())
        return;

    d->setCameraData(cameraData);
    update();

    emit cameraDataChanged(d->cameraData());
}

QGeoCameraData QGeoMapData::cameraData() const
{
    Q_D(const QGeoMapData);
    return d->cameraData();
}

void QGeoMapData::update()
{
    emit updateRequired();
}

void QGeoMapData::setActiveMapType(const QGeoMapType type)
{
    Q_D(QGeoMapData);
    d->setActiveMapType(type);
}

const QGeoMapType QGeoMapData::activeMapType() const
{
    Q_D(const QGeoMapData);
    return d->activeMapType();
}

QString QGeoMapData::pluginString()
{
    Q_D(QGeoMapData);
    return d->pluginString();
}

QGeoCameraCapabilities QGeoMapData::cameraCapabilities()
{
    Q_D(QGeoMapData);
    if (d->engine())
        return d->engine()->cameraCapabilities();
    else
        return QGeoCameraCapabilities();
}

QGeoMappingManagerEngine *QGeoMapData::engine()
{
    Q_D(QGeoMapData);
    return d->engine();
}

QGeoMapDataPrivate::QGeoMapDataPrivate(QGeoMappingManagerEngine *engine, QGeoMapData *parent)
    : width_(0),
      height_(0),
      aspectRatio_(0.0),
      map_(parent),
      engine_(engine),
      controller_(0),
      activeMapType_(QGeoMapType())
{
    pluginString_ = engine_->managerName() + QLatin1Char('_') + QString::number(engine_->managerVersion());
}

QGeoMapDataPrivate::~QGeoMapDataPrivate()
{
    // controller_ is a child of map_, don't need to delete it here

    // TODO map items are not deallocated!
    // However: how to ensure this is done in rendering thread?
}

QGeoMappingManagerEngine *QGeoMapDataPrivate::engine() const
{
    return engine_;
}

QString QGeoMapDataPrivate::pluginString()
{
    return pluginString_;
}

QGeoMapController *QGeoMapDataPrivate::mapController()
{
    if (!controller_)
        controller_ = new QGeoMapController(map_);
    return controller_;
}

void QGeoMapDataPrivate::setCameraData(const QGeoCameraData &cameraData)
{
    QGeoCameraData oldCameraData = cameraData_;
    cameraData_ = cameraData;

    if (engine_) {
        QGeoCameraCapabilities capabilities = engine_->cameraCapabilities();
        if (cameraData_.zoomLevel() < capabilities.minimumZoomLevel())
            cameraData_.setZoomLevel(capabilities.minimumZoomLevel());

        if (cameraData_.zoomLevel() > capabilities.maximumZoomLevel())
            cameraData_.setZoomLevel(capabilities.maximumZoomLevel());

        if (!capabilities.supportsBearing())
            cameraData_.setBearing(0.0);

        if (capabilities.supportsTilting()) {
            if (cameraData_.tilt() < capabilities.minimumTilt())
                cameraData_.setTilt(capabilities.minimumTilt());

            if (cameraData_.tilt() > capabilities.maximumTilt())
                cameraData_.setTilt(capabilities.maximumTilt());
        } else {
            cameraData_.setTilt(0.0);
        }

        if (!capabilities.supportsRolling())
            cameraData_.setRoll(0.0);
    }

    // Do not call this expensive function if the width is 0, since it will get called
    // anyway when it is resized to a width > 0.
    // this is mainly an optimization to the initialization of the geomap, which would otherwise
    // call changeCameraData four or more times
    if (width() > 0)
        map_->changeCameraData(oldCameraData);
}

QGeoCameraData QGeoMapDataPrivate::cameraData() const
{
    return cameraData_;
}

void QGeoMapDataPrivate::resize(int width, int height)
{
    width_ = width;
    height_ = height;
    aspectRatio_ = 1.0 * width_ / height_;
    map_->mapResized(width, height);
    setCameraData(cameraData_);
}

int QGeoMapDataPrivate::width() const
{
    return width_;
}

int QGeoMapDataPrivate::height() const
{
    return height_;
}

double QGeoMapDataPrivate::aspectRatio() const
{
    return aspectRatio_;
}

void QGeoMapDataPrivate::setActiveMapType(const QGeoMapType &type)
{
    activeMapType_ = type;

    map_->changeActiveMapType(type);
    setCameraData(cameraData_);

    map_->update();
}

const QGeoMapType QGeoMapDataPrivate::activeMapType() const
{
  return activeMapType_;
}

QT_END_NAMESPACE
