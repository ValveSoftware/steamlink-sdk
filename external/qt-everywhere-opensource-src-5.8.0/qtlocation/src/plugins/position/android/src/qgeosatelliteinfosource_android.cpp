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

#include <QDebug>

#include "qgeosatelliteinfosource_android_p.h"
#include "jnipositioning.h"

Q_DECLARE_METATYPE(QGeoSatelliteInfo)
Q_DECLARE_METATYPE(QList<QGeoSatelliteInfo>)

#define UPDATE_FROM_COLD_START 2*60*1000

QGeoSatelliteInfoSourceAndroid::QGeoSatelliteInfoSourceAndroid(QObject *parent) :
    QGeoSatelliteInfoSource(parent), m_error(NoError), updatesRunning(false)
{
    qRegisterMetaType< QGeoSatelliteInfo >();
    qRegisterMetaType< QList<QGeoSatelliteInfo> >();
    androidClassKeyForUpdate = AndroidPositioning::registerPositionInfoSource(this);
    androidClassKeyForSingleRequest = AndroidPositioning::registerPositionInfoSource(this);

    requestTimer.setSingleShot(true);
    QObject::connect(&requestTimer, SIGNAL(timeout()),
                     this, SLOT(requestTimeout()));
}

QGeoSatelliteInfoSourceAndroid::~QGeoSatelliteInfoSourceAndroid()
{
    stopUpdates();

    if (requestTimer.isActive()) {
        requestTimer.stop();
        AndroidPositioning::stopUpdates(androidClassKeyForSingleRequest);
    }

    AndroidPositioning::unregisterPositionInfoSource(androidClassKeyForUpdate);
    AndroidPositioning::unregisterPositionInfoSource(androidClassKeyForSingleRequest);
}


void QGeoSatelliteInfoSourceAndroid::setUpdateInterval(int msec)
{
    int previousInterval = updateInterval();
    msec = (((msec > 0) && (msec < minimumUpdateInterval())) || msec < 0)? minimumUpdateInterval() : msec;

    if (msec == previousInterval)
        return;

    QGeoSatelliteInfoSource::setUpdateInterval(msec);

    if (updatesRunning)
        reconfigureRunningSystem();
}

int QGeoSatelliteInfoSourceAndroid::minimumUpdateInterval() const
{
    return 50;
}

QGeoSatelliteInfoSource::Error QGeoSatelliteInfoSourceAndroid::error() const
{
    return m_error;
}

void QGeoSatelliteInfoSourceAndroid::startUpdates()
{
    if (updatesRunning)
        return;

    updatesRunning = true;

    QGeoSatelliteInfoSource::Error error = AndroidPositioning::startSatelliteUpdates(
                androidClassKeyForUpdate, false, updateInterval());
    if (error != QGeoSatelliteInfoSource::NoError) {
        updatesRunning = false;
        m_error = error;
        emit QGeoSatelliteInfoSource::error(m_error);
    }
}

void QGeoSatelliteInfoSourceAndroid::stopUpdates()
{
    if (!updatesRunning)
        return;

    updatesRunning = false;
    AndroidPositioning::stopUpdates(androidClassKeyForUpdate);
}

void QGeoSatelliteInfoSourceAndroid::requestUpdate(int timeout)
{
    if (requestTimer.isActive())
        return;

    if (timeout != 0 && timeout < minimumUpdateInterval()) {
        emit requestTimeout();
        return;
    }

    if (timeout == 0)
        timeout = UPDATE_FROM_COLD_START;

    requestTimer.start(timeout);

    // if updates already running with interval equal or less then timeout
    // then we wait for next update coming through
    // assume that a single update will not be quicker than regular updates anyway
    if (updatesRunning && updateInterval() <= timeout)
        return;

    QGeoSatelliteInfoSource::Error error = AndroidPositioning::startSatelliteUpdates(
                androidClassKeyForSingleRequest, true, timeout);
    if (error != QGeoSatelliteInfoSource::NoError) {
        requestTimer.stop();
        m_error = error;
        emit QGeoSatelliteInfoSource::error(m_error);
    }
}

void QGeoSatelliteInfoSourceAndroid::processSatelliteUpdateInView(const QList<QGeoSatelliteInfo> &satsInView, bool isSingleUpdate)
{
    if (!isSingleUpdate) {
        //if requested while regular updates were running
        if (requestTimer.isActive())
            requestTimer.stop();
        emit QGeoSatelliteInfoSource::satellitesInViewUpdated(satsInView);
        return;
    }

    m_satsInView = satsInView;
}

void QGeoSatelliteInfoSourceAndroid::processSatelliteUpdateInUse(const QList<QGeoSatelliteInfo> &satsInUse, bool isSingleUpdate)
{
    if (!isSingleUpdate) {
        //if requested while regular updates were running
        if (requestTimer.isActive())
            requestTimer.stop();
        emit QGeoSatelliteInfoSource::satellitesInUseUpdated(satsInUse);
        return;
    }

    m_satsInUse = satsInUse;
}

void QGeoSatelliteInfoSourceAndroid::requestTimeout()
{
    AndroidPositioning::stopUpdates(androidClassKeyForSingleRequest);

    const int count = m_satsInView.count();
    if (!count) {
        emit requestTimeout();
        return;
    }

    emit QGeoSatelliteInfoSource::satellitesInViewUpdated(m_satsInView);
    emit QGeoSatelliteInfoSource::satellitesInUseUpdated(m_satsInUse);

    m_satsInUse.clear();
    m_satsInView.clear();
}

/*
  Updates the system assuming that updateInterval
  and/or preferredPositioningMethod have changed.
 */
void QGeoSatelliteInfoSourceAndroid::reconfigureRunningSystem()
{
    if (!updatesRunning)
        return;

    stopUpdates();
    startUpdates();
}

void QGeoSatelliteInfoSourceAndroid::locationProviderDisabled()
{
    m_error = QGeoSatelliteInfoSource::ClosedError;
    emit QGeoSatelliteInfoSource::error(m_error);
}
