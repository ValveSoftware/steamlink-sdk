/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd, author: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
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

#include "qgeosatelliteinfosource_geocluemaster.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusArgument>

#define MINIMUM_UPDATE_INTERVAL 1000

QT_BEGIN_NAMESPACE

namespace
{

void satellite_changed(GeoclueSatellite *satellite, int timestamp, int satellite_used,
                       int satellite_visible, GArray *used_prn, GPtrArray *sat_info,
                       gpointer userdata)
{
    Q_UNUSED(satellite)

    QGeoSatelliteInfoSourceGeoclueMaster *source =
        static_cast<QGeoSatelliteInfoSourceGeoclueMaster *>(userdata);

    QList<int> usedPrns;
    for (unsigned int i = 0; i < used_prn->len; ++i)
        usedPrns.append(g_array_index(used_prn, int, i));

    QList<QGeoSatelliteInfo> satInfos;
    for (unsigned int i = 0; i < sat_info->len; ++i) {
        GValueArray *a = static_cast<GValueArray *>(g_ptr_array_index(sat_info, i));

        QGeoSatelliteInfo satInfo;

        satInfo.setSatelliteIdentifier(g_value_get_int(g_value_array_get_nth(a, 0)));
        satInfo.setAttribute(QGeoSatelliteInfo::Elevation,
                             g_value_get_int(g_value_array_get_nth(a, 1)));
        satInfo.setAttribute(QGeoSatelliteInfo::Azimuth,
                             g_value_get_int(g_value_array_get_nth(a, 2)));
        satInfo.setSignalStrength(g_value_get_int(g_value_array_get_nth(a, 3)));

        satInfos.append(satInfo);
    }

    source->satelliteChanged(timestamp, satellite_used, satellite_visible, usedPrns, satInfos);
}

void satellite_callback(GeoclueSatellite *satellite, int timestamp, int satellite_used,
                        int satellite_visible, GArray *used_prn, GPtrArray *sat_info,
                        GError *error, gpointer userdata)
{
    Q_UNUSED(satellite)

    if (error)
        g_error_free(error);

    QGeoSatelliteInfoSourceGeoclueMaster *source =
        static_cast<QGeoSatelliteInfoSourceGeoclueMaster *>(userdata);

    QList<int> usedPrns;
    for (unsigned int i = 0; i < used_prn->len; ++i)
        usedPrns.append(g_array_index(used_prn, int, i));

    QList<QGeoSatelliteInfo> satInfos;
    for (unsigned int i = 0; i < sat_info->len; ++i) {
        GValueArray *a = static_cast<GValueArray *>(g_ptr_array_index(sat_info, i));

        QGeoSatelliteInfo satInfo;

        satInfo.setSatelliteIdentifier(g_value_get_int(g_value_array_get_nth(a, 0)));
        satInfo.setAttribute(QGeoSatelliteInfo::Elevation,
                             g_value_get_int(g_value_array_get_nth(a, 1)));
        satInfo.setAttribute(QGeoSatelliteInfo::Azimuth,
                             g_value_get_int(g_value_array_get_nth(a, 2)));
        satInfo.setSignalStrength(g_value_get_int(g_value_array_get_nth(a, 3)));

        satInfos.append(satInfo);
    }

    source->requestUpdateFinished(timestamp, satellite_used, satellite_visible, usedPrns, satInfos);
}

}

const QDBusArgument &operator>>(const QDBusArgument &argument, QGeoSatelliteInfo &si)
{
    int a;

    argument.beginStructure();
    argument >> a;
    si.setSatelliteIdentifier(a);
    argument >> a;
    si.setAttribute(QGeoSatelliteInfo::Elevation, a);
    argument >> a;
    si.setAttribute(QGeoSatelliteInfo::Azimuth, a);
    argument >> a;
    si.setSignalStrength(a);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, QList<QGeoSatelliteInfo> &sis)
{
    sis.clear();

    argument.beginArray();
    while (!argument.atEnd()) {
        QGeoSatelliteInfo si;
        argument >> si;
        sis.append(si);
    }
    argument.endArray();

    return argument;
}

QGeoSatelliteInfoSourceGeoclueMaster::QGeoSatelliteInfoSourceGeoclueMaster(QObject *parent)
:   QGeoSatelliteInfoSource(parent), QGeoclueMaster(this), m_sat(0), m_error(NoError),
    m_satellitesChangedConnected(false), m_running(false)
{
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
    if (!hasMasterClient())
        configureSatelliteSource();

    // m_sat is likely to be invalid until Geoclue master selects a position provider.
    if (!m_sat)
        return;

    g_signal_connect(G_OBJECT(m_sat), "satellite-changed", G_CALLBACK(satellite_changed), this);
}

void QGeoSatelliteInfoSourceGeoclueMaster::stopUpdates()
{
    if (!m_running)
        return;

    if (m_sat)
        g_signal_handlers_disconnect_by_func(G_OBJECT(m_sat), gpointer(satellite_changed), this);

    m_running = false;

    // Only stop positioning if single update not requested.
    if (!m_requestTimer.isActive()) {
        cleanupSatelliteSource();
        releaseMasterClient();
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

    if (!hasMasterClient())
        configureSatelliteSource();

    m_requestTimer.start(qMax(timeout, minimumUpdateInterval()));

    if (m_sat)
        geoclue_satellite_get_satellite_async(m_sat, satellite_callback, this);
}

void QGeoSatelliteInfoSourceGeoclueMaster::satelliteChanged(int timestamp, int satellitesUsed,
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
}

void QGeoSatelliteInfoSourceGeoclueMaster::requestUpdateFinished(int timestamp, int satellitesUsed,
                                                                 int satellitesVisible,
                                                                 const QList<int> &usedPrn,
                                                                 const QList<QGeoSatelliteInfo> &satInfos)
{
    m_requestTimer.stop();
    satelliteChanged(timestamp, satellitesUsed, satellitesVisible, usedPrn, satInfos);
}

void QGeoSatelliteInfoSourceGeoclueMaster::requestUpdateTimeout()
{
    // If we end up here, there has not been a valid satellite info update.
    emit requestTimeout();

    // Only stop satellite info if regular updates not active.
    if (!m_running) {
        cleanupSatelliteSource();
        releaseMasterClient();
    }
}

void QGeoSatelliteInfoSourceGeoclueMaster::positionProviderChanged(const QByteArray &service, const QByteArray &path)
{
    if (m_sat)
        cleanupSatelliteSource();

    QByteArray providerService;
    QByteArray providerPath;

    if (service.isEmpty() || path.isEmpty()) {
        // No valid position provider has been selected. This probably means that the GPS provider
        // has not yet obtained a position fix. It can still provide satellite information though.
        if (!m_satellitesChangedConnected) {
            QDBusConnection conn = QDBusConnection::sessionBus();
            conn.connect(QString(), QString(), QStringLiteral("org.freedesktop.Geoclue.Satellite"),
                         QStringLiteral("SatelliteChanged"), this,
                         SLOT(satellitesChanged(QDBusMessage)));
            m_satellitesChangedConnected = true;
            return;
        }
    } else {
        if (m_satellitesChangedConnected) {
            QDBusConnection conn = QDBusConnection::sessionBus();
            conn.disconnect(QString(), QString(),
                            QStringLiteral("org.freedesktop.Geoclue.Satellite"),
                            QStringLiteral("SatelliteChanged"), this,
                            SLOT(satellitesChanged(QDBusMessage)));
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

    m_sat = geoclue_satellite_new(providerService.constData(), providerPath.constData());
    if (!m_sat) {
        m_error = AccessError;
        emit QGeoSatelliteInfoSource::error(m_error);
        return;
    }

    g_signal_connect(G_OBJECT(m_sat), "satellite-changed", G_CALLBACK(satellite_changed), this);
}

void QGeoSatelliteInfoSourceGeoclueMaster::satellitesChanged(const QDBusMessage &message)
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

bool QGeoSatelliteInfoSourceGeoclueMaster::configureSatelliteSource()
{
    return createMasterClient(GEOCLUE_ACCURACY_LEVEL_DETAILED, GEOCLUE_RESOURCE_GPS);
}

void QGeoSatelliteInfoSourceGeoclueMaster::cleanupSatelliteSource()
{
    if (m_sat) {
        g_object_unref(m_sat);
        m_sat = 0;
    }
}

QT_END_NAMESPACE
