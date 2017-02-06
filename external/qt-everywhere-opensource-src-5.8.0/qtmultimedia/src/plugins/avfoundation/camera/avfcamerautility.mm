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

#include "avfcamerautility.h"
#include "avfcameradebug.h"

#include <QtCore/qvector.h>
#include <QtCore/qpair.h>
#include <private/qmultimediautils_p.h>

#include <functional>
#include <algorithm>
#include <limits>

QT_BEGIN_NAMESPACE

AVFPSRange qt_connection_framerates(AVCaptureConnection *videoConnection)
{
    Q_ASSERT(videoConnection);

    AVFPSRange newRange;
    // "The value in the videoMinFrameDuration is equivalent to the reciprocal
    // of the maximum framerate, the value in the videoMaxFrameDuration is equivalent
    // to the reciprocal of the minimum framerate."
    if (videoConnection.supportsVideoMinFrameDuration) {
        const CMTime cmMin = videoConnection.videoMinFrameDuration;
        if (CMTimeCompare(cmMin, kCMTimeInvalid)) { // Has some non-default value:
            if (const Float64 minSeconds = CMTimeGetSeconds(cmMin))
                newRange.second = 1. / minSeconds;
        }
    }

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_9, __IPHONE_5_0)
#if QT_OSX_DEPLOYMENT_TARGET_BELOW(__MAC_10_9)
    if (QSysInfo::MacintoshVersion >= QSysInfo::MV_10_9)
#endif
    {
        if (videoConnection.supportsVideoMaxFrameDuration) {
            const CMTime cmMax = videoConnection.videoMaxFrameDuration;
            if (CMTimeCompare(cmMax, kCMTimeInvalid)) {
                if (const Float64 maxSeconds = CMTimeGetSeconds(cmMax))
                    newRange.first = 1. / maxSeconds;
            }
        }
    }
#endif

    return newRange;
}

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_7, __IPHONE_7_0)

namespace {

inline bool qt_area_sane(const QSize &size)
{
    return !size.isNull() && size.isValid()
           && std::numeric_limits<int>::max() / size.width() >= size.height();
}

struct ResolutionPredicate : std::binary_function<AVCaptureDeviceFormat *, AVCaptureDeviceFormat *, bool>
{
    bool operator() (AVCaptureDeviceFormat *f1, AVCaptureDeviceFormat *f2)const
    {
        Q_ASSERT(f1 && f2);
        const QSize r1(qt_device_format_resolution(f1));
        const QSize r2(qt_device_format_resolution(f2));
        return r1.width() < r2.width() || (r2.width() == r1.width() && r1.height() < r2.height());
    }
};

struct FormatHasNoFPSRange : std::unary_function<AVCaptureDeviceFormat *, bool>
{
    bool operator() (AVCaptureDeviceFormat *format)
    {
        Q_ASSERT(format);
        return !format.videoSupportedFrameRateRanges || !format.videoSupportedFrameRateRanges.count;
    }
};

Float64 qt_find_min_framerate_distance(AVCaptureDeviceFormat *format, Float64 fps)
{
    Q_ASSERT(format && format.videoSupportedFrameRateRanges
             && format.videoSupportedFrameRateRanges.count);

    AVFrameRateRange *range = [format.videoSupportedFrameRateRanges objectAtIndex:0];
    Float64 distance = qAbs(range.maxFrameRate - fps);
    for (NSUInteger i = 1, e = format.videoSupportedFrameRateRanges.count; i < e; ++i) {
        range = [format.videoSupportedFrameRateRanges objectAtIndex:i];
        distance = qMin(distance, qAbs(range.maxFrameRate - fps));
    }

    return distance;
}

} // Unnamed namespace.

QVector<AVCaptureDeviceFormat *> qt_unique_device_formats(AVCaptureDevice *captureDevice, FourCharCode filter)
{
    // 'filter' is the format we prefer if we have duplicates.
    Q_ASSERT(captureDevice);

    QVector<AVCaptureDeviceFormat *> formats;

    if (!captureDevice.formats || !captureDevice.formats.count)
        return formats;

    formats.reserve(captureDevice.formats.count);
    for (AVCaptureDeviceFormat *format in captureDevice.formats) {
        const QSize resolution(qt_device_format_resolution(format));
        if (resolution.isNull() || !resolution.isValid())
            continue;
        formats << format;
    }

    if (!formats.size())
        return formats;

    std::sort(formats.begin(), formats.end(), ResolutionPredicate());

    QSize size(qt_device_format_resolution(formats[0]));
    FourCharCode codec = CMVideoFormatDescriptionGetCodecType(formats[0].formatDescription);
    int last = 0;
    for (int i = 1; i < formats.size(); ++i) {
        const QSize nextSize(qt_device_format_resolution(formats[i]));
        if (nextSize == size) {
            if (codec == filter)
                continue;
            formats[last] = formats[i];
        } else {
            ++last;
            formats[last] = formats[i];
            size = nextSize;
        }
        codec = CMVideoFormatDescriptionGetCodecType(formats[i].formatDescription);
    }
    formats.resize(last + 1);

    return formats;
}

QSize qt_device_format_resolution(AVCaptureDeviceFormat *format)
{
    if (!format || !format.formatDescription)
        return QSize();

    const CMVideoDimensions res = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
    return QSize(res.width, res.height);
}

QSize qt_device_format_high_resolution(AVCaptureDeviceFormat *format)
{
    Q_ASSERT(format);
    QSize res;
#if defined(Q_OS_IOS) && QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_8_0)
    if (QSysInfo::MacintoshVersion >= QSysInfo::MV_IOS_8_0) {
        const CMVideoDimensions hrDim(format.highResolutionStillImageDimensions);
        res.setWidth(hrDim.width);
        res.setHeight(hrDim.height);
    }
#endif
    return res;
}

QVector<AVFPSRange> qt_device_format_framerates(AVCaptureDeviceFormat *format)
{
    Q_ASSERT(format);

    QVector<AVFPSRange> qtRanges;

    if (!format.videoSupportedFrameRateRanges || !format.videoSupportedFrameRateRanges.count)
        return qtRanges;

    qtRanges.reserve(format.videoSupportedFrameRateRanges.count);
    for (AVFrameRateRange *range in format.videoSupportedFrameRateRanges)
        qtRanges << AVFPSRange(range.minFrameRate, range.maxFrameRate);

    return qtRanges;
}

QSize qt_device_format_pixel_aspect_ratio(AVCaptureDeviceFormat *format)
{
    Q_ASSERT(format);

    if (!format.formatDescription) {
        qDebugCamera() << Q_FUNC_INFO << "no format description found";
        return QSize();
    }

    const CMVideoDimensions res = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
    const CGSize resPAR = CMVideoFormatDescriptionGetPresentationDimensions(format.formatDescription, true, false);

    if (qAbs(resPAR.width - res.width) < 1.) {
        // "Pixel aspect ratio is used to adjust the width, leaving the height alone."
        return QSize(1, 1);
    }

    if (!res.width || !resPAR.width)
        return QSize();

    int n, d;
    qt_real_to_fraction(resPAR.width > res.width
                        ? res.width / qreal(resPAR.width)
                        : resPAR.width / qreal(res.width),
                        &n, &d);

    return QSize(n, d);
}

AVCaptureDeviceFormat *qt_find_best_resolution_match(AVCaptureDevice *captureDevice,
                                                     const QSize &request,
                                                     FourCharCode filter)
{
    Q_ASSERT(captureDevice);
    Q_ASSERT(!request.isNull() && request.isValid());

    if (!captureDevice.formats || !captureDevice.formats.count)
        return 0;

    QVector<AVCaptureDeviceFormat *> formats(qt_unique_device_formats(captureDevice, filter));

    for (int i = 0; i < formats.size(); ++i) {
        AVCaptureDeviceFormat *format = formats[i];
        if (qt_device_format_resolution(format) == request)
            return format;
        // iOS only (still images).
        if (qt_device_format_high_resolution(format) == request)
            return format;
    }

    if (!qt_area_sane(request))
        return 0;

    typedef QPair<QSize, AVCaptureDeviceFormat *> FormatPair;

    QVector<FormatPair> pairs; // default|HR sizes
    pairs.reserve(formats.size());

    for (int i = 0; i < formats.size(); ++i) {
        AVCaptureDeviceFormat *format = formats[i];
        const QSize res(qt_device_format_resolution(format));
        if (!res.isNull() && res.isValid() && qt_area_sane(res))
            pairs << FormatPair(res, format);
        const QSize highRes(qt_device_format_high_resolution(format));
        if (!highRes.isNull() && highRes.isValid() && qt_area_sane(highRes))
            pairs << FormatPair(highRes, format);
    }

    if (!pairs.size())
        return 0;

    AVCaptureDeviceFormat *best = pairs[0].second;
    QSize next(pairs[0].first);
    int wDiff = qAbs(request.width() - next.width());
    int hDiff = qAbs(request.height() - next.height());
    const int area = request.width() * request.height();
    int areaDiff = qAbs(area - next.width() * next.height());
    for (int i = 1; i < pairs.size(); ++i) {
        next = pairs[i].first;
        const int newWDiff = qAbs(next.width() - request.width());
        const int newHDiff = qAbs(next.height() - request.height());
        const int newAreaDiff = qAbs(area - next.width() * next.height());

        if ((newWDiff < wDiff && newHDiff < hDiff)
            || ((newWDiff <= wDiff || newHDiff <= hDiff) && newAreaDiff <= areaDiff)) {
            wDiff = newWDiff;
            hDiff = newHDiff;
            best = pairs[i].second;
            areaDiff = newAreaDiff;
        }
    }

    return best;
}

AVCaptureDeviceFormat *qt_find_best_framerate_match(AVCaptureDevice *captureDevice,
                                                    FourCharCode filter,
                                                    Float64 fps)
{
    Q_ASSERT(captureDevice);
    Q_ASSERT(fps > 0.);

    const qreal epsilon = 0.1;

    QVector<AVCaptureDeviceFormat *>sorted(qt_unique_device_formats(captureDevice, filter));
    // Sort formats by their resolution in decreasing order:
    std::sort(sorted.begin(), sorted.end(), std::not2(ResolutionPredicate()));
    // We can use only formats with framerate ranges:
    sorted.erase(std::remove_if(sorted.begin(), sorted.end(), FormatHasNoFPSRange()), sorted.end());

    if (!sorted.size())
        return nil;

    for (int i = 0; i < sorted.size(); ++i) {
        AVCaptureDeviceFormat *format = sorted[i];
        for (AVFrameRateRange *range in format.videoSupportedFrameRateRanges) {
            if (range.maxFrameRate - range.minFrameRate < epsilon) {
                // On OS X ranges are points (built-in camera).
                if (qAbs(fps - range.maxFrameRate) < epsilon)
                    return format;
            }

            if (fps >= range.minFrameRate && fps <= range.maxFrameRate)
                return format;
        }
    }

    Float64 distance = qt_find_min_framerate_distance(sorted[0], fps);
    AVCaptureDeviceFormat *match = sorted[0];
    for (int i = 1; i < sorted.size(); ++i) {
        const Float64 newDistance = qt_find_min_framerate_distance(sorted[i], fps);
        if (newDistance < distance) {
            distance = newDistance;
            match = sorted[i];
        }
    }

    return match;
}

AVFrameRateRange *qt_find_supported_framerate_range(AVCaptureDeviceFormat *format, Float64 fps)
{
    Q_ASSERT(format && format.videoSupportedFrameRateRanges
             && format.videoSupportedFrameRateRanges.count);

    const qreal epsilon = 0.1;

    for (AVFrameRateRange *range in format.videoSupportedFrameRateRanges) {
        if (range.maxFrameRate - range.minFrameRate < epsilon) {
            // On OS X ranges are points (built-in camera).
            if (qAbs(fps - range.maxFrameRate) < epsilon)
                return range;
        }

        if (fps >= range.minFrameRate && fps <= range.maxFrameRate)
            return range;
    }

    AVFrameRateRange *match = [format.videoSupportedFrameRateRanges objectAtIndex:0];
    Float64 distance = qAbs(match.maxFrameRate - fps);
    for (NSUInteger i = 1, e = format.videoSupportedFrameRateRanges.count; i < e; ++i) {
        AVFrameRateRange *range = [format.videoSupportedFrameRateRanges objectAtIndex:i];
        const Float64 newDistance = qAbs(range.maxFrameRate - fps);
        if (newDistance < distance) {
            distance = newDistance;
            match = range;
        }
    }

    return match;
}

bool qt_formats_are_equal(AVCaptureDeviceFormat *f1, AVCaptureDeviceFormat *f2)
{
    if (f1 == f2)
        return true;

    if (![f1.mediaType isEqualToString:f2.mediaType])
        return false;

    return CMFormatDescriptionEqual(f1.formatDescription, f2.formatDescription);
}

bool qt_set_active_format(AVCaptureDevice *captureDevice, AVCaptureDeviceFormat *format, bool preserveFps)
{
    static bool firstSet = true;

    if (!captureDevice || !format)
        return false;

    if (qt_formats_are_equal(captureDevice.activeFormat, format)) {
        if (firstSet) {
            // The capture device format is persistent. The first time we set a format, report that
            // it changed even if the formats are the same.
            // This prevents the session from resetting the format to the default value.
            firstSet = false;
            return true;
        }
        return false;
    }

    firstSet = false;

    const AVFConfigurationLock lock(captureDevice);
    if (!lock) {
        qWarning("Failed to set active format (lock failed)");
        return false;
    }

    // Changing the activeFormat resets the frame rate.
    AVFPSRange fps;
    if (preserveFps)
        fps = qt_current_framerates(captureDevice, nil);

    captureDevice.activeFormat = format;

    if (preserveFps)
        qt_set_framerate_limits(captureDevice, nil, fps.first, fps.second);

    return true;
}

#endif // SDK

void qt_set_framerate_limits(AVCaptureConnection *videoConnection, qreal minFPS, qreal maxFPS)
{
    Q_ASSERT(videoConnection);

    if (minFPS < 0. || maxFPS < 0. || (maxFPS && maxFPS < minFPS)) {
        qDebugCamera() << Q_FUNC_INFO << "invalid framerates (min, max):"
                       << minFPS << maxFPS;
        return;
    }

    CMTime minDuration = kCMTimeInvalid;
    if (maxFPS > 0.) {
        if (!videoConnection.supportsVideoMinFrameDuration)
            qDebugCamera() << Q_FUNC_INFO << "maximum framerate is not supported";
        else
            minDuration = CMTimeMake(1, maxFPS);
    }
    if (videoConnection.supportsVideoMinFrameDuration)
        videoConnection.videoMinFrameDuration = minDuration;

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_9, __IPHONE_5_0)
#if QT_OSX_DEPLOYMENT_TARGET_BELOW(__MAC_10_9)
    if (QSysInfo::MacintoshVersion < QSysInfo::MV_10_9) {
        if (minFPS > 0.)
            qDebugCamera() << Q_FUNC_INFO << "minimum framerate is not supported";
    } else
#endif
    {
        CMTime maxDuration = kCMTimeInvalid;
        if (minFPS > 0.) {
            if (!videoConnection.supportsVideoMaxFrameDuration)
                qDebugCamera() << Q_FUNC_INFO << "minimum framerate is not supported";
            else
                maxDuration = CMTimeMake(1, minFPS);
        }
        if (videoConnection.supportsVideoMaxFrameDuration)
            videoConnection.videoMaxFrameDuration = maxDuration;
    }
#else
    if (minFPS > 0.)
        qDebugCamera() << Q_FUNC_INFO << "minimum framerate is not supported";
#endif
}

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_7, __IPHONE_7_0)

CMTime qt_adjusted_frame_duration(AVFrameRateRange *range, qreal fps)
{
    Q_ASSERT(range);
    Q_ASSERT(fps > 0.);

    if (range.maxFrameRate - range.minFrameRate < 0.1) {
        // Can happen on OS X.
        return range.minFrameDuration;
    }

    if (fps <= range.minFrameRate)
        return range.maxFrameDuration;
    if (fps >= range.maxFrameRate)
        return range.minFrameDuration;

    int n, d;
    qt_real_to_fraction(1. / fps, &n, &d);
    return CMTimeMake(n, d);
}

void qt_set_framerate_limits(AVCaptureDevice *captureDevice, qreal minFPS, qreal maxFPS)
{
    Q_ASSERT(captureDevice);
    if (!captureDevice.activeFormat) {
        qDebugCamera() << Q_FUNC_INFO << "no active capture device format";
        return;
    }

    if (minFPS < 0. || maxFPS < 0. || (maxFPS && maxFPS < minFPS)) {
        qDebugCamera() << Q_FUNC_INFO << "invalid framerates (min, max):"
                       << minFPS << maxFPS;
        return;
    }

    CMTime minFrameDuration = kCMTimeInvalid;
    CMTime maxFrameDuration = kCMTimeInvalid;
    if (maxFPS || minFPS) {
        AVFrameRateRange *range = qt_find_supported_framerate_range(captureDevice.activeFormat,
                                                                    maxFPS ? maxFPS : minFPS);
        if (!range) {
            qDebugCamera() << Q_FUNC_INFO << "no framerate range found, (min, max):"
                           << minFPS << maxFPS;
            return;
        }

        if (maxFPS)
            minFrameDuration = qt_adjusted_frame_duration(range, maxFPS);
        if (minFPS)
            maxFrameDuration = qt_adjusted_frame_duration(range, minFPS);
    }

    const AVFConfigurationLock lock(captureDevice);
    if (!lock) {
        qDebugCamera() << Q_FUNC_INFO << "failed to lock for configuration";
        return;
    }

    // While Apple's docs say kCMTimeInvalid will end in default
    // settings for this format, kCMTimeInvalid on OS X ends with a runtime
    // exception:
    // "The activeVideoMinFrameDuration passed is not supported by the device."
    // Instead, use the first item in the supported frame rates.
#ifdef Q_OS_IOS
    [captureDevice setActiveVideoMinFrameDuration:minFrameDuration];
    [captureDevice setActiveVideoMaxFrameDuration:maxFrameDuration];
#else // Q_OS_OSX
    if (CMTimeCompare(minFrameDuration, kCMTimeInvalid) == 0
            && CMTimeCompare(maxFrameDuration, kCMTimeInvalid) == 0) {
        AVFrameRateRange *range = captureDevice.activeFormat.videoSupportedFrameRateRanges.firstObject;
        minFrameDuration = range.minFrameDuration;
        maxFrameDuration = range.maxFrameDuration;
    }

    if (CMTimeCompare(minFrameDuration, kCMTimeInvalid))
        [captureDevice setActiveVideoMinFrameDuration:minFrameDuration];

#if QT_OSX_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_9)
#if QT_OSX_DEPLOYMENT_TARGET_BELOW(__MAC_10_9)
    if (QSysInfo::MacintoshVersion >= QSysInfo::MV_10_9)
#endif
    {
        if (CMTimeCompare(maxFrameDuration, kCMTimeInvalid))
            [captureDevice setActiveVideoMaxFrameDuration:maxFrameDuration];
    }
#endif // QT_OSX_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_9)
#endif // Q_OS_OSX
}

#endif // Platform SDK >= 10.9, >= 7.0.

void qt_set_framerate_limits(AVCaptureDevice *captureDevice, AVCaptureConnection *videoConnection,
                             qreal minFPS, qreal maxFPS)
{
    Q_ASSERT(captureDevice);
#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_7, __IPHONE_7_0)
    if (QSysInfo::MacintoshVersion >= qt_OS_limit(QSysInfo::MV_10_9, QSysInfo::MV_IOS_7_0))
        qt_set_framerate_limits(captureDevice, minFPS, maxFPS);
    else
#endif
    if (videoConnection)
        qt_set_framerate_limits(videoConnection, minFPS, maxFPS);

}

AVFPSRange qt_current_framerates(AVCaptureDevice *captureDevice, AVCaptureConnection *videoConnection)
{
    Q_ASSERT(captureDevice);

    AVFPSRange fps;
#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_7, __IPHONE_7_0)
    if (QSysInfo::MacintoshVersion >= qt_OS_limit(QSysInfo::MV_10_7, QSysInfo::MV_IOS_7_0)) {
        const CMTime minDuration = captureDevice.activeVideoMinFrameDuration;
        if (CMTimeCompare(minDuration, kCMTimeInvalid)) {
            if (const Float64 minSeconds = CMTimeGetSeconds(minDuration))
                fps.second = 1. / minSeconds; // Max FPS = 1 / MinDuration.
        }

#if QT_OSX_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_9)
#if QT_OSX_DEPLOYMENT_TARGET_BELOW(__MAC_10_9)
        if (QSysInfo::MacintoshVersion >= QSysInfo::MV_10_9)
#endif
        {
            const CMTime maxDuration = captureDevice.activeVideoMaxFrameDuration;
            if (CMTimeCompare(maxDuration, kCMTimeInvalid)) {
                if (const Float64 maxSeconds = CMTimeGetSeconds(maxDuration))
                    fps.first = 1. / maxSeconds; // Min FPS = 1 / MaxDuration.
            }
        }
#endif // QT_OSX_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_9)

    } else {
#else // OSX < 10.7 or iOS < 7.0
    {
#endif // QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_7, __IPHONE_7_0)
        if (videoConnection)
            fps = qt_connection_framerates(videoConnection);
    }

    return fps;
}

QT_END_NAMESPACE
