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

#include "qgeotilespec_p.h"
#include "qgeotilespec_p_p.h"

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QGeoTileSpec::QGeoTileSpec()
    : d(QSharedDataPointer<QGeoTileSpecPrivate>(new QGeoTileSpecPrivate())) {}

QGeoTileSpec::QGeoTileSpec(const QString &plugin, int mapId, int zoom, int x, int y, int version)
        : d(QSharedDataPointer<QGeoTileSpecPrivate>(new QGeoTileSpecPrivate(plugin, mapId, zoom, x, y, version))) {}

QGeoTileSpec::QGeoTileSpec(const QGeoTileSpec &other)
    : d(other.d) {}

QGeoTileSpec::~QGeoTileSpec() {
}

QGeoTileSpec &QGeoTileSpec::operator = (const QGeoTileSpec &other)
{
    if (this == &other)
        return *this;

    d = other.d;
    return *this;
}

QString QGeoTileSpec::plugin() const
{
    return d->plugin_;
}

void QGeoTileSpec::setZoom(int zoom)
{
    d->zoom_ = zoom;
}

int QGeoTileSpec::zoom() const
{
    return d->zoom_;
}

void QGeoTileSpec::setX(int x)
{
    d->x_ = x;
}

int QGeoTileSpec::x() const
{
    return d->x_;
}

void QGeoTileSpec::setY(int y)
{
    d->y_ = y;
}

int QGeoTileSpec::y() const
{
    return d->y_;
}

void QGeoTileSpec::setMapId(int mapId)
{
    d->mapId_ = mapId;
}

int QGeoTileSpec::mapId() const
{
    return d->mapId_;
}

void QGeoTileSpec::setVersion(int version)
{
    d->version_ = version;
}

int QGeoTileSpec::version() const
{
    return d->version_;
}

bool QGeoTileSpec::operator == (const QGeoTileSpec &rhs) const
{
    return (*(d.constData()) == *(rhs.d.constData()));
}

bool QGeoTileSpec::operator < (const QGeoTileSpec &rhs) const
{
    return (*(d.constData()) < *(rhs.d.constData()));
}

unsigned int qHash(const QGeoTileSpec &spec)
{
    unsigned int result = (qHash(spec.plugin()) * 13) % 31;
    result += ((spec.mapId() * 17) % 31) << 5;
    result += ((spec.zoom() * 19) % 31) << 10;
    result += ((spec.x() * 23) % 31) << 15;
    result += ((spec.y() * 29) % 31) << 20;
    result += (spec.version() % 3) << 25;
    return result;
}

QDebug operator<< (QDebug dbg, const QGeoTileSpec &spec)
{
    dbg << spec.plugin() << spec.mapId() << spec.zoom() << spec.x() << spec.y() << spec.version();
    return dbg;
}

QGeoTileSpecPrivate::QGeoTileSpecPrivate()
    : mapId_(0),
    zoom_(-1),
    x_(-1),
    y_(-1),
    version_(-1) {}

QGeoTileSpecPrivate::QGeoTileSpecPrivate(const QGeoTileSpecPrivate &other)
    : QSharedData(other),
      plugin_(other.plugin_),
      mapId_(other.mapId_),
      zoom_(other.zoom_),
      x_(other.x_),
      y_(other.y_),
      version_(other.version_) {}

QGeoTileSpecPrivate::QGeoTileSpecPrivate(const QString &plugin, int mapId, int zoom, int x, int y, int version)
    : plugin_(plugin),
      mapId_(mapId),
      zoom_(zoom),
      x_(x),
      y_(y),
      version_(version) {}

QGeoTileSpecPrivate::~QGeoTileSpecPrivate() {}

QGeoTileSpecPrivate &QGeoTileSpecPrivate::operator = (const QGeoTileSpecPrivate &other)
{
    if (this == &other)
        return *this;

    plugin_ = other.plugin_;
    mapId_ = other.mapId_;
    zoom_ = other.zoom_;
    x_ = other.x_;
    y_ = other.y_;
    version_ = other.version_;

    return *this;
}

bool QGeoTileSpecPrivate::operator == (const QGeoTileSpecPrivate &rhs) const
{
    if (plugin_ != rhs.plugin_)
        return false;

    if (mapId_ != rhs.mapId_)
        return false;

    if (zoom_ != rhs.zoom_)
        return false;

    if (x_ != rhs.x_)
        return false;

    if (y_ != rhs.y_)
        return false;

    if (version_ != rhs.version_)
        return false;

    return true;
}

bool QGeoTileSpecPrivate::operator < (const QGeoTileSpecPrivate &rhs) const
{
    if (plugin_ < rhs.plugin_)
        return true;
    if (plugin_ > rhs.plugin_)
        return false;

    if (mapId_ < rhs.mapId_)
        return true;
    if (mapId_ > rhs.mapId_)
        return false;

    if (zoom_ < rhs.zoom_)
        return true;
    if (zoom_ > rhs.zoom_)
        return false;

    if (x_ < rhs.x_)
        return true;
    if (x_ > rhs.x_)
        return false;

    if (y_ < rhs.y_)
        return true;
    if (y_ > rhs.y_)
        return false;

    return (version_ < rhs.version_);
}

QT_END_NAMESPACE
