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
#include "qgeocameradata_p.h"
#include <QtPositioning/private/qgeocoordinate_p.h>

#include <QVariant>
#include <QVariantAnimation>

#include <QMetaType>

#include <cmath>

QT_BEGIN_NAMESPACE

class QGeoCameraDataPrivate : public QSharedData
{
public:
    QGeoCameraDataPrivate();
    QGeoCameraDataPrivate(const QGeoCameraDataPrivate &rhs);

    QGeoCameraDataPrivate &operator = (const QGeoCameraDataPrivate &rhs);

    bool operator == (const QGeoCameraDataPrivate &rhs) const;

    QGeoCoordinate center_;
    double bearing_;
    double tilt_;
    double roll_;
    double zoomLevel_;
};

QGeoCameraDataPrivate::QGeoCameraDataPrivate()
    : QSharedData(),
      center_(-27.5, 153),
      bearing_(0.0),
      tilt_(0.0),
      roll_(0.0),
      zoomLevel_(9.0) {}

QGeoCameraDataPrivate::QGeoCameraDataPrivate(const QGeoCameraDataPrivate &rhs)
    : QSharedData(rhs),
      center_(rhs.center_),
      bearing_(rhs.bearing_),
      tilt_(rhs.tilt_),
      roll_(rhs.roll_),
      zoomLevel_(rhs.zoomLevel_) {}

QGeoCameraDataPrivate &QGeoCameraDataPrivate::operator = (const QGeoCameraDataPrivate &rhs)
{
    if (this == &rhs)
        return *this;

    center_ = rhs.center_;
    bearing_ = rhs.bearing_;
    tilt_ = rhs.tilt_;
    roll_ = rhs.roll_;
    zoomLevel_ = rhs.zoomLevel_;

    return *this;
}

bool QGeoCameraDataPrivate::operator == (const QGeoCameraDataPrivate &rhs) const
{
    return ((center_ == rhs.center_)
            && (bearing_ == rhs.bearing_)
            && (tilt_ == rhs.tilt_)
            && (roll_ == rhs.roll_)
            && (zoomLevel_ == rhs.zoomLevel_));
}

QVariant cameraInterpolator(const QGeoCameraData &start,
                            const QGeoCameraData &end,
                            qreal progress)
{
    QGeoCameraData result = start;
    QGeoCoordinate from = start.center();
    QGeoCoordinate to = end.center();

    if (from == to) {
        if (progress < 0.5) {
            result.setCenter(from);
        } else {
            result.setCenter(to);
        }
    }
    else {
        QGeoCoordinate coordinateResult = QGeoProjection::coordinateInterpolation(from, to, progress);
        result.setCenter(coordinateResult);
    }

    double sf = 1.0 - progress;
    double ef = progress;

    result.setBearing(sf * start.bearing() + ef * end.bearing());
    result.setTilt(sf * start.tilt() + ef * end.tilt());
    result.setRoll(sf * start.roll() + ef * end.roll());
    result.setZoomLevel(sf * start.zoomLevel() + ef * end.zoomLevel());

    return QVariant::fromValue(result);
}

QGeoCameraData::QGeoCameraData()
    : d(new QGeoCameraDataPrivate())
{
    qRegisterMetaType<QGeoCameraData>();
    qRegisterAnimationInterpolator<QGeoCameraData>(cameraInterpolator);
}

QGeoCameraData::QGeoCameraData(const QGeoCameraData &other)
    : d(other.d) {}

QGeoCameraData::~QGeoCameraData()
{
}

QGeoCameraData &QGeoCameraData::operator = (const QGeoCameraData &other)
{
    if (this == &other)
        return *this;

    d = other.d;
    return *this;
}

bool QGeoCameraData::operator == (const QGeoCameraData &rhs) const
{
    return (*(d.constData()) == *(rhs.d.constData()));
}

bool QGeoCameraData::operator != (const QGeoCameraData &other) const
{
    return !(operator==(other));
}

void QGeoCameraData::setCenter(const QGeoCoordinate &center)
{
    d->center_ = center;
}

QGeoCoordinate QGeoCameraData::center() const
{
    return d->center_;
}

void QGeoCameraData::setBearing(double bearing)
{
    d->bearing_ = bearing;
}

double QGeoCameraData::bearing() const
{
    return d->bearing_;
}

void QGeoCameraData::setTilt(double tilt)
{
    d->tilt_ = tilt;
}

double QGeoCameraData::tilt() const
{
    return d->tilt_;
}

void QGeoCameraData::setRoll(double roll)
{
    d->roll_ = roll;
}

double QGeoCameraData::roll() const
{
    return d->roll_;
}

void QGeoCameraData::setZoomLevel(double zoomFactor)
{
    d->zoomLevel_ = zoomFactor;
}

double QGeoCameraData::zoomLevel() const
{
    return d->zoomLevel_;
}

QT_END_NAMESPACE
