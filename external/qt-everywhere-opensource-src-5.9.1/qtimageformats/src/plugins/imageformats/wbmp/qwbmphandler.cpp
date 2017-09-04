/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the WBMP plugin in the Qt ImageFormats module.
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

#include "qwbmphandler_p.h"

/*!
    \class QWbmpHandler
    \since 5.0
    \brief The QWbmpHandler class provides support for the WBMP image format.
    \internal
*/

#include <qimage.h>
#include <qvariant.h>

QT_BEGIN_NAMESPACE

// This struct represents header of WBMP image file
struct WBMPHeader
{
    quint8 type;              // Type of WBMP image (always equal to 0)
    quint8 format;            // Format of WBMP image
    quint32 width;            // Width of the image already decoded from multibyte integer
    quint32 height;           // Height of the image already decoded from multibyte integer
};
#define WBMPFIXEDHEADER_SIZE 2

// Data renderers and writers which takes care of data alignment endiness and stuff
static bool readMultiByteInt(QIODevice *iodev, quint32 *num)
{
    quint32 res = 0;

    quint8 c;
    unsigned int count = 0;
    do {
        // Do not allow to read longer
        // then we can store in num
        if (++count > sizeof(*num))
            return false;

        if (!iodev->getChar(reinterpret_cast<char *>(&c)))
            return false;

        res = (res << 7) | (c & 0x7F);

    } while (c & 0x80);

    *num = res;
    return true;
}

static bool writeMultiByteInt(QIODevice *iodev, quint32 num)
{
    quint64 tmp = num & 0x7F;
    num >>= 7;

    while (num) {
        quint8 c = num & 0x7F;
        num = num >> 7;
        tmp = (tmp << 8) | (c | 0x80);
    }

    while (tmp) {
        quint8 c = tmp & 0xFF;
        if (!iodev->putChar(c))
            return false;
        tmp >>= 8;
    }
    return true;
}

static bool readWBMPHeader(QIODevice *iodev, WBMPHeader *hdr)
{
    if (!iodev)
        return false;

    uchar tmp[WBMPFIXEDHEADER_SIZE];
    if (iodev->read(reinterpret_cast<char *>(tmp), WBMPFIXEDHEADER_SIZE) == WBMPFIXEDHEADER_SIZE) {
        hdr->type = tmp[0];
        hdr->format = tmp[1];
    } else {
        return false;
    }

    if (readMultiByteInt(iodev, &hdr->width)
        && readMultiByteInt(iodev, &hdr->height)) {
        return true;
    }
    return false;
}

static bool writeWBMPHeader(QIODevice *iodev, const WBMPHeader &hdr)
{
    if (iodev) {
        uchar tmp[WBMPFIXEDHEADER_SIZE];
        tmp[0] = hdr.type;
        tmp[1] = hdr.format;
        if (iodev->write(reinterpret_cast<char *>(tmp),  WBMPFIXEDHEADER_SIZE) != WBMPFIXEDHEADER_SIZE)
            return false;

        if (writeMultiByteInt(iodev, hdr.width) &&
            writeMultiByteInt(iodev, hdr.height))
            return true;
    }
    return false;
}

static bool writeWBMPData(QIODevice *iodev, const QImage &image)
{
    if (iodev) {
        int h = image.height();
        int bpl = (image.width() + 7) / 8;

        for (int l=0; l<h; l++) {
            if (iodev->write(reinterpret_cast<const char *>(image.constScanLine(l)), bpl) != bpl)
                return false;
        }
        return true;
    }
    return false;
}

static bool readWBMPData(QIODevice *iodev, QImage &image)
{
    if (iodev) {
        int h = image.height();
        int bpl = (image.width() + 7) / 8;

        for (int l = 0; l < h; l++) {
            if (iodev->read(reinterpret_cast<char *>(image.scanLine(l)), bpl) != bpl)
                return false;
        }
        return true;
    }
    return false;
}

class WBMPReader
{
public:
    WBMPReader(QIODevice *iodevice);

    QImage readImage();
    bool writeImage(QImage image);

    static bool canRead(QIODevice *iodevice);

private:
    QIODevice *iodev;
    WBMPHeader hdr;
};

// WBMP common reader and writer implementation
WBMPReader::WBMPReader(QIODevice *iodevice) : iodev(iodevice)
{
    memset(&hdr, 0, sizeof(hdr));
}

QImage WBMPReader::readImage()
{
    if (!readWBMPHeader(iodev, &hdr))
        return QImage();

    QImage image(hdr.width, hdr.height, QImage::Format_Mono);
    if (!readWBMPData(iodev, image))
        return QImage();

    return image;
}

bool WBMPReader::writeImage(QImage image)
{
    if (image.format() != QImage::Format_Mono)
        image = image.convertToFormat(QImage::Format_Mono);

    if (image.colorTable().at(0) == image.colorTable().at(1)) {
        // degenerate image: actually blank.
        image.fill((qGray(image.colorTable().at(0)) < 128) ? 0 : 1);
    } else if (qGray(image.colorTable().at(0)) > qGray(image.colorTable().at(1))) {
        // Conform to WBMP's convention about black and white
        image.invertPixels();
    }

    hdr.type = 0;
    hdr.format = 0;
    hdr.width = image.width();
    hdr.height = image.height();

    if (!writeWBMPHeader(iodev, hdr))
        return false;

    if (!writeWBMPData(iodev, image))
        return false;

    return true;
}

bool WBMPReader::canRead(QIODevice *device)
{
    if (device) {

        if (device->isSequential())
            return false;

        // Save previous position
        qint64 oldPos = device->pos();

        WBMPHeader hdr;
        if (readWBMPHeader(device, &hdr)) {
            if ((hdr.type == 0) && (hdr.format == 0)) {
                const qint64 imageSize = hdr.height * ((qint64(hdr.width) + 7) / 8);
                qint64 available = device->bytesAvailable();
                device->seek(oldPos);
                return (imageSize == available);
            }
        }
        device->seek(oldPos);
    }
    return false;
}

/*!
    Constructs an instance of QWbmpHandler initialized to use \a device.
*/
QWbmpHandler::QWbmpHandler(QIODevice *device) :
    m_reader(new WBMPReader(device))
{
}

/*!
    Destructor for QWbmpHandler.
*/
QWbmpHandler::~QWbmpHandler()
{
    delete m_reader;
}

/*!
 * Verifies if some values (magic bytes) are set as expected in the header of the file.
 * If the magic bytes were found, it is assumed that the QWbmpHandler can read the file.
 */
bool QWbmpHandler::canRead() const
{
    bool bCanRead = false;

    QIODevice *device = QImageIOHandler::device();
    if (device) {
        bCanRead = QWbmpHandler::canRead(device);
        if (bCanRead)
            setFormat("wbmp");

    } else {
        qWarning("QWbmpHandler::canRead() called with no device");
    }

    return bCanRead;
}

/*! \reimp
*/
bool QWbmpHandler::read(QImage *image)
{
    bool bSuccess = false;
    QImage img = m_reader->readImage();

    if (!img.isNull()) {
        bSuccess = true;
        *image = img;
    }

    return bSuccess;
}

/*! \reimp
*/
bool QWbmpHandler::write(const QImage &image)
{
    if (image.isNull())
        return false;

    return m_reader->writeImage(image);
}

/*!
    Only Size option is supported
*/
QVariant QWbmpHandler::option(ImageOption option) const
{
    if (option == QImageIOHandler::Size) {
        QIODevice *device = QImageIOHandler::device();
        if (device->isSequential())
            return QVariant();

        // Save old position
        qint64 oldPos = device->pos();

        WBMPHeader hdr;
        if (readWBMPHeader(device, &hdr)) {
            device->seek(oldPos);
            return QSize(hdr.width, hdr.height);
        }

        device->seek(oldPos);

    } else if (option == QImageIOHandler::ImageFormat) {
        return QVariant(QImage::Format_Mono);
    }

    return QVariant();
}

bool QWbmpHandler::supportsOption(ImageOption option) const
{
    return (option == QImageIOHandler::Size) ||
           (option == QImageIOHandler::ImageFormat);
}

bool QWbmpHandler::canRead(QIODevice *device)
{
    return WBMPReader::canRead(device);
}

QT_END_NAMESPACE
