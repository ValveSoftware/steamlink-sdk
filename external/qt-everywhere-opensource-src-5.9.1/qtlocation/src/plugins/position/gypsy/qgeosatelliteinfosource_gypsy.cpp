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

#include "qgeosatelliteinfosource_gypsy_p.h"

#ifdef Q_LOCATION_GYPSY_DEBUG
#include <QDebug>
#endif
#include <QFile>

QT_BEGIN_NAMESPACE

#define UPDATE_TIMEOUT_COLD_START 120000


// Callback function for 'satellites-changed' -signal
static void satellites_changed (GypsySatellite *satellite,
                                GPtrArray *satellites,
                                gpointer userdata)
{
#ifdef Q_LOCATION_GYPSY_DEBUG
    qDebug() << "QGeoSatelliteInfoSourceGypsy Gypsy satellites-changed -signal received.";
#endif
    ((QGeoSatelliteInfoSourceGypsy *)userdata)->satellitesChanged(satellite, satellites);
}

SatelliteGypsyEngine::SatelliteGypsyEngine(QGeoSatelliteInfoSource *parent) :
    m_owner(parent)
{
}
SatelliteGypsyEngine::~SatelliteGypsyEngine()
{
}

// Glib symbols
gulong SatelliteGypsyEngine::eng_g_signal_connect(gpointer instance,
                                                  const gchar *detailed_signal,
                                                  GCallback c_handler,
                                                  gpointer data)
{
    return ::g_signal_connect(instance, detailed_signal, c_handler, data);
}
guint SatelliteGypsyEngine::eng_g_signal_handlers_disconnect_by_func (gpointer instance,
                                                                      gpointer func,
                                                                      gpointer data)
{
    return ::g_signal_handlers_disconnect_by_func(instance, func, data);
}

void SatelliteGypsyEngine::eng_g_free(gpointer mem)
{
    return ::g_free(mem);
}
// Gypsy symbols
GypsyControl *SatelliteGypsyEngine::eng_gypsy_control_get_default (void)
{
    return ::gypsy_control_get_default();
}
char *SatelliteGypsyEngine::eng_gypsy_control_create (GypsyControl *control, const char *device_name, GError **error)
{
    return ::gypsy_control_create(control, device_name, error);
}
GypsyDevice *SatelliteGypsyEngine::eng_gypsy_device_new (const char *object_path)
{
    return ::gypsy_device_new(object_path);
}
GypsySatellite *SatelliteGypsyEngine::eng_gypsy_satellite_new (const char *object_path)
{
    return ::gypsy_satellite_new (object_path);
}
gboolean SatelliteGypsyEngine::eng_gypsy_device_start (GypsyDevice *device, GError **error)
{
    return ::gypsy_device_start(device, error);
}
gboolean SatelliteGypsyEngine::eng_gypsy_device_stop (GypsyDevice *device, GError **error)
{
    // Unfortunately this cannot be done; calling this will stop the GPS device
    // (basically makes gypsy-daemon unusable for anyone), regardless of applications
    // using it (see bug http://bugs.meego.com/show_bug.cgi?id=11707).
    Q_UNUSED(device);
    Q_UNUSED(error);
    return true;
    //return ::gypsy_device_stop (device, error);
}
GypsyDeviceFixStatus SatelliteGypsyEngine::eng_gypsy_device_get_fix_status (GypsyDevice *device, GError **error)
{
    return ::gypsy_device_get_fix_status (device, error);
}
GPtrArray *SatelliteGypsyEngine::eng_gypsy_satellite_get_satellites (GypsySatellite *satellite, GError **error)
{
    return ::gypsy_satellite_get_satellites (satellite, error);
}
void SatelliteGypsyEngine::eng_gypsy_satellite_free_satellite_array (GPtrArray *satellites)
{
    return ::gypsy_satellite_free_satellite_array(satellites);
}
// GConf symbols (mockability due to X11 requirement)
GConfClient *SatelliteGypsyEngine::eng_gconf_client_get_default(void)
{
    return ::gconf_client_get_default();
}
gchar *SatelliteGypsyEngine::eng_gconf_client_get_string(GConfClient *client, const gchar *key, GError** err)
{
    return ::gconf_client_get_string(client, key, err);
}

QGeoSatelliteInfoSourceGypsy::QGeoSatelliteInfoSourceGypsy(QObject *parent) : QGeoSatelliteInfoSource(parent),
    m_engine(0), m_satellite(0), m_device(0), m_requestTimer(this), m_updatesOngoing(false), m_requestOngoing(false)
{
    m_requestTimer.setSingleShot(true);
    QObject::connect(&m_requestTimer, SIGNAL(timeout()), this, SLOT(requestUpdateTimeout()));
}

void QGeoSatelliteInfoSourceGypsy::createEngine()
{
    delete m_engine;
    m_engine = new SatelliteGypsyEngine(this);
}

QGeoSatelliteInfoSourceGypsy::~QGeoSatelliteInfoSourceGypsy()
{
    GError *error = NULL;
    if (m_device) {
        m_engine->eng_gypsy_device_stop (m_device, &error);
        g_object_unref(m_device);
    }
    if (m_satellite)
        g_object_unref(m_satellite);
    if (error)
        g_error_free(error);
    delete m_engine;
}

void QGeoSatelliteInfoSourceGypsy::satellitesChanged(GypsySatellite *satellite,
                                                     GPtrArray *satellites)
{
    if (!satellite || !satellites)
        return;
    // We have satellite data and assume it is valid.
    // If a single updateRequest was active, send signals right away.
    // If a periodic timer was running (meaning that the client wishes
    // to have updates at defined intervals), store the data for later sending.
    QList<QGeoSatelliteInfo> lastSatellitesInView;
    QList<QGeoSatelliteInfo> lastSatellitesInUse;

    unsigned int i;
    for (i = 0; i < satellites->len; i++) {
        GypsySatelliteDetails *details = (GypsySatelliteDetails *)satellites->pdata[i];
        QGeoSatelliteInfo info;
        info.setAttribute(QGeoSatelliteInfo::Elevation, details->elevation);
        info.setAttribute(QGeoSatelliteInfo::Azimuth, details->azimuth);
        info.setSignalStrength(details->snr);
        if (details->in_use)
            lastSatellitesInUse.append(info);
        lastSatellitesInView.append(info);
    }
    bool sendUpdates(false);
    // If a single updateRequest() has been issued:
    if (m_requestOngoing) {
        sendUpdates = true;
        m_requestTimer.stop();
        m_requestOngoing = false;
        // If there is no regular updates ongoing, disconnect now.
        if (!m_updatesOngoing) {
            m_engine->eng_g_signal_handlers_disconnect_by_func(G_OBJECT(m_satellite), (void *)satellites_changed, this);
        }
    }
    // If regular updates are to be delivered as they come:
    if (m_updatesOngoing)
        sendUpdates = true;

    if (sendUpdates) {
        emit satellitesInUseUpdated(lastSatellitesInUse);
        emit satellitesInViewUpdated(lastSatellitesInView);
    }
}

int QGeoSatelliteInfoSourceGypsy::init()
{
    GError *error = NULL;
    char *path;
    GConfClient *client;
    gchar *device_name;

    g_type_init ();
    createEngine();

    client = m_engine->eng_gconf_client_get_default();
    if (!client) {
        qWarning ("QGeoSatelliteInfoSourceGypsy client creation failed.");
        return -1;
    }
    device_name = m_engine->eng_gconf_client_get_string(client, "/apps/geoclue/master/org.freedesktop.Geoclue.GPSDevice", NULL);
    g_object_unref(client);
    QString deviceName(QString::fromLatin1(device_name));
    if (deviceName.isEmpty() ||
            (deviceName.trimmed().at(0) == '/' && !QFile::exists(deviceName.trimmed()))) {
        qWarning ("QGeoSatelliteInfoSourceGypsy Empty/nonexistent GPS device name detected.");
        qWarning ("Use gconftool-2 to set it, e.g. on terminal: ");
        qWarning ("gconftool-2 -t string -s /apps/geoclue/master/org.freedesktop.Geoclue.GPSDevice /dev/ttyUSB0");
        m_engine->eng_g_free(device_name);
        return -1;
    }
    GypsyControl *control = NULL;
    control = m_engine->eng_gypsy_control_get_default();
    if (!control) {
        qWarning("QGeoSatelliteInfoSourceGypsy unable to create Gypsy control.");
        m_engine->eng_g_free(device_name);
        return -1;
    }
    // (path is the DBus path)
    path = m_engine->eng_gypsy_control_create (control, device_name, &error);
    m_engine->eng_g_free(device_name);
    g_object_unref(control);
    if (!path) {
        qWarning ("QGeoSatelliteInfoSourceGypsy error creating client.");
        if (error) {
            qWarning ("error message: %s", error->message);
            g_error_free (error);
        }
        return -1;
    }
    m_device = m_engine->eng_gypsy_device_new (path);
    m_satellite = m_engine->eng_gypsy_satellite_new (path);
    m_engine->eng_g_free(path);
    if (!m_device || !m_satellite) {
        qWarning ("QGeoSatelliteInfoSourceGypsy error creating satellite device.");
        qWarning ("Is GPS device set correctly? If not, use gconftool-2 to set it, e.g.: ");
        qWarning ("gconftool-2 -t string -s /apps/geoclue/master/org.freedesktop.Geoclue.GPSDevice /dev/ttyUSB0");
        if (m_device)
            g_object_unref(m_device);
        if (m_satellite)
            g_object_unref(m_satellite);
        return -1;
    }
    m_engine->eng_gypsy_device_start (m_device, &error);
    if (error) {
        qWarning ("QGeoSatelliteInfoSourceGypsy error starting device: %s ",
                   error->message);
        g_error_free(error);
        g_object_unref(m_device);
        g_object_unref(m_satellite);
        return -1;
    }
    return 0;
}

int QGeoSatelliteInfoSourceGypsy::minimumUpdateInterval() const
{
    return 1;
}

QGeoSatelliteInfoSource::Error QGeoSatelliteInfoSourceGypsy::error() const
{
    return NoError;
}

void QGeoSatelliteInfoSourceGypsy::startUpdates()
{
    if (m_updatesOngoing)
        return;
    // If there is a request timer ongoing, we've connected to the signal already
    if (!m_requestTimer.isActive()) {
        m_engine->eng_g_signal_connect (m_satellite, "satellites-changed",
                          G_CALLBACK (satellites_changed), this);
    }
    m_updatesOngoing = true;
}

void QGeoSatelliteInfoSourceGypsy::stopUpdates()
{
    if (!m_updatesOngoing)
        return;
    m_updatesOngoing = false;
    // Disconnect only if there is no single update request ongoing. Once single update request
    // is completed and it notices that there is no active update ongoing, it will disconnect
    // the signal.
    if (!m_requestTimer.isActive())
        m_engine->eng_g_signal_handlers_disconnect_by_func(G_OBJECT(m_satellite), (void *)satellites_changed, this);
}

void QGeoSatelliteInfoSourceGypsy::requestUpdate(int timeout)
{
    if (m_requestOngoing)
        return;
    if (timeout < 0) {
        emit requestTimeout();
        return;
    }
    m_requestOngoing = true;
    GError *error = 0;
    // If GPS has a fix a already, request current data.
    GypsyDeviceFixStatus fixStatus = m_engine->eng_gypsy_device_get_fix_status(m_device, &error);
    if (!error && (fixStatus != GYPSY_DEVICE_FIX_STATUS_INVALID &&
            fixStatus != GYPSY_DEVICE_FIX_STATUS_NONE)) {
#ifdef Q_LOCATION_GYPSY_DEBUG
        qDebug() << "QGeoSatelliteInfoSourceGypsy fix available, requesting current satellite data";
#endif
        GPtrArray *satelliteData = m_engine->eng_gypsy_satellite_get_satellites(m_satellite, &error);
        if (!error) {
            // The fix was available and we have satellite data to deliver right away.
            satellitesChanged(m_satellite, satelliteData);
            m_engine->eng_gypsy_satellite_free_satellite_array(satelliteData);
            return;
        }
    }
    // No fix is available. If updates are not ongoing already, start them.
    m_requestTimer.setInterval(timeout == 0? UPDATE_TIMEOUT_COLD_START: timeout);
    if (!m_updatesOngoing) {
        m_engine->eng_g_signal_connect (m_satellite, "satellites-changed",
                          G_CALLBACK (satellites_changed), this);
    }
    m_requestTimer.start();
    if (error) {
#ifdef Q_LOCATION_GYPSY_DEBUG
        qDebug() << "QGeoSatelliteInfoSourceGypsy error asking fix status or satellite data: " << error->message;
#endif
        g_error_free(error);
    }
}

void QGeoSatelliteInfoSourceGypsy::requestUpdateTimeout()
{
#ifdef Q_LOCATION_GYPSY_DEBUG
    qDebug("QGeoSatelliteInfoSourceGypsy request update timeout occurred.");
#endif
    // If we end up here, there has not been valid satellite update.
    // Emit timeout and disconnect from signal if regular updates are not
    // ongoing (as we were listening just for one single requestUpdate).
    if (!m_updatesOngoing) {
        m_engine->eng_g_signal_handlers_disconnect_by_func(G_OBJECT(m_satellite), (void *)satellites_changed, this);
    }
    m_requestOngoing = false;
    emit requestTimeout();
}

QT_END_NAMESPACE
