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

namespace
{
    struct TypeLookup
    {
        QVideoFrame::PixelFormat pixelFormat;
        GUID mediaType;
    };

    static const TypeLookup qt_typeLookup[] =
    {
        { QVideoFrame::Format_RGB32,   /*MEDIASUBTYPE_RGB32*/  {0xe436eb7e, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}} },
        { QVideoFrame::Format_BGR24,   /*MEDIASUBTYPE_RGB24*/  {0xe436eb7d, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}} },
        { QVideoFrame::Format_RGB565,  /*MEDIASUBTYPE_RGB565*/ {0xe436eb7b, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}} },
        { QVideoFrame::Format_RGB555,  /*MEDIASUBTYPE_RGB555*/ {0xe436eb7c, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}} },
        { QVideoFrame::Format_AYUV444, /*MEDIASUBTYPE_AYUV*/   {0x56555941, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}} },
        { QVideoFrame::Format_YUYV,    /*MEDIASUBTYPE_YUY2*/   {0x32595559, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}} },
        { QVideoFrame::Format_UYVY,    /*MEDIASUBTYPE_UYVY*/   {0x59565955, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}} },
        { QVideoFrame::Format_IMC1,    /*MEDIASUBTYPE_IMC1*/   {0x31434D49, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}} },
        { QVideoFrame::Format_IMC2,    /*MEDIASUBTYPE_IMC2*/   {0x32434D49, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}} },
        { QVideoFrame::Format_IMC3,    /*MEDIASUBTYPE_IMC3*/   {0x33434D49, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}} },
        { QVideoFrame::Format_IMC4,    /*MEDIASUBTYPE_IMC4*/   {0x34434D49, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}} },
        { QVideoFrame::Format_YV12,    /*MEDIASUBTYPE_YV12*/   {0x32315659, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}} },
        { QVideoFrame::Format_NV12,    /*MEDIASUBTYPE_NV12*/   {0x3231564E, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}} },
        { QVideoFrame::Format_YUV420P, /*MEDIASUBTYPE_IYUV*/   {0x56555949, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}} },
        { QVideoFrame::Format_YUV420P, /*MEDIASUBTYPE_I420*/   {0x30323449, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}} }
    };
}

bool DirectShowMediaType::isPartiallySpecified() const
{
    return majortype == GUID_NULL || formattype == GUID_NULL;
}

bool DirectShowMediaType::isCompatibleWith(const DirectShowMediaType *type) const
{
    if (type->majortype != GUID_NULL && majortype != type->majortype)
        return false;

    if (type->subtype != GUID_NULL && subtype != type->subtype)
        return false;

    if (type->formattype != GUID_NULL) {
        if (formattype != type->formattype)
            return false;
        if (cbFormat != type->cbFormat)
            return false;
        if (cbFormat != 0 && memcmp(pbFormat, type->pbFormat, cbFormat) != 0)
            return false;
    }

    return true;
}

void DirectShowMediaType::init(AM_MEDIA_TYPE *type)
{
    ZeroMemory((PVOID)type, sizeof(*type));
    type->lSampleSize = 1;
    type->bFixedSizeSamples = TRUE;
}

void DirectShowMediaType::copy(AM_MEDIA_TYPE *target, const AM_MEDIA_TYPE &source)
{
    if (!target)
        return;

    *target = source;

    if (source.cbFormat > 0) {
        target->pbFormat = reinterpret_cast<PBYTE>(CoTaskMemAlloc(source.cbFormat));
        memcpy(target->pbFormat, source.pbFormat, source.cbFormat);
    }
    if (target->pUnk)
        target->pUnk->AddRef();
}

void DirectShowMediaType::deleteType(AM_MEDIA_TYPE *type)
{
    freeData(type);

    CoTaskMemFree(type);
}

void DirectShowMediaType::freeData(AM_MEDIA_TYPE *type)
{
    if (type->cbFormat > 0)
        CoTaskMemFree(type->pbFormat);

    if (type->pUnk)
        type->pUnk->Release();
}


GUID DirectShowMediaType::convertPixelFormat(QVideoFrame::PixelFormat format)
{
    const int count = sizeof(qt_typeLookup) / sizeof(TypeLookup);

    for (int i = 0; i < count; ++i)
        if (qt_typeLookup[i].pixelFormat == format)
            return qt_typeLookup[i].mediaType;

    return MEDIASUBTYPE_None;
}

QVideoSurfaceFormat DirectShowMediaType::formatFromType(const AM_MEDIA_TYPE &type)
{
    const int count = sizeof(qt_typeLookup) / sizeof(TypeLookup);

    for (int i = 0; i < count; ++i) {
        if (IsEqualGUID(qt_typeLookup[i].mediaType, type.subtype) && type.cbFormat > 0) {
            if (IsEqualGUID(type.formattype, FORMAT_VideoInfo)) {
                VIDEOINFOHEADER *header = reinterpret_cast<VIDEOINFOHEADER *>(type.pbFormat);

                QVideoSurfaceFormat format(
                        QSize(header->bmiHeader.biWidth, qAbs(header->bmiHeader.biHeight)),
                        qt_typeLookup[i].pixelFormat);

                if (header->AvgTimePerFrame > 0)
                    format.setFrameRate(10000 /header->AvgTimePerFrame);

                format.setScanLineDirection(scanLineDirection(format.pixelFormat(), header->bmiHeader));

                return format;
            } else if (IsEqualGUID(type.formattype, FORMAT_VideoInfo2)) {
                VIDEOINFOHEADER2 *header = reinterpret_cast<VIDEOINFOHEADER2 *>(type.pbFormat);

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

QVideoFrame::PixelFormat DirectShowMediaType::pixelFormatFromType(const AM_MEDIA_TYPE &type)
{
    const int count = sizeof(qt_typeLookup) / sizeof(TypeLookup);

    for (int i = 0; i < count; ++i) {
        if (IsEqualGUID(qt_typeLookup[i].mediaType, type.subtype)) {
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
    case QVideoFrame::Format_RGB32:
    case QVideoFrame::Format_BGR24:
    case QVideoFrame::Format_RGB565:
    case QVideoFrame::Format_RGB555:
        return bmiHeader.biHeight < 0
            ? QVideoSurfaceFormat::TopToBottom
            : QVideoSurfaceFormat::BottomToTop;
    default:
        return QVideoSurfaceFormat::TopToBottom;
    }
}
