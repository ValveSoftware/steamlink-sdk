/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore>
#include <QtTest>

#include "logfilepositionsource.h"

LogFilePositionSource::LogFilePositionSource(QObject *parent)
    : QGeoPositionInfoSource(parent),
      logFile(new QFile(this)),
      timer(new QTimer(this))
{
    connect(timer, SIGNAL(timeout()), this, SLOT(readNextPosition()));

    logFile->setFileName(QFINDTESTDATA("simplelog.txt"));
    if (!logFile->open(QIODevice::ReadOnly))
        qWarning() << "Error: cannot open source file" << logFile->fileName();
}

QGeoPositionInfo LogFilePositionSource::lastKnownPosition(bool /*fromSatellitePositioningMethodsOnly*/) const
{
    return lastPosition;
}

LogFilePositionSource::PositioningMethods LogFilePositionSource::supportedPositioningMethods() const
{
    return AllPositioningMethods;
}

int LogFilePositionSource::minimumUpdateInterval() const
{
    return 200;
}

void LogFilePositionSource::startUpdates()
{
    int interval = updateInterval();
    if (interval < minimumUpdateInterval())
        interval = minimumUpdateInterval();

    timer->start(interval);
}

void LogFilePositionSource::stopUpdates()
{
    timer->stop();
}

void LogFilePositionSource::requestUpdate(int /*timeout*/)
{
    // For simplicity, ignore timeout - assume that if data is not available
    // now, no data will be added to the file later
    if (logFile->canReadLine())
        readNextPosition();
    else
        emit updateTimeout();
}

void LogFilePositionSource::readNextPosition()
{
    QByteArray line = logFile->readLine().trimmed();
    if (!line.isEmpty()) {
        QList<QByteArray> data = line.split(' ');
        double latitude;
        double longitude;
        bool hasLatitude = false;
        bool hasLongitude = false;
        QDateTime timestamp = QDateTime::fromString(QString(data.value(0)), Qt::ISODate);
        latitude = data.value(1).toDouble(&hasLatitude);
        longitude = data.value(2).toDouble(&hasLongitude);

        if (hasLatitude && hasLongitude && timestamp.isValid()) {
            QGeoCoordinate coordinate(latitude, longitude);
            QGeoPositionInfo info(coordinate, timestamp);
            if (info.isValid()) {
                lastPosition = info;
                emit positionUpdated(info);
            }
        }
    }
}

QGeoPositionInfoSource::Error LogFilePositionSource::error() const
{
    return QGeoPositionInfoSource::NoError;
}
