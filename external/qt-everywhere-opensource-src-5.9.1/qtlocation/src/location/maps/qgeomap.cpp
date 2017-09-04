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

#include "qgeomap_p.h"
#include "qgeomap_p_p.h"
#include "qgeocameracapabilities_p.h"
#include "qgeomappingmanagerengine_p.h"
#include "qdeclarativegeomapitembase_p.h"
#include <QDebug>

QT_BEGIN_NAMESPACE

QGeoMap::QGeoMap(QGeoMapPrivate &dd, QObject *parent)
    : QObject(dd,parent)
{
}

QGeoMap::~QGeoMap()
{
    clearParameters();
}

void QGeoMap::setViewportSize(const QSize& size)
{
    Q_D(QGeoMap);
    if (size == d->m_viewportSize)
        return;
    d->m_viewportSize = size;
    d->m_geoProjection->setViewportSize(size);
    d->changeViewportSize(size);
}

QSize QGeoMap::viewportSize() const
{
    Q_D(const QGeoMap);
    return d->m_viewportSize;
}

int QGeoMap::viewportWidth() const
{
    Q_D(const QGeoMap);
    return d->m_viewportSize.width();
}

int QGeoMap::viewportHeight() const
{
    Q_D(const QGeoMap);
    return d->m_viewportSize.height();
}

void QGeoMap::setCameraData(const QGeoCameraData &cameraData)
{
    Q_D(QGeoMap);
    if (cameraData == d->m_cameraData)
        return;
    d->m_cameraData = cameraData;
    d->m_geoProjection->setCameraData(cameraData);
    d->changeCameraData(cameraData);
    emit cameraDataChanged(d->m_cameraData);
}

void QGeoMap::setCameraCapabilities(const QGeoCameraCapabilities &cameraCapabilities)
{
    Q_D(QGeoMap);
    d->setCameraCapabilities(cameraCapabilities);
}

QGeoCameraData QGeoMap::cameraData() const
{
    Q_D(const QGeoMap);
    return d->m_cameraData;
}

void QGeoMap::setActiveMapType(const QGeoMapType type)
{
    Q_D(QGeoMap);
    if (type == d->m_activeMapType)
        return;
    d->m_activeMapType = type;
    d->setCameraCapabilities(d->m_engine->cameraCapabilities(type.mapId())); // emits
    d->changeActiveMapType(type);
    emit activeMapTypeChanged();
}

const QGeoMapType QGeoMap::activeMapType() const
{
    Q_D(const QGeoMap);
    return d->m_activeMapType;
}

double QGeoMap::minimumZoom() const
{
    Q_D(const QGeoMap);
    return d->m_geoProjection->minimumZoom();
}

double QGeoMap::maximumCenterLatitudeAtZoom(const QGeoCameraData &cameraData) const
{
    Q_D(const QGeoMap);
    return d->m_geoProjection->maximumCenterLatitudeAtZoom(cameraData);
}

double QGeoMap::mapWidth() const
{
    Q_D(const QGeoMap);
    return d->m_geoProjection->mapWidth();
}

double QGeoMap::mapHeight() const
{
    Q_D(const QGeoMap);
    return d->m_geoProjection->mapHeight();
}

const QGeoProjection &QGeoMap::geoProjection() const
{
    Q_D(const QGeoMap);
    return *(d->m_geoProjection);
}

QGeoCameraCapabilities QGeoMap::cameraCapabilities() const
{
    Q_D(const QGeoMap);
    return d->m_cameraCapabilities;
}

void QGeoMap::prefetchData()
{

}

void QGeoMap::clearData()
{

}

void QGeoMap::addParameter(QGeoMapParameter *param)
{
    Q_D(QGeoMap);
    if (param && !d->m_mapParameters.contains(param)) {
        d->m_mapParameters.append(param);
        d->addParameter(param);
    }
}

void QGeoMap::removeParameter(QGeoMapParameter *param)
{
    Q_D(QGeoMap);
    if (param && d->m_mapParameters.contains(param)) {
        d->removeParameter(param);
        d->m_mapParameters.removeOne(param);
    }
}

void QGeoMap::clearParameters()
{
    Q_D(QGeoMap);
    for (QGeoMapParameter *p : d->m_mapParameters)
        d->removeParameter(p);
    d->m_mapParameters.clear();
}

QGeoMap::ItemTypes QGeoMap::supportedMapItemTypes() const
{
    Q_D(const QGeoMap);
    return d->supportedMapItemTypes();
}

void QGeoMap::addMapItem(QDeclarativeGeoMapItemBase *item)
{
    Q_D(QGeoMap);
    if (item && !d->m_mapItems.contains(item) && d->supportedMapItemTypes() & item->itemType()) {
        d->m_mapItems.append(item);
        d->addMapItem(item);
    }
}

void QGeoMap::removeMapItem(QDeclarativeGeoMapItemBase *item)
{
    Q_D(QGeoMap);
    if (item && d->m_mapItems.contains(item)) {
        d->removeMapItem(item);
        d->m_mapItems.removeOne(item);
    }
}

void QGeoMap::clearMapItems()
{
    Q_D(QGeoMap);
    for (QDeclarativeGeoMapItemBase *p : d->m_mapItems)
        d->removeMapItem(p);
    d->m_mapItems.clear();
}

QString QGeoMap::copyrightsStyleSheet() const
{
    return QStringLiteral("#copyright-root { background: rgba(255, 255, 255, 128) }");
}

QGeoMapPrivate::QGeoMapPrivate(QGeoMappingManagerEngine *engine, QGeoProjection *geoProjection)
    : QObjectPrivate(),
      m_geoProjection(geoProjection),
      m_engine(engine),
      m_activeMapType(QGeoMapType())
{
    // Setting the default camera caps without emitting anything
    if (engine)
        m_cameraCapabilities = m_engine->cameraCapabilities(m_activeMapType.mapId());
}

QGeoMapPrivate::~QGeoMapPrivate()
{
    if (m_geoProjection)
        delete m_geoProjection;
}

void QGeoMapPrivate::setCameraCapabilities(const QGeoCameraCapabilities &cameraCapabilities)
{
    Q_Q(QGeoMap);
    if (m_cameraCapabilities == cameraCapabilities)
        return;
    QGeoCameraCapabilities oldCaps = m_cameraCapabilities;
    m_cameraCapabilities = cameraCapabilities;
    emit q->cameraCapabilitiesChanged(oldCaps);
}

const QGeoCameraCapabilities &QGeoMapPrivate::cameraCapabilities() const
{
    return m_cameraCapabilities;
}

void QGeoMapPrivate::addParameter(QGeoMapParameter *param)
{
    Q_UNUSED(param)
}

void QGeoMapPrivate::removeParameter(QGeoMapParameter *param)
{
    Q_UNUSED(param)
}

QGeoMap::ItemTypes QGeoMapPrivate::supportedMapItemTypes() const
{
    return QGeoMap::NoItem;
}

void QGeoMapPrivate::addMapItem(QDeclarativeGeoMapItemBase *item)
{
    Q_UNUSED(item)
}

void QGeoMapPrivate::removeMapItem(QDeclarativeGeoMapItemBase *item)
{
    Q_UNUSED(item)
}

QT_END_NAMESPACE
