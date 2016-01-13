/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2013 Ivan Komissarov.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the DDS plugin in the Qt ImageFormats module.
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

#include "ddsheader.h"

#ifndef QT_NO_DATASTREAM

QT_BEGIN_NAMESPACE

QDataStream &operator>>(QDataStream &s, DDSPixelFormat &pixelFormat)
{
    s >> pixelFormat.size;
    s >> pixelFormat.flags;
    s >> pixelFormat.fourCC;
    s >> pixelFormat.rgbBitCount;
    s >> pixelFormat.rBitMask;
    s >> pixelFormat.gBitMask;
    s >> pixelFormat.bBitMask;
    s >> pixelFormat.aBitMask;
    return s;
}

QDataStream &operator<<(QDataStream &s, const DDSPixelFormat &pixelFormat)
{
    s << pixelFormat.size;
    s << pixelFormat.flags;
    s << pixelFormat.fourCC;
    s << pixelFormat.rgbBitCount;
    s << pixelFormat.rBitMask;
    s << pixelFormat.gBitMask;
    s << pixelFormat.bBitMask;
    s << pixelFormat.aBitMask;
    return s;
}

QDataStream &operator>>(QDataStream &s, DDSHeader &header)
{
    s >> header.magic;
    s >> header.size;
    s >> header.flags;
    s >> header.height;
    s >> header.width;
    s >> header.pitchOrLinearSize;
    s >> header.depth;
    s >> header.mipMapCount;
    for (int i = 0; i < DDSHeader::ReservedCount; i++)
        s >> header.reserved1[i];
    s >> header.pixelFormat;
    s >> header.caps;
    s >> header.caps2;
    s >> header.caps3;
    s >> header.caps4;
    s >> header.reserved2;
    return s;
}

QDataStream &operator<<(QDataStream &s, const DDSHeader &header)
{
    s << header.magic;
    s << header.size;
    s << header.flags;
    s << header.height;
    s << header.width;
    s << header.pitchOrLinearSize;
    s << header.depth;
    s << header.mipMapCount;
    for (int i = 0; i < DDSHeader::ReservedCount; i++)
        s << header.reserved1[i];
    s << header.pixelFormat;
    s << header.caps;
    s << header.caps2;
    s << header.caps3;
    s << header.caps4;
    s << header.reserved2;
    return s;
}

QDataStream &operator>>(QDataStream &s, DDSHeaderDX10 &header)
{
    s >> header.dxgiFormat;
    s >> header.resourceDimension;
    s >> header.miscFlag;
    s >> header.arraySize;
    s >> header.reserved;
    return s;
}

QDataStream &operator<<(QDataStream &s, const DDSHeaderDX10 &header)
{
    s << header.dxgiFormat;
    s << header.resourceDimension;
    s << header.miscFlag;
    s << header.arraySize;
    s << header.reserved;
    return s;
}

QT_END_NAMESPACE

#endif // QT_NO_DATASTREAM
