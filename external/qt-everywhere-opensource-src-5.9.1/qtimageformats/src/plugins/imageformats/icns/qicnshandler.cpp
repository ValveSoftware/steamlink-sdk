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

#include "qicnshandler_p.h"

#include <QtCore/qmath.h>
#include <QtCore/qendian.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qbuffer.h>
#include <QtGui/qimage.h>

#ifndef QT_NO_DATASTREAM

QT_BEGIN_NAMESPACE

static const quint8 ICNSBlockHeaderSize = 8;

static const QRgb ICNSColorTableMono[] = {
    qRgb(0xFF, 0xFF, 0xFF),
    qRgb(0x00, 0x00, 0x00)
};
Q_STATIC_ASSERT(sizeof(ICNSColorTableMono) / sizeof(ICNSColorTableMono[0]) == (1 << ICNSEntry::DepthMono));

static const QRgb ICNSColorTable4bit[] = {
    qRgb(0xFF, 0xFF, 0xFF),
    qRgb(0xFC, 0xF3, 0x05),
    qRgb(0xFF, 0x64, 0x02),
    qRgb(0xDD, 0x08, 0x06),
    qRgb(0xF2, 0x08, 0x84),
    qRgb(0x46, 0x00, 0xA5),
    qRgb(0x00, 0x00, 0xD4),
    qRgb(0x02, 0xAB, 0xEA),
    qRgb(0x1F, 0xB7, 0x14),
    qRgb(0x00, 0x64, 0x11),
    qRgb(0x56, 0x2C, 0x05),
    qRgb(0x90, 0x71, 0x3A),
    qRgb(0xC0, 0xC0, 0xC0),
    qRgb(0x80, 0x80, 0x80),
    qRgb(0x40, 0x40, 0x40),
    qRgb(0x00, 0x00, 0x00)
};
Q_STATIC_ASSERT(sizeof(ICNSColorTable4bit) / sizeof(ICNSColorTable4bit[0]) == (1 << ICNSEntry::Depth4bit));

static const QRgb ICNSColorTable8bit[] = {
    qRgb(0xFF, 0xFF, 0xFF),
    qRgb(0xFF, 0xFF, 0xCC),
    qRgb(0xFF, 0xFF, 0x99),
    qRgb(0xFF, 0xFF, 0x66),
    qRgb(0xFF, 0xFF, 0x33),
    qRgb(0xFF, 0xFF, 0x00),
    qRgb(0xFF, 0xCC, 0xFF),
    qRgb(0xFF, 0xCC, 0xCC),
    qRgb(0xFF, 0xCC, 0x99),
    qRgb(0xFF, 0xCC, 0x66),
    qRgb(0xFF, 0xCC, 0x33),
    qRgb(0xFF, 0xCC, 0x00),
    qRgb(0xFF, 0x99, 0xFF),
    qRgb(0xFF, 0x99, 0xCC),
    qRgb(0xFF, 0x99, 0x99),
    qRgb(0xFF, 0x99, 0x66),
    qRgb(0xFF, 0x99, 0x33),
    qRgb(0xFF, 0x99, 0x00),
    qRgb(0xFF, 0x66, 0xFF),
    qRgb(0xFF, 0x66, 0xCC),
    qRgb(0xFF, 0x66, 0x99),
    qRgb(0xFF, 0x66, 0x66),
    qRgb(0xFF, 0x66, 0x33),
    qRgb(0xFF, 0x66, 0x00),
    qRgb(0xFF, 0x33, 0xFF),
    qRgb(0xFF, 0x33, 0xCC),
    qRgb(0xFF, 0x33, 0x99),
    qRgb(0xFF, 0x33, 0x66),
    qRgb(0xFF, 0x33, 0x33),
    qRgb(0xFF, 0x33, 0x00),
    qRgb(0xFF, 0x00, 0xFF),
    qRgb(0xFF, 0x00, 0xCC),
    qRgb(0xFF, 0x00, 0x99),
    qRgb(0xFF, 0x00, 0x66),
    qRgb(0xFF, 0x00, 0x33),
    qRgb(0xFF, 0x00, 0x00),
    qRgb(0xCC, 0xFF, 0xFF),
    qRgb(0xCC, 0xFF, 0xCC),
    qRgb(0xCC, 0xFF, 0x99),
    qRgb(0xCC, 0xFF, 0x66),
    qRgb(0xCC, 0xFF, 0x33),
    qRgb(0xCC, 0xFF, 0x00),
    qRgb(0xCC, 0xCC, 0xFF),
    qRgb(0xCC, 0xCC, 0xCC),
    qRgb(0xCC, 0xCC, 0x99),
    qRgb(0xCC, 0xCC, 0x66),
    qRgb(0xCC, 0xCC, 0x33),
    qRgb(0xCC, 0xCC, 0x00),
    qRgb(0xCC, 0x99, 0xFF),
    qRgb(0xCC, 0x99, 0xCC),
    qRgb(0xCC, 0x99, 0x99),
    qRgb(0xCC, 0x99, 0x66),
    qRgb(0xCC, 0x99, 0x33),
    qRgb(0xCC, 0x99, 0x00),
    qRgb(0xCC, 0x66, 0xFF),
    qRgb(0xCC, 0x66, 0xCC),
    qRgb(0xCC, 0x66, 0x99),
    qRgb(0xCC, 0x66, 0x66),
    qRgb(0xCC, 0x66, 0x33),
    qRgb(0xCC, 0x66, 0x00),
    qRgb(0xCC, 0x33, 0xFF),
    qRgb(0xCC, 0x33, 0xCC),
    qRgb(0xCC, 0x33, 0x99),
    qRgb(0xCC, 0x33, 0x66),
    qRgb(0xCC, 0x33, 0x33),
    qRgb(0xCC, 0x33, 0x00),
    qRgb(0xCC, 0x00, 0xFF),
    qRgb(0xCC, 0x00, 0xCC),
    qRgb(0xCC, 0x00, 0x99),
    qRgb(0xCC, 0x00, 0x66),
    qRgb(0xCC, 0x00, 0x33),
    qRgb(0xCC, 0x00, 0x00),
    qRgb(0x99, 0xFF, 0xFF),
    qRgb(0x99, 0xFF, 0xCC),
    qRgb(0x99, 0xFF, 0x99),
    qRgb(0x99, 0xFF, 0x66),
    qRgb(0x99, 0xFF, 0x33),
    qRgb(0x99, 0xFF, 0x00),
    qRgb(0x99, 0xCC, 0xFF),
    qRgb(0x99, 0xCC, 0xCC),
    qRgb(0x99, 0xCC, 0x99),
    qRgb(0x99, 0xCC, 0x66),
    qRgb(0x99, 0xCC, 0x33),
    qRgb(0x99, 0xCC, 0x00),
    qRgb(0x99, 0x99, 0xFF),
    qRgb(0x99, 0x99, 0xCC),
    qRgb(0x99, 0x99, 0x99),
    qRgb(0x99, 0x99, 0x66),
    qRgb(0x99, 0x99, 0x33),
    qRgb(0x99, 0x99, 0x00),
    qRgb(0x99, 0x66, 0xFF),
    qRgb(0x99, 0x66, 0xCC),
    qRgb(0x99, 0x66, 0x99),
    qRgb(0x99, 0x66, 0x66),
    qRgb(0x99, 0x66, 0x33),
    qRgb(0x99, 0x66, 0x00),
    qRgb(0x99, 0x33, 0xFF),
    qRgb(0x99, 0x33, 0xCC),
    qRgb(0x99, 0x33, 0x99),
    qRgb(0x99, 0x33, 0x66),
    qRgb(0x99, 0x33, 0x33),
    qRgb(0x99, 0x33, 0x00),
    qRgb(0x99, 0x00, 0xFF),
    qRgb(0x99, 0x00, 0xCC),
    qRgb(0x99, 0x00, 0x99),
    qRgb(0x99, 0x00, 0x66),
    qRgb(0x99, 0x00, 0x33),
    qRgb(0x99, 0x00, 0x00),
    qRgb(0x66, 0xFF, 0xFF),
    qRgb(0x66, 0xFF, 0xCC),
    qRgb(0x66, 0xFF, 0x99),
    qRgb(0x66, 0xFF, 0x66),
    qRgb(0x66, 0xFF, 0x33),
    qRgb(0x66, 0xFF, 0x00),
    qRgb(0x66, 0xCC, 0xFF),
    qRgb(0x66, 0xCC, 0xCC),
    qRgb(0x66, 0xCC, 0x99),
    qRgb(0x66, 0xCC, 0x66),
    qRgb(0x66, 0xCC, 0x33),
    qRgb(0x66, 0xCC, 0x00),
    qRgb(0x66, 0x99, 0xFF),
    qRgb(0x66, 0x99, 0xCC),
    qRgb(0x66, 0x99, 0x99),
    qRgb(0x66, 0x99, 0x66),
    qRgb(0x66, 0x99, 0x33),
    qRgb(0x66, 0x99, 0x00),
    qRgb(0x66, 0x66, 0xFF),
    qRgb(0x66, 0x66, 0xCC),
    qRgb(0x66, 0x66, 0x99),
    qRgb(0x66, 0x66, 0x66),
    qRgb(0x66, 0x66, 0x33),
    qRgb(0x66, 0x66, 0x00),
    qRgb(0x66, 0x33, 0xFF),
    qRgb(0x66, 0x33, 0xCC),
    qRgb(0x66, 0x33, 0x99),
    qRgb(0x66, 0x33, 0x66),
    qRgb(0x66, 0x33, 0x33),
    qRgb(0x66, 0x33, 0x00),
    qRgb(0x66, 0x00, 0xFF),
    qRgb(0x66, 0x00, 0xCC),
    qRgb(0x66, 0x00, 0x99),
    qRgb(0x66, 0x00, 0x66),
    qRgb(0x66, 0x00, 0x33),
    qRgb(0x66, 0x00, 0x00),
    qRgb(0x33, 0xFF, 0xFF),
    qRgb(0x33, 0xFF, 0xCC),
    qRgb(0x33, 0xFF, 0x99),
    qRgb(0x33, 0xFF, 0x66),
    qRgb(0x33, 0xFF, 0x33),
    qRgb(0x33, 0xFF, 0x00),
    qRgb(0x33, 0xCC, 0xFF),
    qRgb(0x33, 0xCC, 0xCC),
    qRgb(0x33, 0xCC, 0x99),
    qRgb(0x33, 0xCC, 0x66),
    qRgb(0x33, 0xCC, 0x33),
    qRgb(0x33, 0xCC, 0x00),
    qRgb(0x33, 0x99, 0xFF),
    qRgb(0x33, 0x99, 0xCC),
    qRgb(0x33, 0x99, 0x99),
    qRgb(0x33, 0x99, 0x66),
    qRgb(0x33, 0x99, 0x33),
    qRgb(0x33, 0x99, 0x00),
    qRgb(0x33, 0x66, 0xFF),
    qRgb(0x33, 0x66, 0xCC),
    qRgb(0x33, 0x66, 0x99),
    qRgb(0x33, 0x66, 0x66),
    qRgb(0x33, 0x66, 0x33),
    qRgb(0x33, 0x66, 0x00),
    qRgb(0x33, 0x33, 0xFF),
    qRgb(0x33, 0x33, 0xCC),
    qRgb(0x33, 0x33, 0x99),
    qRgb(0x33, 0x33, 0x66),
    qRgb(0x33, 0x33, 0x33),
    qRgb(0x33, 0x33, 0x00),
    qRgb(0x33, 0x00, 0xFF),
    qRgb(0x33, 0x00, 0xCC),
    qRgb(0x33, 0x00, 0x99),
    qRgb(0x33, 0x00, 0x66),
    qRgb(0x33, 0x00, 0x33),
    qRgb(0x33, 0x00, 0x00),
    qRgb(0x00, 0xFF, 0xFF),
    qRgb(0x00, 0xFF, 0xCC),
    qRgb(0x00, 0xFF, 0x99),
    qRgb(0x00, 0xFF, 0x66),
    qRgb(0x00, 0xFF, 0x33),
    qRgb(0x00, 0xFF, 0x00),
    qRgb(0x00, 0xCC, 0xFF),
    qRgb(0x00, 0xCC, 0xCC),
    qRgb(0x00, 0xCC, 0x99),
    qRgb(0x00, 0xCC, 0x66),
    qRgb(0x00, 0xCC, 0x33),
    qRgb(0x00, 0xCC, 0x00),
    qRgb(0x00, 0x99, 0xFF),
    qRgb(0x00, 0x99, 0xCC),
    qRgb(0x00, 0x99, 0x99),
    qRgb(0x00, 0x99, 0x66),
    qRgb(0x00, 0x99, 0x33),
    qRgb(0x00, 0x99, 0x00),
    qRgb(0x00, 0x66, 0xFF),
    qRgb(0x00, 0x66, 0xCC),
    qRgb(0x00, 0x66, 0x99),
    qRgb(0x00, 0x66, 0x66),
    qRgb(0x00, 0x66, 0x33),
    qRgb(0x00, 0x66, 0x00),
    qRgb(0x00, 0x33, 0xFF),
    qRgb(0x00, 0x33, 0xCC),
    qRgb(0x00, 0x33, 0x99),
    qRgb(0x00, 0x33, 0x66),
    qRgb(0x00, 0x33, 0x33),
    qRgb(0x00, 0x33, 0x00),
    qRgb(0x00, 0x00, 0xFF),
    qRgb(0x00, 0x00, 0xCC),
    qRgb(0x00, 0x00, 0x99),
    qRgb(0x00, 0x00, 0x66),
    qRgb(0x00, 0x00, 0x33),
    qRgb(0xEE, 0x00, 0x00),
    qRgb(0xDD, 0x00, 0x00),
    qRgb(0xBB, 0x00, 0x00),
    qRgb(0xAA, 0x00, 0x00),
    qRgb(0x88, 0x00, 0x00),
    qRgb(0x77, 0x00, 0x00),
    qRgb(0x55, 0x00, 0x00),
    qRgb(0x44, 0x00, 0x00),
    qRgb(0x22, 0x00, 0x00),
    qRgb(0x11, 0x00, 0x00),
    qRgb(0x00, 0xEE, 0x00),
    qRgb(0x00, 0xDD, 0x00),
    qRgb(0x00, 0xBB, 0x00),
    qRgb(0x00, 0xAA, 0x00),
    qRgb(0x00, 0x88, 0x00),
    qRgb(0x00, 0x77, 0x00),
    qRgb(0x00, 0x55, 0x00),
    qRgb(0x00, 0x44, 0x00),
    qRgb(0x00, 0x22, 0x00),
    qRgb(0x00, 0x11, 0x00),
    qRgb(0x00, 0x00, 0xEE),
    qRgb(0x00, 0x00, 0xDD),
    qRgb(0x00, 0x00, 0xBB),
    qRgb(0x00, 0x00, 0xAA),
    qRgb(0x00, 0x00, 0x88),
    qRgb(0x00, 0x00, 0x77),
    qRgb(0x00, 0x00, 0x55),
    qRgb(0x00, 0x00, 0x44),
    qRgb(0x00, 0x00, 0x22),
    qRgb(0x00, 0x00, 0x11),
    qRgb(0xEE, 0xEE, 0xEE),
    qRgb(0xDD, 0xDD, 0xDD),
    qRgb(0xBB, 0xBB, 0xBB),
    qRgb(0xAA, 0xAA, 0xAA),
    qRgb(0x88, 0x88, 0x88),
    qRgb(0x77, 0x77, 0x77),
    qRgb(0x55, 0x55, 0x55),
    qRgb(0x44, 0x44, 0x44),
    qRgb(0x22, 0x22, 0x22),
    qRgb(0x11, 0x11, 0x11),
    qRgb(0x00, 0x00, 0x00)
};
Q_STATIC_ASSERT(sizeof(ICNSColorTable8bit) / sizeof(ICNSColorTable8bit[0]) == (1 << ICNSEntry::Depth8bit));

static inline QDataStream &operator>>(QDataStream &in, ICNSBlockHeader &p)
{
    in >> p.ostype;
    in >> p.length;
    return in;
}

static inline QDataStream &operator<<(QDataStream &out, const ICNSBlockHeader &p)
{
    out << p.ostype;
    out << p.length;
    return out;
}

static inline bool isPowOf2OrDividesBy16(quint32 u, qreal r)
{
    return u == r && ((u % 16 == 0) || (r >= 16 && (u & (u - 1)) == 0));
}

static inline bool isBlockHeaderValid(const ICNSBlockHeader &header, quint64 bound = 0)
{
    return header.ostype != 0 && (bound == 0
                || qBound(quint64(ICNSBlockHeaderSize), quint64(header.length), bound) == header.length);
}

static inline bool isIconCompressed(const ICNSEntry &icon)
{
    return icon.dataFormat == ICNSEntry::PNG || icon.dataFormat == ICNSEntry::JP2;
}

static inline bool isMaskSuitable(const ICNSEntry &mask, const ICNSEntry &icon, ICNSEntry::Depth target)
{
    return mask.variant == icon.variant && mask.depth == target
            && mask.height == icon.height && mask.width == icon.width;
}

static inline QByteArray nameFromOSType(quint32 ostype)
{
    const quint32 bytes = qToBigEndian(ostype);
    return QByteArray((const char*)&bytes, 4);
}

static inline quint32 nameToOSType(const QByteArray &ostype)
{
    if (ostype.size() != 4)
        return 0;
    return qFromBigEndian(*reinterpret_cast<const quint32*>(ostype.constData()));
}

static inline QByteArray nameForCompressedIcon(quint8 iconNumber)
{
    const bool portable = iconNumber < 7;
    const QByteArray base =  portable ? QByteArrayLiteral("icp") : QByteArrayLiteral("ic");
    if (!portable && iconNumber < 10)
        return base + "0" + QByteArray::number(iconNumber);
    return base + QByteArray::number(iconNumber);
}

static inline QVector<QRgb> getColorTable(ICNSEntry::Depth depth)
{
    QVector<QRgb> table;
    uint n = 1 << depth;
    const QRgb *data;
    switch (depth) {
    case ICNSEntry::DepthMono:
        data = ICNSColorTableMono;
        break;
    case ICNSEntry::Depth4bit:
        data = ICNSColorTable4bit;
        break;
    case ICNSEntry::Depth8bit:
        data = ICNSColorTable8bit;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    table.resize(n);
    memcpy(table.data(), data, sizeof(QRgb) * n);
    return table;
}

static bool parseIconEntryData(ICNSEntry &icon, QIODevice *device)
{
    const qint64 oldPos = device->pos();
    if (oldPos != icon.dataOffset && !device->seek(icon.dataOffset))
        return false;

    const QByteArray magic = device->peek(12);
    const bool isPNG = magic.startsWith(QByteArrayLiteral("\211PNG\r\n\032\n\000\000\000\r"));
    const bool isJP2 = !isPNG && magic == QByteArrayLiteral("\000\000\000\014jP  \r\n\207\n");
    if (isPNG || isJP2) {
        // TODO: Add parsing of png/jp2 headers to enable feature reporting by plugin?
        icon.flags = ICNSEntry::IsIcon;
        icon.dataFormat = isPNG? ICNSEntry::PNG : ICNSEntry::JP2;
    }
    if (oldPos != icon.dataOffset && !device->seek(oldPos))
        return false;
    return true;
}

static bool parseIconEntryInfo(ICNSEntry &icon)
{
    const QString ostype = QString::fromLatin1(nameFromOSType(icon.ostype));
    // Typical OSType naming: <junk><group><depth><mask>;
    // For icons OSType should be strictly alphanumeric + '#' character for masks/mono.
    const QString ptrn = QStringLiteral("^(?<junk>[a-z|A-Z]{0,4})(?<group>[a-z|A-Z]{1})(?<depth>[\\d]{0,2})(?<mask>[#mk]{0,2})$");
    QRegularExpression regexp(ptrn);
    QRegularExpressionMatch match = regexp.match(ostype);
    if (!match.hasMatch()) {
        qWarning("parseIconEntryInfo(): Failed, OSType doesn't match: \"%s\"", qPrintable(ostype));
        return false;
    }
    const QString group = match.captured(QStringLiteral("group"));
    const QString depth = match.captured(QStringLiteral("depth"));
    const QString mask = match.captured(QStringLiteral("mask"));
    // Icon group:
    if (!group.isEmpty())
        icon.group = ICNSEntry::Group(group.at(0).toLatin1());

    // That's enough for compressed ones
    if (isIconCompressed(icon))
        return true;
    // Icon depth:
    if (!depth.isEmpty())
        icon.depth = ICNSEntry::Depth(depth.toUInt());
    // Try mono if depth not found
    if (icon.depth == ICNSEntry::DepthUnknown)
        icon.depth = ICNSEntry::DepthMono;
    // Detect size:
    const qreal bytespp = (qreal)icon.depth / 8;
    const qreal r1 = qSqrt(icon.dataLength / bytespp);
    const qreal r2 = qSqrt((icon.dataLength / bytespp) / 2);
    const quint32 r1u = qRound(r1);
    const quint32 r2u = qRound(r2);
    const bool singleEntry = isPowOf2OrDividesBy16(r1u, r1);
    const bool doubleSize = isPowOf2OrDividesBy16(r2u, r2);
    if (singleEntry) {
        icon.flags = mask.isEmpty() ? ICNSEntry::IsIcon : ICNSEntry::IsMask;
        icon.dataFormat = ICNSEntry::RawIcon;
        icon.width = r1u;
        icon.height = r1u;
    } else if (doubleSize) {
        icon.flags = ICNSEntry::IconPlusMask;
        icon.dataFormat = ICNSEntry::RawIcon;
        icon.width = r2u;
        icon.height = r2u;
    } else if (icon.group == ICNSEntry::GroupMini) {
        // Legacy 16x12 icons are an exception from the generic square formula
        const bool doubleSize = icon.dataLength == 192 * bytespp * 2;
        icon.flags = doubleSize ? ICNSEntry::IconPlusMask : ICNSEntry::IsIcon;
        icon.dataFormat = ICNSEntry::RawIcon;
        icon.width = 16;
        icon.height = 12;
    } else if (icon.depth == ICNSEntry::Depth32bit) {
        // We have a formula mismatch in a 32bit icon there, probably RLE24
        icon.dataFormat = ICNSEntry::RLE24;
        icon.flags = mask.isEmpty() ? ICNSEntry::IsIcon : ICNSEntry::IsMask;
        switch (icon.group) {
        case ICNSEntry::GroupSmall:
            icon.width = 16;
            break;
        case ICNSEntry::GroupLarge:
            icon.width = 32;
            break;
        case ICNSEntry::GroupHuge:
            icon.width = 48;
            break;
        case ICNSEntry::GroupThumbnail:
            icon.width = 128;
            break;
        default:
            qWarning("parseIconEntryInfo(): Failed, 32bit icon from an unknown group. OSType: \"%s\"",
                     qPrintable(ostype));
        }
        icon.height = icon.width;
    }
    return true;
}

static QImage readMask(const ICNSEntry &mask, QDataStream &stream)
{
    if ((mask.flags & ICNSEntry::IsMask) == 0)
        return QImage();
    if (mask.depth != ICNSEntry::DepthMono && mask.depth != ICNSEntry::Depth8bit) {
        qWarning("readMask(): Failed, unusual bit depth: %u OSType: \"%s\"",
                 mask.depth, nameFromOSType(mask.ostype).constData());
        return QImage();
    }
    const bool isMono = mask.depth == ICNSEntry::DepthMono;
    const bool doubleSize = mask.flags == ICNSEntry::IconPlusMask;
    const quint32 imageDataSize = (mask.width * mask.height * mask.depth) / 8;
    const qint64 pos = doubleSize ? (mask.dataOffset + imageDataSize) : mask.dataOffset;
    const qint64 oldPos = stream.device()->pos();
    if (!stream.device()->seek(pos))
        return QImage();
    QImage img(mask.width, mask.height, QImage::Format_RGB32);
    quint8 byte = 0;
    quint32 pixel = 0;
    for (quint32 y = 0; y < mask.height; y++) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (quint32 x = 0; x < mask.width; x++) {
            if (pixel % (8 / mask.depth) == 0)
                stream >> byte;
            else if (isMono)
                byte <<= 1;
            const quint8 alpha = isMono ? (((byte >> 7) & 0x01) * 255) : byte;
            line[x] = qRgb(alpha, alpha, alpha);
            pixel++;
        }
    }
    stream.device()->seek(oldPos);
    return img;
}

template <ICNSEntry::Depth depth>
static QImage readLowDepthIcon(const ICNSEntry &icon, QDataStream &stream)
{
    Q_STATIC_ASSERT(depth == ICNSEntry::DepthMono || depth == ICNSEntry::Depth4bit
                    || depth == ICNSEntry::Depth8bit);

    const bool isMono = depth == ICNSEntry::DepthMono;
    const QImage::Format format = isMono ? QImage::Format_Mono : QImage::Format_Indexed8;
    const QVector<QRgb> colortable = getColorTable(depth);
    if (colortable.isEmpty())
        return QImage();
    QImage img(icon.width, icon.height, format);
    img.setColorTable(colortable);
    quint32 pixel = 0;
    quint8 byte = 0;
    for (quint32 y = 0; y < icon.height; y++) {
        for (quint32 x = 0; x < icon.width; x++) {
            if (pixel % (8 / depth) == 0)
                stream >> byte;
            quint8 cindex;
            switch (depth) {
            case ICNSEntry::DepthMono:
                cindex = (byte >> 7) & 0x01; // left 1 bit
                byte <<= 1;
                break;
            case ICNSEntry::Depth4bit:
                cindex = (byte >> 4) & 0x0F; // left 4 bits
                byte <<= 4;
                break;
            default:
                cindex = byte; // 8 bits
                break;
            }
            img.setPixel(x, y, cindex);
            pixel++;
        }
    }
    return img;
}

static QImage read32bitIcon(const ICNSEntry &icon, QDataStream &stream)
{
    QImage img = QImage(icon.width, icon.height, QImage::Format_RGB32);
    if (icon.dataFormat != ICNSEntry::RLE24) {
        for (quint32 y = 0; y < icon.height; y++) {
            QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (quint32 x = 0; x < icon.width; x++) {
                quint8 r, g, b, a;
                stream >> r >> g >> b >> a;
                line[x] = qRgb(r, g, b);
            }
        }
    } else {
        const quint32 estPxsNum = icon.width * icon.height;
        const QByteArray &bytes = stream.device()->peek(4);
        if (bytes.isEmpty())
            return QImage();
        // Zero-padding may be present:
        if (qFromBigEndian<quint32>(*bytes.constData()) == 0)
            stream.skipRawData(4);
        for (quint8 colorNRun = 0; colorNRun < 3; colorNRun++) {
            quint32 pixel = 0;
            QRgb *line = 0;
            while (pixel < estPxsNum && !stream.atEnd()) {
                quint8 byte, value;
                stream >> byte;
                const bool bitIsClear = (byte & 0x80) == 0;
                // If high bit is clear: run of different values; else: same value
                quint8 runLength = bitIsClear ? ((0xFF & byte) + 1) : ((0xFF & byte) - 125);
                // Length of the run for for different values: 1 <= len <= 128
                // Length of the run for same values: 3 <= len <= 130
                if (!bitIsClear)
                    stream >> value;
                for (quint8 i = 0; i < runLength && pixel < estPxsNum; i++) {
                    if (bitIsClear)
                        stream >> value;
                    const quint32 y = pixel / icon.height;
                    const quint32 x = pixel - (icon.width * y);
                    if (pixel % icon.height == 0)
                        line = reinterpret_cast<QRgb *>(img.scanLine(y));
                    QRgb rgb = line[x];
                    const int r = (colorNRun == 0) ? value : qRed(rgb);
                    const int g = (colorNRun == 1) ? value : qGreen(rgb);
                    const int b = (colorNRun == 2) ? value : qBlue(rgb);
                    line[x] = qRgb(r, g, b);
                    pixel++;
                }
            }
        }
    }
    return img;
}

QICNSHandler::QICNSHandler() :
    m_currentIconIndex(0), m_state(ScanNotScanned)
{
}

QByteArray QICNSHandler::name() const
{
    return QByteArrayLiteral("icns");
}

bool QICNSHandler::canRead(QIODevice *device)
{
    if (!device || !device->isReadable()) {
        qWarning("QICNSHandler::canRead() called without a readable device");
        return false;
    }

    if (device->peek(4) == QByteArrayLiteral("icns")) {
        if (device->isSequential()) {
            qWarning("QICNSHandler::canRead() called on a sequential device");
            return false;
        }
        return true;
    }

    return false;
}

bool QICNSHandler::canRead() const
{
    if (m_state == ScanNotScanned && !canRead(device()))
        return false;

    if (m_state != ScanError) {
        setFormat(QByteArrayLiteral("icns"));
        return true;
    }

    return false;
}

bool QICNSHandler::read(QImage *outImage)
{
    QImage img;
    if (!ensureScanned()) {
        qWarning("QICNSHandler::read(): The device wasn't parsed properly!");
        return false;
    }

    const ICNSEntry &icon = m_icons.at(m_currentIconIndex);
    QDataStream stream(device());
    stream.setByteOrder(QDataStream::BigEndian);
    if (!device()->seek(icon.dataOffset))
        return false;

    switch (icon.dataFormat) {
    case ICNSEntry::RawIcon:
    case ICNSEntry::RLE24:
        if (qMin(icon.width, icon.height) == 0)
            break;
        switch (icon.depth) {
        case ICNSEntry::DepthMono:
            img = readLowDepthIcon<ICNSEntry::DepthMono>(icon, stream);
            break;
        case ICNSEntry::Depth4bit:
            img = readLowDepthIcon<ICNSEntry::Depth4bit>(icon, stream);
            break;
        case ICNSEntry::Depth8bit:
            img = readLowDepthIcon<ICNSEntry::Depth8bit>(icon, stream);
            break;
        case ICNSEntry::Depth32bit:
            img = read32bitIcon(icon, stream);
            break;
        default:
            qWarning("QICNSHandler::read(): Failed, unsupported icon bit depth: %u, OSType: \"%s\"",
                     icon.depth, nameFromOSType(icon.ostype).constData());
        }
        if (!img.isNull()) {
            QImage alpha = readMask(getIconMask(icon), stream);
            if (!alpha.isNull())
                img.setAlphaChannel(alpha);
        }
        break;
    default:
        const char *format = 0;
        if (icon.dataFormat == ICNSEntry::PNG)
            format = "png";
        else if (icon.dataFormat == ICNSEntry::JP2)
            format = "jp2";
        // Even if JP2 or PNG magic is not detected, try anyway for unknown formats
        img = QImage::fromData(device()->read(icon.dataLength), format);
        if (img.isNull()) {
            if (format == 0)
                format = "unknown";
            qWarning("QICNSHandler::read(): Failed, compressed format \"%s\" is not supported " \
                     "by your Qt library or this file is corrupt. OSType: \"%s\"",
                     format, nameFromOSType(icon.ostype).constData());
        }
    }
    *outImage = img;
    return !img.isNull();
}

bool QICNSHandler::write(const QImage &image)
{
    /*
    Notes:
    * Experimental implementation. Just for simple converting tasks / testing purposes.
    * Min. size is 16x16, Max. size is 1024x1024.
    * Performs downscale to a square image if width != height.
    * Performs upscale to 16x16, if the image is smaller.
    * Performs downscale to a nearest power of two if size is not a power of two.
    * Currently uses non-hardcoded OSTypes.
    */
    QIODevice *device = this->device();
    if (!device->isWritable() || image.isNull() || qMin(image.width(), image.height()) == 0)
        return false;
    const int minSize = qMin(image.width(), image.height());
    const int oldSize = (minSize < 16) ? 16 : minSize;
    // Calc power of two:
    int size = oldSize;
    uint pow = 0;
    // Note: Values over 10 are reserved for retina icons.
    while (pow < 10 && (size >>= 1))
        pow++;
    const int newSize = 1 << pow;
    QImage img = image;
    // Let's enforce resizing if size differs:
    if (newSize != oldSize || qMax(image.width(), image.height()) != minSize)
        img = img.scaled(newSize, newSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    // Construct OSType and headers:
    const quint32 ostype = nameToOSType(nameForCompressedIcon(pow));
    ICNSBlockHeader fileHeader;
    fileHeader.ostype = ICNSBlockHeader::TypeIcns;
    ICNSBlockHeader tocHeader;
    tocHeader.ostype = ICNSBlockHeader::TypeToc;
    ICNSBlockHeader iconEntry;
    iconEntry.ostype = ostype;
    QByteArray imageData;
    QBuffer buffer(&imageData);
    if (!buffer.open(QIODevice::WriteOnly) || !img.save(&buffer, "png"))
        return false;
    buffer.close();
    iconEntry.length = ICNSBlockHeaderSize + imageData.size();
    tocHeader.length = ICNSBlockHeaderSize * 2;
    fileHeader.length = ICNSBlockHeaderSize + tocHeader.length + iconEntry.length;
    if (!isBlockHeaderValid(iconEntry))
        return false;

    QDataStream stream(device);
    // iconEntry is also a TOC entry
    stream << fileHeader << tocHeader << iconEntry << iconEntry;
    stream.writeRawData(imageData.constData(), imageData.size());
    return stream.status() == QDataStream::Ok;
}

bool QICNSHandler::supportsOption(ImageOption option) const
{
    return option == SubType;
}

QVariant QICNSHandler::option(ImageOption option) const
{
    if (!supportsOption(option) || !ensureScanned())
        return QVariant();

    if (option == SubType) {
        if (imageCount() > 0 && m_currentIconIndex <= imageCount()) {
            const ICNSEntry &icon = m_icons.at(m_currentIconIndex);
            if (icon.variant != 0)
                return QByteArray(nameFromOSType(icon.variant) + '-' + nameFromOSType(icon.ostype));
            return nameFromOSType(icon.ostype);
        }
    }

    return QVariant();
}

int QICNSHandler::imageCount() const
{
    if (!ensureScanned())
        return 0;

    return m_icons.size();
}

bool QICNSHandler::jumpToImage(int imageNumber)
{
    if (imageNumber >= imageCount())
        return false;

    m_currentIconIndex = imageNumber;
    return true;
}

bool QICNSHandler::jumpToNextImage()
{
    return jumpToImage(m_currentIconIndex + 1);
}

bool QICNSHandler::ensureScanned() const
{
    if (m_state == ScanNotScanned) {
        QICNSHandler *that = const_cast<QICNSHandler *>(this);
        that->m_state = that->scanDevice() ? ScanSuccess : ScanError;
    }

    return m_state == ScanSuccess;
}

bool QICNSHandler::addEntry(const ICNSBlockHeader &header, qint64 imgDataOffset, quint32 variant)
{
    // Note: This function returns false only when a device positioning error occurred
    ICNSEntry entry;
    entry.ostype = header.ostype;
    entry.variant = variant;
    entry.dataOffset = imgDataOffset;
    entry.dataLength = header.length - ICNSBlockHeaderSize;
    // Check for known magic numbers:
    if (!parseIconEntryData(entry, device()))
        return false;
    // Parse everything else and index this entry:
    if (parseIconEntryInfo(entry)) {
        if ((entry.flags & ICNSEntry::IsMask) != 0)
            m_masks << entry;
        if ((entry.flags & ICNSEntry::IsIcon) != 0)
            m_icons << entry;
    }
    return true;
}

bool QICNSHandler::scanDevice()
{
    if (m_state == ScanSuccess)
        return true;

    if (!device()->seek(0))
        return false;

    QDataStream stream(device());
    stream.setByteOrder(QDataStream::BigEndian);

    bool scanIsIncomplete = false;
    qint64 filelength = device()->size();
    ICNSBlockHeader blockHeader;
    while (!stream.atEnd() || device()->pos() < filelength) {
        stream >> blockHeader;
        if (stream.status() != QDataStream::Ok)
            return false;

        const qint64 blockDataOffset = device()->pos();
        if (!isBlockHeaderValid(blockHeader)) {
            qWarning("QICNSHandler::scanDevice(): Failed, bad header at pos %s. OSType \"%s\", length %u",
                     QByteArray::number(blockDataOffset).constData(),
                     nameFromOSType(blockHeader.ostype).constData(), blockHeader.length);
            return false;
        }
        const quint64 blockDataLength = blockHeader.length - ICNSBlockHeaderSize;
        const qint64 nextBlockOffset = blockDataOffset + blockDataLength;

        switch (blockHeader.ostype) {
        case ICNSBlockHeader::TypeIcns:
            if (blockDataOffset != ICNSBlockHeaderSize) {
                // Icns container definition should be in the beginning of the device.
                // If we meet this block somewhere else, then just ignore it.
                stream.skipRawData(blockDataLength);
                break;
            }
            filelength = blockHeader.length;
            if (device()->size() < blockHeader.length) {
                qWarning("QICNSHandler::scanDevice(): Failed, file is incomplete.");
                return false;
            }
            break;
        case ICNSBlockHeader::TypeIcnv:
        case ICNSBlockHeader::TypeClut:
            // We don't have a good use for these blocks... yet.
            stream.skipRawData(blockDataLength);
            break;
        case ICNSBlockHeader::TypeTile:
        case ICNSBlockHeader::TypeOver:
        case ICNSBlockHeader::TypeOpen:
        case ICNSBlockHeader::TypeDrop:
        case ICNSBlockHeader::TypeOdrp:
            // Icns container seems to have an embedded icon variant container
            // Let's start a scan for entries
            while (device()->pos() < nextBlockOffset) {
                ICNSBlockHeader icon;
                stream >> icon;
                // Check for incorrect variant entry header and stop scan
                if (!isBlockHeaderValid(icon, blockDataLength))
                    break;
                if (!addEntry(icon, device()->pos(), blockHeader.ostype))
                    return false;
                if (stream.skipRawData(icon.length - ICNSBlockHeaderSize) < 0)
                    return false;
            }
            if (device()->pos() != nextBlockOffset) {
                // Scan of this container didn't end where we expected.
                // Let's generate some output about this incident:
                qWarning("Scan of the icon variant container (\"%s\") failed at pos %s.\n" \
                         "Reason: Scan didn't reach the end of this container's block, " \
                         "delta: %s bytes. This file may be corrupted.",
                         nameFromOSType(blockHeader.ostype).constData(),
                         QByteArray::number(device()->pos()).constData(),
                         QByteArray::number(nextBlockOffset - device()->pos()).constData());
                if (!device()->seek(nextBlockOffset))
                    return false;
            }
            break;
        case ICNSBlockHeader::TypeToc: {
            // Quick scan, table of contents
            if (blockDataOffset != ICNSBlockHeaderSize * 2) {
                // TOC should be the first block in the file after container definition.
                // Ignore and go on with a deep scan.
                stream.skipRawData(blockDataLength);
                break;
            }
            // First image data offset:
            qint64 imgDataOffset = blockDataOffset + blockHeader.length;
            for (uint i = 0, count = blockDataLength / ICNSBlockHeaderSize; i < count; i++) {
                ICNSBlockHeader tocEntry;
                stream >> tocEntry;
                if (!isBlockHeaderValid(tocEntry)) {
                    // TOC contains incorrect header, we should skip TOC since we can't trust it
                    qWarning("QICNSHandler::scanDevice(): Warning! Table of contents contains a bad " \
                             "entry! Stop at device pos: %s bytes. This file may be corrupted.",
                             QByteArray::number(device()->pos()).constData());
                    if (!device()->seek(nextBlockOffset))
                        return false;
                    break;
                }
                if (!addEntry(tocEntry, imgDataOffset))
                    return false;
                imgDataOffset += tocEntry.length;
                // If TOC covers all the blocks in the file, then quick scan is complete
                if (imgDataOffset == filelength)
                    return true;
            }
            // Else just start a deep scan to salvage anything left after TOC's end
            scanIsIncomplete = true;
            break;
        }
        default:
            // Deep scan, block by block
            if (scanIsIncomplete) {
                // Check if entry with this offset is added somewhere
                // But only if we have incomplete TOC, otherwise just try to add
                bool exists = false;
                for (int i = 0; i < m_icons.size() && !exists; i++)
                    exists = m_icons.at(i).dataOffset == blockDataOffset;
                for (int i = 0; i < m_masks.size() && !exists; i++)
                    exists = m_masks.at(i).dataOffset == blockDataOffset;
                if (!exists && !addEntry(blockHeader, blockDataOffset))
                    return false;
            } else if (!addEntry(blockHeader, blockDataOffset)) {
                return false;
            }
            stream.skipRawData(blockDataLength);
            break;
        }
    }
    return true;
}

const ICNSEntry &QICNSHandler::getIconMask(const ICNSEntry &icon) const
{
    const bool is32bit = icon.depth == ICNSEntry::Depth32bit;
    ICNSEntry::Depth targetDepth = is32bit ? ICNSEntry::Depth8bit : ICNSEntry::DepthMono;
    for (int i = 0; i < m_masks.size(); i++) {
        const ICNSEntry &mask = m_masks.at(i);
        if (isMaskSuitable(mask, icon, targetDepth))
            return mask;
    }
    return icon;
}

QT_END_NAMESPACE

#endif // QT_NO_DATASTREAM
