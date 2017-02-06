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

#include <QTimerEvent>
#include <QDebug>
#include <QtCore/qglobal.h>

#include "qgeopositioninfosource_cl_p.h"

#define MINIMUM_UPDATE_INTERVAL 1000

@interface PositionLocationDelegate : NSObject <CLLocationManagerDelegate>
{
    QGeoPositionInfoSourceCL *m_positionInfoSource;
}
@end

@implementation PositionLocationDelegate
- (id)initWithInfoSource:(QGeoPositionInfoSourceCL*) positionInfoSource
{
    self = [super init];
    if (self) {
        m_positionInfoSource = positionInfoSource;
    }
    return self;
}

- (void)locationManager:(CLLocationManager *)manager didUpdateToLocation:(CLLocation *)newLocation fromLocation:(CLLocation *)oldLocation
{
    Q_UNUSED(manager)
    Q_UNUSED(oldLocation)

    // Convert location timestamp to QDateTime
    QDateTime timeStamp;
    NSTimeInterval locationTimeStamp = [newLocation.timestamp timeIntervalSince1970];
    timeStamp.setTime_t((uint) locationTimeStamp);
    timeStamp.setTime(timeStamp.time().addMSecs((uint)(locationTimeStamp * 1000) % 1000));

    // Construct position info from location data
    QGeoPositionInfo location(QGeoCoordinate(newLocation.coordinate.latitude,
                                             newLocation.coordinate.longitude,
                                             newLocation.altitude),
                                             timeStamp);
    if (newLocation.horizontalAccuracy >= 0)
        location.setAttribute(QGeoPositionInfo::HorizontalAccuracy, newLocation.horizontalAccuracy);
    if (newLocation.verticalAccuracy >= 0)
        location.setAttribute(QGeoPositionInfo::VerticalAccuracy, newLocation.verticalAccuracy);
#ifndef Q_OS_TVOS
    if (newLocation.course >= 0)
        location.setAttribute(QGeoPositionInfo::Direction, newLocation.course);
    if (newLocation.speed >= 0)
        location.setAttribute(QGeoPositionInfo::GroundSpeed, newLocation.speed);
#endif

    m_positionInfoSource->locationDataAvailable(location);
}

- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error
{
    Q_UNUSED(manager)
    m_positionInfoSource->setError(QGeoPositionInfoSource::AccessError);

    qWarning() << QString::fromNSString([error localizedDescription]);

    if ([error code] == 0
        && QString::fromNSString([error domain]) == QStringLiteral("kCLErrorDomain"))
      qWarning() << "(is Wi-Fi turned on?)";
}
@end

QT_BEGIN_NAMESPACE

QGeoPositionInfoSourceCL::QGeoPositionInfoSourceCL(QObject *parent)
    : QGeoPositionInfoSource(parent)
    , m_locationManager(0)
    , m_started(false)
    , m_updateTimer(0)
    , m_updateTimeout(0)
    , m_positionError(QGeoPositionInfoSource::NoError)
{
}

QGeoPositionInfoSourceCL::~QGeoPositionInfoSourceCL()
{
    stopUpdates();
    [m_locationManager release];
}

void QGeoPositionInfoSourceCL::setUpdateInterval(int msec)
{
    // If msec is 0 we send updates as data becomes available, otherwise we force msec to be equal
    // to or larger than the minimum update interval.
    if (msec != 0 && msec < minimumUpdateInterval())
        msec = minimumUpdateInterval();

    QGeoPositionInfoSource::setUpdateInterval(msec);

    // Must timeout if update takes longer than specified interval
    m_updateTimeout = msec;
    if (m_started) setTimeoutInterval(m_updateTimeout);
}

bool QGeoPositionInfoSourceCL::enableLocationManager()
{
    if (!m_locationManager) {
        m_locationManager = [[CLLocationManager alloc] init];
        m_locationManager.desiredAccuracy = kCLLocationAccuracyBest;
        m_locationManager.delegate = [[PositionLocationDelegate alloc] initWithInfoSource:this];

        // These two methods are new in iOS 8. They require NSLocationAlwaysUsageDescription
        // and NSLocationWhenInUseUsageDescription to be set in Info.plist to work (methods are
        // noop if there are no such entries in plist).
        if ([m_locationManager respondsToSelector:@selector(requestAlwaysAuthorization)])
            [m_locationManager performSelector:@selector(requestAlwaysAuthorization)];
        if ([m_locationManager respondsToSelector:@selector(requestWhenInUseAuthorization)])
            [m_locationManager performSelector:@selector(requestWhenInUseAuthorization)];
    }

    return (m_locationManager != 0);
}

void QGeoPositionInfoSourceCL::setTimeoutInterval(int msec)
{
    // Start timeout timer
    if (m_updateTimer) killTimer(m_updateTimer);
    if (msec > 0) m_updateTimer = startTimer(msec);
    else m_updateTimer = 0;
}

void QGeoPositionInfoSourceCL::startUpdates()
{
    if (enableLocationManager()) {
#ifdef Q_OS_TVOS
        [m_locationManager requestLocation];    // service will run long enough for one location update
#else
        [m_locationManager startUpdatingLocation];
#endif
        m_started = true;

        setTimeoutInterval(m_updateTimeout);
    } else setError(QGeoPositionInfoSource::AccessError);
}

void QGeoPositionInfoSourceCL::stopUpdates()
{
    if (m_locationManager) {
        [m_locationManager stopUpdatingLocation];
        m_started = false;

        // Stop timeout timer
        setTimeoutInterval(0);
    } else setError(QGeoPositionInfoSource::AccessError);
}

void QGeoPositionInfoSourceCL::requestUpdate(int timeout)
{
    // Get a single update within timeframe
    if (timeout < minimumUpdateInterval() && timeout != 0)
        emit updateTimeout();
    else if (enableLocationManager()) {
        // This will force LM to generate a new update
        [m_locationManager stopUpdatingLocation];
#ifdef Q_OS_TVOS
        [m_locationManager requestLocation];    // service will run long enough for one location update
#else
        [m_locationManager startUpdatingLocation];
#endif

        setTimeoutInterval(timeout);
    } else setError(QGeoPositionInfoSource::AccessError);
}

void QGeoPositionInfoSourceCL::timerEvent( QTimerEvent * event )
{
    // Update timed out?
    if (event->timerId() == m_updateTimer) {
        emit updateTimeout();

        // Only timeout once since last data
        setTimeoutInterval(0);

        // Started for single update?
        if (!m_started) stopUpdates();
    }
}

QGeoPositionInfoSource::PositioningMethods QGeoPositionInfoSourceCL::supportedPositioningMethods() const
{
    // CoreLocation doesn't say which positioning method(s) it used
    return QGeoPositionInfoSource::AllPositioningMethods;
}

int QGeoPositionInfoSourceCL::minimumUpdateInterval() const
{
    return MINIMUM_UPDATE_INTERVAL;
}

void QGeoPositionInfoSourceCL::locationDataAvailable(QGeoPositionInfo location)
{
    // Signal position data available
    m_lastUpdate = location;
    emit positionUpdated(location);

    // Started for single update?
    if (!m_started) stopUpdates();
    // ...otherwise restart timeout timer
    else setTimeoutInterval(m_updateTimeout);
}

QGeoPositionInfo QGeoPositionInfoSourceCL::lastKnownPosition(bool fromSatellitePositioningMethodsOnly) const
{
    Q_UNUSED(fromSatellitePositioningMethodsOnly)

    return m_lastUpdate;
}

QGeoPositionInfoSource::Error QGeoPositionInfoSourceCL::error() const
{
    return m_positionError;
}

void QGeoPositionInfoSourceCL::setError(QGeoPositionInfoSource::Error positionError)
{
    m_positionError = positionError;
    emit QGeoPositionInfoSource::error(positionError);
}

#include "moc_qgeopositioninfosource_cl_p.cpp"

QT_END_NAMESPACE
