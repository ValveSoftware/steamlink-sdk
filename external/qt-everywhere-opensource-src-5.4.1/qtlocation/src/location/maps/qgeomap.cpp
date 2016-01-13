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

#include "qgeomap_p.h"
#include "qgeomapdata_p.h"

#include "qgeotilecache_p.h"
#include "qgeotilespec_p.h"

#include "qgeocameracapabilities_p.h"
#include "qgeomapcontroller_p.h"

#include <QtPositioning/private/qgeoprojection_p.h>
#include <QtPositioning/private/qdoublevector3d_p.h>

#include "qgeocameratiles_p.h"
#include "qgeotilerequestmanager_p.h"
#include "qgeomapscene_p.h"

#include "qgeomappingmanager_p.h"

#include <QMutex>
#include <QMap>

#include <qnumeric.h>

#include <QtQuick/QSGNode>

#include <cmath>

QT_BEGIN_NAMESPACE

QGeoMap::QGeoMap(QGeoMapData *mapData, QObject *parent)
    : QObject(parent),
      mapData_(mapData)
{
    connect(mapData_, SIGNAL(cameraDataChanged(QGeoCameraData)), this, SIGNAL(cameraDataChanged(QGeoCameraData)));
    connect(mapData_, SIGNAL(updateRequired()), this, SIGNAL(updateRequired()));
    connect(mapData_, SIGNAL(activeMapTypeChanged()), this, SIGNAL(activeMapTypeChanged()));
    connect(mapData_, SIGNAL(copyrightsChanged(QImage,QPoint)), this, SIGNAL(copyrightsChanged(QImage,QPoint)));
}

QGeoMap::~QGeoMap()
{
    delete mapData_;
}

QGeoMapController *QGeoMap::mapController()
{
    return mapData_->mapController();
}

QSGNode *QGeoMap::updateSceneGraph(QSGNode *oldNode, QQuickWindow *window)
{
    return mapData_->updateSceneGraph(oldNode, window);
}

void QGeoMap::resize(int width, int height)
{
    mapData_->resize(width, height);
}

int QGeoMap::width() const
{
    return mapData_->width();
}

int QGeoMap::height() const
{
    return mapData_->height();
}

QGeoCameraCapabilities QGeoMap::cameraCapabilities() const
{
    return mapData_->cameraCapabilities();
}

void QGeoMap::setCameraData(const QGeoCameraData &cameraData)
{
    mapData_->setCameraData(cameraData);
}

void QGeoMap::cameraStopped()
{
    mapData_->prefetchData();
}

QGeoCameraData QGeoMap::cameraData() const
{
    return mapData_->cameraData();
}

QGeoCoordinate QGeoMap::screenPositionToCoordinate(const QDoubleVector2D &pos, bool clipToViewport) const
{
    return mapData_->screenPositionToCoordinate(pos, clipToViewport);
}

QDoubleVector2D QGeoMap::coordinateToScreenPosition(const QGeoCoordinate &coordinate, bool clipToViewport) const
{
    return mapData_->coordinateToScreenPosition(coordinate, clipToViewport);
}

void QGeoMap::update()
{
    emit mapData_->update();
}

void QGeoMap::setActiveMapType(const QGeoMapType type)
{
    mapData_->setActiveMapType(type);
}

const QGeoMapType QGeoMap::activeMapType() const
{
    return mapData_->activeMapType();
}

QString QGeoMap::pluginString()
{
    return mapData_->pluginString();
}

QT_END_NAMESPACE
