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

#include "qdeclarativepositionsource_p.h"
#include "qdeclarativeposition_p.h"

#include <QtCore/QCoreApplication>
#include <QtQml/qqmlinfo.h>
#include <QtQml/qqml.h>
#include <qnmeapositioninfosource.h>
#include <QFile>
#include <QTcpSocket>
#include <QTimer>

QT_BEGIN_NAMESPACE

/*!
    \qmltype PositionSource
    \instantiates QDeclarativePositionSource
    \inqmlmodule QtPositioning
    \since 5.2

    \brief The PositionSource type provides the device's current position.

    The PositionSource type provides information about the user device's
    current position. The position is available as a \l{Position} type, which
    contains all the standard parameters typically available from GPS and other
    similar systems, including longitude, latitude, speed and accuracy details.

    As different position sources are available on different platforms and
    devices, these are categorized by their basic type (Satellite, NonSatellite,
    and AllPositioningMethods). The available methods for the current platform
    can be enumerated in the \l{supportedPositioningMethods} property.

    To indicate which methods are suitable for your application, set the
    \l{preferredPositioningMethods} property. If the preferred methods are not
    available, the default source of location data for the platform will be
    chosen instead. If no default source is available (because none are installed
    for the runtime platform, or because it is disabled), the \l{valid} property
    will be set to false.

    The \l updateInterval property can then be used to indicate how often your
    application wishes to receive position updates. The \l{start}(),
    \l{stop}() and \l{update}() methods can be used to control the operation
    of the PositionSource, as well as the \l{active} property, which when set
    is equivalent to calling \l{start}() or \l{stop}().

    When the PositionSource is active, position updates can be retrieved
    either by simply using the \l{position} property in a binding (as the
    value of another item's property), or by providing an implementation of
    the \c {onPositionChanged} signal-handler.

    \section2 Example Usage

    The following example shows a simple PositionSource used to receive
    updates every second and print the longitude and latitude out to
    the console.

    \code
    PositionSource {
        id: src
        updateInterval: 1000
        active: true

        onPositionChanged: {
            var coord = src.position.coordinate;
            console.log("Coordinate:", coord.longitude, coord.latitude);
        }
    }
    \endcode

    The \l{geoflickr}{GeoFlickr} example application shows how to use
    a PositionSource in your application to retrieve local data for users
    from a REST web service.

    \sa {QtPositioning::Position}, {QGeoPositionInfoSource}

*/

/*!
    \qmlsignal PositionSource::updateTimeout()

    If \l update() was called, this signal is emitted if the current position could not be
    retrieved within a certain amount of time.

    If \l start() was called, this signal is emitted if the position engine determines that
    it is not able to provide further regular updates.

    \since Qt Positioning 5.5

    \sa QGeoPositionInfoSource::updateTimeout()
*/


QDeclarativePositionSource::QDeclarativePositionSource()
:   m_positionSource(0), m_preferredPositioningMethods(NoPositioningMethods), m_nmeaFile(0),
    m_nmeaSocket(0), m_active(false), m_singleUpdate(false), m_updateInterval(0),
    m_sourceError(NoError)
{
}

QDeclarativePositionSource::~QDeclarativePositionSource()
{
    delete m_nmeaFile;
    delete m_nmeaSocket;
    delete m_positionSource;
}


/*!
    \qmlproperty string PositionSource::name

    This property holds the unique internal name for the plugin currently
    providing position information.

    Setting the property causes the PositionSource to use a particular positioning provider.  If
    the PositionSource is active at the time that the name property is changed, it will become
    inactive.  If the specified positioning provider cannot be loaded the position source will
    become invalid.

    Changing the name property may cause the \l {updateInterval}, \l {supportedPositioningMethods}
    and \l {preferredPositioningMethods} properties to change as well.
*/


QString QDeclarativePositionSource::name() const
{
    if (m_positionSource)
        return m_positionSource->sourceName();
    else
        return QString();
}

void QDeclarativePositionSource::setName(const QString &newName)
{
    if (m_positionSource && m_positionSource->sourceName() == newName)
        return;

    const QString previousName = name();
    int previousUpdateInterval = updateInterval();
    PositioningMethods previousPositioningMethods = supportedPositioningMethods();
    PositioningMethods previousPreferredPositioningMethods = preferredPositioningMethods();

    delete m_positionSource;
    if (newName.isEmpty())
        m_positionSource = QGeoPositionInfoSource::createDefaultSource(this);
    else
        m_positionSource = QGeoPositionInfoSource::createSource(newName, this);

    if (m_positionSource) {
        connect(m_positionSource, SIGNAL(positionUpdated(QGeoPositionInfo)),
                this, SLOT(positionUpdateReceived(QGeoPositionInfo)));
        connect(m_positionSource, SIGNAL(error(QGeoPositionInfoSource::Error)),
                this, SLOT(sourceErrorReceived(QGeoPositionInfoSource::Error)));
        connect(m_positionSource, SIGNAL(updateTimeout()),
                this, SLOT(updateTimeoutReceived()));

        m_positionSource->setUpdateInterval(m_updateInterval);
        m_positionSource->setPreferredPositioningMethods(
            static_cast<QGeoPositionInfoSource::PositioningMethods>(int(m_preferredPositioningMethods)));

        setPosition(m_positionSource->lastKnownPosition());
    }

    if (previousUpdateInterval != updateInterval())
        emit updateIntervalChanged();

    if (previousPreferredPositioningMethods != preferredPositioningMethods())
        emit preferredPositioningMethodsChanged();

    if (previousPositioningMethods != supportedPositioningMethods())
        emit supportedPositioningMethodsChanged();

    emit validityChanged();

    if (m_active) {
        m_active = false;
        emit activeChanged();
    }

    if (previousName != name())
        emit nameChanged();
}

/*!
    \qmlproperty bool PositionSource::valid

    This property is true if the PositionSource object has acquired a valid
    backend plugin to provide data. If false, other methods on the PositionSource
    will have no effect.

    Applications should check this property to determine whether positioning is
    available and enabled on the runtime platform, and react accordingly.
*/
bool QDeclarativePositionSource::isValid() const
{
    return (m_positionSource != 0);
}

/*!
    \internal
*/
void QDeclarativePositionSource::setNmeaSource(const QUrl &nmeaSource)
{
    if (nmeaSource.scheme() == QLatin1String("socket")) {
        if (m_nmeaSocket
                && nmeaSource.host() == m_nmeaSocket->peerName()
                && nmeaSource.port() == m_nmeaSocket->peerPort()) {
            return;
        }

        delete m_nmeaSocket;
        m_nmeaSocket = new QTcpSocket();

        connect(m_nmeaSocket, static_cast<void (QTcpSocket::*)(QAbstractSocket::SocketError)> (&QAbstractSocket::error),
                this, &QDeclarativePositionSource::socketError);
        connect(m_nmeaSocket, &QTcpSocket::connected,
                this, &QDeclarativePositionSource::socketConnected);

        m_nmeaSocket->connectToHost(nmeaSource.host(), nmeaSource.port(), QTcpSocket::ReadOnly);
    } else {
        // Strip the filename. This is clumsy but the file may be prefixed in several
        // ways: "file:///", "qrc:///", "/", "" in platform dependent manner.
        QString localFileName = nmeaSource.toString();
        if (!QFile::exists(localFileName)) {
            if (localFileName.startsWith("qrc:///")) {
                localFileName.remove(0, 7);
            } else if (localFileName.startsWith("file:///")) {
                localFileName.remove(0, 7);
            } else if (localFileName.startsWith("qrc:/")) {
                localFileName.remove(0, 5);
            }
            if (!QFile::exists(localFileName) && localFileName.startsWith('/')) {
                localFileName.remove(0,1);
            }
        }
        if (m_nmeaFileName == localFileName)
            return;
        m_nmeaFileName = localFileName;

        PositioningMethods previousPositioningMethods = supportedPositioningMethods();

        // The current position source needs to be deleted
        // because QNmeaPositionInfoSource can be bound only to a one file.
        delete m_nmeaSocket;
        m_nmeaSocket = 0;
        delete m_positionSource;
        m_positionSource = 0;
        setPosition(QGeoPositionInfo());
        // Create the NMEA source based on the given data. QML has automatically set QUrl
        // type to point to correct path. If the file is not found, check if the file actually
        // was an embedded resource file.
        delete m_nmeaFile;
        m_nmeaFile = new QFile(localFileName);
        if (!m_nmeaFile->exists()) {
            localFileName.prepend(":");
            m_nmeaFile->setFileName(localFileName);
        }
        if (m_nmeaFile->exists()) {
#ifdef QDECLARATIVE_POSITION_DEBUG
            qDebug() << "QDeclarativePositionSource NMEA File was found: " << localFileName;
#endif
            m_positionSource = new QNmeaPositionInfoSource(QNmeaPositionInfoSource::SimulationMode);
            (qobject_cast<QNmeaPositionInfoSource *>(m_positionSource))->setDevice(m_nmeaFile);
            connect(m_positionSource, SIGNAL(positionUpdated(QGeoPositionInfo)),
                    this, SLOT(positionUpdateReceived(QGeoPositionInfo)));
            connect(m_positionSource, SIGNAL(error(QGeoPositionInfoSource::Error)),
                    this, SLOT(sourceErrorReceived(QGeoPositionInfoSource::Error)));
            connect(m_positionSource, SIGNAL(updateTimeout()),
                    this, SLOT(updateTimeoutReceived()));

            setPosition(m_positionSource->lastKnownPosition());
            if (m_active && !m_singleUpdate) {
                // Keep on updating even though source changed
                QTimer::singleShot(0, this, SLOT(start()));
            }
        } else {
            qmlInfo(this) << QStringLiteral("Nmea file not found") << localFileName;
#ifdef QDECLARATIVE_POSITION_DEBUG
            qDebug() << "QDeclarativePositionSource NMEA File was not found: " << localFileName;
#endif
            if (m_active) {
                m_active = false;
                m_singleUpdate = false;
                emit activeChanged();
            }
        }

        if (previousPositioningMethods != supportedPositioningMethods())
            emit supportedPositioningMethodsChanged();
    }

    m_nmeaSource = nmeaSource;
    emit nmeaSourceChanged();
}

/*!
    \internal
*/
void QDeclarativePositionSource::socketConnected()
{
#ifdef QDECLARATIVE_POSITION_DEBUG
    qDebug() << "Socket connected: " << m_nmeaSocket->peerName();
#endif
    PositioningMethods previousPositioningMethods = supportedPositioningMethods();

    // The current position source needs to be deleted
    // because QNmeaPositionInfoSource can be bound only to a one file.
    delete m_nmeaFile;
    m_nmeaFile = 0;
    delete m_positionSource;

    m_positionSource = new QNmeaPositionInfoSource(QNmeaPositionInfoSource::RealTimeMode);
    (qobject_cast<QNmeaPositionInfoSource *>(m_positionSource))->setDevice(m_nmeaSocket);

    connect(m_positionSource, &QNmeaPositionInfoSource::positionUpdated,
            this, &QDeclarativePositionSource::positionUpdateReceived);
    connect(m_positionSource, SIGNAL(error(QGeoPositionInfoSource::Error)),
            this, SLOT(sourceErrorReceived(QGeoPositionInfoSource::Error)));
    connect(m_positionSource, SIGNAL(updateTimeout()),
            this, SLOT(updateTimeoutReceived()));

    setPosition(m_positionSource->lastKnownPosition());

    if (m_active && !m_singleUpdate) {
        // Keep on updating even though source changed
        QTimer::singleShot(0, this, SLOT(start()));
    }

    if (previousPositioningMethods != supportedPositioningMethods())
        emit supportedPositioningMethodsChanged();
}

/*!
    \internal
*/
void QDeclarativePositionSource::socketError(QAbstractSocket::SocketError error)
{
    delete m_nmeaSocket;
    m_nmeaSocket = 0;

    switch (error) {
    case QAbstractSocket::UnknownSocketError:
        m_sourceError = QDeclarativePositionSource::UnknownSourceError;
        break;
    case QAbstractSocket::SocketAccessError:
        m_sourceError = QDeclarativePositionSource::AccessError;
        break;
    case QAbstractSocket::RemoteHostClosedError:
        m_sourceError = QDeclarativePositionSource::ClosedError;
        break;
    default:
        qWarning() << "Connection failed! QAbstractSocket::SocketError" << error;
        m_sourceError = QDeclarativePositionSource::SocketError;
        break;
    }

    emit sourceErrorChanged();
}


void QDeclarativePositionSource::updateTimeoutReceived()
{
    if (!m_active)
        return;

    if (m_singleUpdate) {
        m_singleUpdate = false;

        // only singleUpdate based timeouts change activity
        // continuous updates may resume again (see QGeoPositionInfoSource::startUpdates())
        m_active = false;
        emit activeChanged();
    }

    emit updateTimeout();
}

void QDeclarativePositionSource::setPosition(const QGeoPositionInfo &pi)
{
    m_position.setPosition(pi);
    emit positionChanged();
}

/*!
    \internal
*/
void QDeclarativePositionSource::setUpdateInterval(int updateInterval)
{
    if (m_positionSource) {
        int previousUpdateInterval = m_positionSource->updateInterval();

        m_updateInterval = updateInterval;

        if (previousUpdateInterval != updateInterval) {
            m_positionSource->setUpdateInterval(updateInterval);
            if (previousUpdateInterval != m_positionSource->updateInterval())
                emit updateIntervalChanged();
        }
    } else {
        if (m_updateInterval != updateInterval) {
            m_updateInterval = updateInterval;
            emit updateIntervalChanged();
        }
    }
}

/*!
    \qmlproperty url PositionSource::nmeaSource

    This property holds the source for NMEA (National Marine Electronics Association)
    position-specification data (file). One purpose of this property is to be of
    development convenience.

    Setting this property will override any other position source. Currently only
    files local to the .qml -file are supported. The NMEA source is created in simulation mode,
    meaning that the data and time information in the NMEA source data is used to provide
    positional updates at the rate at which the data was originally recorded.

    If nmeaSource has been set for a PositionSource object, there is no way to revert
    back to non-file sources.
*/

QUrl QDeclarativePositionSource::nmeaSource() const
{
    return m_nmeaSource;
}

/*!
    \qmlproperty int PositionSource::updateInterval

    This property holds the desired interval between updates (milliseconds).

    \sa {QGeoPositionInfoSource::updateInterval()}
*/

int QDeclarativePositionSource::updateInterval() const
{
    if (!m_positionSource)
        return m_updateInterval;

    return m_positionSource->updateInterval();
}

/*!
    \qmlproperty enumeration PositionSource::supportedPositioningMethods

    This property holds the supported positioning methods of the
    current source.

    \list
    \li PositionSource.NoPositioningMethods - No positioning methods supported (no source).
    \li PositionSource.SatellitePositioningMethods - Satellite-based positioning methods such as GPS are supported.
    \li PositionSource.NonSatellitePositioningMethods - Non-satellite-based methods are supported.
    \li PositionSource.AllPositioningMethods - Both satellite-based and non-satellite positioning methods are supported.
    \endlist

*/

QDeclarativePositionSource::PositioningMethods QDeclarativePositionSource::supportedPositioningMethods() const
{
    if (m_positionSource) {
        return static_cast<QDeclarativePositionSource::PositioningMethods>(
            int(m_positionSource->supportedPositioningMethods()));
    }
    return QDeclarativePositionSource::NoPositioningMethods;
}

/*!
    \qmlproperty enumeration PositionSource::preferredPositioningMethods

    This property holds the preferred positioning methods of the
    current source.

    \list
    \li PositionSource.NoPositioningMethods - No positioning method is preferred.
    \li PositionSource.SatellitePositioningMethods - Satellite-based positioning methods such as GPS should be preferred.
    \li PositionSource.NonSatellitePositioningMethods - Non-satellite-based methods should be preferred.
    \li PositionSource.AllPositioningMethods - Any positioning methods are acceptable.
    \endlist

*/

void QDeclarativePositionSource::setPreferredPositioningMethods(PositioningMethods methods)
{
    if (m_positionSource) {
        PositioningMethods previousPreferredPositioningMethods = preferredPositioningMethods();

        m_preferredPositioningMethods = methods;

        if (previousPreferredPositioningMethods != methods) {
            m_positionSource->setPreferredPositioningMethods(
                static_cast<QGeoPositionInfoSource::PositioningMethods>(int(methods)));
            if (previousPreferredPositioningMethods != m_positionSource->preferredPositioningMethods())
                emit preferredPositioningMethodsChanged();
        }
    } else {
        if (m_preferredPositioningMethods != methods) {
            m_preferredPositioningMethods = methods;
            emit preferredPositioningMethodsChanged();
        }
    }
}

QDeclarativePositionSource::PositioningMethods QDeclarativePositionSource::preferredPositioningMethods() const
{
    if (m_positionSource) {
        return static_cast<QDeclarativePositionSource::PositioningMethods>(
            int(m_positionSource->preferredPositioningMethods()));
    }
    return m_preferredPositioningMethods;
}

/*!
    \qmlmethod PositionSource::start()

    Requests updates from the location source.
    Uses \l updateInterval if set, default interval otherwise.
    If there is no source available, this method has no effect.

    \sa stop, update, active
*/

void QDeclarativePositionSource::start()
{
    if (!m_positionSource)
        return;

    m_positionSource->startUpdates();
    if (!m_active) {
        m_active = true;
        emit activeChanged();
    }
}

/*!
    \qmlmethod PositionSource::update()

    A convenience method to request single update from the location source.
    If there is no source available, this method has no effect.

    If the position source is not active, it will be activated for as
    long as it takes to receive an update, or until the request times
    out.  The request timeout period is source-specific.

    \sa start, stop, active
*/

void QDeclarativePositionSource::update()
{
    if (m_positionSource) {
        if (!m_active) {
            m_active = true;
            m_singleUpdate = true;
            emit activeChanged();
        }
        // Use default timeout value. Set active before calling the
        // update request because on some platforms there may
        // be results immediately.
        m_positionSource->requestUpdate();
    }
}

/*!
    \qmlmethod PositionSource::stop()

    Stops updates from the location source.
    If there is no source available or it is not active,
    this method has no effect.

    \sa start, update, active
*/

void QDeclarativePositionSource::stop()
{
    if (m_positionSource) {
        m_positionSource->stopUpdates();
        if (m_active) {
            m_active = false;
            emit activeChanged();
        }
    }
}

/*!
    \qmlproperty bool PositionSource::active

    This property indicates whether the position source is active.
    Setting this property to false equals calling \l stop, and
    setting this property true equals calling \l start.

    \sa start, stop, update
*/
void QDeclarativePositionSource::setActive(bool active)
{
    if (active == m_active)
        return;

    if (active)
        QTimer::singleShot(0, this, SLOT(start())); // delay ensures all properties have been set
    else
        stop();
}

bool QDeclarativePositionSource::isActive() const
{
    return m_active;
}

/*!
    \qmlproperty Position PositionSource::position

    This property holds the last known positional data.
    It is a read-only property.

    The Position type has different positional member variables,
    whose validity can be checked with appropriate validity functions
    (for example sometimes an update does not have speed or altitude data).

    However, whenever a \c {positionChanged} signal has been received, at least
    position::coordinate::latitude, position::coordinate::longitude, and position::timestamp can
    be assumed to be valid.

    \sa start, stop, update
*/

QDeclarativePosition *QDeclarativePositionSource::position()
{
    return &m_position;
}

void QDeclarativePositionSource::positionUpdateReceived(const QGeoPositionInfo &update)
{
    setPosition(update);

    if (m_singleUpdate && m_active) {
        m_active = false;
        m_singleUpdate = false;
        emit activeChanged();
    }
}


/*!
    \qmlproperty enumeration PositionSource::sourceError

    This property holds the error which last occurred with the PositionSource.

    \list
    \li PositionSource.AccessError - The connection setup to the remote positioning backend failed because the
        application lacked the required privileges.
    \li PositionSource.ClosedError - The positioning backend closed the connection, which happens for example in case
        the user is switching location services to off. As soon as the location service is re-enabled
        regular updates will resume.
    \li PositionSource.NoError - No error has occurred.
    \li PositionSource.UnknownSourceError - An unidentified error occurred.
    \li PositionSource.SocketError - An error occurred while connecting to an nmea source using a socket.
    \endlist

*/

QDeclarativePositionSource::SourceError QDeclarativePositionSource::sourceError() const
{
    return m_sourceError;
}

void QDeclarativePositionSource::componentComplete()
{
    if (!m_positionSource) {
        int previousUpdateInterval = updateInterval();
        PositioningMethods previousPositioningMethods = supportedPositioningMethods();
        PositioningMethods previousPreferredPositioningMethods = preferredPositioningMethods();

        m_positionSource = QGeoPositionInfoSource::createDefaultSource(this);
        if (m_positionSource) {
            connect(m_positionSource, SIGNAL(positionUpdated(QGeoPositionInfo)),
                    this, SLOT(positionUpdateReceived(QGeoPositionInfo)));
            connect(m_positionSource, SIGNAL(error(QGeoPositionInfoSource::Error)),
                    this, SLOT(sourceErrorReceived(QGeoPositionInfoSource::Error)));
            connect(m_positionSource, SIGNAL(updateTimeout()),
                    this, SLOT(updateTimeoutReceived()));

            m_positionSource->setUpdateInterval(m_updateInterval);
            m_positionSource->setPreferredPositioningMethods(
                static_cast<QGeoPositionInfoSource::PositioningMethods>(int(m_preferredPositioningMethods)));

            setPosition(m_positionSource->lastKnownPosition());
        }

        if (previousUpdateInterval != updateInterval())
            emit updateIntervalChanged();

        if (previousPreferredPositioningMethods != preferredPositioningMethods())
            emit preferredPositioningMethodsChanged();

        if (previousPositioningMethods != supportedPositioningMethods())
            emit supportedPositioningMethodsChanged();

        emit validityChanged();

        if (m_active) {
            m_active = false;
            emit activeChanged();
        }

        emit nameChanged();
    }
}

/*!
    \internal
*/
void QDeclarativePositionSource::sourceErrorReceived(const QGeoPositionInfoSource::Error error)
{
    if (error == QGeoPositionInfoSource::AccessError)
        m_sourceError = QDeclarativePositionSource::AccessError;
    else if (error == QGeoPositionInfoSource::ClosedError)
        m_sourceError = QDeclarativePositionSource::ClosedError;
    else if (error == QGeoPositionInfoSource::NoError)
        return; //nothing to do
    else
        m_sourceError = QDeclarativePositionSource::UnknownSourceError;

    emit sourceErrorChanged();
}

#include "moc_qdeclarativepositionsource_p.cpp"

QT_END_NAMESPACE
