/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd.
** Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
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

#include "qgeopositioninfosource_geocluemaster_p.h"

#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QVariantMap>
#include <QtCore/QtNumeric>

#ifdef Q_LOCATION_GEOCLUE_DEBUG
#include <QDebug>
#endif

#include <dbus/dbus-glib.h>

QT_BEGIN_NAMESPACE

#define MINIMUM_UPDATE_INTERVAL 1000
#define UPDATE_TIMEOUT_COLD_START 120000

namespace
{

void position_changed(GeocluePosition *position, GeocluePositionFields fields, int timestamp,
                      double latitude, double longitude, double altitude,
                      GeoclueAccuracy *accuracy, QGeoPositionInfoSourceGeoclueMaster *master)
{
    Q_UNUSED(position)

    if (fields & GEOCLUE_POSITION_FIELDS_LATITUDE && fields & GEOCLUE_POSITION_FIELDS_LONGITUDE)
        master->updatePosition(fields, timestamp, latitude, longitude, altitude, accuracy);
    else
        master->regularUpdateFailed();
}

void velocity_changed(GeoclueVelocity *velocity, GeoclueVelocityFields fields, int timestamp,
                      double speed, double direction, double climb,
                      QGeoPositionInfoSourceGeoclueMaster *master)
{
    Q_UNUSED(velocity)

    if (fields == GEOCLUE_VELOCITY_FIELDS_NONE)
        master->velocityUpdateFailed();
    else
        master->velocityUpdateSucceeded(fields, timestamp, speed, direction, climb);
}

void position_callback(GeocluePosition *pos, GeocluePositionFields fields, int timestamp,
                       double latitude, double longitude, double altitude,
                       GeoclueAccuracy *accuracy, GError *error, gpointer userdata)
{
    Q_UNUSED(pos)

    if (error)
        g_error_free(error);

    QGeoPositionInfoSourceGeoclueMaster *master =
        static_cast<QGeoPositionInfoSourceGeoclueMaster *>(userdata);

    if (fields & GEOCLUE_POSITION_FIELDS_LATITUDE && fields & GEOCLUE_POSITION_FIELDS_LONGITUDE)
        master->updatePosition(fields, timestamp, latitude, longitude, altitude, accuracy);
}

double knotsToMetersPerSecond(double knots)
{
    return knots * 1852.0 / 3600.0;
}

}

QGeoPositionInfoSourceGeoclueMaster::QGeoPositionInfoSourceGeoclueMaster(QObject *parent)
:   QGeoPositionInfoSource(parent), QGeoclueMaster(this), m_pos(0), m_vel(0),
    m_lastVelocityIsFresh(false), m_regularUpdateTimedOut(false), m_lastVelocity(qQNaN()),
    m_lastDirection(qQNaN()), m_lastClimb(qQNaN()), m_lastPositionFromSatellite(false),
    m_methods(AllPositioningMethods), m_running(false), m_error(NoError)
{
#ifndef QT_NO_DATASTREAM
    // Load the last known location
    QFile file(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
               QStringLiteral("/qtposition-geoclue"));
    if (file.open(QIODevice::ReadOnly)) {
        QDataStream out(&file);
        out >> m_lastPosition;
    }
#endif

    m_requestTimer.setSingleShot(true);
    QObject::connect(&m_requestTimer, SIGNAL(timeout()), this, SLOT(requestUpdateTimeout()));

    setPreferredPositioningMethods(AllPositioningMethods);
}

QGeoPositionInfoSourceGeoclueMaster::~QGeoPositionInfoSourceGeoclueMaster()
{
#ifndef QT_NO_DATASTREAM
    if (m_lastPosition.isValid()) {
        QSaveFile file(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
                       QStringLiteral("/qtposition-geoclue"));
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QDataStream out(&file);
            // Only save position and timestamp.
            out << QGeoPositionInfo(m_lastPosition.coordinate(), m_lastPosition.timestamp());
            file.commit();
        }
    }
#endif

    if (m_pos)
        g_object_unref (m_pos);
    if (m_vel)
        g_object_unref(m_vel);
}

void QGeoPositionInfoSourceGeoclueMaster::velocityUpdateFailed()
{
#ifdef Q_LOCATION_GEOCLUE_DEBUG
        qDebug() << "QGeoPositionInfoSourceGeoclueMaster velocity update failed.";
#endif
    // Set the velocitydata non-fresh.
    m_lastVelocityIsFresh = false;
}

void QGeoPositionInfoSourceGeoclueMaster::velocityUpdateSucceeded(GeoclueVelocityFields fields,
                                                                  int timestamp, double speed,
                                                                  double direction, double climb)
{
    Q_UNUSED(timestamp);

#ifdef Q_LOCATION_GEOCLUE_DEBUG
        qDebug() << "QGeoPositionInfoSourceGeoclueMaster velocity update succeeded, speed: " << speed;
#endif
    // Store the velocity and mark it as fresh. Simple but hopefully adequate.
    if (fields & GEOCLUE_VELOCITY_FIELDS_SPEED)
        m_lastVelocity = knotsToMetersPerSecond(speed);
    else
        m_lastVelocity = qQNaN();

    if (fields & GEOCLUE_VELOCITY_FIELDS_DIRECTION)
        m_lastDirection = direction;
    else
        m_lastDirection = qQNaN();

    if (fields & GEOCLUE_VELOCITY_FIELDS_CLIMB)
        m_lastClimb = climb;
    else
        m_lastClimb = qQNaN();

    m_lastVelocityIsFresh = true;
}

void QGeoPositionInfoSourceGeoclueMaster::regularUpdateFailed()
{
#ifdef Q_LOCATION_GEOCLUE_DEBUG
        qDebug() << "QGeoPositionInfoSourceGeoclueMaster regular update failed.";
#endif

    m_lastVelocityIsFresh = false;
    if (m_running && !m_regularUpdateTimedOut) {
        m_regularUpdateTimedOut = true;
        emit updateTimeout();
    }
}

void QGeoPositionInfoSourceGeoclueMaster::updatePosition(GeocluePositionFields fields,
                                                         int timestamp, double latitude,
                                                         double longitude, double altitude,
                                                         GeoclueAccuracy *accuracy)
{
    if (m_requestTimer.isActive())
        m_requestTimer.stop();

    QGeoCoordinate coordinate(latitude, longitude);
    if (fields & GEOCLUE_POSITION_FIELDS_ALTITUDE)
        coordinate.setAltitude(altitude);

    m_lastPosition = QGeoPositionInfo(coordinate, QDateTime::fromTime_t(timestamp));

    if (accuracy) {
        double horizontalAccuracy = qQNaN();
        double verticalAccuracy = qQNaN();
        GeoclueAccuracyLevel accuracyLevel = GEOCLUE_ACCURACY_LEVEL_NONE;
        geoclue_accuracy_get_details(accuracy, &accuracyLevel, &horizontalAccuracy, &verticalAccuracy);

        m_lastPositionFromSatellite = accuracyLevel & GEOCLUE_ACCURACY_LEVEL_DETAILED;

        if (!qIsNaN(horizontalAccuracy))
            m_lastPosition.setAttribute(QGeoPositionInfo::HorizontalAccuracy, horizontalAccuracy);
        if (!qIsNaN(verticalAccuracy))
            m_lastPosition.setAttribute(QGeoPositionInfo::VerticalAccuracy, verticalAccuracy);
    }

    if (m_lastVelocityIsFresh) {
        if (!qIsNaN(m_lastVelocity))
            m_lastPosition.setAttribute(QGeoPositionInfo::GroundSpeed, m_lastVelocity);
        if (!qIsNaN(m_lastDirection))
            m_lastPosition.setAttribute(QGeoPositionInfo::Direction, m_lastDirection);
        if (!qIsNaN(m_lastClimb))
            m_lastPosition.setAttribute(QGeoPositionInfo::VerticalSpeed, m_lastClimb);
        m_lastVelocityIsFresh = false;
    }

    m_regularUpdateTimedOut = false;

    emit positionUpdated(m_lastPosition);

#ifdef Q_LOCATION_GEOCLUE_DEBUG
    qDebug() << "Lat, lon, alt, speed:"
             << m_lastPosition.coordinate().latitude()
             << m_lastPosition.coordinate().longitude()
             << m_lastPosition.coordinate().altitude()
             << m_lastPosition.attribute(QGeoPositionInfo::GroundSpeed);
#endif

    // Only stop positioning if regular updates not active.
    if (!m_running) {
        cleanupPositionSource();
        releaseMasterClient();
    }
}

void QGeoPositionInfoSourceGeoclueMaster::cleanupPositionSource()
{
    if (m_pos) {
        g_object_unref(m_pos);
        m_pos = 0;
    }
    if (m_vel) {
        g_object_unref(m_vel);
        m_vel = 0;
    }
}

void QGeoPositionInfoSourceGeoclueMaster::setOptions()
{
    if (!m_pos)
        return;

    QVariantMap options;
    options.insert(QStringLiteral("UpdateInterval"), updateInterval());

    GHashTable *gOptions = g_hash_table_new(g_str_hash, g_str_equal);

    for (QVariantMap::ConstIterator i = options.constBegin(); i != options.constEnd(); ++i) {
        char *key = qstrdup(i.key().toUtf8().constData());

        const QVariant v = i.value();

        GValue *value = new GValue;
        *value = G_VALUE_INIT;

        switch (v.userType()) {
        case QMetaType::QString:
            g_value_init(value, G_TYPE_STRING);
            g_value_set_string(value, v.toString().toUtf8().constData());
            break;
        case QMetaType::Int:
            g_value_init(value, G_TYPE_INT);
            g_value_set_int(value, v.toInt());
            break;
        default:
            qWarning("Unexpected type %d %s", v.userType(), v.typeName());
        }

        g_hash_table_insert(gOptions, key, value);
    }

    geoclue_provider_set_options(GEOCLUE_PROVIDER(m_pos), gOptions, 0);

    GHashTableIter iter;
    char *key;
    GValue *value;

    g_hash_table_iter_init(&iter, gOptions);
    while (g_hash_table_iter_next(&iter, reinterpret_cast<void **>(&key), reinterpret_cast<void **>(&value))) {
        delete[] key;
        delete value;
    }

    g_hash_table_destroy(gOptions);
}

void QGeoPositionInfoSourceGeoclueMaster::setUpdateInterval(int msec)
{
    QGeoPositionInfoSource::setUpdateInterval(qMax(minimumUpdateInterval(), msec));
    setOptions();
}

void QGeoPositionInfoSourceGeoclueMaster::setPreferredPositioningMethods(PositioningMethods methods)
{
    PositioningMethods previousPreferredPositioningMethods = preferredPositioningMethods();
    QGeoPositionInfoSource::setPreferredPositioningMethods(methods);
    if (previousPreferredPositioningMethods == preferredPositioningMethods())
        return;

#ifdef Q_LOCATION_GEOCLUE_DEBUG
    qDebug() << "QGeoPositionInfoSourceGeoclueMaster requested to set methods to" << methods
             << ", and set them to:" << preferredPositioningMethods();
#endif

    m_lastVelocityIsFresh = false;
    m_regularUpdateTimedOut = false;

    // Don't start Geoclue provider until necessary. Don't currently have a master client, no need
    // no recreate one.
    if (!hasMasterClient())
        return;

    // Free potential previous sources, because new requirements can't be set for the client
    // (creating a position object after changing requirements seems to fail).
    cleanupPositionSource();
    releaseMasterClient();

    // Restart Geoclue provider with new requirements.
    configurePositionSource();
    setOptions();
}

QGeoPositionInfo QGeoPositionInfoSourceGeoclueMaster::lastKnownPosition(bool fromSatellitePositioningMethodsOnly) const
{
    if (fromSatellitePositioningMethodsOnly) {
        if (m_lastPositionFromSatellite)
            return m_lastPosition;
        else
            return QGeoPositionInfo();
    }
    return m_lastPosition;
}

QGeoPositionInfoSourceGeoclueMaster::PositioningMethods QGeoPositionInfoSourceGeoclueMaster::supportedPositioningMethods() const
{
    // There is no really knowing which methods the GeoClue master supports.
    return AllPositioningMethods;
}

void QGeoPositionInfoSourceGeoclueMaster::startUpdates()
{
    if (m_running) {
#ifdef Q_LOCATION_GEOCLUE_DEBUG
      qDebug() << "QGeoPositionInfoSourceGeoclueMaster already running";
#endif
        return;
    }

    m_running = true;

    // Start Geoclue provider.
    if (!hasMasterClient()) {
        configurePositionSource();
        setOptions();
    }

    // Emit last known position on start.
    if (m_lastPosition.isValid()) {
        QMetaObject::invokeMethod(this, "positionUpdated", Qt::QueuedConnection,
                                  Q_ARG(QGeoPositionInfo, m_lastPosition));
    }
}

int QGeoPositionInfoSourceGeoclueMaster::minimumUpdateInterval() const
{
    return MINIMUM_UPDATE_INTERVAL;
}

void QGeoPositionInfoSourceGeoclueMaster::stopUpdates()
{
    if (!m_running)
        return;

    if (m_pos)
        g_signal_handlers_disconnect_by_func(G_OBJECT(m_pos), (void *)position_changed, this);
    if (m_vel)
        g_signal_handlers_disconnect_by_func(G_OBJECT(m_vel), (void *)velocity_changed, this);

    m_running = false;

    // Only stop positioning if single update not requested.
    if (!m_requestTimer.isActive()) {
        cleanupPositionSource();
        releaseMasterClient();
    }
}

void QGeoPositionInfoSourceGeoclueMaster::requestUpdate(int timeout)
{
    if (timeout < minimumUpdateInterval() && timeout != 0) {
        emit updateTimeout();
        return;
    }
    if (m_requestTimer.isActive()) {
#ifdef Q_LOCATION_GEOCLUE_DEBUG
      qDebug() << "QGeoPositionInfoSourceGeoclueMaster request timer was active, ignoring startUpdates.";
#endif
        return;
    }

    if (!hasMasterClient()) {
        configurePositionSource();
        setOptions();
    }

    // Create better logic for timeout value (specs leave it impl dependant).
    // Especially if there are active updates ongoing, there is no point of waiting
    // for whole cold start time.
    m_requestTimer.start(timeout ? timeout : UPDATE_TIMEOUT_COLD_START);

    if (m_pos)
        geoclue_position_get_position_async(m_pos, position_callback, this);
}

void QGeoPositionInfoSourceGeoclueMaster::requestUpdateTimeout()
{
#ifdef Q_LOCATION_GEOCLUE_DEBUG
    qDebug() << "QGeoPositionInfoSourceGeoclueMaster requestUpdate timeout occurred.";
#endif
    // If we end up here, there has not been valid position update.
    emit updateTimeout();

    // Only stop positioning if regular updates not active.
    if (!m_running) {
        cleanupPositionSource();
        releaseMasterClient();
    }
}

void QGeoPositionInfoSourceGeoclueMaster::positionProviderChanged(const QByteArray &service, const QByteArray &path)
{
    if (m_pos)
        cleanupPositionSource();

    if (service.isEmpty() || path.isEmpty()) {
        if (!m_regularUpdateTimedOut) {
            m_regularUpdateTimedOut = true;
            emit updateTimeout();
        }
        return;
    }

    m_pos = geoclue_position_new(service.constData(), path.constData());
    if (m_pos) {
        if (m_running) {
            g_signal_connect(G_OBJECT(m_pos), "position-changed",
                             G_CALLBACK(position_changed), this);
        }

        // Get the current position immediately.
        geoclue_position_get_position_async(m_pos, position_callback, this);
        setOptions();

        m_vel = geoclue_velocity_new(service.constData(), path.constData());
        if (m_vel && m_running) {
            g_signal_connect(G_OBJECT(m_vel), "velocity-changed",
                             G_CALLBACK(velocity_changed), this);
        }
    }
}

void QGeoPositionInfoSourceGeoclueMaster::configurePositionSource()
{
    GeoclueAccuracyLevel accuracy;
    GeoclueResourceFlags resourceFlags;

    switch (preferredPositioningMethods()) {
    case SatellitePositioningMethods:
        accuracy = GEOCLUE_ACCURACY_LEVEL_DETAILED;
        resourceFlags = GEOCLUE_RESOURCE_GPS;
        break;
    case NonSatellitePositioningMethods:
        accuracy = GEOCLUE_ACCURACY_LEVEL_NONE;
        resourceFlags = GeoclueResourceFlags(GEOCLUE_RESOURCE_CELL | GEOCLUE_RESOURCE_NETWORK);
        break;
    case AllPositioningMethods:
        accuracy = GEOCLUE_ACCURACY_LEVEL_NONE;
        resourceFlags = GEOCLUE_RESOURCE_ALL;
        break;
    default:
        qWarning("GeoPositionInfoSourceGeoClueMaster unknown preferred method.");
        m_error = UnknownSourceError;
        emit QGeoPositionInfoSource::error(m_error);
        return;
    }

    if (createMasterClient(accuracy, resourceFlags)) {
        m_error = NoError;
    } else {
        m_error = UnknownSourceError;
        emit QGeoPositionInfoSource::error(m_error);
    }
}

QGeoPositionInfoSource::Error QGeoPositionInfoSourceGeoclueMaster::error() const
{
    return m_error;
}

#include "moc_qgeopositioninfosource_geocluemaster_p.cpp"

QT_END_NAMESPACE
