/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qgeopositioninfosource_winrt_p.h"

#include <QCoreApplication>
#include <QMutex>
#include <qfunctions_winrt.h>
#ifdef Q_OS_WINRT
#include <private/qeventdispatcher_winrt_p.h>
#endif

#include <functional>
#include <windows.system.h>
#include <windows.devices.geolocation.h>
#include <windows.foundation.h>
#include <windows.foundation.collections.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Devices::Geolocation;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;

typedef ITypedEventHandler<Geolocator *, PositionChangedEventArgs *> GeoLocatorPositionHandler;
typedef ITypedEventHandler<Geolocator *, StatusChangedEventArgs *> GeoLocatorStatusHandler;
typedef IAsyncOperationCompletedHandler<Geoposition*> PositionHandler;
#if _MSC_VER >= 1900
typedef IAsyncOperationCompletedHandler<GeolocationAccessStatus> AccessHandler;
#endif

Q_DECLARE_METATYPE(QGeoPositionInfo)

QT_BEGIN_NAMESPACE

#ifndef Q_OS_WINRT
namespace QEventDispatcherWinRT {
HRESULT runOnXamlThread(const std::function<HRESULT ()> &delegate, bool waitForRun = true)
{
    Q_UNUSED(waitForRun);
    return delegate();
}
}
#endif

class QGeoPositionInfoSourceWinRTPrivate {
public:
    ComPtr<IGeolocator> locator;
    QTimer periodicTimer;
    QTimer singleUpdateTimer;
    QGeoPositionInfo lastPosition;
    QGeoPositionInfoSource::Error positionError;
    EventRegistrationToken statusToken;
    EventRegistrationToken positionToken;
    QMutex mutex;
    bool updatesOngoing;
};


QGeoPositionInfoSourceWinRT::QGeoPositionInfoSourceWinRT(QObject *parent)
    : QGeoPositionInfoSource(parent)
    , d_ptr(new QGeoPositionInfoSourceWinRTPrivate)
{
    Q_D(QGeoPositionInfoSourceWinRT);
    d->positionError = QGeoPositionInfoSource::NoError;
    d->updatesOngoing = false;

    qRegisterMetaType<QGeoPositionInfo>();

    requestAccess();
    HRESULT hr = QEventDispatcherWinRT::runOnXamlThread([this, d]() {
        HRESULT hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Devices_Geolocation_Geolocator).Get(),
                                        &d->locator);
        RETURN_HR_IF_FAILED("Could not initialize native location services.");

        // StatusChanged throws an exception on Windows 8.1
#if _MSC_VER >= 1900
        hr = d->locator->add_StatusChanged(Callback<GeoLocatorStatusHandler>(this,
                                                                             &QGeoPositionInfoSourceWinRT::onStatusChanged).Get(),
                                           &d->statusToken);
        RETURN_HR_IF_FAILED("Could not add status callback.");
#endif

        hr = d->locator->put_ReportInterval(1000);
        RETURN_HR_IF_FAILED("Could not initialize report interval.");

        return hr;
    });
    Q_ASSERT_SUCCEEDED(hr);

    hr = d->locator->put_DesiredAccuracy(PositionAccuracy::PositionAccuracy_Default);
    if (FAILED(hr)) {
        setError(QGeoPositionInfoSource::UnknownSourceError);
        qErrnoWarning(hr, "Could not initialize desired accuracy.");
        return;
    }

    d->positionToken.value = 0;

    d->periodicTimer.setSingleShot(true);
    d->periodicTimer.setInterval(minimumUpdateInterval());
    connect(&d->periodicTimer, &QTimer::timeout, this, &QGeoPositionInfoSourceWinRT::virtualPositionUpdate);

    d->singleUpdateTimer.setSingleShot(true);
    connect(&d->singleUpdateTimer, &QTimer::timeout, this, &QGeoPositionInfoSourceWinRT::singleUpdateTimeOut);

    setPreferredPositioningMethods(QGeoPositionInfoSource::AllPositioningMethods);

    connect(this, &QGeoPositionInfoSourceWinRT::nativePositionUpdate, this, &QGeoPositionInfoSourceWinRT::updateSynchronized);
}

QGeoPositionInfoSourceWinRT::~QGeoPositionInfoSourceWinRT()
{
}

QGeoPositionInfo QGeoPositionInfoSourceWinRT::lastKnownPosition(bool fromSatellitePositioningMethodsOnly) const
{
    Q_D(const QGeoPositionInfoSourceWinRT);
    Q_UNUSED(fromSatellitePositioningMethodsOnly)
    return d->lastPosition;
}

QGeoPositionInfoSource::PositioningMethods QGeoPositionInfoSourceWinRT::supportedPositioningMethods() const
{
    Q_D(const QGeoPositionInfoSourceWinRT);

    PositionStatus status;
    HRESULT hr = QEventDispatcherWinRT::runOnXamlThread([d, &status]() {
        HRESULT hr = d->locator->get_LocationStatus(&status);
        return hr;
    });
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

void QGeoPositionInfoSourceWinRT::setPreferredPositioningMethods(QGeoPositionInfoSource::PositioningMethods methods)
{
    Q_D(QGeoPositionInfoSourceWinRT);

    PositioningMethods previousPreferredPositioningMethods = preferredPositioningMethods();
    QGeoPositionInfoSource::setPreferredPositioningMethods(methods);
    if (previousPreferredPositioningMethods == preferredPositioningMethods())
        return;

    bool needsRestart = d->positionToken.value != 0;

    if (needsRestart)
        stopHandler();

    PositionAccuracy acc = methods & PositioningMethod::SatellitePositioningMethods ?
                PositionAccuracy::PositionAccuracy_High :
                PositionAccuracy::PositionAccuracy_Default;
    HRESULT hr = QEventDispatcherWinRT::runOnXamlThread([d, acc]() {
        HRESULT hr = d->locator->put_DesiredAccuracy(acc);
        return hr;
    });
    RETURN_VOID_IF_FAILED("Could not set positioning accuracy.");

    if (needsRestart)
        startHandler();
}

void QGeoPositionInfoSourceWinRT::setUpdateInterval(int msec)
{
    Q_D(QGeoPositionInfoSourceWinRT);
    // Windows Phone 8.1 and Windows 10 do not support 0 interval
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
    if (msec == 0)
        msec = minimumUpdateInterval();
#endif

    // If msec is 0 we send updates as data becomes available, otherwise we force msec to be equal
    // to or larger than the minimum update interval.
    if (msec != 0 && msec < minimumUpdateInterval())
        msec = minimumUpdateInterval();

    HRESULT hr = d->locator->put_ReportInterval(msec);
    if (FAILED(hr)) {
        setError(QGeoPositionInfoSource::UnknownSourceError);
        qErrnoWarning(hr, "Failed to set update interval");
        return;
    }

    d->periodicTimer.setInterval(qMax(msec, minimumUpdateInterval()));

    QGeoPositionInfoSource::setUpdateInterval(msec);
}

int QGeoPositionInfoSourceWinRT::minimumUpdateInterval() const
{
    // We use one second to reduce potential timer events
    // in case the platform itself stops reporting
    return 1000;
}

void QGeoPositionInfoSourceWinRT::startUpdates()
{
    Q_D(QGeoPositionInfoSourceWinRT);

    if (d->updatesOngoing)
        return;

    if (!startHandler())
        return;
    d->updatesOngoing = true;
    d->periodicTimer.start();
}

void QGeoPositionInfoSourceWinRT::stopUpdates()
{
    Q_D(QGeoPositionInfoSourceWinRT);

    stopHandler();
    d->updatesOngoing = false;
    d->periodicTimer.stop();
}

bool QGeoPositionInfoSourceWinRT::startHandler()
{
    Q_D(QGeoPositionInfoSourceWinRT);

    // Check if already attached
    if (d->positionToken.value != 0)
        return true;

    if (preferredPositioningMethods() == QGeoPositionInfoSource::NoPositioningMethods) {
        setError(QGeoPositionInfoSource::UnknownSourceError);
        return false;
    }

    if (!requestAccess() || !checkNativeState())
        return false;

    HRESULT hr = QEventDispatcherWinRT::runOnXamlThread([this, d]() {
        HRESULT hr;

        // We need to call this at least once on Windows 10 Mobile.
        // Unfortunately this operation does not have a completion handler
        // registered. That could have helped in the single update case
        ComPtr<IAsyncOperation<Geoposition*>> op;
        hr = d->locator->GetGeopositionAsync(&op);

        hr = d->locator->add_PositionChanged(Callback<GeoLocatorPositionHandler>(this,
                                                                                 &QGeoPositionInfoSourceWinRT::onPositionChanged).Get(),
                                             &d->positionToken);
        return hr;
    });
    if (FAILED(hr)) {
        setError(QGeoPositionInfoSource::UnknownSourceError);
        qErrnoWarning(hr, "Could not add position handler");
        return false;
    }

    return true;
}

void QGeoPositionInfoSourceWinRT::stopHandler()
{
    Q_D(QGeoPositionInfoSourceWinRT);

    if (!d->positionToken.value)
        return;
    QEventDispatcherWinRT::runOnXamlThread([d]() {
        d->locator->remove_PositionChanged(d->positionToken);
        return S_OK;
    });
    d->positionToken.value = 0;
}

void QGeoPositionInfoSourceWinRT::requestUpdate(int timeout)
{
    Q_D(QGeoPositionInfoSourceWinRT);

    if (timeout != 0 && timeout < minimumUpdateInterval()) {
        emit updateTimeout();
        return;
    }

    if (timeout == 0)
        timeout = 2*60*1000; // Maximum time for cold start (see Android)

    startHandler();
    d->singleUpdateTimer.start(timeout);
}

void QGeoPositionInfoSourceWinRT::virtualPositionUpdate()
{
    Q_D(QGeoPositionInfoSourceWinRT);
    QMutexLocker locker(&d->mutex);

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
    if (d->lastPosition.isValid()) {
        QGeoPositionInfo sent = d->lastPosition;
        sent.setTimestamp(sent.timestamp().addMSecs(updateInterval()));
        d->lastPosition = sent;
        emit positionUpdated(sent);
    }
    d->periodicTimer.start();
}

void QGeoPositionInfoSourceWinRT::singleUpdateTimeOut()
{
    Q_D(QGeoPositionInfoSourceWinRT);
    QMutexLocker locker(&d->mutex);

    if (d->singleUpdateTimer.isActive()) {
        emit updateTimeout();
        if (!d->updatesOngoing)
            stopHandler();
    }
}

void QGeoPositionInfoSourceWinRT::updateSynchronized(QGeoPositionInfo currentInfo)
{
    Q_D(QGeoPositionInfoSourceWinRT);
    QMutexLocker locker(&d->mutex);

    d->periodicTimer.stop();
    d->lastPosition = currentInfo;

    if (d->updatesOngoing)
        d->periodicTimer.start();

    if (d->singleUpdateTimer.isActive()) {
        d->singleUpdateTimer.stop();
        if (!d->updatesOngoing)
        stopHandler();
    }

    emit positionUpdated(currentInfo);
}

QGeoPositionInfoSource::Error QGeoPositionInfoSourceWinRT::error() const
{
    Q_D(const QGeoPositionInfoSourceWinRT);
    return d->positionError;
}

void QGeoPositionInfoSourceWinRT::setError(QGeoPositionInfoSource::Error positionError)
{
    Q_D(QGeoPositionInfoSourceWinRT);

    if (positionError == d->positionError)
        return;
    d->positionError = positionError;
    emit QGeoPositionInfoSource::error(positionError);
}

bool QGeoPositionInfoSourceWinRT::checkNativeState()
{
    Q_D(QGeoPositionInfoSourceWinRT);

    PositionStatus status;
    HRESULT hr = d->locator->get_LocationStatus(&status);
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

HRESULT QGeoPositionInfoSourceWinRT::onPositionChanged(IGeolocator *locator, IPositionChangedEventArgs *args)
{
    Q_UNUSED(locator);

    HRESULT hr;
    ComPtr<IGeoposition> pos;
    hr = args->get_Position(&pos);
    RETURN_HR_IF_FAILED("Could not access position object.");

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

    IReference<double> *heading;
    hr = coord->get_Heading(&heading);
    if (SUCCEEDED(hr) && heading) {
        double value;
        hr = heading->get_Value(&value);
        double mod = 0;
        value = modf(value, &mod);
        value += static_cast<int>(mod) % 360;
        if (value >=0 && value <= 359) // get_Value might return nan/-nan
            currentInfo.setAttribute(QGeoPositionInfo::Direction, value);
    }

    DateTime dateTime;
    hr = coord->get_Timestamp(&dateTime);

    if (dateTime.UniversalTime > 0) {
        ULARGE_INTEGER uLarge;
        uLarge.QuadPart = dateTime.UniversalTime;
        FILETIME fileTime;
        fileTime.dwHighDateTime = uLarge.HighPart;
        fileTime.dwLowDateTime = uLarge.LowPart;
        SYSTEMTIME systemTime;
        if (FileTimeToSystemTime(&fileTime, &systemTime)) {
            currentInfo.setTimestamp(QDateTime(QDate(systemTime.wYear, systemTime.wMonth,
                                                     systemTime.wDay),
                                               QTime(systemTime.wHour, systemTime.wMinute,
                                                     systemTime.wSecond, systemTime.wMilliseconds),
                                               Qt::UTC));
        }
    }

    emit nativePositionUpdate(currentInfo);

    return S_OK;
}

HRESULT QGeoPositionInfoSourceWinRT::onStatusChanged(IGeolocator*, IStatusChangedEventArgs *args)
{
    PositionStatus st;
    args->get_Status(&st);
    return S_OK;
}

bool QGeoPositionInfoSourceWinRT::requestAccess() const
{
#if _MSC_VER >= 1900 && defined(Q_OS_WINRT)
    static GeolocationAccessStatus accessStatus = GeolocationAccessStatus_Unspecified;
    static ComPtr<IGeolocatorStatics> statics;

    if (accessStatus == GeolocationAccessStatus_Allowed)
        return true;
    else if (accessStatus == GeolocationAccessStatus_Denied)
        return false;

    ComPtr<IAsyncOperation<GeolocationAccessStatus>> op;
    HRESULT hr;
    hr = QEventDispatcherWinRT::runOnXamlThread([&op]() {
        HRESULT hr;
        hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Devices_Geolocation_Geolocator).Get(),
                                    IID_PPV_ARGS(&statics));
        RETURN_HR_IF_FAILED("Could not access Geolocation Statics.");

        hr = statics->RequestAccessAsync(&op);
        return hr;
    });
    Q_ASSERT_SUCCEEDED(hr);

    // We cannot wait inside the XamlThread as that would deadlock
    QWinRTFunctions::await(op, &accessStatus);
    return accessStatus == GeolocationAccessStatus_Allowed;
#else // _MSC_VER < 1900
    return true;
#endif // _MSC_VER < 1900
}

QT_END_NAMESPACE
