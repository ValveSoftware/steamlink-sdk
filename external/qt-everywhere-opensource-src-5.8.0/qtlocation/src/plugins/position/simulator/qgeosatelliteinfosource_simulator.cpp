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
