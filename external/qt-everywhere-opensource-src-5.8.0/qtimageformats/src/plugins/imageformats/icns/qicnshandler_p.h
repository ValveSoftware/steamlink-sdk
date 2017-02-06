/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Alex Char.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QICNSHANDLER_P_H
#define QICNSHANDLER_P_H

#include <QtGui/qimageiohandler.h>
#include <QtCore/qvector.h>

#ifndef QT_NO_DATASTREAM

#define MAKEOSTYPE(c0,c1,c2,c3) (((quint8)c0 << 24) | ((quint8)c1 << 16) | ((quint8)c2 << 8) | (quint8)c3)

QT_BEGIN_NAMESPACE

struct ICNSBlockHeader
{
    enum OS {
        TypeIcns = MAKEOSTYPE('i', 'c', 'n', 's'), // Icns container magic
        TypeToc  = MAKEOSTYPE('T', 'O', 'C', ' '), // Table of contents
        TypeIcnv = MAKEOSTYPE('i', 'c', 'n', 'V'), // Icon Composer version
        // Legacy:
        TypeClut = MAKEOSTYPE('c', 'l', 'u', 't'), // Color look-up table (pre-OS X resources)
        TypeTile = MAKEOSTYPE('t', 'i', 'l', 'e'), // Container (icon variants)
        TypeOver = MAKEOSTYPE('o', 'v', 'e', 'r'), // Container (icon variants)
        TypeOpen = MAKEOSTYPE('o', 'p', 'e', 'n'), // Container (icon variants)
        TypeDrop = MAKEOSTYPE('d', 'r', 'o', 'p'), // Container (icon variants)
        TypeOdrp = MAKEOSTYPE('o', 'd', 'r', 'p'), // Container (icon variants)
    };

    quint32 ostype;
    quint32 length;
};

struct ICNSEntry
{
    enum Group {
        GroupUnknown    = 0,   // Default for invalid ones
        GroupMini       = 'm', // "mini" (16x12)
        GroupSmall      = 's', // "small" (16x16)
        GroupLarge      = 'l', // "large" (32x32)
        GroupHuge       = 'h', // "huge" (48x48)
        GroupThumbnail  = 't', // "thumbnail" (128x128)
        GroupPortable   = 'p', // "portable"? (Speculation, used for png/jp2)
        GroupCompressed = 'c', // "compressed"? (Speculation, used for png/jp2)
        // Legacy icons:
        GroupICON       = 'N', // "ICON" (32x32)
    };
    enum Depth {
        DepthUnknown    = 0,    // Default for invalid or compressed ones
        DepthMono       = 1,
        Depth4bit       = 4,
        Depth8bit       = 8,
        Depth32bit      = 32
    };
    enum Flags {
        Unknown         = 0x0,              // Default for invalid ones
        IsIcon          = 0x1,              // Contains a raw icon without alpha or compressed icon
        IsMask          = 0x2,              // Contains alpha mask
        IconPlusMask    = IsIcon | IsMask   // Contains raw icon and mask combined in one entry (double size)
    };
    enum Format {
        FormatUnknown   = 0,    // Default for invalid or undetermined ones
        RawIcon,                // Raw legacy icon, uncompressed
        RLE24,                  // Raw 32bit icon, data is compressed
        PNG,                    // Compressed icon in PNG format
        JP2                     // Compressed icon in JPEG2000 format
    };

    quint32 ostype;     // Real OSType
    quint32 variant;    // Virtual OSType: a parent container, zero if parent is icns root
    Group group;        // ASCII character number
    quint32 width;      // For uncompressed icons only, zero for compressed ones for now
    quint32 height;     // For uncompressed icons only, zero for compressed ones fow now
    Depth depth;        // Color depth
    Flags flags;        // Flags to determine the type of entry
    Format dataFormat;  // Format of the image data
    quint32 dataLength; // Length of the image data in bytes
    qint64 dataOffset;  // Offset from the initial position of the file/device

    ICNSEntry() :
        ostype(0), variant(0), group(GroupUnknown), width(0), height(0), depth(DepthUnknown),
        flags(Unknown), dataFormat(FormatUnknown), dataLength(0), dataOffset(0)
    {
    }
};
Q_DECLARE_TYPEINFO(ICNSEntry, Q_MOVABLE_TYPE);

class QICNSHandler : public QImageIOHandler
{
public:
    QICNSHandler();

    bool canRead() const;
    bool read(QImage *image);
    bool write(const QImage &image);

    QByteArray name() const;

    bool supportsOption(ImageOption option) const;
    QVariant option(ImageOption option) const;

    int imageCount() const;
    bool jumpToImage(int imageNumber);
    bool jumpToNextImage();

    static bool canRead(QIODevice *device);

private:
    bool ensureScanned() const;
    bool scanDevice();
    bool addEntry(const ICNSBlockHeader &header, qint64 imgDataOffset, quint32 variant = 0);
    const ICNSEntry &getIconMask(const ICNSEntry &icon) const;

private:
    enum ScanState {
        ScanError       = -1,
        ScanNotScanned  = 0,
        ScanSuccess     = 1,
    };

    int m_currentIconIndex;
    QVector<ICNSEntry> m_icons;
    QVector<ICNSEntry> m_masks;
    ScanState m_state;
};

QT_END_NAMESPACE

#endif // QT_NO_DATASTREAM

#endif /* QICNSHANDLER_P_H */
