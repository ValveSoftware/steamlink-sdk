/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include "location_provider_qt.h"

#include <math.h>

#include "type_conversion.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QThread>
#include <QtPositioning/QGeoPositionInfoSource>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "content/public/browser/browser_thread.h"
#include "device/geolocation/geolocation_provider.h"
#include "device/geolocation/geolocation_provider_impl.h"

namespace QtWebEngineCore {

using content::BrowserThread;

class QtPositioningHelper : public QObject {
    Q_OBJECT
public:
    QtPositioningHelper(LocationProviderQt *provider);
    ~QtPositioningHelper();

    Q_INVOKABLE void start(bool highAccuracy);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void refresh();

private Q_SLOTS:
    void updatePosition(const QGeoPositionInfo &);
    void error(QGeoPositionInfoSource::Error positioningError);
    void timeout();

private:
    LocationProviderQt *m_locationProvider;
    QGeoPositionInfoSource *m_positionInfoSource;
    base::WeakPtrFactory<LocationProviderQt> m_locationProviderFactory;

    void postToLocationProvider(const base::Closure &task);
    friend class LocationProviderQt;
};

QtPositioningHelper::QtPositioningHelper(LocationProviderQt *provider)
    : m_locationProvider(provider)
    , m_positionInfoSource(0)
    , m_locationProviderFactory(provider)
{
    Q_ASSERT(provider);
}

QtPositioningHelper::~QtPositioningHelper()
{
}

static bool isHighAccuracySource(const QGeoPositionInfoSource *source)
{
    return source->supportedPositioningMethods().testFlag(
                QGeoPositionInfoSource::SatellitePositioningMethods);
}

void QtPositioningHelper::start(bool highAccuracy)
{
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    m_positionInfoSource = QGeoPositionInfoSource::createDefaultSource(this);
    if (!m_positionInfoSource) {
        qWarning("Failed to initialize location provider: The system either has no default "
                 "position source, no valid plugins could be found or the user does not have "
                 "the right permissions.");
        error(QGeoPositionInfoSource::UnknownSourceError);
        return;
    }

    // Find high accuracy source if the default source is not already one.
    if (highAccuracy && !isHighAccuracySource(m_positionInfoSource)) {
        Q_FOREACH (const QString &name, QGeoPositionInfoSource::availableSources()) {
            if (name == m_positionInfoSource->sourceName())
                continue;
            QGeoPositionInfoSource *source = QGeoPositionInfoSource::createSource(name, this);
            if (source && isHighAccuracySource(source)) {
                delete m_positionInfoSource;
                m_positionInfoSource = source;
                break;
            }
            delete source;
        }
        m_positionInfoSource->setPreferredPositioningMethods(
                    QGeoPositionInfoSource::SatellitePositioningMethods);
    }

    connect(m_positionInfoSource, &QGeoPositionInfoSource::positionUpdated, this, &QtPositioningHelper::updatePosition);
    // disambiguate the error getter and the signal in QGeoPositionInfoSource.
    connect(m_positionInfoSource, static_cast<void (QGeoPositionInfoSource::*)(QGeoPositionInfoSource::Error)>(&QGeoPositionInfoSource::error)
            , this, &QtPositioningHelper::error);
    connect(m_positionInfoSource, &QGeoPositionInfoSource::updateTimeout, this, &QtPositioningHelper::timeout);

    m_positionInfoSource->startUpdates();
    return;
}

void QtPositioningHelper::stop()
{
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!m_positionInfoSource)
        return;
    m_positionInfoSource->stopUpdates();
}

void QtPositioningHelper::refresh()
{
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!m_positionInfoSource)
        return;
    m_positionInfoSource->stopUpdates();
}

void QtPositioningHelper::updatePosition(const QGeoPositionInfo &pos)
{
    if (!pos.isValid())
        return;
    Q_ASSERT(m_positionInfoSource->error() == QGeoPositionInfoSource::NoError);
    device::Geoposition newPos;
    newPos.error_code = device::Geoposition::ERROR_CODE_NONE;
    newPos.error_message.clear();

    newPos.timestamp = toTime(pos.timestamp());
    newPos.latitude = pos.coordinate().latitude();
    newPos.longitude = pos.coordinate().longitude();

    const double altitude = pos.coordinate().altitude();
    if (!qIsNaN(altitude))
        newPos.altitude = altitude;

    // Chromium's geoposition needs a valid (as in >=0.) accuracy field.
    // try and get an accuracy estimate from QGeoPositionInfo.
    // If we don't have any accuracy info, 100m seems a pesimistic enough default.
    if (!pos.hasAttribute(QGeoPositionInfo::VerticalAccuracy) && !pos.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
        newPos.accuracy = 100;
    else {
        const double vAccuracy = pos.hasAttribute(QGeoPositionInfo::VerticalAccuracy) ? pos.attribute(QGeoPositionInfo::VerticalAccuracy) : 0;
        const double hAccuracy = pos.hasAttribute(QGeoPositionInfo::HorizontalAccuracy) ? pos.attribute(QGeoPositionInfo::HorizontalAccuracy) : 0;
        newPos.accuracy = sqrt(vAccuracy * vAccuracy + hAccuracy * hAccuracy);
    }

    // And now the "nice to have" fields (-1 means invalid).
    newPos.speed =  pos.hasAttribute(QGeoPositionInfo::GroundSpeed) ? pos.attribute(QGeoPositionInfo::GroundSpeed) : -1;
    newPos.heading =  pos.hasAttribute(QGeoPositionInfo::Direction) ? pos.attribute(QGeoPositionInfo::Direction) : -1;

    if (m_locationProvider)
        postToLocationProvider(base::Bind(&LocationProviderQt::updatePosition, m_locationProviderFactory.GetWeakPtr(), newPos));
}

void QtPositioningHelper::error(QGeoPositionInfoSource::Error positioningError)
{
    Q_ASSERT(positioningError != QGeoPositionInfoSource::NoError);
    device::Geoposition newPos;
    switch (positioningError) {
    case QGeoPositionInfoSource::AccessError:
        newPos.error_code = device::Geoposition::ERROR_CODE_PERMISSION_DENIED;
        break;
    case QGeoPositionInfoSource::ClosedError:
    case QGeoPositionInfoSource::UnknownSourceError: // position unavailable is as good as it gets in Geoposition
    default:
        newPos.error_code = device::Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
        break;
    }
    if (m_locationProvider)
        postToLocationProvider(base::Bind(&LocationProviderQt::updatePosition, m_locationProviderFactory.GetWeakPtr(), newPos));
}

void QtPositioningHelper::timeout()
{
    device::Geoposition newPos;
    // content::Geoposition::ERROR_CODE_TIMEOUT is not handled properly in the renderer process, and the timeout
    // argument used in JS never comes all the way to the browser process.
    // Let's just treat it like any other error where the position is unavailable.
    newPos.error_code = device::Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
    if (m_locationProvider)
        postToLocationProvider(base::Bind(&LocationProviderQt::updatePosition, m_locationProviderFactory.GetWeakPtr(), newPos));
}

inline void QtPositioningHelper::postToLocationProvider(const base::Closure &task)
{
    static_cast<device::GeolocationProviderImpl*>(device::GeolocationProvider::GetInstance())->task_runner()->PostTask(FROM_HERE, task);
}

LocationProviderQt::LocationProviderQt()
    : m_positioningHelper(0)
{
}

LocationProviderQt::~LocationProviderQt()
{
    if (m_positioningHelper) {
        m_positioningHelper->m_locationProvider = 0;
        m_positioningHelper->m_locationProviderFactory.InvalidateWeakPtrs();
        m_positioningHelper->deleteLater();
    }
}

bool LocationProviderQt::StartProvider(bool highAccuracy)
{
    QThread *guiThread = qApp->thread();
    if (!m_positioningHelper) {
        m_positioningHelper = new QtPositioningHelper(this);
        m_positioningHelper->moveToThread(guiThread);
    }

    QMetaObject::invokeMethod(m_positioningHelper, "start", Qt::QueuedConnection, Q_ARG(bool, highAccuracy));
    return true;
}

void LocationProviderQt::StopProvider()
{
    if (m_positioningHelper)
        QMetaObject::invokeMethod(m_positioningHelper, "stop", Qt::QueuedConnection);
}

void LocationProviderQt::OnPermissionGranted()
{
    if (m_positioningHelper)
        QMetaObject::invokeMethod(m_positioningHelper, "refresh", Qt::QueuedConnection);
}

void LocationProviderQt::SetUpdateCallback(const LocationProviderUpdateCallback& callback)
{
    m_callback = callback;
}

void LocationProviderQt::updatePosition(const device::Geoposition &position)
{
    m_lastKnownPosition = position;
    m_callback.Run(this, position);
}

} // namespace QtWebEngineCore

#include "location_provider_qt.moc"
