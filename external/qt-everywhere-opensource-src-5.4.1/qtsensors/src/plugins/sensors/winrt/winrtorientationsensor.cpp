/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include "winrtorientationsensor.h"
#include "winrtcommon.h"

#include <QtSensors/QOrientationSensor>

QT_USE_NAMESPACE

#include <wrl.h>
#include <windows.devices.sensors.h>
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Devices::Sensors;

typedef ITypedEventHandler<SimpleOrientationSensor *, SimpleOrientationSensorOrientationChangedEventArgs *> SimpleOrientationReadingHandler;

QT_USE_NAMESPACE

class WinRtOrientationSensorPrivate
{
public:
    WinRtOrientationSensorPrivate(WinRtOrientationSensor *p) : q(p)
    {
        token.value = 0;
    }

    QOrientationReading reading;

    ComPtr<ISimpleOrientationSensor> sensor;
    EventRegistrationToken token;

    HRESULT readingChanged(ISimpleOrientationSensor *,
                           ISimpleOrientationSensorOrientationChangedEventArgs *args)
    {
        SimpleOrientation value;
        HRESULT hr = args->get_Orientation(&value);
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Failed to get orientation value." << qt_error_string(hr);
            return hr;
        }

        DateTime dateTime;
        hr = args->get_Timestamp(&dateTime);
        if (FAILED(hr)) {
            qCWarning(lcWinRtSensors) << "Failed to get compass reading timestamp." << qt_error_string(hr);
            return hr;
        }

        switch (value) {
        default:
            reading.setOrientation(QOrientationReading::Undefined);
            break;
        case SimpleOrientation_NotRotated:
            reading.setOrientation(QOrientationReading::TopUp);
            break;
        case SimpleOrientation_Rotated90DegreesCounterclockwise:
            reading.setOrientation(QOrientationReading::RightUp);
            break;
        case SimpleOrientation_Rotated180DegreesCounterclockwise:
            reading.setOrientation(QOrientationReading::TopDown);
            break;
        case SimpleOrientation_Rotated270DegreesCounterclockwise:
            reading.setOrientation(QOrientationReading::LeftUp);
            break;
        case SimpleOrientation_Faceup:
            reading.setOrientation(QOrientationReading::FaceUp);
            break;
        case SimpleOrientation_Facedown:
            reading.setOrientation(QOrientationReading::FaceDown);
            break;
        }

        reading.setTimestamp(dateTimeToMsSinceEpoch(dateTime));
        q->newReadingAvailable();
        return S_OK;
    }

private:
    WinRtOrientationSensor *q;
};

WinRtOrientationSensor::WinRtOrientationSensor(QSensor *sensor)
    : QSensorBackend(sensor), d_ptr(new WinRtOrientationSensorPrivate(this))
{
    Q_D(WinRtOrientationSensor);
    HStringReference classId(RuntimeClass_Windows_Devices_Sensors_SimpleOrientationSensor);
    ComPtr<ISimpleOrientationSensorStatics> factory;
    HRESULT hr = RoGetActivationFactory(classId.Get(), IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        qCWarning(lcWinRtSensors) << "Unable to initialize orientation sensor factory."
                                  << qt_error_string(hr);
        sensorError(hr);
        return;
    }

    hr = factory->GetDefault(&d->sensor);
    if (FAILED(hr) || !d->sensor) {
        qCWarning(lcWinRtSensors) << "Unable to get default orientation sensor."
                                  << qt_error_string(hr);
        sensorError(hr);
        return;
    }

    setReading<QOrientationReading>(&d->reading);
}

WinRtOrientationSensor::~WinRtOrientationSensor()
{
}

void WinRtOrientationSensor::start()
{
    Q_D(WinRtOrientationSensor);
    if (!d->sensor)
        return;
    if (d->token.value)
        return;

    ComPtr<SimpleOrientationReadingHandler> callback =
            Callback<SimpleOrientationReadingHandler>(d, &WinRtOrientationSensorPrivate::readingChanged);
    HRESULT hr = d->sensor->add_OrientationChanged(callback.Get(), &d->token);
    if (FAILED(hr)) {
        qCWarning(lcWinRtSensors) << "Unable to attach to reading changed event."
                                  << qt_error_string(hr);
        sensorError(hr);
        return;
    }
}

void WinRtOrientationSensor::stop()
{
    Q_D(WinRtOrientationSensor);
    if (!d->sensor)
        return;
    if (!d->token.value)
        return;

    HRESULT hr = d->sensor->remove_OrientationChanged(d->token);
    if (FAILED(hr)) {
        qCWarning(lcWinRtSensors) << "Unable to detach from reading changed event."
                                  << qt_error_string(hr);
        sensorError(hr);
        return;
    }

    d->token.value = 0;
}
