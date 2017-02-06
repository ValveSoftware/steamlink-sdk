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
#include <qgeosatelliteinfosource.h>
#include "qgeopositioninfosourcefactory.h"
#include "qgeopositioninfosource_p.h"
#include <QPluginLoader>
#include <QStringList>
#include <QCryptographicHash>
#include <QtCore/private/qfactoryloader_p.h>
#include <QFile>

QT_BEGIN_NAMESPACE

/*!
    \class QGeoSatelliteInfoSource
    \inmodule QtPositioning
    \ingroup QtPositioning-positioning
    \since 5.2

    \brief The QGeoSatelliteInfoSource class is an abstract base class for the distribution of satellite information updates.

    The static function QGeoSatelliteInfoSource::createDefaultSource() creates a default
    satellite data source that is appropriate for the platform, if one is
    available. Otherwise, available QGeoPositionInfoSourceFactory plugins will
    be checked for one that has a satellite data source available.

    Call startUpdates() and stopUpdates() to start and stop regular updates,
    or requestUpdate() to request a single update.
    When an update is available, satellitesInViewUpdated() and/or
    satellitesInUseUpdated() will be emitted.

    If regular satellite updates are required, setUpdateInterval() can be used
    to specify how often these updates should be emitted. If no interval is
    specified, updates are simply provided whenever they are available.
    For example:

    \code
        // Emit updates every 10 seconds if available
        QGeoSatelliteInfoSource *source = QGeoSatelliteInfoSource::createDefaultSource(0);
        if (source)
            source->setUpdateInterval(10000);
    \endcode

    To remove an update interval that was previously set, call
    setUpdateInterval() with a value of 0.

    Note that the satellite source may have a minimum value requirement for
    update intervals, as returned by minimumUpdateInterval().
*/

class QGeoSatelliteInfoSourcePrivate
{
public:
    int interval;
    QString providerName;
};

/*!
    Creates a satellite source with the specified \a parent.
*/
QGeoSatelliteInfoSource::QGeoSatelliteInfoSource(QObject *parent)
        : QObject(parent),
        d(new QGeoSatelliteInfoSourcePrivate)
{
    d->interval = 0;
}

/*!
    Destroys the satellite source.
*/
QGeoSatelliteInfoSource::~QGeoSatelliteInfoSource()
{
    delete d;
}

/*!
    Returns the unique name of the satellite source implementation in use.

    This is the same name that can be passed to createSource() in order to
    create a new instance of a particular satellite source implementation.
*/
QString QGeoSatelliteInfoSource::sourceName() const
{
    return d->providerName;
}


/*!
    \property QGeoSatelliteInfoSource::updateInterval
    \brief This property holds the requested interval in milliseconds between each update.

    If the update interval is not set (or is set to 0) the
    source will provide updates as often as necessary.

    If the update interval is set, the source will provide updates at an
    interval as close to the requested interval as possible. If the requested
    interval is less than the minimumUpdateInterval(),
    the minimum interval is used instead.

    Changes to the update interval will happen as soon as is practical, however the
    time the change takes may vary between implementations.  Whether or not the elapsed
    time from the previous interval is counted as part of the new interval is also
    implementation dependent.

    The default value for this property is 0.

    Note: Subclass implementations must call the base implementation of
    setUpdateInterval() so that updateInterval() returns the correct value.
*/
void QGeoSatelliteInfoSource::setUpdateInterval(int msec)
{
    d->interval = msec;
}

int QGeoSatelliteInfoSource::updateInterval() const
{
    return d->interval;
}



/*!
    Creates and returns a source with the specified \a parent that reads
    from the system's default source of satellite update information, or the
    highest priority available plugin.

    Returns 0 if the system has no default satellite source, no valid plugins
    could be found or the user does not have the permission to access the satellite data.
*/
QGeoSatelliteInfoSource *QGeoSatelliteInfoSource::createDefaultSource(QObject *parent)
{
    QList<QJsonObject> plugins = QGeoPositionInfoSourcePrivate::pluginsSorted();
    foreach (const QJsonObject &obj, plugins) {
        if (obj.value(QStringLiteral("Satellite")).isBool()
                && obj.value(QStringLiteral("Satellite")).toBool())
        {
            const QString testableKey = QStringLiteral("Testable");
            if (obj.contains(testableKey) && !obj.value(testableKey).toBool()) {
                static bool inTest = qEnvironmentVariableIsSet("QT_QTESTLIB_RUNNING");
                if (inTest)
                    continue;
            }
            QGeoPositionInfoSourcePrivate d;
            d.metaData = obj;
            d.loadPlugin();
            QGeoSatelliteInfoSource *s = 0;
            if (d.factory)
                s = d.factory->satelliteInfoSource(parent);
            if (s)
                s->d->providerName = d.metaData.value(QStringLiteral("Provider")).toString();
            return s;
        }
    }

    return 0;
}

/*!
    Creates and returns a source with the given \a parent,
    by loading the plugin named \a sourceName.

    Returns 0 if the plugin cannot be found.
*/
QGeoSatelliteInfoSource *QGeoSatelliteInfoSource::createSource(const QString &sourceName, QObject *parent)
{
    QHash<QString, QJsonObject> plugins = QGeoPositionInfoSourcePrivate::plugins();
    if (plugins.contains(sourceName)) {
        QGeoPositionInfoSourcePrivate d;
        d.metaData = plugins.value(sourceName);
        d.loadPlugin();
        QGeoSatelliteInfoSource *src = 0;
        if (d.factory)
            src = d.factory->satelliteInfoSource(parent);
        if (src)
            src->d->providerName = d.metaData.value(QStringLiteral("Provider")).toString();
        return src;
    }

    return 0;
}

/*!
    Returns a list of available source plugins, including the default system
    backend if one is available.
*/
QStringList QGeoSatelliteInfoSource::availableSources()
{
    QStringList plugins;
    const QHash<QString, QJsonObject> meta = QGeoPositionInfoSourcePrivate::plugins();
    for (auto it = meta.cbegin(), end = meta.cend(); it != end; ++it) {
        if (it.value().value(QStringLiteral("Satellite")).isBool()
                && it.value().value(QStringLiteral("Satellite")).toBool()) {
            plugins << it.key();
        }
    }

    return plugins;
}

/*!
    \fn void QGeoSatelliteInfoSource::satellitesInViewUpdated(const QList<QGeoSatelliteInfo> &satellites);

    If startUpdates() or requestUpdate() is called, this signal is emitted
    when an update is available on the satellites that are
    currently in view.

    The \a satellites parameter holds the satellites currently in view.
*/

/*!
    \fn void QGeoSatelliteInfoSource::satellitesInUseUpdated(const QList<QGeoSatelliteInfo> &satellites);

    If startUpdates() or requestUpdate() is called, this signal is emitted
    when an update is available on the number of satellites that are
    currently in use.

    These are the satellites that are used to get a "fix" - that
    is, those used to determine the current position.

    The \a satellites parameter holds the satellites currently in use.
*/

/*!
    \property QGeoSatelliteInfoSource::minimumUpdateInterval
    \brief This property holds the minimum time (in milliseconds) required to retrieve a satellite update.

    This is the minimum value accepted by setUpdateInterval() and
    requestUpdate().
*/


/*!
    \fn virtual void QGeoSatelliteInfoSource::startUpdates() = 0;

    Starts emitting updates at regular intervals. The updates will be
    provided whenever new satellite information becomes available.

    If satellite information cannot be retrieved or some other
    form of timeout has occurred the satellitesInViewUpdated()
    and satellitesInUseUpdated() signals may be emitted with
    empty parameter lists.

    \sa satellitesInViewUpdated(), satellitesInUseUpdated()
*/

/*!
    \fn virtual void QGeoSatelliteInfoSource::stopUpdates() = 0;

    Stops emitting updates at regular intervals.
*/

/*!
    \fn virtual void QGeoSatelliteInfoSource::requestUpdate(int timeout = 0);

    Attempts to get the current satellite information and emit
    satellitesInViewUpdated() and satellitesInUseUpdated() with this
    information. If the current satellite information cannot be found
    within the given \a timeout (in milliseconds) or if \a timeout is less than the value returned by
    minimumUpdateInterval(), requestTimeout() is
    emitted.

    If the timeout is zero, the timeout defaults to a reasonable timeout
    period as appropriate for the source.

    This does nothing if another update request is in progress. However
    it can be called even if startUpdates() has already been called and
    regular updates are in progress.
*/

/*!
    \fn void QGeoSatelliteInfoSource::requestTimeout();

    Emitted if requestUpdate() was called and the current satellite
    information could not be retrieved within the specified timeout.

    While the triggering of this signal may be considered an error condition,
    it does not imply the emission of the \c error() signal. Only the emission of
    \c requestTimeout() is required to indicate a timeout.
*/

/*!
    \fn QGeoSatelliteInfoSource::Error QGeoSatelliteInfoSource::error() const = 0

    Returns the last error that occurred.

    This signal is not emitted when a requestTimeout() has occurred.
*/

/*!
    \fn void QGeoSatelliteInfoSource::error(QGeoSatelliteInfoSource::Error satelliteError)

    This signal is emitted after an error occurred. The \a satelliteError
    parameter describes the type of error that occurred.

*/

/*!
    \enum QGeoSatelliteInfoSource::Error

    The Error enumeration represents the errors which can occur.

    \value AccessError The connection setup to the satellite backend failed because the
        application lacked the required privileges.
    \value ClosedError  The satellite backend closed the connection, which happens for example in case
        the user is switching location services to off. This object becomes invalid and should be deleted.
        A new satellite source can be created by calling createDefaultSource() later on.
    \value NoError No error has occurred.
    \value UnknownSourceError An unidentified error occurred.
 */


#include "moc_qgeosatelliteinfosource.cpp"

QT_END_NAMESPACE
