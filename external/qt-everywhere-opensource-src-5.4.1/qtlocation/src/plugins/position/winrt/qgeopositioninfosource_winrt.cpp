/****************************************************************************
**
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

#include "qgeopositioninfosource_winrt_p.h"

#include <QCoreApplication>

#include <windows.system.h>
#include <windows.devices.geolocation.h>
#include <windows.foundation.collections.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Devices::Geolocation;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::System;

typedef ITypedEventHandler<Geolocator *, PositionChangedEventArgs *> GeoLocatorPositionHandler;
typedef ITypedEventHandler<Geolocator *, StatusChangedEventArgs *> GeoLocatorStatusHandler;

QT_BEGIN_NAMESPACE

QGeoPositionInfoSourceWinrt::QGeoPositionInfoSourceWinrt(QObject *parent)
    : QGeoPositionInfoSource(parent)
    , m_positionError(QGeoPositionInfoSource::NoError)
    , m_updatesOngoing(false)
{
    HRESULT hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Devices_Geolocation_Geolocator).Get(),
                                    &m_locator);

    if (FAILED(hr)) {
        setError(QGeoPositionInfoSource::UnknownSourceError);
        qErrnoWarning(hr, "Could not initialize native location services");
        return;
    }

    hr = m_locator->put_ReportInterval(minimumUpdateInterval());
    if (FAILED(hr)) {
        setError(QGeoPositionInfoSource::UnknownSourceError);
        qErrnoWarning(hr, "Could not initialize report interval");
        return;
    }
    hr = m_locator->put_DesiredAccuracy(PositionAccuracy::PositionAccuracy_High);
    if (FAILED(hr)) {
        setError(QGeoPositionInfoSource::UnknownSourceError);
        qErrnoWarning(hr, "Could not initialize desired accuracy");
        return;
    }

    m_positionToken.value = 0;

    m_periodicTimer.setSingleShot(true);
    m_periodicTimer.setInterval(minimumUpdateInterval());
    connect(&m_periodicTimer, SIGNAL(timeout()), this, SLOT(virtualPositionUpdate()));

    m_singleUpdateTimer.setSingleShot(true);
    connect(&m_singleUpdateTimer, SIGNAL(timeout()), this, SLOT(singleUpdateTimeOut()));

    setPreferredPositioningMethods(QGeoPositionInfoSource::AllPositioningMethods);
}

QGeoPositionInfoSourceWinrt::~QGeoPositionInfoSourceWinrt()
{
}

QGeoPositionInfo QGeoPositionInfoSourceWinrt::lastKnownPosition(bool fromSatellitePositioningMethodsOnly) const
{
    Q_UNUSED(fromSatellitePositioningMethodsOnly)
    return m_lastPosition;
}

QGeoPositionInfoSource::PositioningMethods QGeoPositionInfoSourceWinrt::supportedPositioningMethods() const
{
    PositionStatus status;
    HRESULT hr = m_locator->get_LocationStatus(&status);
    if (FAILED(hr))
        return QGeoPositionInfoSource::NoPositioningMethods;

    switch (status) {
    case PositionStatus::PositionStatus_NoData:
    case PositionStatus::PositionStatus_Disabled:
    case PositionStatus::PositionStatus_NotAvailable:
        return QGeoPositionInfoSource::NoPositioningMethods;
    }

    return QGeoPositionInfoSource::AllPositioningMethods;
}

void QGeoPositionInfoSourceWinrt::setPreferredPositioningMethods(QGeoPositionInfoSource::PositioningMethods methods)
{
    PositioningMethods previousPreferredPositioningMethods = preferredPositioningMethods();
    QGeoPositionInfoSource::setPreferredPositioningMethods(methods);
    if (previousPreferredPositioningMethods == preferredPositioningMethods())
        return;

    bool needsRestart = m_positionToken.value != 0;

    if (needsRestart)
        stopHandler();

    HRESULT hr;
    if (methods & PositioningMethod::SatellitePositioningMethods)
        hr = m_locator->put_DesiredAccuracy(PositionAccuracy::PositionAccuracy_High);
    else
        hr = m_locator->put_DesiredAccuracy(PositionAccuracy::PositionAccuracy_Default);

    if (FAILED(hr)) {
        qErrnoWarning(hr, "Could not set positioning accuracy");
        return;
    }

    if (needsRestart)
        startHandler();
}

void QGeoPositionInfoSourceWinrt::setUpdateInterval(int msec)
{
    // If msec is 0 we send updates as data becomes available, otherwise we force msec to be equal
    // to or larger than the minimum update interval.
    if (msec != 0 && msec < minimumUpdateInterval())
        msec = minimumUpdateInterval();

    HRESULT hr = m_locator->put_ReportInterval(msec);
    if (FAILED(hr)) {
        setError(QGeoPositionInfoSource::UnknownSourceError);
        qErrnoWarning(hr, "Failed to set update interval");
        return;
    }
    if (msec != 0)
        m_periodicTimer.setInterval(msec);
    else
        m_periodicTimer.setInterval(minimumUpdateInterval());

    QGeoPositionInfoSource::setUpdateInterval(msec);
}

int QGeoPositionInfoSourceWinrt::minimumUpdateInterval() const
{
    // We use one second to reduce potential timer events
    // in case the platform itself stops reporting
    return 1000;
}

void QGeoPositionInfoSourceWinrt::startUpdates()
{
    if (m_updatesOngoing)
        return;

    if (!startHandler())
        return;
    m_updatesOngoing = true;
    m_periodicTimer.start();
}

void QGeoPositionInfoSourceWinrt::stopUpdates()
{
    stopHandler();
    m_updatesOngoing = false;
    m_periodicTimer.stop();
}

bool QGeoPositionInfoSourceWinrt::startHandler()
{
    // Check if already attached
    if (m_positionToken.value != 0)
        return true;

    if (preferredPositioningMethods() == QGeoPositionInfoSource::NoPositioningMethods) {
        setError(QGeoPositionInfoSource::UnknownSourceError);
        return false;
    }

    if (!checkNativeState())
        return false;

    HRESULT hr = m_locator->add_PositionChanged(Callback<GeoLocatorPositionHandler>(this,
                                                &QGeoPositionInfoSourceWinrt::onPositionChanged).Get(),
                                                &m_positionToken);

    if (FAILED(hr)) {
        setError(QGeoPositionInfoSource::UnknownSourceError);
        qErrnoWarning(hr, "Could not add position handler");
        return false;
    }
    return true;
}

void QGeoPositionInfoSourceWinrt::stopHandler()
{
    if (!m_positionToken.value)
        return;
    m_locator->remove_PositionChanged(m_positionToken);
    m_positionToken.value = 0;
}

void QGeoPositionInfoSourceWinrt::requestUpdate(int timeout)
{
    if (timeout < minimumUpdateInterval()) {
        emit updateTimeout();
        return;
    }
    startHandler();
    m_singleUpdateTimer.start(timeout);
}

void QGeoPositionInfoSourceWinrt::virtualPositionUpdate()
{
    // Need to check if services are still running and ok
    if (!checkNativeState()) {
        stopUpdates();
        return;
    }

    // The operating system did not provide information in time
    // Hence we send a virtual position update to keep same behavior
    // between backends.
    // This only applies to the periodic timer, not for single requests
    // We can only do this if we received a valid position before
    if (m_lastPosition.isValid()) {
        QGeoPositionInfo sent = m_lastPosition;
        sent.setTimestamp(QDateTime::currentDateTime());
        m_lastPosition = sent;
        emit positionUpdated(sent);
    }
    m_periodicTimer.start();
}

void QGeoPositionInfoSourceWinrt::singleUpdateTimeOut()
{
    emit updateTimeout();
    if (!m_updatesOngoing)
        stopHandler();
}

QGeoPositionInfoSource::Error QGeoPositionInfoSourceWinrt::error() const
{
    return m_positionError;
}

void QGeoPositionInfoSourceWinrt::setError(QGeoPositionInfoSource::Error positionError)
{
    if (positionError == m_positionError)
        return;
    m_positionError = positionError;
    emit QGeoPositionInfoSource::error(positionError);
}

bool QGeoPositionInfoSourceWinrt::checkNativeState()
{
    PositionStatus status;
    HRESULT hr = m_locator->get_LocationStatus(&status);
    if (FAILED(hr)) {
        setError(QGeoPositionInfoSource::UnknownSourceError);
        qErrnoWarning(hr, "Could not query status");
        return false;
    }

    bool result = false;
    switch (status) {
    case PositionStatus::PositionStatus_NotAvailable:
        setError(QGeoPositionInfoSource::UnknownSourceError);
        break;
    case PositionStatus::PositionStatus_Disabled:
    case PositionStatus::PositionStatus_NoData:
        setError(QGeoPositionInfoSource::ClosedError);
        break;
    default:
        setError(QGeoPositionInfoSource::NoError);
        result = true;
        break;
    }
    return result;
}

HRESULT QGeoPositionInfoSourceWinrt::onPositionChanged(IGeolocator *locator, IPositionChangedEventArgs *args)
{
    Q_UNUSED(locator);

    m_periodicTimer.stop();

    HRESULT hr;
    ComPtr<IGeoposition> pos;
    hr = args->get_Position(&pos);
    if (FAILED(hr))
        qErrnoWarning(hr, "Could not access position object");

    QGeoPositionInfo currentInfo;

    ComPtr<IGeocoordinate> coord;
    hr = pos->get_Coordinate(&coord);
    if (FAILED(hr))
        qErrnoWarning(hr, "Could not access coordinate");

    DOUBLE lat;
    hr = coord->get_Latitude(&lat);
    if (FAILED(hr))
        qErrnoWarning(hr, "Could not access latitude");

    DOUBLE lon;
    hr = coord->get_Longitude(&lon);
    if (FAILED(hr))
        qErrnoWarning(hr, "Could not access longitude");

    // Depending on data source altitude can
    // be identified or not
    IReference<double> *alt;
    hr = coord->get_Altitude(&alt);
    if (SUCCEEDED(hr) && alt) {
        double altd;
        hr = alt->get_Value(&altd);
        currentInfo.setCoordinate(QGeoCoordinate(lat, lon, altd));
    } else {
        currentInfo.setCoordinate(QGeoCoordinate(lat, lon));
    }

    DOUBLE accuracy;
    hr = coord->get_Accuracy(&accuracy);
    if (SUCCEEDED(hr))
        currentInfo.setAttribute(QGeoPositionInfo::HorizontalAccuracy, accuracy);

    IReference<double> *altAccuracy;
    hr = coord->get_AltitudeAccuracy(&altAccuracy);
    if (SUCCEEDED(hr) && altAccuracy) {
        double value;
        hr = alt->get_Value(&value);
        currentInfo.setAttribute(QGeoPositionInfo::VerticalAccuracy, value);
    }

    IReference<double> *speed;
    hr = coord->get_Speed(&speed);
    if (SUCCEEDED(hr) && speed) {
        double value;
        hr = speed->get_Value(&value);
        currentInfo.setAttribute(QGeoPositionInfo::GroundSpeed, value);
    }

    currentInfo.setTimestamp(QDateTime::currentDateTime());

    m_lastPosition = currentInfo;

    if (m_updatesOngoing)
        m_periodicTimer.start();

    if (m_singleUpdateTimer.isActive()) {
        m_singleUpdateTimer.stop();
        if (!m_updatesOngoing)
            stopHandler();
    }

    emit positionUpdated(currentInfo);
    return S_OK;
}

QT_END_NAMESPACE
