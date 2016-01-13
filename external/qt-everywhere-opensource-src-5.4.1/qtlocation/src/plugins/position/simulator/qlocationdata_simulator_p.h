/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#ifndef QGEOPOSITIONINFODATA_SIMULATOR_P_H
#define QGEOPOSITIONINFODATA_SIMULATOR_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/QMetaType>
#include <QtCore/QDateTime>
#include <QtCore/QList>

QT_BEGIN_NAMESPACE

struct QGeoPositionInfoData
{
    QGeoPositionInfoData();

    // Coordinate information
    double latitude;
    double longitude;
    double altitude;

    // Attributes
    // ### transmit whether attributes are set or not
    qreal direction;
    qreal groundSpeed;
    qreal verticalSpeed;
    qreal magneticVariation;
    qreal horizontalAccuracy;
    qreal verticalAccuracy;

    // DateTime info
    QDateTime dateTime;

    int minimumInterval;
    bool enabled;
};

struct QGeoSatelliteInfoData
{
    struct SatelliteInfo
    {
        SatelliteInfo();

        // This enum duplicates the SatelliteSystem enum defined in qgeosatelliteinfo.h, which cannot be
        // included as this file must compile with Qt4 (it is used by Qt Simulator)
        enum SatelliteSystem
        {
            Undefined = 0x00,
            GPS = 0x01,
            GLONASS = 0x02
        };

        qreal azimuth;
        qreal elevation;
        int signalStrength;
        bool inUse;
        SatelliteSystem satelliteSystem;
        int satelliteIdentifier;
    };

    QList<SatelliteInfo> satellites;
};

void qt_registerLocationTypes();

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QGeoPositionInfoData)
Q_DECLARE_METATYPE(QGeoSatelliteInfoData)
Q_DECLARE_METATYPE(QGeoSatelliteInfoData::SatelliteInfo)

#endif // QGEOPOSITIONINFODATA_SIMULATOR_P_H
