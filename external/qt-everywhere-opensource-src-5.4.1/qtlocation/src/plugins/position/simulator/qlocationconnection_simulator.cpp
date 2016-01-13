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

#include "qlocationconnection_simulator_p.h"
#include "qgeopositioninfosource_simulator_p.h"
#include "qlocationdata_simulator_p.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QDataStream>
#include <QtCore/QEventLoop>

#include <QtNetwork/QLocalSocket>

#include <QtSimulator/connection.h>
#include <QtSimulator/version.h>
#include <QtSimulator/connectionworker.h>

QT_BEGIN_NAMESPACE

const QString simulatorName(QString::fromLatin1("QtSimulator_Mobility_ServerName1.3.0.0"));
const quint16 simulatorPort = 0xbeef + 1;

namespace Simulator
{
    LocationConnection::LocationConnection()
        : mConnection(new Connection(Connection::Client, simulatorName, simulatorPort, Version(1,3,0,0)))
    {
        qt_registerLocationTypes();
        mWorker = mConnection->connectToServer(Connection::simulatorHostName(true), simulatorPort);
        if (!mWorker)
            return;

        mWorker->addReceiver(this);

        // register for location notifications
        mWorker->call("setRequestsLocationInfo");
    }

    LocationConnection::~LocationConnection()
    {
        delete mWorker;
        delete mConnection;
    }

    bool LocationConnection::ensureSimulatorConnection()
    {
        static LocationConnection locationConnection;
        return locationConnection.mWorker;
    }

    void LocationConnection::initialLocationDataSent()
    {
        emit initialDataReceived();
    }

    void LocationConnection::setLocationData(const QGeoPositionInfoData &data)
    {
        *qtPositionInfo() = data;
    }

    void LocationConnection::setSatelliteData(const QGeoSatelliteInfoData &data)
    {
        *qtSatelliteInfo() = data;
    }

#include "moc_qlocationconnection_simulator_p.cpp"

} // namespace

QGeoPositionInfoData *qtPositionInfo()
{
    static QGeoPositionInfoData *positionInfo = 0;
    if (!positionInfo) {
        positionInfo = new QGeoPositionInfoData;
    }

    return positionInfo;
}

QGeoSatelliteInfoData *qtSatelliteInfo()
{
    static QGeoSatelliteInfoData *satelliteInfo = 0;
    if (!satelliteInfo) {
        satelliteInfo = new QGeoSatelliteInfoData;
    }

    return satelliteInfo;
}


QT_END_NAMESPACE
