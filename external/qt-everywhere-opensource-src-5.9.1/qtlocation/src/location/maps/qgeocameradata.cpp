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
#include "qgeocameradata_p.h"
#include <QtPositioning/private/qgeocoordinate_p.h>
#include <QtPositioning/private/qwebmercator_p.h>
#include <QtCore/QVariant>
#include <QtCore/QVariantAnimation>

QT_BEGIN_NAMESPACE

class QGeoCameraDataPrivate : public QSharedData
{
public:
    QGeoCameraDataPrivate();
    QGeoCameraDataPrivate(const QGeoCameraDataPrivate &rhs);

    QGeoCameraDataPrivate &operator = (const QGeoCameraDataPrivate &rhs);

    bool operator == (const QGeoCameraDataPrivate &rhs) const;

    QGeoCoordinate m_center;
    double m_bearing;
    double m_tilt;
    double m_roll;
    double m_fieldOfView;
    double m_zoomLevel;
};

QGeoCameraDataPrivate::QGeoCameraDataPrivate()
    : QSharedData(),
      m_center(0, 0),
      m_bearing(0.0),
      m_tilt(0.0),
      m_roll(0.0),
      m_fieldOfView(45.0),
      m_zoomLevel(0.0) {}

QGeoCameraDataPrivate::QGeoCameraDataPrivate(const QGeoCameraDataPrivate &rhs)
    : QSharedData(rhs),
      m_center(rhs.m_center),
      m_bearing(rhs.m_bearing),
      m_tilt(rhs.m_tilt),
      m_roll(rhs.m_roll),
      m_fieldOfView(rhs.m_fieldOfView),
      m_zoomLevel(rhs.m_zoomLevel) {}

QGeoCameraDataPrivate &QGeoCameraDataPrivate::operator = (const QGeoCameraDataPrivate &rhs)
{
    if (this == &rhs)
        return *this;

    m_center = rhs.m_center;
    m_bearing = rhs.m_bearing;
    m_tilt = rhs.m_tilt;
    m_roll = rhs.m_roll;
    m_fieldOfView = rhs.m_fieldOfView;
    m_zoomLevel = rhs.m_zoomLevel;

    return *this;
}

bool QGeoCameraDataPrivate::operator == (const QGeoCameraDataPrivate &rhs) const
{
    return ((m_center == rhs.m_center)
            && (m_bearing == rhs.m_bearing)
            && (m_tilt == rhs.m_tilt)
            && (m_roll == rhs.m_roll)
            && (m_fieldOfView == rhs.m_fieldOfView)
            && (m_zoomLevel == rhs.m_zoomLevel));
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
        QGeoCoordinate coordinateResult = QWebMercator::coordinateInterpolation(from, to, progress);
        result.setCenter(coordinateResult);
    }

    double sf = 1.0 - progress;
    double ef = progress;

    result.setBearing(sf * start.bearing() + ef * end.bearing());
    result.setTilt(sf * start.tilt() + ef * end.tilt());
    result.setRoll(sf * start.roll() + ef * end.roll());
    result.setFieldOfView(sf * start.fieldOfView() + ef * end.fieldOfView());
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
    d->m_center = center;
}

QGeoCoordinate QGeoCameraData::center() const
{
    return d->m_center;
}

void QGeoCameraData::setBearing(double bearing)
{
    d->m_bearing = bearing;
}

double QGeoCameraData::bearing() const
{
    return d->m_bearing;
}

void QGeoCameraData::setTilt(double tilt)
{
    d->m_tilt = tilt;
}

double QGeoCameraData::tilt() const
{
    return d->m_tilt;
}

void QGeoCameraData::setRoll(double roll)
{
    d->m_roll = roll;
}

double QGeoCameraData::roll() const
{
    return d->m_roll;
}

void QGeoCameraData::setFieldOfView(double fieldOfView)
{
    d->m_fieldOfView = fieldOfView;
}

double QGeoCameraData::fieldOfView() const
{
    return d->m_fieldOfView;
}

void QGeoCameraData::setZoomLevel(double zoomFactor)
{
    d->m_zoomLevel = zoomFactor;
}

double QGeoCameraData::zoomLevel() const
{
    return d->m_zoomLevel;
}

QT_END_NAMESPACE
