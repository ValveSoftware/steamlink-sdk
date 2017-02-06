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

#include "qgeopositioninfosource_android_p.h"
#include "jnipositioning.h"
//#include <QDebug>
#include <QGeoPositionInfo>

Q_DECLARE_METATYPE(QGeoPositionInfo)
#define UPDATE_FROM_COLD_START 2*60*1000


QGeoPositionInfoSourceAndroid::QGeoPositionInfoSourceAndroid(QObject *parent) :
    QGeoPositionInfoSource(parent), updatesRunning(false), m_error(NoError)
{
    qRegisterMetaType< QGeoPositionInfo >();
    androidClassKeyForUpdate = AndroidPositioning::registerPositionInfoSource(this);
    androidClassKeyForSingleRequest = AndroidPositioning::registerPositionInfoSource(this);

    //qDebug() << "androidClassKey: "  << androidClassKeyForUpdate << androidClassKeyForSingleRequest;
    //by default use all methods
    setPreferredPositioningMethods(AllPositioningMethods);

    m_requestTimer.setSingleShot(true);
    QObject::connect(&m_requestTimer, SIGNAL(timeout()), this, SLOT(requestTimeout()));
}

QGeoPositionInfoSourceAndroid::~QGeoPositionInfoSourceAndroid()
{
    stopUpdates();

    if (m_requestTimer.isActive()) {
        m_requestTimer.stop();
        AndroidPositioning::stopUpdates(androidClassKeyForSingleRequest);
    }

    AndroidPositioning::unregisterPositionInfoSource(androidClassKeyForUpdate);
    AndroidPositioning::unregisterPositionInfoSource(androidClassKeyForSingleRequest);
}

void QGeoPositionInfoSourceAndroid::setUpdateInterval(int msec)
{
    int previousInterval = updateInterval();
    msec = (((msec > 0) && (msec < minimumUpdateInterval())) || msec < 0)? minimumUpdateInterval() : msec;

    if (msec == previousInterval)
        return;

    QGeoPositionInfoSource::setUpdateInterval(msec);

    if (updatesRunning)
        reconfigureRunningSystem();
}

QGeoPositionInfo QGeoPositionInfoSourceAndroid::lastKnownPosition(bool fromSatellitePositioningMethodsOnly) const
{
    return AndroidPositioning::lastKnownPosition(fromSatellitePositioningMethodsOnly);
}

QGeoPositionInfoSource::PositioningMethods QGeoPositionInfoSourceAndroid::supportedPositioningMethods() const
{
    return AndroidPositioning::availableProviders();
}

void QGeoPositionInfoSourceAndroid::setPreferredPositioningMethods(QGeoPositionInfoSource::PositioningMethods methods)
{
    PositioningMethods previousPreferredPositioningMethods = preferredPositioningMethods();
    QGeoPositionInfoSource::setPreferredPositioningMethods(methods);
    if (previousPreferredPositioningMethods == preferredPositioningMethods())
        return;

    if (updatesRunning)
        reconfigureRunningSystem();
}

int QGeoPositionInfoSourceAndroid::minimumUpdateInterval() const
{
    return 50;
}

QGeoPositionInfoSource::Error QGeoPositionInfoSourceAndroid::error() const
{
    return m_error;
}

void QGeoPositionInfoSourceAndroid::setError(Error error)
{
    // qDebug() << "setError: " << error;
    if (error != QGeoPositionInfoSource::NoError)
    {
        m_error = error;
        emit QGeoPositionInfoSource::error(m_error);
    }
}

void QGeoPositionInfoSourceAndroid::startUpdates()
{
    if (updatesRunning)
        return;

    if (preferredPositioningMethods() == 0) {
        setError(UnknownSourceError);
        return;
    }

    updatesRunning = true;
    QGeoPositionInfoSource::Error error = AndroidPositioning::startUpdates(androidClassKeyForUpdate);
    if (error != QGeoPositionInfoSource::NoError)
        updatesRunning = false;

    setError(error);
}

void QGeoPositionInfoSourceAndroid::stopUpdates()
{
    if (!updatesRunning)
        return;

    updatesRunning = false;
    AndroidPositioning::stopUpdates(androidClassKeyForUpdate);
}

void QGeoPositionInfoSourceAndroid::requestUpdate(int timeout)
{
    if (m_requestTimer.isActive())
        return;

    if (timeout != 0 && timeout < minimumUpdateInterval()) {
        emit updateTimeout();
        return;
    }

    if (timeout == 0)
        timeout = UPDATE_FROM_COLD_START;

    m_requestTimer.start(timeout);

    // if updates already running with interval equal to timeout
    // then we wait for next update coming through
    // assume that a single update will not be quicker than regular updates anyway
    if (updatesRunning && updateInterval() <= timeout)
        return;

    QGeoPositionInfoSource::Error error = AndroidPositioning::requestUpdate(androidClassKeyForSingleRequest);
    if (error != QGeoPositionInfoSource::NoError)
        m_requestTimer.stop();

    setError(error);
}

void QGeoPositionInfoSourceAndroid::processPositionUpdate(const QGeoPositionInfo &pInfo)
{
    //single update request and served as part of regular update
    if (m_requestTimer.isActive())
        m_requestTimer.stop();

    emit positionUpdated(pInfo);
}

// Might still be called multiple times (once for each provider)
void QGeoPositionInfoSourceAndroid::processSinglePositionUpdate(const QGeoPositionInfo &pInfo)
{
    //timeout but we received a late update -> ignore
    if (!m_requestTimer.isActive())
        return;

    queuedSingleUpdates.append(pInfo);
}

void QGeoPositionInfoSourceAndroid::locationProviderDisabled()
{
    setError(QGeoPositionInfoSource::ClosedError);
}

void QGeoPositionInfoSourceAndroid::requestTimeout()
{
    AndroidPositioning::stopUpdates(androidClassKeyForSingleRequest);
    //no queued update to process -> timeout
    const int count = queuedSingleUpdates.count();

    if (!count) {
        emit updateTimeout();
        return;
    }

    //pick best
    QGeoPositionInfo best = queuedSingleUpdates[0];
    for (int i = 1; i < count; i++) {
        const QGeoPositionInfo info = queuedSingleUpdates[i];

        //anything newer by 20s is always better
        const int timeDelta = best.timestamp().secsTo(info.timestamp());
        if (abs(timeDelta) > 20) {
            if (timeDelta > 0)
                best = info;
            continue;
        }

        //compare accuracy
        if (info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy) &&
                info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
        {
            best = info.attribute(QGeoPositionInfo::HorizontalAccuracy) <
                    best.attribute(QGeoPositionInfo::HorizontalAccuracy) ? info : best;
            continue;
        }

        //prefer info with accuracy information
        if (info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
            best = info;
    }

    queuedSingleUpdates.clear();
    emit positionUpdated(best);
}

/*
  Updates the system assuming that updateInterval
  and/or preferredPositioningMethod have changed.
 */
void QGeoPositionInfoSourceAndroid::reconfigureRunningSystem()
{
    if (!updatesRunning)
        return;

    stopUpdates();
    startUpdates();
}
