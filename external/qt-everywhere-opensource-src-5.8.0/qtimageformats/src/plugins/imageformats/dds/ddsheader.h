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

#ifndef DDSHEADER_H
#define DDSHEADER_H

#include <QtCore/QDataStream>

#ifndef QT_NO_DATASTREAM

QT_BEGIN_NAMESPACE

enum Format {
    FormatUnknown              = 0,

    FormatR8G8B8               = 20,
    FormatA8R8G8B8             = 21,
    FormatX8R8G8B8             = 22,
    FormatR5G6B5               = 23,
    FormatX1R5G5B5             = 24,
    FormatA1R5G5B5             = 25,
    FormatA4R4G4B4             = 26,
    FormatR3G3B2               = 27,
    FormatA8                   = 28,
    FormatA8R3G3B2             = 29,
    FormatX4R4G4B4             = 30,
    FormatA2B10G10R10          = 31,
    FormatA8B8G8R8             = 32,
    FormatX8B8G8R8             = 33,
    FormatG16R16               = 34,
    FormatA2R10G10B10          = 35,
    FormatA16B16G16R16         = 36,

    FormatA8P8                 = 40,
    FormatP8                   = 41,

    FormatL8                   = 50,
    FormatA8L8                 = 51,
    FormatA4L4                 = 52,

    FormatV8U8                 = 60,
    FormatL6V5U5               = 61,
    FormatX8L8V8U8             = 62,
    FormatQ8W8V8U8             = 63,
    FormatV16U16               = 64,
    FormatA2W10V10U10          = 67,

    FormatUYVY                 = 0x59565955, // "UYVY"
    FormatR8G8B8G8             = 0x47424752, // "RGBG"
    FormatYUY2                 = 0x32595559, // "YUY2"
    FormatG8R8G8B8             = 0x42475247, // "GRGB"
    FormatDXT1                 = 0x31545844, // "DXT1"
    FormatDXT2                 = 0x32545844, // "DXT2"
    FormatDXT3                 = 0x33545844, // "DXT3"
    FormatDXT4                 = 0x34545844, // "DXT4"
    FormatDXT5                 = 0x35545844, // "DXT5"
    FormatRXGB                 = 0x42475852, // "RXGB"
    FormatATI2                 = 0x32495441, // "ATI2"

    FormatD16Lockable         = 70,
    FormatD32                  = 71,
    FormatD15S1                = 73,
    FormatD24S8                = 75,
    FormatD24X8                = 77,
    FormatD24X4S4              = 79,
    FormatD16                  = 80,

    FormatD32FLockable        = 82,
    FormatD24FS8               = 83,

    FormatD32Lockable         = 84,
    FormatS8Lockable          = 85,

    FormatL16                  = 81,

    FormatVertexData           =100,
    FormatIndex16              =101,
    FormatIndex32              =102,

    FormatQ16W16V16U16         = 110,

    FormatMulti2ARGB8         = 0x3154454d, // "MET1"

    FormatR16F                 = 111,
    FormatG16R16F              = 112,
    FormatA16B16G16R16F        = 113,

    FormatR32F                 = 114,
    FormatG32R32F              = 115,
    FormatA32B32G32R32F        = 116,

    FormatCxV8U8               = 117,

    FormatA1                   = 118,
    FormatA2B10G10R10_XR_BIAS  = 119,
    FormatBinaryBuffer         = 199,

    FormatP4,
    FormatA4P4,

    FormatLast                 = 0x7fffffff
};

struct DDSPixelFormat
{
    enum DDSPixelFormatFlags {
        FlagAlphaPixels     = 0x00000001,
        FlagAlpha           = 0x00000002,
        FlagFourCC          = 0x00000004,
        FlagPaletteIndexed4 = 0x00000008,
        FlagPaletteIndexed8 = 0x00000020,
        FlagRGB             = 0x00000040,
        FlagYUV             = 0x00000200,
        FlagLuminance       = 0x00020000,
        FlagNormal          = 0x00080000,
        FlagRGBA = FlagAlphaPixels | FlagRGB,
        FlagLA = FlagAlphaPixels | FlagLuminance
    };

    quint32 size;
    quint32 flags;
    quint32 fourCC;
    quint32 rgbBitCount;
    quint32 rBitMask;
    quint32 gBitMask;
    quint32 bBitMask;
    quint32 aBitMask;
};

QDataStream &operator>>(QDataStream &s, DDSPixelFormat &pixelFormat);
QDataStream &operator<<(QDataStream &s, const DDSPixelFormat &pixelFormat);

struct DDSHeader
{
    enum DDSFlags {
        FlagCaps        = 0x000001,
        FlagHeight      = 0x000002,
        FlagWidth       = 0x000004,
        FlagPitch       = 0x000008,
        FlagPixelFormat = 0x001000,
        FlagMipmapCount = 0x020000,
        FlagLinearSize  = 0x080000,
        FlagDepth       = 0x800000
    };

    enum DDSCapsFlags {
        CapsComplex = 0x000008,
        CapsTexture = 0x001000,
        CapsMipmap  = 0x400000
    };

    enum DDSCaps2Flags {
        Caps2CubeMap          = 0x0200,
        Caps2CubeMapPositiveX = 0x0400,
        Caps2CubeMapNegativeX = 0x0800,
        Caps2CubeMapPositiveY = 0x1000,
        Caps2CubeMapNegativeY = 0x2000,
        Caps2CubeMapPositiveZ = 0x4000,
        Caps2CubeMapNegativeZ = 0x8000,
        Caps2Volume           = 0x200000
    };

    enum { ReservedCount = 11 };

    quint32 magic;
    quint32 size;
    quint32 flags;
    quint32 height;
    quint32 width;
    quint32 pitchOrLinearSize;
    quint32 depth;
    quint32 mipMapCount;
    quint32 reserved1[ReservedCount];
    DDSPixelFormat pixelFormat;
    quint32 caps;
    quint32 caps2;
    quint32 caps3;
    quint32 caps4;
    quint32 reserved2;
};

QDataStream &operator>>(QDataStream &s, DDSHeader &header);
QDataStream &operator<<(QDataStream &s, const DDSHeader &header);

struct DDSHeaderDX10
{
    quint32 dxgiFormat;
    quint32 resourceDimension;
    quint32 miscFlag;
    quint32 arraySize;
    quint32 reserved;
};

QDataStream &operator>>(QDataStream &s, DDSHeaderDX10 &header);
QDataStream &operator<<(QDataStream &s, const DDSHeaderDX10 &header);

QT_END_NAMESPACE

#endif // QT_NO_DATASTREAM

#endif // DDSHEADER_H
