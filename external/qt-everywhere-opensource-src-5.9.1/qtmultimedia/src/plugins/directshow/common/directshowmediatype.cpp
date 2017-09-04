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

#include "directshowmediatype.h"
#include "directshowglobal.h"

namespace
{
    struct TypeLookup
    {
        QVideoFrame::PixelFormat pixelFormat;
        GUID mediaType;
    };

    static const TypeLookup qt_typeLookup[] =
    {
        { QVideoFrame::Format_ARGB32, MEDIASUBTYPE_ARGB32 },
        { QVideoFrame::Format_RGB32, MEDIASUBTYPE_RGB32 },
        { QVideoFrame::Format_RGB24, MEDIASUBTYPE_RGB24 },
        { QVideoFrame::Format_RGB565, MEDIASUBTYPE_RGB565 },
        { QVideoFrame::Format_RGB555, MEDIASUBTYPE_RGB555 },
        { QVideoFrame::Format_AYUV444, MEDIASUBTYPE_AYUV },
        { QVideoFrame::Format_YUYV, MEDIASUBTYPE_YUY2 },
        { QVideoFrame::Format_UYVY, MEDIASUBTYPE_UYVY },
        { QVideoFrame::Format_IMC1, MEDIASUBTYPE_IMC1 },
        { QVideoFrame::Format_IMC2, MEDIASUBTYPE_IMC2 },
        { QVideoFrame::Format_IMC3, MEDIASUBTYPE_IMC3 },
        { QVideoFrame::Format_IMC4, MEDIASUBTYPE_IMC4 },
        { QVideoFrame::Format_YV12, MEDIASUBTYPE_YV12 },
        { QVideoFrame::Format_NV12, MEDIASUBTYPE_NV12 },
        { QVideoFrame::Format_YUV420P, MEDIASUBTYPE_IYUV },
        { QVideoFrame::Format_YUV420P, MEDIASUBTYPE_I420 },
        { QVideoFrame::Format_Jpeg, MEDIASUBTYPE_MJPG }
    };
}

bool DirectShowMediaType::isPartiallySpecified(const AM_MEDIA_TYPE *mediaType)
{
    return mediaType->majortype == GUID_NULL || mediaType->formattype == GUID_NULL;
}

DirectShowMediaType::DirectShowMediaType()
    : mediaType({ GUID_NULL, GUID_NULL, TRUE, FALSE, 1, GUID_NULL, nullptr, 0, nullptr})
{
}

DirectShowMediaType::DirectShowMediaType(const AM_MEDIA_TYPE &type)
    : DirectShowMediaType()
{
    copy(&mediaType, &type);
}

DirectShowMediaType::DirectShowMediaType(AM_MEDIA_TYPE &&type)
    : DirectShowMediaType()
{
    move(&mediaType, type);
}

DirectShowMediaType::DirectShowMediaType(const DirectShowMediaType &other)
    : DirectShowMediaType()
{
    copy(&mediaType, &other.mediaType);
}

DirectShowMediaType::DirectShowMediaType(DirectShowMediaType &&other)
    : DirectShowMediaType()
{
    move(&mediaType, other.mediaType);
}

DirectShowMediaType &DirectShowMediaType::operator=(const DirectShowMediaType &other)
{
    copy(&mediaType, &other.mediaType);
    return *this;
}

DirectShowMediaType &DirectShowMediaType::operator=(DirectShowMediaType &&other)
{
    move(&mediaType, other.mediaType);
    return *this;
}

void DirectShowMediaType::init(AM_MEDIA_TYPE *type)
{
    Q_ASSERT(type);
    SecureZeroMemory(reinterpret_cast<void *>(type), sizeof(AM_MEDIA_TYPE));
    type->lSampleSize = 1;
    type->bFixedSizeSamples = TRUE;
}

void DirectShowMediaType::copy(AM_MEDIA_TYPE *target, const AM_MEDIA_TYPE *source)
{
    if (!(target && source))
        return;

    if (target == source)
        return;

    clear(*target);

    copyToUninitialized(target, source);
}

void DirectShowMediaType::copyToUninitialized(AM_MEDIA_TYPE *target, const AM_MEDIA_TYPE *source)
{
    *target = *source;

    if (source->cbFormat > 0) {
        target->pbFormat = reinterpret_cast<PBYTE>(CoTaskMemAlloc(source->cbFormat));
        memcpy(target->pbFormat, source->pbFormat, source->cbFormat);
    }
    if (target->pUnk)
        target->pUnk->AddRef();
}

void DirectShowMediaType::move(AM_MEDIA_TYPE *target, AM_MEDIA_TYPE **source)
{
    if (!target || !source || !(*source))
        return;

    if (target == *source)
        return;

    clear(*target);
    *target = *(*source);
    SecureZeroMemory(reinterpret_cast<void *>(*source), sizeof(AM_MEDIA_TYPE));
    *source = nullptr;
}

void DirectShowMediaType::move(AM_MEDIA_TYPE *target, AM_MEDIA_TYPE &source)
{
    AM_MEDIA_TYPE *srcPtr = &source;
    move(target, &srcPtr);
}

/**
 * @brief DirectShowMediaType::deleteType - Used for AM_MEDIA_TYPE structures that have
 *        been allocated by CoTaskMemAlloc or CreateMediaType.
 * @param type
 */
void DirectShowMediaType::deleteType(AM_MEDIA_TYPE *type)
{
    if (!type)
        return;

    clear(*type);
    CoTaskMemFree(type);
}

bool DirectShowMediaType::isCompatible(const AM_MEDIA_TYPE *a, const AM_MEDIA_TYPE *b)
{
    if (b->majortype != GUID_NULL && a->majortype != b->majortype)
        return false;

    if (b->subtype != GUID_NULL && a->subtype != b->subtype)
        return false;

    if (b->formattype != GUID_NULL) {
        if (a->formattype != b->formattype)
            return false;
        if (a->cbFormat != b->cbFormat)
            return false;
        if (a->cbFormat != 0 && memcmp(a->pbFormat, b->pbFormat, a->cbFormat) != 0)
            return false;
    }

    return true;
}

/**
 * @brief DirectShowMediaType::clear - Clears all member data, and releases allocated buffers.
 *        Use this to release automatic AM_MEDIA_TYPE structures.
 * @param type
 */
void DirectShowMediaType::clear(AM_MEDIA_TYPE &type)
{
    if (type.cbFormat > 0)
        CoTaskMemFree(type.pbFormat);

    if (type.pUnk)
        type.pUnk->Release();

    SecureZeroMemory(&type, sizeof(type));
}


GUID DirectShowMediaType::convertPixelFormat(QVideoFrame::PixelFormat format)
{
    const int count = sizeof(qt_typeLookup) / sizeof(TypeLookup);

    for (int i = 0; i < count; ++i)
        if (qt_typeLookup[i].pixelFormat == format)
            return qt_typeLookup[i].mediaType;

    return MEDIASUBTYPE_None;
}

QVideoSurfaceFormat DirectShowMediaType::videoFormatFromType(const AM_MEDIA_TYPE *type)
{
    if (!type)
        return QVideoSurfaceFormat();

    const int count = sizeof(qt_typeLookup) / sizeof(TypeLookup);

    for (int i = 0; i < count; ++i) {
        if (IsEqualGUID(qt_typeLookup[i].mediaType, type->subtype) && type->cbFormat > 0) {
            if (IsEqualGUID(type->formattype, FORMAT_VideoInfo)) {
                VIDEOINFOHEADER *header = reinterpret_cast<VIDEOINFOHEADER *>(type->pbFormat);

                QVideoSurfaceFormat format(
                        QSize(header->bmiHeader.biWidth, qAbs(header->bmiHeader.biHeight)),
                        qt_typeLookup[i].pixelFormat);

                if (header->AvgTimePerFrame > 0)
                    format.setFrameRate(10000 /header->AvgTimePerFrame);

                format.setScanLineDirection(scanLineDirection(format.pixelFormat(), header->bmiHeader));

                return format;
            } else if (IsEqualGUID(type->formattype, FORMAT_VideoInfo2)) {
                VIDEOINFOHEADER2 *header = reinterpret_cast<VIDEOINFOHEADER2 *>(type->pbFormat);

                QVideoSurfaceFormat format(
                        QSize(header->bmiHeader.biWidth, qAbs(header->bmiHeader.biHeight)),
                        qt_typeLookup[i].pixelFormat);

                if (header->AvgTimePerFrame > 0)
                    format.setFrameRate(10000 / header->AvgTimePerFrame);

                format.setScanLineDirection(scanLineDirection(format.pixelFormat(), header->bmiHeader));

                return format;
            }
        }
    }
    return QVideoSurfaceFormat();
}

QVideoFrame::PixelFormat DirectShowMediaType::pixelFormatFromType(const AM_MEDIA_TYPE *type)
{
    if (!type)
        return QVideoFrame::Format_Invalid;

    const int count = sizeof(qt_typeLookup) / sizeof(TypeLookup);

    for (int i = 0; i < count; ++i) {
        if (IsEqualGUID(qt_typeLookup[i].mediaType, type->subtype)) {
            return qt_typeLookup[i].pixelFormat;
        }
    }

    return QVideoFrame::Format_Invalid;
}

#define PAD_TO_DWORD(x)  (((x) + 3) & ~3)
int DirectShowMediaType::bytesPerLine(const QVideoSurfaceFormat &format)
{
    switch (format.pixelFormat()) {
    // 32 bpp packed formats.
    case QVideoFrame::Format_ARGB32:
    case QVideoFrame::Format_RGB32:
    case QVideoFrame::Format_AYUV444:
        return format.frameWidth() * 4;
    // 24 bpp packed formats.
    case QVideoFrame::Format_RGB24:
        return PAD_TO_DWORD(format.frameWidth() * 3);
    // 16 bpp packed formats.
    case QVideoFrame::Format_RGB565:
    case QVideoFrame::Format_RGB555:
    case QVideoFrame::Format_YUYV:
    case QVideoFrame::Format_UYVY:
        return PAD_TO_DWORD(format.frameWidth() * 2);
    // Planar formats.
    case QVideoFrame::Format_YV12:
    case QVideoFrame::Format_YUV420P:
    case QVideoFrame::Format_IMC1:
    case QVideoFrame::Format_IMC2:
    case QVideoFrame::Format_IMC3:
    case QVideoFrame::Format_IMC4:
    case QVideoFrame::Format_NV12:
        return format.frameWidth();
    default:
        return 0;
    }
}

QVideoSurfaceFormat::Direction DirectShowMediaType::scanLineDirection(QVideoFrame::PixelFormat pixelFormat, const BITMAPINFOHEADER &bmiHeader)
{
    /* MSDN http://msdn.microsoft.com/en-us/library/windows/desktop/dd318229(v=vs.85).aspx */
    /* For uncompressed RGB bitmaps:
     *    if biHeight is positive, the bitmap is a bottom-up DIB with the origin at the lower left corner.
     *    If biHeight is negative, the bitmap is a top-down DIB with the origin at the upper left corner.
     *
     * For YUV bitmaps:
     *    the bitmap is always top-down, regardless of the sign of biHeight.
     *    Decoders should offer YUV formats with postive biHeight, but for backward compatibility they should accept YUV formats with either positive or negative biHeight.
     *
     * For compressed formats:
     *    biHeight must be positive, regardless of image orientation.
     */
    switch (pixelFormat)
    {
    case QVideoFrame::Format_ARGB32:
    case QVideoFrame::Format_RGB32:
    case QVideoFrame::Format_RGB24:
    case QVideoFrame::Format_RGB565:
    case QVideoFrame::Format_RGB555:
        return bmiHeader.biHeight < 0
            ? QVideoSurfaceFormat::TopToBottom
            : QVideoSurfaceFormat::BottomToTop;
    default:
        return QVideoSurfaceFormat::TopToBottom;
    }
}
