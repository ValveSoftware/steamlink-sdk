/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd.
** Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
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

#include "qgeopositioninfosource_geocluemaster.h"

#include <geoclue_interface.h>
#include <position_interface.h>
#include <velocity_interface.h>

#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QVariantMap>
#include <QtCore/QtNumeric>
#include <QtCore/QLoggingCategory>
#include <QtDBus/QDBusMetaType>

#ifndef QT_NO_DATASTREAM
#include <QtCore/QDataStream>
#endif

Q_DECLARE_LOGGING_CATEGORY(lcPositioningGeoclue)

#define MINIMUM_UPDATE_INTERVAL 1000
#define UPDATE_TIMEOUT_COLD_START 120000

QT_BEGIN_NAMESPACE

namespace
{

double knotsToMetersPerSecond(double knots)
{
    return knots * 1852.0 / 3600.0;
}

}

QGeoPositionInfoSourceGeoclueMaster::QGeoPositionInfoSourceGeoclueMaster(QObject *parent)
:   QGeoPositionInfoSource(parent), m_master(new QGeoclueMaster(this)), m_provider(0), m_pos(0),
    m_vel(0), m_lastVelocityIsFresh(false), m_regularUpdateTimedOut(false), m_lastVelocity(qQNaN()),
    m_lastDirection(qQNaN()), m_lastClimb(qQNaN()), m_lastPositionFromSatellite(false),
    m_running(false), m_error(NoError)
{
    qDBusRegisterMetaType<Accuracy>();

#ifndef QT_NO_DATASTREAM
    // Load the last known location
    QFile file(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
               QStringLiteral("/qtposition-geoclue"));
    if (file.open(QIODevice::ReadOnly)) {
        QDataStream out(&file);
        out >> m_lastPosition;
    }
#endif

    connect(m_master, SIGNAL(positionProviderChanged(QString,QString,QString,QString)),
            this, SLOT(positionProviderChanged(QString,QString,QString,QString)));

    m_requestTimer.setSingleShot(true);
    connect(&m_requestTimer, SIGNAL(timeout()), this, SLOT(requestUpdateTimeout()));

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

    cleanupPositionSource();
}

void QGeoPositionInfoSourceGeoclueMaster::positionUpdateFailed()
{
    qCDebug(lcPositioningGeoclue) << "position update failed.";

    m_lastVelocityIsFresh = false;
    if (m_running && !m_regularUpdateTimedOut) {
        m_regularUpdateTimedOut = true;
        emit updateTimeout();
    }
}

void QGeoPositionInfoSourceGeoclueMaster::updatePosition(PositionFields fields, int timestamp,
                                                         double latitude, double longitude,
                                                         double altitude, Accuracy accuracy)
{
    if (m_requestTimer.isActive())
        m_requestTimer.stop();

    QGeoCoordinate coordinate(latitude, longitude);
    if (fields & Altitude)
        coordinate.setAltitude(altitude);

    m_lastPosition = QGeoPositionInfo(coordinate, QDateTime::fromTime_t(timestamp));

    m_lastPositionFromSatellite = accuracy.level() == Accuracy::Detailed;

    if (!qIsNaN(accuracy.horizontal()))
        m_lastPosition.setAttribute(QGeoPositionInfo::HorizontalAccuracy, accuracy.horizontal());
    if (!qIsNaN(accuracy.vertical()))
        m_lastPosition.setAttribute(QGeoPositionInfo::VerticalAccuracy, accuracy.vertical());

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

    qCDebug(lcPositioningGeoclue) << m_lastPosition;

    // Only stop positioning if regular updates not active.
    if (!m_running) {
        cleanupPositionSource();
        m_master->releaseMasterClient();
    }
}

void QGeoPositionInfoSourceGeoclueMaster::velocityUpdateFailed()
{
    qCDebug(lcPositioningGeoclue) << "velocity update failed.";

    // Set the velocitydata non-fresh.
    m_lastVelocityIsFresh = false;
}

void QGeoPositionInfoSourceGeoclueMaster::updateVelocity(VelocityFields fields, int timestamp,
                                                         double speed, double direction,
                                                         double climb)
{
    Q_UNUSED(timestamp)

    // Store the velocity and mark it as fresh. Simple but hopefully adequate.
    m_lastVelocity = (fields & Speed) ? knotsToMetersPerSecond(speed) : qQNaN();
    m_lastDirection = (fields & Direction) ? direction : qQNaN();
    m_lastClimb = (fields & Climb) ? climb : qQNaN();
    m_lastVelocityIsFresh = true;

    qCDebug(lcPositioningGeoclue) << m_lastVelocity << m_lastDirection << m_lastClimb;
}

void QGeoPositionInfoSourceGeoclueMaster::cleanupPositionSource()
{
    qCDebug(lcPositioningGeoclue) << "cleaning up position source";

    if (m_provider)
        m_provider->RemoveReference();
    delete m_provider;
    m_provider = 0;
    delete m_pos;
    m_pos = 0;
    delete m_vel;
    m_vel = 0;
}

void QGeoPositionInfoSourceGeoclueMaster::setOptions()
{
    if (!m_provider)
        return;

    QVariantMap options;
    options.insert(QStringLiteral("UpdateInterval"), updateInterval());

    m_provider->SetOptions(options);
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

    qCDebug(lcPositioningGeoclue) << "requested to set methods to" << methods
                                  << ", and set them to:" << preferredPositioningMethods();

    m_lastVelocityIsFresh = false;
    m_regularUpdateTimedOut = false;

    // Don't start Geoclue provider until necessary. Don't currently have a master client, no need
    // no recreate one.
    if (!m_master->hasMasterClient())
        return;

    // Free potential previous sources, because new requirements can't be set for the client
    // (creating a position object after changing requirements seems to fail).
    cleanupPositionSource();
    m_master->releaseMasterClient();

    // Restart Geoclue provider with new requirements.
    configurePositionSource();
    setOptions();
}

QGeoPositionInfo QGeoPositionInfoSourceGeoclueMaster::lastKnownPosition(bool fromSatellitePositioningMethodsOnly) const
{
    if (fromSatellitePositioningMethodsOnly && !m_lastPositionFromSatellite)
        return QGeoPositionInfo();

    return m_lastPosition;
}

QGeoPositionInfoSourceGeoclueMaster::PositioningMethods QGeoPositionInfoSourceGeoclueMaster::supportedPositioningMethods() const
{
    return AllPositioningMethods;
}

void QGeoPositionInfoSourceGeoclueMaster::startUpdates()
{
    if (m_running) {
        qCDebug(lcPositioningGeoclue) << "already running.";
        return;
    }

    m_running = true;

    qCDebug(lcPositioningGeoclue) << "starting updates";

    // Start Geoclue provider.
    if (!m_master->hasMasterClient()) {
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
    if (!m_running) {
        qCDebug(lcPositioningGeoclue) << "already stopped.";
        return;
    }

    qCDebug(lcPositioningGeoclue) << "stopping updates";

    if (m_pos) {
        disconnect(m_pos, SIGNAL(PositionChanged(qint32,qint32,double,double,double,Accuracy)),
                   this, SLOT(positionChanged(qint32,qint32,double,double,double,Accuracy)));
    }

    if (m_vel) {
        disconnect(m_vel, SIGNAL(VelocityChanged(qint32,qint32,double,double,double)),
                   this, SLOT(velocityChanged(qint32,qint32,double,double,double)));
    }

    m_running = false;

    // Only stop positioning if single update not requested.
    if (!m_requestTimer.isActive()) {
        cleanupPositionSource();
        m_master->releaseMasterClient();
    }
}

void QGeoPositionInfoSourceGeoclueMaster::requestUpdate(int timeout)
{
    if (timeout < minimumUpdateInterval() && timeout != 0) {
        emit updateTimeout();
        return;
    }
    if (m_requestTimer.isActive()) {
        qCDebug(lcPositioningGeoclue) << "request timer was active, ignoring startUpdates.";
        return;
    }

    if (!m_master->hasMasterClient()) {
        configurePositionSource();
        setOptions();
    }

    // Create better logic for timeout value (specs leave it impl dependant).
    // Especially if there are active updates ongoing, there is no point of waiting
    // for whole cold start time.
    m_requestTimer.start(timeout ? timeout : UPDATE_TIMEOUT_COLD_START);

    if (m_pos) {
        QDBusPendingReply<qint32, qint32, double, double, double, Accuracy> reply = m_pos->GetPosition();
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
        connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                this, SLOT(getPositionFinished(QDBusPendingCallWatcher*)));
    }
}

void QGeoPositionInfoSourceGeoclueMaster::positionProviderChanged(const QString &name,
                                                                  const QString &description,
                                                                  const QString &service,
                                                                  const QString &path)
{
    Q_UNUSED(name)
    Q_UNUSED(description)

    cleanupPositionSource();

    if (service.isEmpty() || path.isEmpty()) {
        if (!m_regularUpdateTimedOut) {
            m_regularUpdateTimedOut = true;
            emit updateTimeout();
        }
        return;
    }

    qCDebug(lcPositioningGeoclue) << "position provider changed to" << name;

    m_provider = new OrgFreedesktopGeoclueInterface(service, path, QDBusConnection::sessionBus());
    m_provider->AddReference();

    m_pos = new OrgFreedesktopGeocluePositionInterface(service, path, QDBusConnection::sessionBus());

    if (m_running) {
        connect(m_pos, SIGNAL(PositionChanged(qint32,qint32,double,double,double,Accuracy)),
                this, SLOT(positionChanged(qint32,qint32,double,double,double,Accuracy)));
    }

    // Get the current position immediately.
    QDBusPendingReply<qint32, qint32, double, double, double, Accuracy> reply = m_pos->GetPosition();
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(getPositionFinished(QDBusPendingCallWatcher*)));

    setOptions();

    m_vel = new OrgFreedesktopGeoclueVelocityInterface(service, path, QDBusConnection::sessionBus());
    if (m_vel->isValid() && m_running) {
        connect(m_vel, SIGNAL(VelocityChanged(qint32,qint32,double,double,double)),
                this, SLOT(velocityChanged(qint32,qint32,double,double,double)));
    }
}

void QGeoPositionInfoSourceGeoclueMaster::requestUpdateTimeout()
{
    qCDebug(lcPositioningGeoclue) << "request update timeout occurred.";

    // If we end up here, there has not been valid position update.
    emit updateTimeout();

    // Only stop positioning if regular updates not active.
    if (!m_running) {
        cleanupPositionSource();
        m_master->releaseMasterClient();
    }
}

void QGeoPositionInfoSourceGeoclueMaster::getPositionFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<qint32, qint32, double, double, double, Accuracy> reply = *watcher;
    watcher->deleteLater();

    if (reply.isError())
        return;

    PositionFields fields = static_cast<PositionFields>(reply.argumentAt<0>());

    qCDebug(lcPositioningGeoclue) << "got position update with fields" << int(fields);

    if (fields & Latitude && fields & Longitude) {
        qint32 timestamp = reply.argumentAt<1>();
        double latitude = reply.argumentAt<2>();
        double longitude = reply.argumentAt<3>();
        double altitude = reply.argumentAt<4>();
        Accuracy accuracy = reply.argumentAt<5>();
        updatePosition(fields, timestamp, latitude, longitude, altitude, accuracy);
    }
}

void QGeoPositionInfoSourceGeoclueMaster::positionChanged(qint32 fields, qint32 timestamp, double latitude, double longitude, double altitude, const Accuracy &accuracy)
{
    PositionFields pFields = static_cast<PositionFields>(fields);

    qCDebug(lcPositioningGeoclue) << "position changed with fields" << fields;

    if (pFields & Latitude && pFields & Longitude)
        updatePosition(pFields, timestamp, latitude, longitude, altitude, accuracy);
    else
        positionUpdateFailed();
}

void QGeoPositionInfoSourceGeoclueMaster::velocityChanged(qint32 fields, qint32 timestamp, double speed, double direction, double climb)
{
    VelocityFields vFields = static_cast<VelocityFields>(fields);

    if (vFields == NoVelocityFields)
        velocityUpdateFailed();
    else
        updateVelocity(vFields, timestamp, speed, direction, climb);
}

void QGeoPositionInfoSourceGeoclueMaster::configurePositionSource()
{
    qCDebug(lcPositioningGeoclue);

    bool created = false;

    switch (preferredPositioningMethods()) {
    case SatellitePositioningMethods:
        created = m_master->createMasterClient(Accuracy::Detailed, QGeoclueMaster::ResourceGps);
        break;
    case NonSatellitePositioningMethods:
        created = m_master->createMasterClient(Accuracy::None, QGeoclueMaster::ResourceCell | QGeoclueMaster::ResourceNetwork);
        break;
    case AllPositioningMethods:
        created = m_master->createMasterClient(Accuracy::None, QGeoclueMaster::ResourceAll);
        break;
    default:
        qWarning("QGeoPositionInfoSourceGeoclueMaster unknown preferred method.");
        m_error = UnknownSourceError;
        emit QGeoPositionInfoSource::error(m_error);
        return;
    }

    if (!created) {
        m_error = UnknownSourceError;
        emit QGeoPositionInfoSource::error(m_error);
    }
}

QGeoPositionInfoSource::Error QGeoPositionInfoSourceGeoclueMaster::error() const
{
    return m_error;
}

QT_END_NAMESPACE
