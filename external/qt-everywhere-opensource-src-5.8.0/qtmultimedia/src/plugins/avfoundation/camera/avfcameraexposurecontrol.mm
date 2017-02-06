/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "avfcameraexposurecontrol.h"
#include "avfcamerautility.h"
#include "avfcamerasession.h"
#include "avfcameraservice.h"
#include "avfcameradebug.h"

#include <QtCore/qvariant.h>
#include <QtCore/qpointer.h>
#include <QtCore/qdebug.h>
#include <QtCore/qpair.h>

#include <AVFoundation/AVFoundation.h>

#include <limits>

QT_BEGIN_NAMESPACE

namespace {

// All these methods to work with exposure/ISO/SS in custom mode
// are quite new (iOS 8 or later and no OS X support).

#if QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)

// Misc. helpers to check values/ranges:

bool qt_check_ISO_conversion(float isoValue)
{
    if (isoValue >= std::numeric_limits<int>::max())
        return false;
    if (isoValue <= std::numeric_limits<int>::min())
        return false;
    return true;
}

bool qt_check_ISO_range(AVCaptureDeviceFormat *format)
{
    // Qt is using int for ISO, AVFoundation - float. It looks like the ISO range
    // at the moment can be represented by int (it's max - min > 100, etc.).
    Q_ASSERT(format);
    if (format.maxISO - format.minISO < 1.) {
        // ISO is in some strange units?
        return false;
    }

    return qt_check_ISO_conversion(format.minISO)
           && qt_check_ISO_conversion(format.maxISO);
}

bool qt_check_exposure_duration(AVCaptureDevice *captureDevice, CMTime duration)
{
    Q_ASSERT(captureDevice);

    AVCaptureDeviceFormat *activeFormat = captureDevice.activeFormat;
    if (!activeFormat) {
        qDebugCamera() << Q_FUNC_INFO << "failed to obtain capture device format";
        return false;
    }

    return CMTimeCompare(duration, activeFormat.minExposureDuration) != -1
           && CMTimeCompare(activeFormat.maxExposureDuration, duration) != -1;
}

bool qt_check_ISO_value(AVCaptureDevice *captureDevice, int newISO)
{
    Q_ASSERT(captureDevice);

    AVCaptureDeviceFormat *activeFormat = captureDevice.activeFormat;
    if (!activeFormat) {
        qDebugCamera() << Q_FUNC_INFO << "failed to obtain capture device format";
        return false;
    }

    return !(newISO < activeFormat.minISO || newISO > activeFormat.maxISO);
}

bool qt_exposure_duration_equal(AVCaptureDevice *captureDevice, qreal qDuration)
{
    Q_ASSERT(captureDevice);
    const CMTime avDuration = CMTimeMakeWithSeconds(qDuration, captureDevice.exposureDuration.timescale);
    return !CMTimeCompare(avDuration, captureDevice.exposureDuration);
}

bool qt_iso_equal(AVCaptureDevice *captureDevice, int iso)
{
    Q_ASSERT(captureDevice);
    return qFuzzyCompare(float(iso), captureDevice.ISO);
}

bool qt_exposure_bias_equal(AVCaptureDevice *captureDevice, qreal bias)
{
    Q_ASSERT(captureDevice);
    return qFuzzyCompare(bias, qreal(captureDevice.exposureTargetBias));
}

// Converters:

bool qt_convert_exposure_mode(AVCaptureDevice *captureDevice, QCameraExposure::ExposureMode mode,
                              AVCaptureExposureMode &avMode)
{
    // Test if mode supported and convert.
    Q_ASSERT(captureDevice);

    if (mode == QCameraExposure::ExposureAuto) {
        if ([captureDevice isExposureModeSupported:AVCaptureExposureModeContinuousAutoExposure]) {
            avMode = AVCaptureExposureModeContinuousAutoExposure;
            return true;
        }
    }

    if (mode == QCameraExposure::ExposureManual) {
        if ([captureDevice isExposureModeSupported:AVCaptureExposureModeCustom]) {
            avMode = AVCaptureExposureModeCustom;
            return true;
        }
    }

    return false;
}

// We set ISO/exposure duration with completion handlers, completion handlers try
// to avoid dangling pointers (thus QPointer for QObjects) and not to create
// a reference loop (in case we have ARC).

void qt_set_exposure_bias(QPointer<AVFCameraService> service, QPointer<AVFCameraExposureControl> control,
                          AVCaptureDevice *captureDevice, float bias)
{
    Q_ASSERT(captureDevice);

    __block AVCaptureDevice *device = captureDevice; //For ARC.

    void (^completionHandler)(CMTime syncTime) = ^(CMTime) {
        // Test that service control is still alive and that
        // capture device is our device, if yes - emit actual value changed.
        if (service) {
            if (control) {
                if (service->session() && service->session()->videoCaptureDevice() == device)
                    Q_EMIT control->actualValueChanged(int(QCameraExposureControl::ExposureCompensation));
            }
        }
        device = nil;
    };

    [captureDevice setExposureTargetBias:bias completionHandler:completionHandler];
}

void qt_set_duration_iso(QPointer<AVFCameraService> service, QPointer<AVFCameraExposureControl> control,
                         AVCaptureDevice *captureDevice, CMTime duration, float iso)
{
    Q_ASSERT(captureDevice);

    __block AVCaptureDevice *device = captureDevice; //For ARC.
    const bool setDuration = CMTimeCompare(duration, AVCaptureExposureDurationCurrent);
    const bool setISO = !qFuzzyCompare(iso, AVCaptureISOCurrent);

    void (^completionHandler)(CMTime syncTime) = ^(CMTime) {
        // Test that service control is still alive and that
        // capture device is our device, if yes - emit actual value changed.
        if (service) {
            if (control) {
                if (service->session() && service->session()->videoCaptureDevice() == device) {
                    if (setDuration)
                        Q_EMIT control->actualValueChanged(int(QCameraExposureControl::ShutterSpeed));
                    if (setISO)
                        Q_EMIT control->actualValueChanged(int(QCameraExposureControl::ISO));
                }
            }
        }
        device = nil;
    };

    [captureDevice setExposureModeCustomWithDuration:duration
                                                 ISO:iso
                                   completionHandler:completionHandler];
}

#endif // QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)

} // Unnamed namespace.

AVFCameraExposureControl::AVFCameraExposureControl(AVFCameraService *service)
    : m_service(service),
      m_session(Q_NULLPTR)
{
    Q_ASSERT(service);
    m_session = m_service->session();
    Q_ASSERT(m_session);

    connect(m_session, SIGNAL(stateChanged(QCamera::State)), SLOT(cameraStateChanged()));
}

bool AVFCameraExposureControl::isParameterSupported(ExposureParameter parameter) const
{
#if QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)
    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice)
        return false;

    // These are the parameters we have an API to support:
    return parameter == QCameraExposureControl::ISO
           || parameter == QCameraExposureControl::ShutterSpeed
           || parameter == QCameraExposureControl::ExposureCompensation
           || parameter == QCameraExposureControl::ExposureMode;
#else
    Q_UNUSED(parameter)
    return false;
#endif
}

QVariantList AVFCameraExposureControl::supportedParameterRange(ExposureParameter parameter,
                                                               bool *continuous) const
{
    QVariantList parameterRange;
#if QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)

    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice || !isParameterSupported(parameter)) {
        qDebugCamera() << Q_FUNC_INFO << "parameter not supported";
        return parameterRange;
    }

    if (continuous)
        *continuous = false;

    AVCaptureDeviceFormat *activeFormat = captureDevice.activeFormat;

    if (parameter == QCameraExposureControl::ISO) {
        if (!activeFormat) {
            qDebugCamera() << Q_FUNC_INFO << "failed to obtain capture device format";
            return parameterRange;
        }

        if (!qt_check_ISO_range(activeFormat)) {
            qDebugCamera() << Q_FUNC_INFO << "ISO range can not be represented as int";
            return parameterRange;
        }

        parameterRange << QVariant(int(activeFormat.minISO));
        parameterRange << QVariant(int(activeFormat.maxISO));
        if (continuous)
            *continuous = true;
    } else if (parameter == QCameraExposureControl::ExposureCompensation) {
        parameterRange << captureDevice.minExposureTargetBias;
        parameterRange << captureDevice.maxExposureTargetBias;
        if (continuous)
            *continuous = true;
    } else if (parameter == QCameraExposureControl::ShutterSpeed) {
        if (!activeFormat) {
            qDebugCamera() << Q_FUNC_INFO << "failed to obtain capture device format";
            return parameterRange;
        }

        // CMTimeGetSeconds returns Float64, test the conversion below, if it's valid?
        parameterRange << qreal(CMTimeGetSeconds(activeFormat.minExposureDuration));
        parameterRange << qreal(CMTimeGetSeconds(activeFormat.maxExposureDuration));

        if (continuous)
            *continuous = true;
    } else if (parameter == QCameraExposureControl::ExposureMode) {
        if ([captureDevice isExposureModeSupported:AVCaptureExposureModeCustom])
            parameterRange << QVariant::fromValue(QCameraExposure::ExposureManual);

        if ([captureDevice isExposureModeSupported:AVCaptureExposureModeContinuousAutoExposure])
            parameterRange << QVariant::fromValue(QCameraExposure::ExposureAuto);
    }
#else
    Q_UNUSED(parameter)
    Q_UNUSED(continuous)
#endif
    return parameterRange;
}

QVariant AVFCameraExposureControl::requestedValue(ExposureParameter parameter) const
{
    if (!isParameterSupported(parameter)) {
        qDebugCamera() << Q_FUNC_INFO << "parameter not supported";
        return QVariant();
    }

    if (parameter == QCameraExposureControl::ExposureMode)
        return m_requestedMode;

    if (parameter == QCameraExposureControl::ExposureCompensation)
        return m_requestedCompensation;

    if (parameter == QCameraExposureControl::ShutterSpeed)
        return m_requestedShutterSpeed;

    if (parameter == QCameraExposureControl::ISO)
        return m_requestedISO;

    return QVariant();
}

QVariant AVFCameraExposureControl::actualValue(ExposureParameter parameter) const
{
#if QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)
    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice || !isParameterSupported(parameter)) {
        // Actually, at the moment !captiredevice => !isParameterSupported.
        qDebugCamera() << Q_FUNC_INFO << "parameter not supported";
        return QVariant();
    }

    if (parameter == QCameraExposureControl::ExposureMode) {
        // This code expects exposureMode to be continuous by default ...
        if (captureDevice.exposureMode == AVCaptureExposureModeContinuousAutoExposure)
            return QVariant::fromValue(QCameraExposure::ExposureAuto);
        return QVariant::fromValue(QCameraExposure::ExposureManual);
    }

    if (parameter == QCameraExposureControl::ExposureCompensation)
        return captureDevice.exposureTargetBias;

    if (parameter == QCameraExposureControl::ShutterSpeed)
        return qreal(CMTimeGetSeconds(captureDevice.exposureDuration));

    if (parameter == QCameraExposureControl::ISO) {
        if (captureDevice.activeFormat && qt_check_ISO_range(captureDevice.activeFormat)
            && qt_check_ISO_conversion(captureDevice.ISO)) {
            // Can be represented as int ...
            return int(captureDevice.ISO);
        } else {
            qDebugCamera() << Q_FUNC_INFO << "ISO can not be represented as int";
            return QVariant();
        }
    }
#else
    Q_UNUSED(parameter)
#endif
    return QVariant();
}

bool AVFCameraExposureControl::setValue(ExposureParameter parameter, const QVariant &value)
{
    if (parameter == QCameraExposureControl::ExposureMode)
        return setExposureMode(value);
    else if (parameter == QCameraExposureControl::ExposureCompensation)
        return setExposureCompensation(value);
    else if (parameter == QCameraExposureControl::ShutterSpeed)
        return setShutterSpeed(value);
    else if (parameter == QCameraExposureControl::ISO)
        return setISO(value);

    return false;
}

bool AVFCameraExposureControl::setExposureMode(const QVariant &value)
{
#if QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)
    if (!value.canConvert<QCameraExposure::ExposureMode>()) {
        qDebugCamera() << Q_FUNC_INFO << "invalid exposure mode value,"
                       << "QCameraExposure::ExposureMode expected";
        return false;
    }

    const QCameraExposure::ExposureMode qtMode = value.value<QCameraExposure::ExposureMode>();
    if (qtMode != QCameraExposure::ExposureAuto && qtMode != QCameraExposure::ExposureManual) {
        qDebugCamera() << Q_FUNC_INFO << "exposure mode not supported";
        return false;
    }

    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice) {
        m_requestedMode = value;
        Q_EMIT requestedValueChanged(int(QCameraExposureControl::ExposureMode));
        return true;
    }

    AVCaptureExposureMode avMode = AVCaptureExposureModeAutoExpose;
    if (!qt_convert_exposure_mode(captureDevice, qtMode, avMode)) {
        qDebugCamera() << Q_FUNC_INFO << "exposure mode not supported";
        return false;
    }

    const AVFConfigurationLock lock(captureDevice);
    if (!lock) {
        qDebugCamera() << Q_FUNC_INFO << "failed to lock a capture device"
                       << "for configuration";
        return false;
    }

    m_requestedMode = value;
    [captureDevice setExposureMode:avMode];
    Q_EMIT requestedValueChanged(int(QCameraExposureControl::ExposureMode));
    Q_EMIT actualValueChanged(int(QCameraExposureControl::ExposureMode));

    return true;
#else
    Q_UNUSED(value)
    return false;
#endif
}

bool AVFCameraExposureControl::setExposureCompensation(const QVariant &value)
{
#if QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)
    if (!value.canConvert<qreal>()) {
        qDebugCamera() << Q_FUNC_INFO << "invalid exposure compensation"
                       <<"value, floating point number expected";
        return false;
    }

    const qreal bias = value.toReal();
    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice) {
        m_requestedCompensation = value;
        Q_EMIT requestedValueChanged(int(QCameraExposureControl::ExposureCompensation));
        return true;
    }

    if (bias < captureDevice.minExposureTargetBias || bias > captureDevice.maxExposureTargetBias) {
        // TODO: mixed fp types!
        qDebugCamera() << Q_FUNC_INFO << "exposure compenstation value is"
                       << "out of range";
        return false;
    }

    const AVFConfigurationLock lock(captureDevice);
    if (!lock) {
        qDebugCamera() << Q_FUNC_INFO << "failed to lock for configuration";
        return false;
    }

    qt_set_exposure_bias(m_service, this, captureDevice, bias);
    m_requestedCompensation = value;
    Q_EMIT requestedValueChanged(int(QCameraExposureControl::ExposureCompensation));

    return true;
#else
    Q_UNUSED(value)
    return false;
#endif
}

bool AVFCameraExposureControl::setShutterSpeed(const QVariant &value)
{
#if QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)
    if (value.isNull())
        return setExposureMode(QVariant::fromValue(QCameraExposure::ExposureAuto));

    if (!value.canConvert<qreal>()) {
        qDebugCamera() << Q_FUNC_INFO << "invalid shutter speed"
                       << "value, floating point number expected";
        return false;
    }

    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice) {
        m_requestedShutterSpeed = value;
        Q_EMIT requestedValueChanged(int(QCameraExposureControl::ShutterSpeed));
        return true;
    }

    const CMTime newDuration = CMTimeMakeWithSeconds(value.toReal(),
                                                     captureDevice.exposureDuration.timescale);
    if (!qt_check_exposure_duration(captureDevice, newDuration)) {
        qDebugCamera() << Q_FUNC_INFO << "shutter speed value is out of range";
        return false;
    }

    const AVFConfigurationLock lock(captureDevice);
    if (!lock) {
        qDebugCamera() << Q_FUNC_INFO << "failed to lock for configuration";
        return false;
    }

    // Setting the shutter speed (exposure duration in Apple's terms,
    // since there is no shutter actually) will also reset
    // exposure mode into custom mode.
    qt_set_duration_iso(m_service, this, captureDevice, newDuration, AVCaptureISOCurrent);

    m_requestedShutterSpeed = value;
    Q_EMIT requestedValueChanged(int(QCameraExposureControl::ShutterSpeed));

    return true;
#else
    Q_UNUSED(value)
    return false;
#endif
}

bool AVFCameraExposureControl::setISO(const QVariant &value)
{
#if QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)
    if (value.isNull())
        return setExposureMode(QVariant::fromValue(QCameraExposure::ExposureAuto));

    if (!value.canConvert<int>()) {
        qDebugCamera() << Q_FUNC_INFO << "invalid ISO value, int expected";
        return false;
    }

    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice) {
        m_requestedISO = value;
        Q_EMIT requestedValueChanged(int(QCameraExposureControl::ISO));
        return true;
    }

    if (!qt_check_ISO_value(captureDevice, value.toInt())) {
        qDebugCamera() << Q_FUNC_INFO << "ISO value is out of range";
        return false;
    }

    const AVFConfigurationLock lock(captureDevice);
    if (!lock) {
        qDebugCamera() << Q_FUNC_INFO << "failed to lock a capture device"
                       << "for configuration";
        return false;
    }

    // Setting the ISO will also reset
    // exposure mode to the custom mode.
    qt_set_duration_iso(m_service, this, captureDevice, AVCaptureExposureDurationCurrent, value.toInt());

    m_requestedISO = value;
    Q_EMIT requestedValueChanged(int(QCameraExposureControl::ISO));

    return true;
#else
    Q_UNUSED(value)
    return false;
#endif
}

void AVFCameraExposureControl::cameraStateChanged()
{
#if QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)
    if (m_session->state() != QCamera::ActiveState)
        return;

    AVCaptureDevice *captureDevice = m_session->videoCaptureDevice();
    if (!captureDevice) {
        qDebugCamera() << Q_FUNC_INFO << "capture device is nil, but the session"
                       << "state is 'active'";
        return;
    }

    Q_EMIT parameterRangeChanged(int(QCameraExposureControl::ExposureCompensation));
    Q_EMIT parameterRangeChanged(int(QCameraExposureControl::ExposureMode));
    Q_EMIT parameterRangeChanged(int(QCameraExposureControl::ShutterSpeed));
    Q_EMIT parameterRangeChanged(int(QCameraExposureControl::ISO));

    const AVFConfigurationLock lock(captureDevice);

    CMTime newDuration = AVCaptureExposureDurationCurrent;
    bool setCustomMode = false;

    if (!m_requestedShutterSpeed.isNull()
        && !qt_exposure_duration_equal(captureDevice, m_requestedShutterSpeed.toReal())) {
        newDuration = CMTimeMakeWithSeconds(m_requestedShutterSpeed.toReal(),
                                            captureDevice.exposureDuration.timescale);
        if (!qt_check_exposure_duration(captureDevice, newDuration)) {
            qDebugCamera() << Q_FUNC_INFO << "requested exposure duration is out of range";
            return;
        }
        setCustomMode = true;
    }

    float newISO = AVCaptureISOCurrent;
    if (!m_requestedISO.isNull() && !qt_iso_equal(captureDevice, m_requestedISO.toInt())) {
        newISO = m_requestedISO.toInt();
        if (!qt_check_ISO_value(captureDevice, newISO)) {
            qDebugCamera() << Q_FUNC_INFO << "requested ISO value is out of range";
            return;
        }
        setCustomMode = true;
    }

    if (!m_requestedCompensation.isNull()
        && !qt_exposure_bias_equal(captureDevice, m_requestedCompensation.toReal())) {
        // TODO: mixed fpns.
        const qreal bias = m_requestedCompensation.toReal();
        if (bias < captureDevice.minExposureTargetBias || bias > captureDevice.maxExposureTargetBias) {
            qDebugCamera() << Q_FUNC_INFO << "exposure compenstation value is"
                           << "out of range";
            return;
        }
        if (!lock) {
            qDebugCamera() << Q_FUNC_INFO << "failed to lock for configuration";
            return;
        }
        qt_set_exposure_bias(m_service, this, captureDevice, bias);
    }

    // Setting shutter speed (exposure duration) or ISO values
    // also reset exposure mode into Custom. With this settings
    // we ignore any attempts to set exposure mode.

    if (setCustomMode) {
        if (!lock)
            qDebugCamera() << Q_FUNC_INFO << "failed to lock for configuration";
        else
            qt_set_duration_iso(m_service, this, captureDevice, newDuration, newISO);
        return;
    }

    if (!m_requestedMode.isNull()) {
        QCameraExposure::ExposureMode qtMode = m_requestedMode.value<QCameraExposure::ExposureMode>();
        AVCaptureExposureMode avMode = AVCaptureExposureModeContinuousAutoExposure;
        if (!qt_convert_exposure_mode(captureDevice, qtMode, avMode)) {
            qDebugCamera() << Q_FUNC_INFO << "requested exposure mode is not supported";
            return;
        }

        if (avMode == captureDevice.exposureMode)
            return;

        if (!lock) {
            qDebugCamera() << Q_FUNC_INFO << "failed to lock for configuration";
            return;
        }

        [captureDevice setExposureMode:avMode];
        Q_EMIT actualValueChanged(int(QCameraExposureControl::ExposureMode));
    }
#endif
}

QT_END_NAMESPACE

#include "moc_avfcameraexposurecontrol.cpp"
