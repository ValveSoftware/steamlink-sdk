/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include "winrtambientlightsensor.h"
#include "winrtcommon.h"

#include <QtSensors/QAmbientLightSensor>
#include <private/qeventdispatcher_winrt_p.h>

#include <functional>
#include <wrl.h>
#include <windows.devices.sensors.h>
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Devices::Sensors;

typedef ITypedEventHandler<LightSensor *, LightSensorReadingChangedEventArgs *> LightSensorReadingHandler;

QT_USE_NAMESPACE

class WinRtAmbientLightSensorPrivate
{
public:
    WinRtAmbientLightSensorPrivate(WinRtAmbientLightSensor *p)
        : minimumReportInterval(0), q(p)
    {
        token.value = 0;
    }

    QAmbientLightReading reading;

    ComPtr<ILightSensor> sensor;
    EventRegistrationToken token;
    quint32 minimumReportInterval;

    HRESULT readingChanged(ILightSensor *, ILightSensorReadingChangedEventArgs *args)
    {
        ComPtr<ILightSensorReading> value;
        HRESULT hr = args->get_Reading(&value);
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Failed to get light sensor reading" << qt_error_string(hr);
            return hr;
        }

        FLOAT lux;
        hr = value->get_IlluminanceInLux(&lux);
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Failed to get illuminance value" << qt_error_string(hr);
            return hr;
        }

        DateTime dateTime;
        hr = value->get_Timestamp(&dateTime);
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Failed to get light sensor reading timestamp" << qt_error_string(hr);
            return hr;
        }

        // Using same values as BB light sensor
        if (lux < 10)
            reading.setLightLevel(QAmbientLightReading::Dark);
        else if (lux < 80)
            reading.setLightLevel(QAmbientLightReading::Twilight);
        else if (lux < 400)
            reading.setLightLevel(QAmbientLightReading::Light);
        else if (lux < 2500)
            reading.setLightLevel(QAmbientLightReading::Bright);
        else
            reading.setLightLevel(QAmbientLightReading::Sunny);

        reading.setTimestamp(dateTimeToMsSinceEpoch(dateTime));
        q->newReadingAvailable();
        return S_OK;
    }

private:
    WinRtAmbientLightSensor *q;
};

WinRtAmbientLightSensor::WinRtAmbientLightSensor(QSensor *sensor)
    : QSensorBackend(sensor), d_ptr(new WinRtAmbientLightSensorPrivate(this))
{
    Q_D(WinRtAmbientLightSensor);
    HRESULT hr = QEventDispatcherWinRT::runOnXamlThread([d]() {
        HStringReference classId(RuntimeClass_Windows_Devices_Sensors_LightSensor);
        ComPtr<ILightSensorStatics> factory;
        HRESULT hr = RoGetActivationFactory(classId.Get(), IID_PPV_ARGS(&factory));
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Unable to initialize light sensor factory."
                                      << qt_error_string(hr);
            return hr;
        }

        hr = factory->GetDefault(&d->sensor);
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Unable to get default light sensor."
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

    setReading<QAmbientLightReading>(&d->reading);
}

WinRtAmbientLightSensor::~WinRtAmbientLightSensor()
{
}

void WinRtAmbientLightSensor::start()
{
    Q_D(WinRtAmbientLightSensor);
    if (!d->sensor)
        return;
    if (d->token.value)
        return;

    HRESULT hr = QEventDispatcherWinRT::runOnXamlThread([d]() {
        ComPtr<LightSensorReadingHandler> callback =
            Callback<LightSensorReadingHandler>(d, &WinRtAmbientLightSensorPrivate::readingChanged);
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

void WinRtAmbientLightSensor::stop()
{
    Q_D(WinRtAmbientLightSensor);
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
