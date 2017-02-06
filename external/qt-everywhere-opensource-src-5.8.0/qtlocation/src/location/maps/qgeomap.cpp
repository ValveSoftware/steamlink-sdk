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
#include <QDebug>

QT_BEGIN_NAMESPACE

QGeoMap::QGeoMap(QGeoMapPrivate &dd, QObject *parent)
    : QObject(dd,parent)
{
}

QGeoMap::~QGeoMap()
{
}

void QGeoMap::setViewportSize(const QSize& size)
{
    Q_D(QGeoMap);
    if (size == d->m_viewportSize)
        return;
    d->m_viewportSize = size;
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
    d->changeCameraData(cameraData);
    emit cameraDataChanged(d->m_cameraData);
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
    d->changeActiveMapType(type);
    emit activeMapTypeChanged();
}

const QGeoMapType QGeoMap::activeMapType() const
{
    Q_D(const QGeoMap);
    return d->m_activeMapType;
}


QGeoCameraCapabilities QGeoMap::cameraCapabilities() const
{
    Q_D(const QGeoMap);
    if (!d->m_engine.isNull())
        return d->m_engine->cameraCapabilities();
    else
        return QGeoCameraCapabilities();
}

void QGeoMap::prefetchData()
{

}

void QGeoMap::clearData()
{

}

QGeoMapPrivate::QGeoMapPrivate(QGeoMappingManagerEngine *engine)
    : QObjectPrivate(),
      m_engine(engine),
      m_activeMapType(QGeoMapType())
{
}

QGeoMapPrivate::~QGeoMapPrivate()
{
}

QT_END_NAMESPACE
