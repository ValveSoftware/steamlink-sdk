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

#include "avfcameraviewfindersettingscontrol.h"
#include "avfimageencodercontrol.h"
#include "avfimagecapturecontrol.h"
#include "avfcamerautility.h"
#include "avfcamerasession.h"
#include "avfcameraservice.h"
#include "avfcameradebug.h"
#include "avfcameracontrol.h"

#include <QtMultimedia/qmediaencodersettings.h>

#include <QtCore/qsysinfo.h>
#include <QtCore/qdebug.h>

#include <AVFoundation/AVFoundation.h>

QT_BEGIN_NAMESPACE

AVFImageEncoderControl::AVFImageEncoderControl(AVFCameraService *service)
    : m_service(service)
{
    Q_ASSERT(service);
}

QStringList AVFImageEncoderControl::supportedImageCodecs() const
{
    return QStringList() << QLatin1String("jpeg");
}

QString AVFImageEncoderControl::imageCodecDescription(const QString &codecName) const
{
    if (codecName == QLatin1String("jpeg"))
        return tr("JPEG image");

    return QString();
}

QList<QSize> AVFImageEncoderControl::supportedResolutions(const QImageEncoderSettings &settings,
                                                          bool *continuous) const
{
    Q_UNUSED(settings)

    QList<QSize> resolutions;

    if (!videoCaptureDeviceIsValid())
        return resolutions;

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_7, __IPHONE_7_0)
    if (QSysInfo::MacintoshVersion >= qt_OS_limit(QSysInfo::MV_10_7, QSysInfo::MV_IOS_7_0)) {
        AVCaptureDevice *captureDevice = m_service->session()->videoCaptureDevice();
        const QVector<AVCaptureDeviceFormat *> formats(qt_unique_device_formats(captureDevice,
                                                       m_service->session()->defaultCodec()));

        for (int i = 0; i < formats.size(); ++i) {
            AVCaptureDeviceFormat *format = formats[i];

            const QSize res(qt_device_format_resolution(format));
            if (!res.isNull() && res.isValid())
                resolutions << res;
#if defined(Q_OS_IOS) && QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)
            if (QSysInfo::MacintoshVersion >= QSysInfo::MV_IOS_8_0) {
                // From Apple's docs (iOS):
                // By default, AVCaptureStillImageOutput emits images with the same dimensions as
                // its source AVCaptureDevice instance’s activeFormat.formatDescription. However,
                // if you set this property to YES, the receiver emits still images at the capture
                // device’s highResolutionStillImageDimensions value.
                const QSize hrRes(qt_device_format_high_resolution(format));
                if (!hrRes.isNull() && hrRes.isValid())
                    resolutions << res;
            }
#endif
        }
    } else {
#else
    {
#endif
        // TODO: resolutions without AVCaptureDeviceFormat ...
    }

    if (continuous)
        *continuous = false;

    return resolutions;
}

QImageEncoderSettings AVFImageEncoderControl::requestedSettings() const
{
    return m_settings;
}

QImageEncoderSettings AVFImageEncoderControl::imageSettings() const
{
    QImageEncoderSettings settings;

    if (!videoCaptureDeviceIsValid())
        return settings;

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_7, __IPHONE_7_0)
    if (QSysInfo::MacintoshVersion >= qt_OS_limit(QSysInfo::MV_10_7, QSysInfo::MV_IOS_7_0)) {
        AVCaptureDevice *captureDevice = m_service->session()->videoCaptureDevice();
        if (!captureDevice.activeFormat) {
            qDebugCamera() << Q_FUNC_INFO << "no active format";
            return settings;
        }

        QSize res(qt_device_format_resolution(captureDevice.activeFormat));
#if defined(Q_OS_IOS) && QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)
        if (QSysInfo::MacintoshVersion >= QSysInfo::MV_IOS_8_0) {
            if (!m_service->imageCaptureControl() || !m_service->imageCaptureControl()->stillImageOutput()) {
                qDebugCamera() << Q_FUNC_INFO << "no still image output";
                return settings;
            }

            AVCaptureStillImageOutput *stillImageOutput = m_service->imageCaptureControl()->stillImageOutput();
            if (stillImageOutput.highResolutionStillImageOutputEnabled)
                res = qt_device_format_high_resolution(captureDevice.activeFormat);
        }
#endif
        if (res.isNull() || !res.isValid()) {
            qDebugCamera() << Q_FUNC_INFO << "failed to exctract the image resolution";
            return settings;
        }

        settings.setResolution(res);
    } else {
#else
    {
#endif
        // TODO: resolution without AVCaptureDeviceFormat.
    }

    settings.setCodec(QLatin1String("jpeg"));

    return settings;
}

void AVFImageEncoderControl::setImageSettings(const QImageEncoderSettings &settings)
{
    if (m_settings == settings)
        return;

    m_settings = settings;
    applySettings();
}

bool AVFImageEncoderControl::applySettings()
{
    if (!videoCaptureDeviceIsValid())
        return false;

    AVFCameraSession *session = m_service->session();
    if (!session || (session->state() != QCamera::ActiveState
        && session->state() != QCamera::LoadedState)
            || !m_service->cameraControl()->captureMode().testFlag(QCamera::CaptureStillImage)) {
        return false;
    }

    if (!m_service->imageCaptureControl()
        || !m_service->imageCaptureControl()->stillImageOutput()) {
        qDebugCamera() << Q_FUNC_INFO << "no still image output";
        return false;
    }

    if (m_settings.codec().size()
        && m_settings.codec() != QLatin1String("jpeg")) {
        qDebugCamera() << Q_FUNC_INFO << "unsupported codec:" << m_settings.codec();
        return false;
    }

    QSize res(m_settings.resolution());
    if (res.isNull()) {
        qDebugCamera() << Q_FUNC_INFO << "invalid resolution:" << res;
        return false;
    }

    if (!res.isValid()) {
        // Invalid == default value.
        // Here we could choose the best format available, but
        // activeFormat is already equal to 'preset high' by default,
        // which is good enough, otherwise we can end in some format with low framerates.
        return false;
    }

    bool activeFormatChanged = false;

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_7, __IPHONE_7_0)
    if (QSysInfo::MacintoshVersion >= qt_OS_limit(QSysInfo::MV_10_7, QSysInfo::MV_IOS_7_0)) {
        AVCaptureDevice *captureDevice = m_service->session()->videoCaptureDevice();
        AVCaptureDeviceFormat *match = qt_find_best_resolution_match(captureDevice, res,
                                                                     m_service->session()->defaultCodec());

        if (!match) {
            qDebugCamera() << Q_FUNC_INFO << "unsupported resolution:" << res;
            return false;
        }

        activeFormatChanged = qt_set_active_format(captureDevice, match, true);

#if defined(Q_OS_IOS) && QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)
        if (QSysInfo::MacintoshVersion >= QSysInfo::MV_IOS_8_0) {
            AVCaptureStillImageOutput *imageOutput = m_service->imageCaptureControl()->stillImageOutput();
            if (res == qt_device_format_high_resolution(captureDevice.activeFormat))
                imageOutput.highResolutionStillImageOutputEnabled = YES;
            else
                imageOutput.highResolutionStillImageOutputEnabled = NO;
        }
#endif
    } else {
#else
    {
#endif
        // TODO: resolution without capture device format ...
    }

    return activeFormatChanged;
}

bool AVFImageEncoderControl::videoCaptureDeviceIsValid() const
{
    if (!m_service->session() || !m_service->session()->videoCaptureDevice())
        return false;

    AVCaptureDevice *captureDevice = m_service->session()->videoCaptureDevice();
    if (!captureDevice.formats || !captureDevice.formats.count)
        return false;

    return true;
}

QT_END_NAMESPACE

#include "moc_avfimageencodercontrol.cpp"
