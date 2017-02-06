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

#include "avfvideoencodersettingscontrol.h"

#include "avfcameraservice.h"
#include "avfcamerautility.h"
#include "avfcamerasession.h"
#include "avfcamerarenderercontrol.h"

#include <AVFoundation/AVFoundation.h>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC_WITH_ARGS(QStringList, supportedCodecs, (QStringList() << QLatin1String("avc1")
                                                                       << QLatin1String("jpeg")
                                                         #ifdef Q_OS_OSX
                                                                       << QLatin1String("ap4h")
                                                                       << QLatin1String("apcn")
                                                         #endif
                                                         ))

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_7, __IPHONE_7_0)
static bool format_supports_framerate(AVCaptureDeviceFormat *format, qreal fps)
{
    if (format && fps > qreal(0)) {
        const qreal epsilon = 0.1;
        for (AVFrameRateRange *range in format.videoSupportedFrameRateRanges) {
            if (range.maxFrameRate - range.minFrameRate < epsilon) {
                if (qAbs(fps - range.maxFrameRate) < epsilon)
                    return true;
            }

            if (fps >= range.minFrameRate && fps <= range.maxFrameRate)
                return true;
        }
    }

    return false;
}
#endif

static bool real_list_contains(const QList<qreal> &list, qreal value)
{
    Q_FOREACH (qreal r, list) {
        if (qFuzzyCompare(r, value))
            return true;
    }
    return false;
}

AVFVideoEncoderSettingsControl::AVFVideoEncoderSettingsControl(AVFCameraService *service)
    : QVideoEncoderSettingsControl()
    , m_service(service)
    , m_restoreFormat(nil)
{
}

QList<QSize> AVFVideoEncoderSettingsControl::supportedResolutions(const QVideoEncoderSettings &settings,
                                                                  bool *continuous) const
{
    Q_UNUSED(settings)

    if (continuous)
        *continuous = true;

    // AVFoundation seems to support any resolution for recording, with the following limitations:
    // - The recording resolution can't be higher than the camera's active resolution
    // - On OS X, the recording resolution is automatically adjusted to have the same aspect ratio as
    //   the camera's active resolution
    QList<QSize> resolutions;
    resolutions.append(QSize(32, 32));

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_7, __IPHONE_7_0)
    if (QSysInfo::MacintoshVersion >= qt_OS_limit(QSysInfo::MV_10_7, QSysInfo::MV_IOS_7_0)) {
        AVCaptureDevice *device = m_service->session()->videoCaptureDevice();
        if (device) {
            int maximumWidth = 0;
            const QVector<AVCaptureDeviceFormat *> formats(qt_unique_device_formats(device,
                                                                                    m_service->session()->defaultCodec()));
            for (int i = 0; i < formats.size(); ++i) {
                const QSize res(qt_device_format_resolution(formats[i]));
                if (res.width() > maximumWidth)
                    maximumWidth = res.width();
            }

            if (maximumWidth > 0)
                resolutions.append(QSize(maximumWidth, maximumWidth));
        }
    }
#endif

    if (resolutions.count() == 1)
        resolutions.append(QSize(3840, 3840));

    return resolutions;
}

QList<qreal> AVFVideoEncoderSettingsControl::supportedFrameRates(const QVideoEncoderSettings &settings,
                                                                 bool *continuous) const
{
#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_7, __IPHONE_7_0)
    QList<qreal> uniqueFrameRates;

    if (QSysInfo::MacintoshVersion >= qt_OS_limit(QSysInfo::MV_10_7, QSysInfo::MV_IOS_7_0)) {
        AVCaptureDevice *device = m_service->session()->videoCaptureDevice();
        if (!device)
            return uniqueFrameRates;

        if (continuous)
            *continuous = false;

        QVector<AVFPSRange> allRates;

        if (!settings.resolution().isValid()) {
            const QVector<AVCaptureDeviceFormat *> formats(qt_unique_device_formats(device, 0));
            for (int i = 0; i < formats.size(); ++i) {
                AVCaptureDeviceFormat *format = formats.at(i);
                allRates += qt_device_format_framerates(format);
            }
        } else {
            AVCaptureDeviceFormat *format = qt_find_best_resolution_match(device,
                                                                          settings.resolution(),
                                                                          m_service->session()->defaultCodec());
            if (format)
                allRates = qt_device_format_framerates(format);
        }

        for (int j = 0; j < allRates.size(); ++j) {
            if (!real_list_contains(uniqueFrameRates, allRates[j].first))
                uniqueFrameRates.append(allRates[j].first);
            if (!real_list_contains(uniqueFrameRates, allRates[j].second))
                uniqueFrameRates.append(allRates[j].second);
        }
    }

    return uniqueFrameRates;
#else
    return QList<qreal>();
#endif
}

QStringList AVFVideoEncoderSettingsControl::supportedVideoCodecs() const
{
    return *supportedCodecs;
}

QString AVFVideoEncoderSettingsControl::videoCodecDescription(const QString &codecName) const
{
    if (codecName == QLatin1String("avc1"))
        return QStringLiteral("H.264");
    else if (codecName == QLatin1String("jpeg"))
        return QStringLiteral("M-JPEG");
#ifdef Q_OS_OSX
    else if (codecName == QLatin1String("ap4h"))
        return QStringLiteral("Apple ProRes 4444");
    else if (codecName == QLatin1String("apcn"))
        return QStringLiteral("Apple ProRes 422 Standard Definition");
#endif

    return QString();
}

QVideoEncoderSettings AVFVideoEncoderSettingsControl::videoSettings() const
{
    return m_actualSettings;
}

void AVFVideoEncoderSettingsControl::setVideoSettings(const QVideoEncoderSettings &settings)
{
    if (m_requestedSettings == settings)
        return;

    m_requestedSettings = m_actualSettings = settings;
}

NSDictionary *AVFVideoEncoderSettingsControl::applySettings(AVCaptureConnection *connection)
{
    if (m_service->session()->state() != QCamera::LoadedState &&
        m_service->session()->state() != QCamera::ActiveState) {
        return nil;
    }

    AVCaptureDevice *device = m_service->session()->videoCaptureDevice();
    if (!device)
        return nil;

    AVFPSRange currentFps = qt_current_framerates(device, connection);
    const bool needFpsChange = m_requestedSettings.frameRate() > 0
                               && m_requestedSettings.frameRate() != currentFps.second;

    NSMutableDictionary *videoSettings = [NSMutableDictionary dictionary];

    // -- Codec

    // AVVideoCodecKey is the only mandatory key
    QString codec = m_requestedSettings.codec().isEmpty() ? supportedCodecs->first() : m_requestedSettings.codec();
    if (!supportedCodecs->contains(codec)) {
        qWarning("Unsupported codec: '%s'", codec.toLocal8Bit().constData());
        codec = supportedCodecs->first();
    }
    [videoSettings setObject:codec.toNSString() forKey:AVVideoCodecKey];
    m_actualSettings.setCodec(codec);

    // -- Resolution

    int w = m_requestedSettings.resolution().width();
    int h = m_requestedSettings.resolution().height();

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_7, __IPHONE_7_0)
    if (AVCaptureDeviceFormat *currentFormat = device.activeFormat) {
        CMFormatDescriptionRef formatDesc = currentFormat.formatDescription;
        CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(formatDesc);

        // We have to change the device's activeFormat in 3 cases:
        // - the requested recording resolution is higher than the current device resolution
        // - the requested recording resolution has a different aspect ratio than the current device aspect ratio
        // - the requested frame rate is not available for the current device format
        AVCaptureDeviceFormat *newFormat = nil;
        if ((w <= 0 || h <= 0)
                && m_requestedSettings.frameRate() > 0
                && !format_supports_framerate(currentFormat, m_requestedSettings.frameRate())) {

            newFormat = qt_find_best_framerate_match(device,
                                                     m_service->session()->defaultCodec(),
                                                     m_requestedSettings.frameRate());

        } else if (w > 0 && h > 0) {
            AVCaptureDeviceFormat *f = qt_find_best_resolution_match(device,
                                                                     m_requestedSettings.resolution(),
                                                                     m_service->session()->defaultCodec());

            if (f) {
                CMVideoDimensions d = CMVideoFormatDescriptionGetDimensions(f.formatDescription);
                qreal fAspectRatio = qreal(d.width) / d.height;

                if (w > dim.width || h > dim.height
                        || qAbs((qreal(dim.width) / dim.height) - fAspectRatio) > 0.01) {
                    newFormat = f;
                }
            }
        }

        if (qt_set_active_format(device, newFormat, !needFpsChange)) {
            m_restoreFormat = [currentFormat retain];
            formatDesc = newFormat.formatDescription;
            dim = CMVideoFormatDescriptionGetDimensions(formatDesc);
        }

        if (w > 0 && h > 0) {
            // Make sure the recording resolution has the same aspect ratio as the device's
            // current resolution
            qreal deviceAspectRatio = qreal(dim.width) / dim.height;
            qreal recAspectRatio = qreal(w) / h;
            if (qAbs(deviceAspectRatio - recAspectRatio) > 0.01) {
                if (recAspectRatio > deviceAspectRatio)
                    w = qRound(h * deviceAspectRatio);
                else
                    h = qRound(w / deviceAspectRatio);
            }

            // recording resolution can't be higher than the device's active resolution
            w = qMin(w, dim.width);
            h = qMin(h, dim.height);
        }
    }
#endif

    if (w > 0 && h > 0) {
        // Width and height must be divisible by 2
        w += w & 1;
        h += h & 1;

        [videoSettings setObject:[NSNumber numberWithInt:w] forKey:AVVideoWidthKey];
        [videoSettings setObject:[NSNumber numberWithInt:h] forKey:AVVideoHeightKey];
        m_actualSettings.setResolution(w, h);
    } else {
        m_actualSettings.setResolution(qt_device_format_resolution(device.activeFormat));
    }

    // -- FPS

    if (needFpsChange) {
        m_restoreFps = currentFps;
        const qreal fps = m_requestedSettings.frameRate();
        qt_set_framerate_limits(device, connection, fps, fps);
    }
    m_actualSettings.setFrameRate(qt_current_framerates(device, connection).second);

    // -- Codec Settings

    NSMutableDictionary *codecProperties = [NSMutableDictionary dictionary];
    int bitrate = -1;
    float quality = -1.f;

    if (m_requestedSettings.encodingMode() == QMultimedia::ConstantQualityEncoding) {
        if (m_requestedSettings.quality() != QMultimedia::NormalQuality) {
            if (codec != QLatin1String("jpeg")) {
                qWarning("ConstantQualityEncoding is not supported for codec: '%s'", codec.toLocal8Bit().constData());
            } else {
                switch (m_requestedSettings.quality()) {
                case QMultimedia::VeryLowQuality:
                    quality = 0.f;
                    break;
                case QMultimedia::LowQuality:
                    quality = 0.25f;
                    break;
                case QMultimedia::HighQuality:
                    quality = 0.75f;
                    break;
                case QMultimedia::VeryHighQuality:
                    quality = 1.f;
                    break;
                default:
                    quality = -1.f; // NormalQuality, let the system decide
                    break;
                }
            }
        }
    } else if (m_requestedSettings.encodingMode() == QMultimedia::AverageBitRateEncoding){
        if (codec != QLatin1String("avc1"))
            qWarning("AverageBitRateEncoding is not supported for codec: '%s'", codec.toLocal8Bit().constData());
        else
            bitrate = m_requestedSettings.bitRate();
    } else {
        qWarning("Encoding mode is not supported");
    }

    if (bitrate != -1)
        [codecProperties setObject:[NSNumber numberWithInt:bitrate] forKey:AVVideoAverageBitRateKey];
    if (quality != -1.f)
        [codecProperties setObject:[NSNumber numberWithFloat:quality] forKey:AVVideoQualityKey];

    [videoSettings setObject:codecProperties forKey:AVVideoCompressionPropertiesKey];

    return videoSettings;
}

void AVFVideoEncoderSettingsControl::unapplySettings(AVCaptureConnection *connection)
{
    m_actualSettings = m_requestedSettings;

    AVCaptureDevice *device = m_service->session()->videoCaptureDevice();
    if (!device)
        return;

    const bool needFpsChanged = m_restoreFps.first || m_restoreFps.second;

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_7, __IPHONE_7_0)
    if (m_restoreFormat) {
        qt_set_active_format(device, m_restoreFormat, !needFpsChanged);
        [m_restoreFormat release];
        m_restoreFormat = nil;
    }
#endif

    if (needFpsChanged) {
        qt_set_framerate_limits(device, connection, m_restoreFps.first, m_restoreFps.second);
        m_restoreFps = AVFPSRange();
    }
}

QT_END_NAMESPACE

#include "moc_avfvideoencodersettingscontrol.cpp"
