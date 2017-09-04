/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include "winrtcompass.h"
#include "winrtcommon.h"

#include <QtSensors/QCompass>
#include <private/qeventdispatcher_winrt_p.h>

#include <functional>
#include <wrl.h>
#include <windows.devices.sensors.h>
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Devices::Sensors;

typedef ITypedEventHandler<Compass *, CompassReadingChangedEventArgs *> CompassReadingHandler;

QT_USE_NAMESPACE

class WinRtCompassPrivate
{
public:
    WinRtCompassPrivate(WinRtCompass *p)
        : minimumReportInterval(0), q(p)
    {
        token.value = 0;
    }

    QCompassReading reading;

    ComPtr<ICompass> sensor;
    EventRegistrationToken token;
    quint32 minimumReportInterval;

    HRESULT readingChanged(ICompass *, ICompassReadingChangedEventArgs *args)
    {
        ComPtr<ICompassReading> value;
        HRESULT hr = args->get_Reading(&value);
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Failed to get light sensor reading." << qt_error_string(hr);
            return hr;
        }

        DOUBLE heading;
        hr = value->get_HeadingMagneticNorth(&heading);
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Failed to get compass heading." << qt_error_string(hr);
            return hr;
        }

        DateTime dateTime;
        hr = value->get_Timestamp(&dateTime);
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Failed to get compass reading timestamp." << qt_error_string(hr);
            return hr;
        }
        ComPtr<ICompassReadingHeadingAccuracy> accuracyReading;
        hr = value.As(&accuracyReading);
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Failed to cast compass reading to obtain accuracy." << qt_error_string(hr);
            return hr;
        }

        MagnetometerAccuracy accuracy;
        hr = accuracyReading->get_HeadingAccuracy(&accuracy);
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Failed to get compass reading accuracy." << qt_error_string(hr);
            return hr;
        }

        switch (accuracy) {
        default:
        case MagnetometerAccuracy_Unknown:
            reading.setCalibrationLevel(0.00);
            break;
        case MagnetometerAccuracy_Unreliable:
            reading.setCalibrationLevel(0.33);
            break;
        case MagnetometerAccuracy_Approximate:
            reading.setCalibrationLevel(0.67);
            break;
        case MagnetometerAccuracy_High:
            reading.setCalibrationLevel(1.00);
            break;
        }

        reading.setAzimuth(heading);
        reading.setTimestamp(dateTimeToMsSinceEpoch(dateTime));
        q->newReadingAvailable();
        return S_OK;
    }

private:
    WinRtCompass *q;
};

WinRtCompass::WinRtCompass(QSensor *sensor)
    : QSensorBackend(sensor), d_ptr(new WinRtCompassPrivate(this))
{
    Q_D(WinRtCompass);
    HRESULT hr = QEventDispatcherWinRT::runOnXamlThread([d]() {
        HStringReference classId(RuntimeClass_Windows_Devices_Sensors_Compass);
        ComPtr<ICompassStatics> factory;
        HRESULT hr = RoGetActivationFactory(classId.Get(), IID_PPV_ARGS(&factory));
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Unable to initialize light sensor factory."
                                      << qt_error_string(hr);
            return hr;
        }

        hr = factory->GetDefault(&d->sensor);
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Unable to get default compass."
                                      << qt_error_string(hr);
        }
        return hr;
    });
    if (FAILED(hr) || !d->sensor) {
        sensorError(hr);
        return;
    }

    hr = d->sensor->get_MinimumReportInterval(&d->minimumReportInterval);
    if (FAILED(hr)) {
        qCWarning(lcWinRtSensors) << "Unable to get the minimum report interval."
                                  << qt_error_string(hr);
        sensorError(hr);
        return;
    }

    addDataRate(1, 1000 / d->minimumReportInterval); // dataRate in Hz
    sensor->setDataRate(1);

    setReading<QCompassReading>(&d->reading);
}

WinRtCompass::~WinRtCompass()
{
}

void WinRtCompass::start()
{
    Q_D(WinRtCompass);
    if (!d->sensor)
        return;
    if (d->token.value)
        return;

    HRESULT hr = QEventDispatcherWinRT::runOnXamlThread([d]() {
        ComPtr<CompassReadingHandler> callback =
            Callback<CompassReadingHandler>(d, &WinRtCompassPrivate::readingChanged);
        return d->sensor->add_ReadingChanged(callback.Get(), &d->token);
    });
    if (FAILED(hr)) {
        qCWarning(lcWinRtSensors) << "Unable to attach to reading changed event."
                                  << qt_error_string(hr);
        sensorError(hr);
        return;
    }

    int dataRate = sensor()->dataRate();
    if (!dataRate)
        return;

    quint32 reportInterval = qMax(d->minimumReportInterval, quint32(1000/dataRate));
    hr = d->sensor->put_ReportInterval(reportInterval);
    if (FAILED(hr)) {
        qCWarning(lcWinRtSensors) << "Unable to attach to set report interval."
                                  << qt_error_string(hr);
        sensorError(hr);
    }
}

void WinRtCompass::stop()
{
    Q_D(WinRtCompass);
    if (!d->sensor)
        return;
    if (!d->token.value)
        return;
    HRESULT hr = QEventDispatcherWinRT::runOnXamlThread([d]() {
        return d->sensor->remove_ReadingChanged(d->token);
    });
    if (FAILED(hr)) {
        qCWarning(lcWinRtSensors) << "Unable to detach from reading changed event."
                                  << qt_error_string(hr);
        sensorError(hr);
        return;
    }
    hr = d->sensor->put_ReportInterval(0);
    if (FAILED(hr)) {
        qCWarning(lcWinRtSensors) << "Unable to reset report interval."
                                  << qt_error_string(hr);
        sensorError(hr);
        return;
    }
    d->token.value = 0;
}
