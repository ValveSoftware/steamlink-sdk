/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Ivan Komissarov.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the DDS plugin in the Qt ImageFormats module.
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

#include "qddshandler.h"

#include <QtCore/qdebug.h>
#include <QtGui/qimage.h>

#include <cmath>

#include "ddsheader.h"

#include <cmath>

#ifndef QT_NO_DATASTREAM

QT_BEGIN_NAMESPACE

enum Colors {
    Red = 0,
    Green,
    Blue,
    Alpha,
    ColorCount
};

enum DXTVersions {
    One = 1,
    Two = 2,
    Three = 3,
    Four = 4,
    Five = 5,
    RXGB = 6
};

// All magic numbers are little-endian as long as dds format has little
// endian byte order
static const quint32 ddsMagic = 0x20534444; // "DDS "
static const quint32 dx10Magic = 0x30315844; // "DX10"

static const qint64 headerSize = 128;
static const quint32 ddsSize = 124; // headerSize without magic
static const quint32 pixelFormatSize = 32;

struct FaceOffset
{
    int x, y;
};

static const FaceOffset faceOffsets[6] = { {2, 1}, {0, 1}, {1, 0}, {1, 2}, {1, 1}, {3, 1} };

static int faceFlags[6] = {
    DDSHeader::Caps2CubeMapPositiveX,
    DDSHeader::Caps2CubeMapNegativeX,
    DDSHeader::Caps2CubeMapPositiveY,
    DDSHeader::Caps2CubeMapNegativeY,
    DDSHeader::Caps2CubeMapPositiveZ,
    DDSHeader::Caps2CubeMapNegativeZ
};

struct FormatInfo
{
    Format format;
    quint32 flags;
    quint32 bitCount;
    quint32 rBitMask;
    quint32 gBitMask;
    quint32 bBitMask;
    quint32 aBitMask;
};

static const FormatInfo formatInfos[] = {
    { FormatA8R8G8B8,    DDSPixelFormat::FlagRGBA, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 },
    { FormatX8R8G8B8,    DDSPixelFormat::FlagRGB,  32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000 },
    { FormatA2B10G10R10, DDSPixelFormat::FlagRGBA, 32, 0x000003ff, 0x000ffc00, 0x3ff00000, 0xc0000000 },
    { FormatA8B8G8R8,    DDSPixelFormat::FlagRGBA, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 },
    { FormatX8B8G8R8,    DDSPixelFormat::FlagRGB,  32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000 },
    { FormatG16R16,      DDSPixelFormat::FlagRGBA, 32, 0x0000ffff, 0xffff0000, 0x00000000, 0x00000000 },
    { FormatG16R16,      DDSPixelFormat::FlagRGB,  32, 0x0000ffff, 0xffff0000, 0x00000000, 0x00000000 },
    { FormatA2R10G10B10, DDSPixelFormat::FlagRGBA, 32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000 },

    { FormatR8G8B8,      DDSPixelFormat::FlagRGB,  24, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000 },

    { FormatR5G6B5,      DDSPixelFormat::FlagRGB,  16, 0x0000f800, 0x000007e0, 0x0000001f, 0x00000000 },
    { FormatX1R5G5B5,    DDSPixelFormat::FlagRGB,  16, 0x00007c00, 0x000003e0, 0x0000001f, 0x00000000 },
    { FormatA1R5G5B5,    DDSPixelFormat::FlagRGBA, 16, 0x00007c00, 0x000003e0, 0x0000001f, 0x00008000 },
    { FormatA4R4G4B4,    DDSPixelFormat::FlagRGBA, 16, 0x00000f00, 0x000000f0, 0x0000000f, 0x0000f000 },
    { FormatA8R3G3B2,    DDSPixelFormat::FlagRGBA, 16, 0x000000e0, 0x0000001c, 0x00000003, 0x0000ff00 },
    { FormatX4R4G4B4,    DDSPixelFormat::FlagRGB,  16, 0x00000f00, 0x000000f0, 0x0000000f, 0x00000000 },
    { FormatA8L8,        DDSPixelFormat::FlagLA,   16, 0x000000ff, 0x00000000, 0x00000000, 0x0000ff00 },
    { FormatL16,   DDSPixelFormat::FlagLuminance,  16, 0x0000ffff, 0x00000000, 0x00000000, 0x00000000 },

    { FormatR3G3B2,      DDSPixelFormat::FlagRGB,  8,  0x000000e0, 0x0000001c, 0x00000003, 0x00000000 },
    { FormatA8,        DDSPixelFormat::FlagAlpha,  8,  0x00000000, 0x00000000, 0x00000000, 0x000000ff },
    { FormatL8,    DDSPixelFormat::FlagLuminance,  8,  0x000000ff, 0x00000000, 0x00000000, 0x00000000 },
    { FormatA4L4,        DDSPixelFormat::FlagLA,   8,  0x0000000f, 0x00000000, 0x00000000, 0x000000f0 },

    { FormatV8U8,        DDSPixelFormat::FlagNormal, 16, 0x000000ff, 0x0000ff00, 0x00000000, 0x00000000 },
    { FormatL6V5U5,                                0, 16, 0x0000001f, 0x000003e0, 0x0000fc00, 0x00000000 },
    { FormatX8L8V8U8,                              0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000 },
    { FormatQ8W8V8U8,    DDSPixelFormat::FlagNormal, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 },
    { FormatV16U16,      DDSPixelFormat::FlagNormal, 32, 0x0000ffff, 0xffff0000, 0x00000000, 0x00000000 },
    { FormatA2W10V10U10, DDSPixelFormat::FlagNormal, 32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000 }
};
static const size_t formatInfosSize = sizeof(formatInfos)/sizeof(FormatInfo);

static const Format knownFourCCs[] = {
    FormatA16B16G16R16,
    FormatV8U8,
    FormatUYVY,
    FormatR8G8B8G8,
    FormatYUY2,
    FormatG8R8G8B8,
    FormatDXT1,
    FormatDXT2,
    FormatDXT3,
    FormatDXT4,
    FormatDXT5,
    FormatRXGB,
    FormatATI2,
    FormatQ16W16V16U16,
    FormatR16F,
    FormatG16R16F,
    FormatA16B16G16R16F,
    FormatR32F,
    FormatG32R32F,
    FormatA32B32G32R32F,
    FormatCxV8U8
};
static const size_t knownFourCCsSize = sizeof(knownFourCCs)/sizeof(Format);

struct FormatName
{
    Format format;
    const char *const name;
};
static const FormatName formatNames[] = {
    { FormatUnknown,  "unknown" },

    { FormatR8G8B8,   "R8G8B8"  },
    { FormatA8R8G8B8, "A8R8G8B8" },
    { FormatX8R8G8B8, "X8R8G8B8" },
    { FormatR5G6B5,   "R5G6B5" },
    { FormatX1R5G5B5, "X1R5G5B5" },
    { FormatA1R5G5B5, "A1R5G5B5" },
    { FormatA4R4G4B4, "A4R4G4B4" },
    { FormatR3G3B2, "R3G3B2" },
    { FormatA8, "A8" },
    { FormatA8R3G3B2, "A8R3G3B2" },
    { FormatX4R4G4B4, "X4R4G4B4" },
    { FormatA2B10G10R10, "A2B10G10R10" },
    { FormatA8B8G8R8, "A8B8G8R8" },
    { FormatX8B8G8R8, "X8B8G8R8" },
    { FormatG16R16, "G16R16" },
    { FormatA2R10G10B10, "A2R10G10B10" },
    { FormatA16B16G16R16, "A16B16G16R16" },

    { FormatA8P8, "A8P8" },
    { FormatP8, "P8" },

    { FormatL8, "L8" },
    { FormatA8L8, "A8L8" },
    { FormatA4L4, "A4L4" },

    { FormatV8U8, "V8U8" },
    { FormatL6V5U5, "L6V5U5" },
    { FormatX8L8V8U8, "X8L8V8U8" },
    { FormatQ8W8V8U8, "Q8W8V8U8" },
    { FormatV16U16, "V16U16" },
    { FormatA2W10V10U10, "A2W10V10U10" },

    { FormatUYVY, "UYVY" },
    { FormatR8G8B8G8, "R8G8_B8G8" },
    { FormatYUY2, "YUY2" },
    { FormatG8R8G8B8, "G8R8_G8B8" },
    { FormatDXT1, "DXT1" },
    { FormatDXT2, "DXT2" },
    { FormatDXT3, "DXT3" },
    { FormatDXT4, "DXT4" },
    { FormatDXT5, "DXT5" },
    { FormatRXGB, "RXGB" },
    { FormatATI2, "ATI2" },

    { FormatD16Lockable, "D16Lockable" },
    { FormatD32, "D32" },
    { FormatD15S1, "D15S1" },
    { FormatD24S8, "D24S8" },
    { FormatD24X8, "D24X8" },
    { FormatD24X4S4, "D24X4S4" },
    { FormatD16, "D16" },

    { FormatD32FLockable, "D32FLockable" },
    { FormatD24FS8, "D24FS8" },

    { FormatD32Lockable, "D32Lockable" },
    { FormatS8Lockable, "S8Lockable" },

    { FormatL16, "L16" },

    { FormatVertexData, "VertexData" },
    { FormatIndex32, "Index32" },
    { FormatIndex32, "Index32" },

    { FormatQ16W16V16U16, "Q16W16V16U16" },

    { FormatMulti2ARGB8, "Multi2ARGB8" },

    { FormatR16F, "R16F" },
    { FormatG16R16F, "G16R16F" },
    { FormatA16B16G16R16F, "A16B16G16R16F" },

    { FormatR32F, "R32F" },
    { FormatG32R32F, "G32R32F" },
    { FormatA32B32G32R32F, "A32B32G32R32F" },

    { FormatCxV8U8, "CxV8U8" },

    { FormatA1, "A1" },
    { FormatA2B10G10R10_XR_BIAS, "A2B10G10R10_XR_BIAS" },
    { FormatBinaryBuffer, "BinaryBuffer" },

    { FormatP4, "P4" },
    { FormatA4P4, "A4P4" }
};
static const size_t formatNamesSize = sizeof(formatNames)/sizeof(FormatName);

static inline int maskToShift(quint32 mask)
{
    if (mask == 0)
        return 0;

    int result = 0;
    while (!((mask >> result) & 1))
        result++;
    return result;
}

static inline int maskLength(quint32 mask)
{
    int result = 0;
    while (mask) {
       if (mask & 1)
           result++;
       mask >>= 1;
    }
    return result;
}

static inline quint32 readValue(QDataStream &s, quint32 size)
{
    Q_ASSERT(size == 8 || size == 16 || size == 24 || size == 32);

    quint32 value = 0;
    quint8 tmp;
    for (unsigned bit = 0; bit < size; bit += 8) {
        s >> tmp;
        value += (quint32(tmp) << bit);
    }
    return value;
}

static inline bool hasAlpha(const DDSHeader &dds)
{
    return (dds.pixelFormat.flags & (DDSPixelFormat::FlagAlphaPixels | DDSPixelFormat::FlagAlpha)) != 0;
}

static inline bool isCubeMap(const DDSHeader &dds)
{
    return (dds.caps2 & DDSHeader::Caps2CubeMap) != 0;
}

static inline QRgb yuv2rgb(quint8 Y, quint8 U, quint8 V)
{
    return qRgb(quint8(Y + 1.13983 * (V - 128)),
                quint8(Y - 0.39465 * (U - 128) - 0.58060 * (V - 128)),
                quint8(Y + 2.03211 * (U - 128)));
}

static Format getFormat(const DDSHeader &dds)
{
    const DDSPixelFormat &format = dds.pixelFormat;
    if (format.flags & DDSPixelFormat::FlagPaletteIndexed4) {
        return FormatP4;
    } else if (format.flags & DDSPixelFormat::FlagPaletteIndexed8) {
        return FormatP8;
    } else if (format.flags & DDSPixelFormat::FlagFourCC) {
        for (size_t i = 0; i < knownFourCCsSize; ++i) {
            if (dds.pixelFormat.fourCC == knownFourCCs[i])
                return knownFourCCs[i];
        }
    } else {
        for (size_t i = 0; i < formatInfosSize; ++i) {
            const FormatInfo &info = formatInfos[i];
            if ((format.flags & info.flags) == info.flags &&
                 format.rgbBitCount == info.bitCount &&
                 format.rBitMask == info.rBitMask &&
                 format.gBitMask == info.gBitMask &&
                 format.bBitMask == info.bBitMask &&
                 format.aBitMask == info.aBitMask) {
                return info.format;
            }
        }
    }

    return FormatUnknown;
}

static inline quint8 getNormalZ(quint8 nx, quint8 ny)
{
    const double fx = nx / 127.5 - 1.0;
    const double fy = ny / 127.5 - 1.0;
    const double fxfy = 1.0 - fx * fx - fy * fy;
    return fxfy > 0 ? 255 * std::sqrt(fxfy) : 0;
}

static inline void decodeColor(quint16 color, quint8 &red, quint8 &green, quint8 &blue)
{
    red = ((color >> 11) & 0x1f) << 3;
    green = ((color >> 5) & 0x3f) << 2;
    blue = (color & 0x1f) << 3;
}

static inline quint8 calcC2(quint8 c0, quint8 c1)
{
    return 2.0 * c0 / 3.0 + c1 / 3.0;
}

static inline quint8 calcC2a(quint8 c0, quint8 c1)
{
    return c0 / 2.0 + c1 / 2.0;
}

static inline quint8 calcC3(quint8 c0, quint8 c1)
{
    return c0 / 3.0 + 2.0 * c1 / 3.0;
}

static void DXTFillColors(QRgb *result, quint16 c0, quint16 c1, quint32 table, bool dxt1a = false)
{
    quint8 r[4];
    quint8 g[4];
    quint8 b[4];
    quint8 a[4];

    a[0] = a[1] = a[2] = a[3] = 255;

    decodeColor(c0, r[0], g[0], b[0]);
    decodeColor(c1, r[1], g[1], b[1]);
    if (!dxt1a) {
        r[2] = calcC2(r[0], r[1]);
        g[2] = calcC2(g[0], g[1]);
        b[2] = calcC2(b[0], b[1]);
        r[3] = calcC3(r[0], r[1]);
        g[3] = calcC3(g[0], g[1]);
        b[3] = calcC3(b[0], b[1]);
    } else {
        r[2] = calcC2a(r[0], r[1]);
        g[2] = calcC2a(g[0], g[1]);
        b[2] = calcC2a(b[0], b[1]);
        r[3] = g[3] = b[3] = a[3] = 0;
    }

    for (int k = 0; k < 4; k++)
        for (int l = 0; l < 4; l++) {
            unsigned index = table & 0x0003;
            table >>= 2;

            result[k * 4 + l] = qRgba(r[index], g[index], b[index], a[index]);
        }
}

template <DXTVersions version>
inline void setAlphaDXT32Helper(QRgb *rgbArr, quint64 alphas)
{
    Q_STATIC_ASSERT(version == Two || version == Three);
    for (int i = 0; i < 16; i++) {
        quint8 alpha = 16 * (alphas & 0x0f);
        QRgb rgb = rgbArr[i];
        if (version == Two) // DXT2
            rgbArr[i] = qRgba(qRed(rgb) * alpha / 0xff, qGreen(rgb) * alpha / 0xff, qBlue(rgb) * alpha / 0xff, alpha);
        else if (version == Three) // DXT3
            rgbArr[i] = qRgba(qRed(rgb), qGreen(rgb), qBlue(rgb), alpha);
        alphas = alphas >> 4;
    }
}

template <DXTVersions version>
inline void setAlphaDXT45Helper(QRgb *rgbArr, quint64 alphas)
{
    Q_STATIC_ASSERT(version == Four || version == Five);
    quint8 a[8];
    a[0] = alphas & 0xff;
    a[1] = (alphas >> 8) & 0xff;
    if (a[0] > a[1]) {
        a[2] = (6*a[0] + 1*a[1]) / 7;
        a[3] = (5*a[0] + 2*a[1]) / 7;
        a[4] = (4*a[0] + 3*a[1]) / 7;
        a[5] = (3*a[0] + 4*a[1]) / 7;
        a[6] = (2*a[0] + 5*a[1]) / 7;
        a[7] = (1*a[0] + 6*a[1]) / 7;
    } else {
        a[2] = (4*a[0] + 1*a[1]) / 5;
        a[3] = (3*a[0] + 2*a[1]) / 5;
        a[4] = (2*a[0] + 3*a[1]) / 5;
        a[5] = (1*a[0] + 4*a[1]) / 5;
        a[6] = 0;
        a[7] = 255;
    }
    alphas >>= 16;
    for (int i = 0; i < 16; i++) {
        quint8 index = alphas & 0x07;
        quint8 alpha = a[index];
        QRgb rgb = rgbArr[i];
        if (version == Four) // DXT4
            rgbArr[i] = qRgba(qRed(rgb) * alpha / 0xff, qGreen(rgb) * alpha / 0xff, qBlue(rgb) * alpha / 0xff, alpha);
        else if (version == Five) // DXT5
            rgbArr[i] = qRgba(qRed(rgb), qGreen(rgb), qBlue(rgb), alpha);
        alphas = alphas >> 3;
    }
}

template <DXTVersions version>
inline void setAlphaDXT(QRgb *rgbArr, quint64 alphas)
{
    Q_UNUSED(rgbArr);
    Q_UNUSED(alphas);
}

template <>
inline void setAlphaDXT<Two>(QRgb *rgbArr, quint64 alphas)
{
    setAlphaDXT32Helper<Two>(rgbArr, alphas);
}

template <>
inline void setAlphaDXT<Three>(QRgb *rgbArr, quint64 alphas)
{
    setAlphaDXT32Helper<Three>(rgbArr, alphas);
}

template <>
inline void setAlphaDXT<Four>(QRgb *rgbArr, quint64 alphas)
{
    setAlphaDXT45Helper<Four>(rgbArr, alphas);
}

template <>
inline void setAlphaDXT<Five>(QRgb *rgbArr, quint64 alphas)
{
    setAlphaDXT45Helper<Five>(rgbArr, alphas);
}

template <>
inline void setAlphaDXT<RXGB>(QRgb *rgbArr, quint64 alphas)
{
    setAlphaDXT45Helper<Five>(rgbArr, alphas);
}

static inline QRgb invertRXGBColors(QRgb pixel)
{
    return qRgb(qAlpha(pixel), qGreen(pixel), qBlue(pixel));
}

template <DXTVersions version>
static QImage readDXT(QDataStream &s, quint32 width, quint32 height)
{
    QImage::Format format = (version == Two || version == Four) ?
                QImage::Format_ARGB32_Premultiplied : QImage::Format_ARGB32;

    QImage image(width, height, format);

    for (quint32 i = 0; i < height; i += 4) {
        for (quint32 j = 0; j < width; j += 4) {
            quint64 alpha = 0;
            quint16 c0, c1;
            quint32 table;
            if (version != One)
                s >> alpha;
            s >> c0;
            s >> c1;
            s >> table;

            QRgb arr[16];

            DXTFillColors(arr, c0, c1, table, version == One && c0 <= c1);
            setAlphaDXT<version>(arr, alpha);

            const quint32 kMax = qMin<quint32>(4, height - i);
            const quint32 lMax = qMin<quint32>(4, width - j);
            for (quint32 k = 0; k < kMax; k++) {
                QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(i + k));
                for (quint32 l = 0; l < lMax; l++) {
                    QRgb pixel = arr[k * 4 + l];
                    if (version == RXGB)
                        pixel = invertRXGBColors(pixel);

                    line[j + l] = pixel;
                }
            }
        }
    }
    return image;
}

static inline QImage readDXT1(QDataStream &s, quint32 width, quint32 height)
{
    return readDXT<One>(s, width, height);
}

static inline QImage readDXT2(QDataStream &s, quint32 width, quint32 height)
{
    return readDXT<Two>(s, width, height);
}

static inline QImage readDXT3(QDataStream &s, quint32 width, quint32 height)
{
    return readDXT<Three>(s, width, height);
}

static inline QImage readDXT4(QDataStream &s, quint32 width, quint32 height)
{
    return readDXT<Four>(s, width, height);
}

static inline QImage readDXT5(QDataStream &s, quint32 width, quint32 height)
{
    return readDXT<Five>(s, width, height);
}

static inline QImage readRXGB(QDataStream &s, quint32 width, quint32 height)
{
    return readDXT<RXGB>(s, width, height);
}

static QImage readATI2(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_RGB32);

    for (quint32 i = 0; i < height; i += 4) {
        for (quint32 j = 0; j < width; j += 4) {
            quint64 alpha1;
            quint64 alpha2;
            s >> alpha1;
            s >> alpha2;

            QRgb arr[16];
            memset(arr, 0, sizeof(QRgb) * 16);
            setAlphaDXT<Five>(arr, alpha1);
            for (int k = 0; k < 16; ++k) {
                quint8 a = qAlpha(arr[k]);
                arr[k] = qRgba(0, 0, a, 0);
            }
            setAlphaDXT<Five>(arr, alpha2);

            const quint32 kMax = qMin<quint32>(4, height - i);
            const quint32 lMax = qMin<quint32>(4, width - j);
            for (quint32 k = 0; k < kMax; k++) {
                QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(i + k));
                for (quint32 l = 0; l < lMax; l++) {
                    QRgb pixel = arr[k * 4 + l];
                    const quint8 nx = qAlpha(pixel);
                    const quint8 ny = qBlue(pixel);
                    const quint8 nz = getNormalZ(nx, ny);
                    line[j + l] = qRgb(nx, ny, nz);
                }
            }
        }
    }
    return image;
}

static QImage readUnsignedImage(QDataStream &s, const DDSHeader &dds, quint32 width, quint32 height, bool hasAlpha)
{
    quint32 flags = dds.pixelFormat.flags;

    quint32 masks[ColorCount];
    quint8 shifts[ColorCount];
    quint8 bits[ColorCount];
    masks[Red] = dds.pixelFormat.rBitMask;
    masks[Green] = dds.pixelFormat.gBitMask;
    masks[Blue] = dds.pixelFormat.bBitMask;
    masks[Alpha] = hasAlpha ? dds.pixelFormat.aBitMask : 0;
    for (int i = 0; i < ColorCount; ++i) {
        shifts[i] = maskToShift(masks[i]);
        bits[i] = maskLength(masks[i]);

        // move mask to the left
        if (bits[i] <= 8)
            masks[i] = (masks[i] >> shifts[i]) << (8 - bits[i]);
    }

    const QImage::Format format = hasAlpha ? QImage::Format_ARGB32 : QImage::Format_RGB32;

    QImage image(width, height, format);

    for (quint32 y = 0; y < height; y++) {
        for (quint32 x = 0; x < width; x++) {
            QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));

            quint32 value = readValue(s, dds.pixelFormat.rgbBitCount);
            quint8 colors[ColorCount];

            for (int c = 0; c < ColorCount; ++c) {
                if (bits[c] > 8) {
                    // truncate unneseccary bits
                    colors[c] = (value & masks[c]) >> shifts[c] >> (bits[c] - 8);
                } else {
                    // move color to the left
                    quint8 color = value >> shifts[c] << (8 - bits[c]) & masks[c];
                    if (masks[c])
                        colors[c] = color * 0xff / masks[c];
                    else
                        colors[c] = 0;
                }
            }

            if (flags & DDSPixelFormat::FlagLuminance)
                line[x] = qRgba(colors[Red], colors[Red], colors[Red], colors[Alpha]);
            else if (flags & DDSPixelFormat::FlagYUV)
                line[x] = yuv2rgb(colors[Red], colors[Green], colors[Blue]);
            else
                line[x] = qRgba(colors[Red], colors[Green], colors[Blue], colors[Alpha]);
        }
    }

    return image;
}

static double readFloat16(QDataStream &s)
{
    quint16 value;
    s >> value;

    double sign = (value & 0x8000) == 0x8000 ? -1.0 : 1.0;
    qint8 exp = (value & 0x7C00) >> 10;
    quint16 fraction = value & 0x3FF;

    if (exp == 0)
        return sign * std::pow(2.0, -14.0) * fraction / 1024.0;
    else
        return sign * std::pow(2.0, exp - 15) * (1 + fraction / 1024.0);
}

static inline float readFloat32(QDataStream &s)
{
    Q_ASSERT(sizeof(float) == 4);
    float value;
    // TODO: find better way to avoid setting precision each time
    QDataStream::FloatingPointPrecision precision = s.floatingPointPrecision();
    s.setFloatingPointPrecision(QDataStream::SinglePrecision);
    s >> value;
    s.setFloatingPointPrecision(precision);
    return value;
}

static QImage readR16F(QDataStream &s, const quint32 width, const quint32 height)
{
    QImage image(width, height, QImage::Format_RGB32);

    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            quint8 r = readFloat16(s) * 255;
            line[x] = qRgba(r, 0, 0, 0);
        }
    }

    return image;
}

static QImage readRG16F(QDataStream &s, const quint32 width, const quint32 height)
{
    QImage image(width, height, QImage::Format_RGB32);

    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            quint8 r = readFloat16(s) * 255;
            quint8 g = readFloat16(s) * 255;
            line[x] = qRgba(r, g, 0, 0);
        }
    }

    return image;
}

static QImage readARGB16F(QDataStream &s, const quint32 width, const quint32 height)
{
    QImage image(width, height, QImage::Format_ARGB32);

    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            quint8 colors[ColorCount];
            for (int c = 0; c < ColorCount; ++c)
                colors[c] = readFloat16(s) * 255;

            line[x] = qRgba(colors[Red], colors[Green], colors[Blue], colors[Alpha]);
        }
    }

    return image;
}

static QImage readR32F(QDataStream &s, const quint32 width, const quint32 height)
{
    QImage image(width, height, QImage::Format_RGB32);

    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            quint8 r = readFloat32(s) * 255;
            line[x] = qRgba(r, 0, 0, 0);
        }
    }

    return image;
}

static QImage readRG32F(QDataStream &s, const quint32 width, const quint32 height)
{
    QImage image(width, height, QImage::Format_RGB32);

    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            quint8 r = readFloat32(s) * 255;
            quint8 g = readFloat32(s) * 255;
            line[x] = qRgba(r, g, 0, 0);
        }
    }

    return image;
}

static QImage readARGB32F(QDataStream &s, const quint32 width, const quint32 height)
{
    QImage image(width, height, QImage::Format_ARGB32);

    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            quint8 colors[ColorCount];
            for (int c = 0; c < ColorCount; ++c)
                colors[c] = readFloat32(s) * 255;
            line[x] = qRgba(colors[Red], colors[Green], colors[Blue], colors[Alpha]);
        }
    }

    return image;
}

static QImage readQ16W16V16U16(QDataStream &s, const quint32 width, const quint32 height)
{
    QImage image(width, height, QImage::Format_ARGB32);

    quint8 colors[ColorCount];
    qint16 tmp;
    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            for (int i = 0; i < ColorCount; i++) {
                s >> tmp;
                colors[i] = (tmp + 0x7FFF) >> 8;
            }
            line[x] = qRgba(colors[Red], colors[Green], colors[Blue], colors[Alpha]);
        }
    }

    return image;
}

static QImage readCxV8U8(QDataStream &s, const quint32 width, const quint32 height)
{
    QImage image(width, height, QImage::Format_RGB32);

    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            qint8 v, u;
            s >> v >> u;

            const quint8 vn = v + 128;
            const quint8 un = u + 128;
            const quint8 c = getNormalZ(vn, un);

            line[x] = qRgb(vn, un, c);
        }
    }

    return image;
}

static QImage readPalette8Image(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_Indexed8);
    for (int i = 0; i < 256; ++i) {
        quint8 r, g, b, a;
        s >> r >> g >> b >> a;
        image.setColor(i, qRgba(r, g, b, a));
    }

    for (quint32 y = 0; y < height; y++) {
        for (quint32 x = 0; x < width; x++) {
            quint8 index;
            s >> index;
            image.setPixel(x, y, index);
        }
    }

    return image;
}

static QImage readPalette4Image(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_Indexed8);
    for (int i = 0; i < 16; ++i) {
        quint8 r, g, b, a;
        s >> r >> g >> b >> a;
        image.setColor(i, qRgba(r, g, b, a));
    }

    for (quint32 y = 0; y < height; y++) {
        quint8 index;
        for (quint32 x = 0; x < width - 1; ) {
            s >> index;
            image.setPixel(x++, y, (index & 0x0f) >> 0);
            image.setPixel(x++, y, (index & 0xf0) >> 4);
        }
        if (width % 2 == 1) {
            s >> index;
            image.setPixel(width - 1, y, (index & 0x0f) >> 0);
        }
    }

    return image;
}

static QImage readARGB16(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_ARGB32);

    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            quint8 colors[ColorCount];
            for (int i = 0; i < ColorCount; ++i) {
                quint16 color;
                s >> color;
                colors[i] = quint8(color >> 8);
            }
            line[x] = qRgba(colors[Red], colors[Green], colors[Blue], colors[Alpha]);
        }
    }

    return image;
}

static QImage readV8U8(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_RGB32);

    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            qint8 v, u;
            s >> v >> u;
            line[x] = qRgb(v + 128, u + 128, 255);
        }
    }

    return image;
}

static QImage readL6V5U5(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_ARGB32);

    quint16 tmp;
    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            s >> tmp;
            quint8 r = qint8((tmp & 0x001f) >> 0) * 0xff/0x1f + 128;
            quint8 g = qint8((tmp & 0x03e0) >> 5) * 0xff/0x1f + 128;
            quint8 b = quint8((tmp & 0xfc00) >> 10) * 0xff/0x3f;
            line[x] = qRgba(r, g, 0xff, b);
        }
    }
    return image;
}

static QImage readX8L8V8U8(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_ARGB32);

    quint8 a, l;
    qint8 v, u;
    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            s >> v >> u >> a >> l;
            line[x] = qRgba(v + 128, u + 128, 255, a);
        }
    }

    return image;
}

static QImage readQ8W8V8U8(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_ARGB32);

    quint8 colors[ColorCount];
    qint8 tmp;
    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            for (int i = 0; i < ColorCount; i++) {
                s >> tmp;
                colors[i] = tmp + 128;
            }
            line[x] = qRgba(colors[Red], colors[Green], colors[Blue], colors[Alpha]);
        }
    }

    return image;
}

static QImage readV16U16(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_RGB32);

    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            qint16 v, u;
            s >> v >> u;
            v = (v + 0x8000) >> 8;
            u = (u + 0x8000) >> 8;
            line[x] = qRgb(v, u, 255);
        }
    }

    return image;
}

static QImage readA2W10V10U10(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_ARGB32);

    quint32 tmp;
    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            s >> tmp;
            quint8 r = qint8((tmp & 0x3ff00000) >> 20 >> 2) + 128;
            quint8 g = qint8((tmp & 0x000ffc00) >> 10 >> 2) + 128;
            quint8 b = qint8((tmp & 0x000003ff) >> 0 >> 2) + 128;
            quint8 a = 0xff * ((tmp & 0xc0000000) >> 30) / 3;
            // dunno why we should swap b and r here
            std::swap(b, r);
            line[x] = qRgba(r, g, b, a);
        }
    }

    return image;
}

static QImage readUYVY(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_RGB32);

    quint8 uyvy[4];
    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width - 1; ) {
            s >> uyvy[0] >> uyvy[1] >> uyvy[2] >> uyvy[3];
            line[x++] = yuv2rgb(uyvy[1], uyvy[0], uyvy[2]);
            line[x++] = yuv2rgb(uyvy[3], uyvy[0], uyvy[2]);
        }
        if (width % 2 == 1) {
            s >> uyvy[0] >> uyvy[1] >> uyvy[2] >> uyvy[3];
            line[width - 1] = yuv2rgb(uyvy[1], uyvy[0], uyvy[2]);
        }
    }

    return image;
}

static QImage readR8G8B8G8(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_RGB32);
    quint8 rgbg[4];
    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width - 1; ) {
            s >> rgbg[1] >> rgbg[0] >> rgbg[3] >> rgbg[2];
            line[x++] = qRgb(rgbg[0], rgbg[1], rgbg[2]);
            line[x++] = qRgb(rgbg[0], rgbg[3], rgbg[2]);
        }
        if (width % 2 == 1) {
            s >> rgbg[1] >> rgbg[0] >> rgbg[3] >> rgbg[2];
            line[width - 1] = qRgb(rgbg[0], rgbg[1], rgbg[2]);
        }
    }

    return image;
}

static QImage readYUY2(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_RGB32);

    quint8 yuyv[4];
    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width - 1; ) {
            s >> yuyv[0] >> yuyv[1] >> yuyv[2] >> yuyv[3];
            line[x++] = yuv2rgb(yuyv[0], yuyv[1], yuyv[3]);
            line[x++] = yuv2rgb(yuyv[2], yuyv[1], yuyv[3]);
        }
        if (width % 2 == 1) {
            s >> yuyv[0] >> yuyv[1] >> yuyv[2] >> yuyv[3];
            line[width - 1] = yuv2rgb(yuyv[2], yuyv[1], yuyv[3]);
        }
    }

    return image;
}

static QImage readG8R8G8B8(QDataStream &s, quint32 width, quint32 height)
{
    QImage image(width, height, QImage::Format_RGB32);
    quint8 grgb[4];
    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width - 1; ) {
            s >> grgb[1] >> grgb[0] >> grgb[3] >> grgb[2];
            line[x++] = qRgb(grgb[1], grgb[0], grgb[3]);
            line[x++] = qRgb(grgb[1], grgb[2], grgb[3]);
        }
        if (width % 2 == 1) {
            s >> grgb[1] >> grgb[0] >> grgb[3] >> grgb[2];
            line[width - 1] = qRgb(grgb[1], grgb[0], grgb[3]);
        }
    }

    return image;
}

static QImage readA2R10G10B10(QDataStream &s, const DDSHeader &dds, quint32 width, quint32 height)
{
    QImage image = readUnsignedImage(s, dds, width, height, true);
    for (quint32 y = 0; y < height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (quint32 x = 0; x < width; x++) {
            QRgb pixel = image.pixel(x, y);
            line[x] = qRgba(qBlue(pixel), qGreen(pixel), qRed(pixel), qAlpha(pixel));
        }
    }
    return image;
}

static QImage readLayer(QDataStream &s, const DDSHeader &dds, const int format, quint32 width, quint32 height)
{
    if (width * height == 0)
        return QImage();

    switch (format) {
    case FormatR8G8B8:
    case FormatX8R8G8B8:
    case FormatR5G6B5:
    case FormatR3G3B2:
    case FormatX1R5G5B5:
    case FormatX4R4G4B4:
    case FormatX8B8G8R8:
    case FormatG16R16:
    case FormatL8:
    case FormatL16:
        return readUnsignedImage(s, dds, width, height, false);
    case FormatA8R8G8B8:
    case FormatA1R5G5B5:
    case FormatA4R4G4B4:
    case FormatA8:
    case FormatA8R3G3B2:
    case FormatA8B8G8R8:
    case FormatA8L8:
    case FormatA4L4:
        return readUnsignedImage(s, dds, width, height, true);
    case FormatA2R10G10B10:
    case FormatA2B10G10R10:
        return readA2R10G10B10(s, dds, width, height);
    case FormatP8:
    case FormatA8P8:
        return readPalette8Image(s, width, height);
    case FormatP4:
    case FormatA4P4:
        return readPalette4Image(s, width, height);
    case FormatA16B16G16R16:
        return readARGB16(s, width, height);
    case FormatV8U8:
        return readV8U8(s, width, height);
    case FormatL6V5U5:
        return readL6V5U5(s, width, height);
    case FormatX8L8V8U8:
        return readX8L8V8U8(s, width, height);
    case FormatQ8W8V8U8:
        return readQ8W8V8U8(s, width, height);
    case FormatV16U16:
        return readV16U16(s, width, height);
    case FormatA2W10V10U10:
        return readA2W10V10U10(s, width, height);
    case FormatUYVY:
        return readUYVY(s, width, height);
    case FormatR8G8B8G8:
        return readR8G8B8G8(s, width, height);
    case FormatYUY2:
        return readYUY2(s, width, height);
    case FormatG8R8G8B8:
        return readG8R8G8B8(s, width, height);
    case FormatDXT1:
        return readDXT1(s, width, height);
    case FormatDXT2:
        return readDXT2(s, width, height);
    case FormatDXT3:
        return readDXT3(s, width, height);
    case FormatDXT4:
        return readDXT4(s, width, height);
    case FormatDXT5:
        return readDXT5(s, width, height);
    case FormatRXGB:
        return readRXGB(s, width, height);
    case FormatATI2:
        return readATI2(s, width, height);
    case FormatR16F:
        return readR16F(s, width, height);
    case FormatG16R16F:
        return readRG16F(s, width, height);
    case FormatA16B16G16R16F:
        return readARGB16F(s, width, height);
    case FormatR32F:
        return readR32F(s, width, height);
    case FormatG32R32F:
        return readRG32F(s, width, height);
    case FormatA32B32G32R32F:
        return readARGB32F(s, width, height);
    case FormatD16Lockable:
    case FormatD32:
    case FormatD15S1:
    case FormatD24S8:
    case FormatD24X8:
    case FormatD24X4S4:
    case FormatD16:
    case FormatD32FLockable:
    case FormatD24FS8:
    case FormatD32Lockable:
    case FormatS8Lockable:
    case FormatVertexData:
    case FormatIndex16:
    case FormatIndex32:
        break;
    case FormatQ16W16V16U16:
        return readQ16W16V16U16(s, width, height);
    case FormatMulti2ARGB8:
        break;
    case FormatCxV8U8:
        return readCxV8U8(s, width, height);
    case FormatA1:
    case FormatA2B10G10R10_XR_BIAS:
    case FormatBinaryBuffer:
    case FormatLast:
        break;
    }

    return QImage();
}

static inline QImage readTexture(QDataStream &s, const DDSHeader &dds, const int format, const int mipmapLevel)
{
    quint32 width = dds.width / (1 << mipmapLevel);
    quint32 height = dds.height / (1 << mipmapLevel);
    return readLayer(s, dds, format, width, height);
}

static qint64 mipmapSize(const DDSHeader &dds, const int format, const int level)
{
    quint32 w = dds.width/(1 << level);
    quint32 h = dds.height/(1 << level);

    switch (format) {
    case FormatR8G8B8:
    case FormatX8R8G8B8:
    case FormatR5G6B5:
    case FormatX1R5G5B5:
    case FormatX4R4G4B4:
    case FormatX8B8G8R8:
    case FormatG16R16:
    case FormatL8:
    case FormatL16:
        return w * h * dds.pixelFormat.rgbBitCount / 8;
    case FormatA8R8G8B8:
    case FormatA1R5G5B5:
    case FormatA4R4G4B4:
    case FormatA8:
    case FormatA8R3G3B2:
    case FormatA2B10G10R10:
    case FormatA8B8G8R8:
    case FormatA2R10G10B10:
    case FormatA8L8:
    case FormatA4L4:
        return w * h * dds.pixelFormat.rgbBitCount / 8;
    case FormatP8:
        return 256 + w * h * 8;
    case FormatA16B16G16R16:
        return w * h * 4 * 2;
    case FormatA8P8:
        break;
    case FormatV8U8:
    case FormatL6V5U5:
        return w * h * 2;
    case FormatX8L8V8U8:
    case FormatQ8W8V8U8:
    case FormatV16U16:
    case FormatA2W10V10U10:
        return w * h * 4;
    case FormatUYVY:
    case FormatR8G8B8G8:
    case FormatYUY2:
    case FormatG8R8G8B8:
        return w * h * 2;
    case FormatDXT1:
        return ((w + 3)/4) * ((h + 3)/4) * 8;
    case FormatDXT2:
    case FormatDXT3:
    case FormatDXT4:
    case FormatDXT5:
        return ((w + 3)/4) * ((h + 3)/4) * 16;
    case FormatD16Lockable:
    case FormatD32:
    case FormatD15S1:
    case FormatD24S8:
    case FormatD24X8:
    case FormatD24X4S4:
    case FormatD16:
    case FormatD32FLockable:
    case FormatD24FS8:
    case FormatD32Lockable:
    case FormatS8Lockable:
    case FormatVertexData:
    case FormatIndex16:
    case FormatIndex32:
        break;
    case FormatQ16W16V16U16:
        return w * h * 4 * 2;
    case FormatMulti2ARGB8:
        break;
    case FormatR16F:
        return w * h * 1 * 2;
    case FormatG16R16F:
        return w * h * 2 * 2;
    case FormatA16B16G16R16F:
        return w * h * 4 * 2;
    case FormatR32F:
        return w * h * 1 * 4;
    case FormatG32R32F:
        return w * h * 2 * 4;
    case FormatA32B32G32R32F:
        return w * h * 4 * 4;
    case FormatCxV8U8:
        return w * h * 2;
    case FormatA1:
    case FormatA2B10G10R10_XR_BIAS:
    case FormatBinaryBuffer:
    case FormatLast:
        break;
    }

    return 0;
}

static qint64 mipmapOffset(const DDSHeader &dds, const int format, const int level)
{
    qint64 result = 0;
    for (int i = 0; i < level; ++i)
        result += mipmapSize(dds, format, i);
    return result;
}

static QImage readCubeMap(QDataStream &s, const DDSHeader &dds, const int fmt)
{
    QImage::Format format = hasAlpha(dds) ? QImage::Format_ARGB32 : QImage::Format_RGB32;
    QImage image(4 * dds.width, 3 * dds.height, format);

    image.fill(0);

    for (int i = 0; i < 6; i++) {
        if (!(dds.caps2 & faceFlags[i]))
            continue; // Skip face.

        const QImage face = readLayer(s, dds, fmt, dds.width, dds.height);

        // Compute face offsets.
        int offset_x = faceOffsets[i].x * dds.width;
        int offset_y = faceOffsets[i].y * dds.height;

        // Copy face on the image.
        for (quint32 y = 0; y < dds.height; y++) {
            const QRgb *src = reinterpret_cast<const QRgb *>(face.scanLine(y));
            QRgb *dst = reinterpret_cast<QRgb *>(image.scanLine(y + offset_y)) + offset_x;
            memcpy(dst, src, sizeof(QRgb) * dds.width);
        }
    }

    return image;
}

static QByteArray formatName(int format)
{
    for (size_t i = 0; i < formatNamesSize; ++i) {
        if (formatNames[i].format == format)
            return formatNames[i].name;
    }

    return formatNames[0].name;
}

static int formatByName(const QByteArray &name)
{
    const QByteArray loweredName = name.toLower();
    for (size_t i = 0; i < formatNamesSize; ++i) {
        if (QByteArray(formatNames[i].name).toLower() == loweredName)
            return formatNames[i].format;
    }

    return FormatUnknown;
}

QDDSHandler::QDDSHandler() :
    m_header(),
    m_format(FormatA8R8G8B8),
    m_header10(),
    m_currentImage(0),
    m_scanState(ScanNotScanned)
{
}

QByteArray QDDSHandler::name() const
{
    return QByteArrayLiteral("dds");
}

bool QDDSHandler::canRead() const
{
    if (m_scanState == ScanNotScanned && !canRead(device()))
        return false;

    if (m_scanState != ScanError) {
        setFormat(QByteArrayLiteral("dds"));
        return true;
    }

    return false;
}

bool QDDSHandler::read(QImage *outImage)
{
    if (!ensureScanned() || device()->isSequential())
        return false;

    qint64 pos = headerSize + mipmapOffset(m_header, m_format, m_currentImage);
    if (!device()->seek(pos))
        return false;
    QDataStream s(device());
    s.setByteOrder(QDataStream::LittleEndian);

    QImage image = isCubeMap(m_header) ?
                readCubeMap(s, m_header, m_format) :
                readTexture(s, m_header, m_format, m_currentImage);

    bool ok = s.status() == QDataStream::Ok && !image.isNull();
    if (ok)
        *outImage = image;
    return ok;
}

bool QDDSHandler::write(const QImage &outImage)
{
    if (m_format != FormatA8R8G8B8) {
        qWarning() << "Format" << formatName(m_format) << "is not supported";
        return false;
    }

    const QImage image = outImage.convertToFormat(QImage::Format_ARGB32);

    QDataStream s(device());
    s.setByteOrder(QDataStream::LittleEndian);

    DDSHeader dds;
    // Filling header
    dds.magic = ddsMagic;
    dds.size = 124;
    dds.flags = DDSHeader::FlagCaps | DDSHeader::FlagHeight |
                DDSHeader::FlagWidth | DDSHeader::FlagPixelFormat;
    dds.height = image.height();
    dds.width = image.width();
    dds.pitchOrLinearSize = 128;
    dds.depth = 0;
    dds.mipMapCount = 0;
    for (int i = 0; i < DDSHeader::ReservedCount; i++)
        dds.reserved1[i] = 0;
    dds.caps = DDSHeader::CapsTexture;
    dds.caps2 = 0;
    dds.caps3 = 0;
    dds.caps4 = 0;
    dds.reserved2 = 0;

    // Filling pixelformat
    dds.pixelFormat.size = 32;
    dds.pixelFormat.flags = DDSPixelFormat::FlagAlphaPixels | DDSPixelFormat::FlagRGB;
    dds.pixelFormat.fourCC = 0;
    dds.pixelFormat.rgbBitCount = 32;
    dds.pixelFormat.aBitMask = 0xff000000;
    dds.pixelFormat.rBitMask = 0x00ff0000;
    dds.pixelFormat.gBitMask = 0x0000ff00;
    dds.pixelFormat.bBitMask = 0x000000ff;

    s << dds;
    for (int height = 0; height < image.height(); height++) {
        for (int width = 0; width < image.width(); width++) {
            QRgb pixel = image.pixel(width, height);
            quint32 color;
            quint8 alpha = qAlpha(pixel);
            quint8 red = qRed(pixel);
            quint8 green = qGreen(pixel);
            quint8 blue = qBlue(pixel);
            color = (alpha << 24) + (red << 16) + (green << 8) + blue;
            s << color;
        }
    }

    return true;
}

QVariant QDDSHandler::option(QImageIOHandler::ImageOption option) const
{
    if (!supportsOption(option) || !ensureScanned())
        return QVariant();

    switch (option) {
    case QImageIOHandler::Size:
        return QSize(m_header.width, m_header.height);
    case QImageIOHandler::SubType:
        return formatName(m_format);
    case QImageIOHandler::SupportedSubTypes:
        return QVariant::fromValue(QList<QByteArray>() << formatName(FormatA8R8G8B8));
    default:
        break;
    }

    return QVariant();
}

void QDDSHandler::setOption(QImageIOHandler::ImageOption option, const QVariant &value)
{
    if (option == QImageIOHandler::SubType) {
        const QByteArray subType = value.toByteArray();
        m_format = formatByName(subType.toUpper());
        if (m_format == FormatUnknown)
            qWarning() << "unknown format" << subType;
    }
}

bool QDDSHandler::supportsOption(QImageIOHandler::ImageOption option) const
{
    return (option == QImageIOHandler::Size)
            || (option == QImageIOHandler::SubType)
            || (option == QImageIOHandler::SupportedSubTypes);
}

int QDDSHandler::imageCount() const
{
    if (!ensureScanned())
        return 0;

    return qMax<quint32>(1, m_header.mipMapCount);
}

bool QDDSHandler::jumpToImage(int imageNumber)
{
    if (imageNumber >= imageCount())
        return false;

    m_currentImage = imageNumber;
    return true;
}

bool QDDSHandler::canRead(QIODevice *device)
{
    if (!device) {
        qWarning() << "DDSHandler::canRead() called with no device";
        return false;
    }

    if (device->isSequential())
        return false;

    return device->peek(4) == QByteArrayLiteral("DDS ");
}

bool QDDSHandler::ensureScanned() const
{
    if (m_scanState != ScanNotScanned)
        return m_scanState == ScanSuccess;

    m_scanState = ScanError;

    QDDSHandler *that = const_cast<QDDSHandler *>(this);
    that->m_format = FormatUnknown;

    if (device()->isSequential()) {
        qWarning() << "Sequential devices are not supported";
        return false;
    }

    qint64 oldPos = device()->pos();
    device()->seek(0);

    QDataStream s(device());
    s.setByteOrder(QDataStream::LittleEndian);
    s >> that->m_header;
    if (m_header.pixelFormat.fourCC == dx10Magic)
        s >> that->m_header10;

    device()->seek(oldPos);

    if (s.status() != QDataStream::Ok)
        return false;

    if (!verifyHeader(m_header))
        return false;

    that->m_format = getFormat(m_header);
    if (that->m_format == FormatUnknown)
        return false;

    m_scanState = ScanSuccess;
    return true;
}

bool QDDSHandler::verifyHeader(const DDSHeader &dds) const
{
    quint32 flags = dds.flags;
    quint32 requiredFlags = DDSHeader::FlagCaps | DDSHeader::FlagHeight
            | DDSHeader::FlagWidth | DDSHeader::FlagPixelFormat;
    if ((flags & requiredFlags) != requiredFlags) {
        qWarning() << "Wrong dds.flags - not all required flags present. "
                      "Actual flags :" << flags;
        return false;
    }

    if (dds.size != ddsSize) {
        qWarning() << "Wrong dds.size: actual =" << dds.size
                   << "expected =" << ddsSize;
        return false;
    }

    if (dds.pixelFormat.size != pixelFormatSize) {
        qWarning() << "Wrong dds.pixelFormat.size: actual =" << dds.pixelFormat.size
                   << "expected =" << pixelFormatSize;
        return false;
    }

    if (dds.width > INT_MAX || dds.height > INT_MAX) {
        qWarning() << "Can't read image with w/h bigger than INT_MAX";
        return false;
    }

    return true;
}

QT_END_NAMESPACE

#endif // QT_NO_DATASTREAM
