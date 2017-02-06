/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
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

#include "qgeosatelliteinfosource_geocluemaster.h"

#include <geoclue_interface.h>
#include <satellite_interface.h>

#include <QtCore/QLoggingCategory>
#include <QtDBus/QDBusPendingCallWatcher>

Q_DECLARE_LOGGING_CATEGORY(lcPositioningGeoclue)

#define MINIMUM_UPDATE_INTERVAL 1000

QT_BEGIN_NAMESPACE

QGeoSatelliteInfoSourceGeoclueMaster::QGeoSatelliteInfoSourceGeoclueMaster(QObject *parent)
:   QGeoSatelliteInfoSource(parent), m_master(new QGeoclueMaster(this)), m_provider(0), m_sat(0),
    m_error(NoError), m_satellitesChangedConnected(false), m_running(false)
{
    connect(m_master, SIGNAL(positionProviderChanged(QString,QString,QString,QString)),
            this, SLOT(positionProviderChanged(QString,QString,QString,QString)));

    m_requestTimer.setSingleShot(true);
    connect(&m_requestTimer, SIGNAL(timeout()), this, SLOT(requestUpdateTimeout()));
}

QGeoSatelliteInfoSourceGeoclueMaster::~QGeoSatelliteInfoSourceGeoclueMaster()
{
    cleanupSatelliteSource();
}

int QGeoSatelliteInfoSourceGeoclueMaster::minimumUpdateInterval() const
{
    return MINIMUM_UPDATE_INTERVAL;
}

void QGeoSatelliteInfoSourceGeoclueMaster::setUpdateInterval(int msec)
{
    if (msec < 0 || (msec > 0 && msec < MINIMUM_UPDATE_INTERVAL))
        msec = MINIMUM_UPDATE_INTERVAL;

    QGeoSatelliteInfoSource::setUpdateInterval(msec);
}

QGeoSatelliteInfoSource::Error QGeoSatelliteInfoSourceGeoclueMaster::error() const
{
    return m_error;
}

void QGeoSatelliteInfoSourceGeoclueMaster::startUpdates()
{
    if (m_running)
        return;

    m_running = true;

    // Start Geoclue provider.
    if (!m_master->hasMasterClient())
        configureSatelliteSource();

    m_requestTimer.start(updateInterval());
}

void QGeoSatelliteInfoSourceGeoclueMaster::stopUpdates()
{
    if (!m_running)
        return;

    if (m_sat) {
        disconnect(m_sat, SIGNAL(SatelliteChanged(qint32,qint32,qint32,QList<qint32>,QList<QGeoSatelliteInfo>)),
                   this, SLOT(satelliteChanged(qint32,qint32,qint32,QList<qint32>,QList<QGeoSatelliteInfo>)));
    }

    m_running = false;

    // Only stop positioning if single update not requested.
    if (!m_requestTimer.isActive()) {
        cleanupSatelliteSource();
        m_master->releaseMasterClient();
    }
}

void QGeoSatelliteInfoSourceGeoclueMaster::requestUpdate(int timeout)
{
    if (timeout < minimumUpdateInterval() && timeout != 0) {
        emit requestTimeout();
        return;
    }

    if (m_requestTimer.isActive())
        return;

    if (!m_master->hasMasterClient())
        configureSatelliteSource();

    m_requestTimer.start(qMax(timeout, minimumUpdateInterval()));

    if (m_sat) {
        QDBusPendingReply<qint32, qint32, qint32, QList<qint32>, QList<QGeoSatelliteInfo> > reply =
            m_sat->GetSatellite();
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
        connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                this, SLOT(getSatelliteFinished(QDBusPendingCallWatcher*)));
    }
}

void QGeoSatelliteInfoSourceGeoclueMaster::updateSatelliteInfo(int timestamp, int satellitesUsed,
                                                               int satellitesVisible,
                                                               const QList<int> &usedPrn,
                                                               const QList<QGeoSatelliteInfo> &satInfos)
{
    Q_UNUSED(timestamp)

    QList<QGeoSatelliteInfo> inUse;

    foreach (const QGeoSatelliteInfo &si, satInfos)
        if (usedPrn.contains(si.satelliteIdentifier()))
            inUse.append(si);

    if (satInfos.length() != satellitesVisible) {
        qWarning("QGeoSatelliteInfoSourceGeoclueMaster number of in view QGeoSatelliteInfos (%d) "
                 "does not match expected number of in view satellites (%d).", satInfos.length(),
                 satellitesVisible);
    }

    if (inUse.length() != satellitesUsed) {
        qWarning("QGeoSatelliteInfoSourceGeoclueMaster number of in use QGeoSatelliteInfos (%d) "
                 "does not match expected number of in use satellites (%d).", inUse.length(),
                 satellitesUsed);
    }

    m_inView = satInfos;
    emit satellitesInViewUpdated(m_inView);

    m_inUse = inUse;
    emit satellitesInUseUpdated(m_inUse);

    m_requestTimer.start(updateInterval());
}

void QGeoSatelliteInfoSourceGeoclueMaster::requestUpdateTimeout()
{
    // If we end up here, there has not been a valid satellite info update.
    if (m_running) {
        m_inView.clear();
        m_inUse.clear();
        emit satellitesInViewUpdated(m_inView);
        emit satellitesInUseUpdated(m_inUse);
    } else {
        emit requestTimeout();

        // Only stop satellite info if regular updates not active.
        cleanupSatelliteSource();
        m_master->releaseMasterClient();
    }
}

void QGeoSatelliteInfoSourceGeoclueMaster::getSatelliteFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<qint32, qint32, qint32, QList<qint32>, QList<QGeoSatelliteInfo> > reply = *watcher;
    watcher->deleteLater();

    if (reply.isError())
        return;

    m_requestTimer.stop();
    updateSatelliteInfo(reply.argumentAt<0>(), reply.argumentAt<1>(), reply.argumentAt<2>(),
                        reply.argumentAt<3>(), reply.argumentAt<4>());
}

void QGeoSatelliteInfoSourceGeoclueMaster::satelliteChanged(int timestamp, int satellitesUsed, int satellitesVisible, const QList<int> &usedPrn, const QList<QGeoSatelliteInfo> &satInfos)
{
    updateSatelliteInfo(timestamp, satellitesUsed, satellitesVisible, usedPrn, satInfos);
}

void QGeoSatelliteInfoSourceGeoclueMaster::positionProviderChanged(const QString &name,
                                                                   const QString &description,
                                                                   const QString &service,
                                                                   const QString &path)
{
    Q_UNUSED(name)
    Q_UNUSED(description)

    cleanupSatelliteSource();

    QString providerService;
    QString providerPath;

    if (service.isEmpty() || path.isEmpty()) {
        // No valid position provider has been selected. This probably means that the GPS provider
        // has not yet obtained a position fix. It can still provide satellite information though.
        if (!m_satellitesChangedConnected) {
            QDBusConnection conn = QDBusConnection::sessionBus();
            conn.connect(QString(), QString(), QStringLiteral("org.freedesktop.Geoclue.Satellite"),
                         QStringLiteral("SatelliteChanged"), this,
                         SLOT(satelliteChanged(QDBusMessage)));
            m_satellitesChangedConnected = true;
            return;
        }
    } else {
        if (m_satellitesChangedConnected) {
            QDBusConnection conn = QDBusConnection::sessionBus();
            conn.disconnect(QString(), QString(),
                            QStringLiteral("org.freedesktop.Geoclue.Satellite"),
                            QStringLiteral("SatelliteChanged"), this,
                            SLOT(satelliteChanged(QDBusMessage)));
            m_satellitesChangedConnected = false;
        }

        providerService = service;
        providerPath = path;
    }

    if (providerService.isEmpty() || providerPath.isEmpty()) {
        m_error = AccessError;
        emit QGeoSatelliteInfoSource::error(m_error);
        return;
    }

    m_provider = new OrgFreedesktopGeoclueInterface(providerService, providerPath, QDBusConnection::sessionBus());
    m_provider->AddReference();

    m_sat = new OrgFreedesktopGeoclueSatelliteInterface(providerService, providerPath, QDBusConnection::sessionBus());

    if (m_running) {
        connect(m_sat, SIGNAL(SatelliteChanged(qint32,qint32,qint32,QList<qint32>,QList<QGeoSatelliteInfo>)),
                this, SLOT(satelliteChanged(qint32,qint32,qint32,QList<qint32>,QList<QGeoSatelliteInfo>)));
    }
}

void QGeoSatelliteInfoSourceGeoclueMaster::satelliteChanged(const QDBusMessage &message)
{
    QVariantList arguments = message.arguments();
    if (arguments.length() != 5)
        return;

    int timestamp = arguments.at(0).toInt();
    int usedSatellites = arguments.at(1).toInt();
    int visibleSatellites = arguments.at(2).toInt();

    QDBusArgument dbusArgument = arguments.at(3).value<QDBusArgument>();

    QList<int> usedPrn;
    dbusArgument >> usedPrn;

    dbusArgument = arguments.at(4).value<QDBusArgument>();

    QList<QGeoSatelliteInfo> satelliteInfos;
    dbusArgument >> satelliteInfos;

    satelliteChanged(timestamp, usedSatellites, visibleSatellites, usedPrn, satelliteInfos);
}

void QGeoSatelliteInfoSourceGeoclueMaster::configureSatelliteSource()
{
    if (!m_master->createMasterClient(Accuracy::Detailed, QGeoclueMaster::ResourceGps)) {
        m_error = UnknownSourceError;
        emit QGeoSatelliteInfoSource::error(m_error);
    }
}

void QGeoSatelliteInfoSourceGeoclueMaster::cleanupSatelliteSource()
{
    if (m_provider)
        m_provider->RemoveReference();
    delete m_provider;
    m_provider = 0;
    delete m_sat;
    m_sat = 0;
}

QT_END_NAMESPACE
