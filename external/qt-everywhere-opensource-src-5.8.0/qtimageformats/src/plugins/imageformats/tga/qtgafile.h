/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the TGA plugin in the Qt ImageFormats module.
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

#ifndef QTGAFILE_H
#define QTGAFILE_H

#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtCore/QCoreApplication>

QT_BEGIN_NAMESPACE

class QIODevice;

class QTgaFile
{
    Q_DECLARE_TR_FUNCTIONS(QTgaFile)

public:
    enum Compression {
        NoCompression = 0,
        RleCompression = 1
    };

    enum HeaderOffset {
        IdLength = 0,          /* 00h  Size of Image ID field */
        ColorMapType = 1,      /* 01h  Color map type */
        ImageType = 2,         /* 02h  Image type code */
        CMapStart = 3,         /* 03h  Color map origin */
        CMapLength = 5,        /* 05h  Color map length */
        CMapDepth = 7,         /* 07h  Depth of color map entries */
        XOffset = 8,           /* 08h  X origin of image */
        YOffset = 10,          /* 0Ah  Y origin of image */
        Width = 12,            /* 0Ch  Width of image */
        Height = 14,           /* 0Eh  Height of image */
        PixelDepth = 16,       /* 10h  Image pixel size */
        ImageDescriptor = 17,  /* 11h  Image descriptor byte */
        HeaderSize = 18
    };

    enum FooterOffset {
        ExtensionOffset = 0,
        DeveloperOffset = 4,
        SignatureOffset = 8,
        FooterSize = 26
    };

    QTgaFile(QIODevice *);
    ~QTgaFile();

    inline bool isValid() const;
    inline QString errorMessage() const;
    QImage readImage();
    inline int xOffset() const;
    inline int yOffset() const;
    inline int width() const;
    inline int height() const;
    inline QSize size() const;
    inline Compression compression() const;

private:
    static inline quint16 littleEndianInt(const unsigned char *d);

    QString mErrorMessage;
    unsigned char mHeader[HeaderSize];
    QIODevice *mDevice;
};

inline bool QTgaFile::isValid() const
{
    return mErrorMessage.isEmpty();
}

inline QString QTgaFile::errorMessage() const
{
    return mErrorMessage;
}

/*!
    \internal
    Returns the integer encoded in the two little endian bytes at \a d.
*/
inline quint16 QTgaFile::littleEndianInt(const unsigned char *d)
{
    return d[0] + d[1] * 256;
}

inline int QTgaFile::xOffset() const
{
    return littleEndianInt(&mHeader[XOffset]);
}

inline int QTgaFile::yOffset() const
{
    return littleEndianInt(&mHeader[YOffset]);
}

inline int QTgaFile::width() const
{
    return littleEndianInt(&mHeader[Width]);
}

inline int QTgaFile::height() const
{
    return littleEndianInt(&mHeader[Height]);
}

inline QSize QTgaFile::size() const
{
    return QSize(width(), height());
}

inline QTgaFile::Compression QTgaFile::compression() const
{
    // TODO: for now, only handle type 2 files, with no color table
    // and no compression
    return NoCompression;
}

QT_END_NAMESPACE

#endif // QTGAFILE_H
