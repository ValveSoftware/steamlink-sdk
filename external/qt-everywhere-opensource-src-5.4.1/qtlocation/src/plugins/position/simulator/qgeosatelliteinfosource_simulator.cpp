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

#include "qgeosatelliteinfosource_simulator_p.h"
#include "qlocationconnection_simulator_p.h"
#include "qlocationdata_simulator_p.h"

QT_BEGIN_NAMESPACE

QGeoSatelliteInfoSourceSimulator::QGeoSatelliteInfoSourceSimulator(QObject *parent)
    : QGeoSatelliteInfoSource(parent)
    , timer(new QTimer(this))
    , requestTimer(new QTimer(this))
{
    Simulator::LocationConnection::ensureSimulatorConnection();

    connect(timer, SIGNAL(timeout()), this, SLOT(updateData()));
    requestTimer->setSingleShot(true);
    connect(requestTimer, SIGNAL(timeout()), this, SLOT(updateData()));
}

bool QGeoSatelliteInfoSourceSimulator::isConnected() const
{
    return Simulator::LocationConnection::ensureSimulatorConnection();
}

void QGeoSatelliteInfoSourceSimulator::startUpdates()
{
    int interval = updateInterval();
    if (interval < minimumUpdateInterval())
        interval = minimumUpdateInterval();
    timer->setInterval(interval);
    timer->start();
}

void QGeoSatelliteInfoSourceSimulator::stopUpdates()
{
    timer->stop();
}

void QGeoSatelliteInfoSourceSimulator::requestUpdate(int timeout)
{
    if (!requestTimer->isActive()) {
        // Get a single update within timeframe
        if (timeout == 0)
            timeout = minimumUpdateInterval();

        if (timeout < minimumUpdateInterval())
            emit requestTimeout();
        else
            requestTimer->start(timeout);
    }
}

void QGeoSatelliteInfoSourceSimulator::setUpdateInterval(int msec)
{
    // msec should be equal to or larger than the minimum update interval; 0 is a special case
    // that currently behaves as if the interval is set to the minimum update interval
    if (msec != 0 && msec < minimumUpdateInterval())
        msec = minimumUpdateInterval();

    QGeoSatelliteInfoSource::setUpdateInterval(msec);
    if (timer->isActive()) {
        timer->setInterval(msec);
        timer->start();
    }
}

int QGeoSatelliteInfoSourceSimulator::minimumUpdateInterval() const
{
    return qtPositionInfo()->minimumInterval;
}

void QGeoSatelliteInfoSourceSimulator::updateData()
{
    QList<QGeoSatelliteInfo> satellitesInUse;
    QList<QGeoSatelliteInfo> satellitesInView;

    QGeoSatelliteInfoData *data = qtSatelliteInfo();
    for(int i = 0; i < data->satellites.count(); i++) {
        QGeoSatelliteInfoData::SatelliteInfo info = data->satellites.at(i);
        QGeoSatelliteInfo satInfo;
        satInfo.setAttribute(QGeoSatelliteInfo::Azimuth, info.azimuth);
        satInfo.setAttribute(QGeoSatelliteInfo::Elevation, info.elevation);
        satInfo.setSignalStrength(info.signalStrength);
        satInfo.setSatelliteSystem(static_cast<QGeoSatelliteInfo::SatelliteSystem>(info.satelliteSystem));
        satInfo.setSatelliteIdentifier(info.satelliteIdentifier);
        satellitesInView.append(satInfo);
        if (info.inUse)
            satellitesInUse.append(satInfo);
    }
    emit satellitesInViewUpdated(satellitesInView);
    emit satellitesInUseUpdated(satellitesInUse);
}

#include "moc_qgeosatelliteinfosource_simulator_p.cpp"
QT_END_NAMESPACE
