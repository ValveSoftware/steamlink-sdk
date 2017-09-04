/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "qlocationdata_simulator_p.h"

#include <QtCore/QDataStream>

QT_BEGIN_NAMESPACE

QGeoPositionInfoData::QGeoPositionInfoData()
    : latitude(0.0),
      longitude(0.0),
      altitude(0.0),
      direction(0.0),
      groundSpeed(0.0),
      verticalSpeed(0.0),
      magneticVariation(0.0),
      horizontalAccuracy(0.0),
      verticalAccuracy(0.0),
      dateTime(),
      minimumInterval(0),
      enabled(false) {}

QGeoSatelliteInfoData::SatelliteInfo::SatelliteInfo()
    : azimuth(0.0),
      elevation(0.0),
      signalStrength(0),
      inUse(false),
      satelliteSystem(Undefined),
      satelliteIdentifier(0) {}

void qt_registerLocationTypes()
{
    qRegisterMetaTypeStreamOperators<QGeoPositionInfoData>("QGeoPositionInfoData");
    qRegisterMetaTypeStreamOperators<QGeoSatelliteInfoData>("QGeoSatelliteInfoData");
    qRegisterMetaTypeStreamOperators<QGeoSatelliteInfoData::SatelliteInfo>("QGeoSatelliteInfoData::SatelliteInfo");
}

QDataStream &operator<<(QDataStream &out, const QGeoPositionInfoData &s)
{
    out << s.latitude << s.longitude << s.altitude;
    out << s.direction << s.groundSpeed << s.verticalSpeed << s.magneticVariation << s.horizontalAccuracy << s.verticalAccuracy;
    out << s.dateTime;
    out << s.minimumInterval << s.enabled;
    return out;
}

QDataStream &operator>>(QDataStream &in, QGeoPositionInfoData &s)
{
    in >> s.latitude >> s.longitude >> s.altitude;
    in >> s.direction >> s.groundSpeed >> s.verticalSpeed >> s.magneticVariation >> s.horizontalAccuracy >> s.verticalAccuracy;
    in >> s.dateTime;
    in >> s.minimumInterval >> s.enabled;
    return in;
}

QDataStream &operator<<(QDataStream &out, const QGeoSatelliteInfoData &s)
{
    out << s.satellites;
    return out;
}

QDataStream &operator>>(QDataStream &in, QGeoSatelliteInfoData &s)
{
    in >> s.satellites;
    return in;
}

QDataStream &operator<<(QDataStream &out, const QGeoSatelliteInfoData::SatelliteInfo &s)
{
    out << s.azimuth << s.elevation << s.signalStrength << s.inUse << static_cast<qint32>(s.satelliteSystem) << s.satelliteIdentifier;
    return out;
}

QDataStream &operator>>(QDataStream &in, QGeoSatelliteInfoData::SatelliteInfo &s)
{
    qint32 satelliteSystem;
    in >> s.azimuth >> s.elevation >> s.signalStrength >> s.inUse >> satelliteSystem >> s.satelliteIdentifier;
    s.satelliteSystem = static_cast<QGeoSatelliteInfoData::SatelliteInfo::SatelliteSystem>(satelliteSystem);
    return in;
}

QT_END_NAMESPACE
