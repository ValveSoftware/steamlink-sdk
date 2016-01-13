/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the WebP plugins in the Qt ImageFormats module.
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

#include "qwebphandler_p.h"
#include "webp/encode.h"
#include <qimage.h>
#include <qdebug.h>
#include <qvariant.h>

static const int riffHeaderSize = 12; // RIFF_HEADER_SIZE from webp/format_constants.h

QWebpHandler::QWebpHandler() :
    m_lossless(false),
    m_quality(75),
    m_scanState(ScanNotScanned)
{
}

bool QWebpHandler::canRead() const
{
    if (m_scanState == ScanNotScanned && !canRead(device()))
        return false;

    if (m_scanState != ScanError) {
        setFormat(QByteArrayLiteral("webp"));
        return true;
    }
    return false;
}

bool QWebpHandler::canRead(QIODevice *device)
{
    if (!device) {
        qWarning("QWebpHandler::canRead() called with no device");
        return false;
    }

    QByteArray header = device->peek(riffHeaderSize);
    return header.startsWith("RIFF") && header.endsWith("WEBP");
}

bool QWebpHandler::ensureScanned() const
{
    if (m_scanState != ScanNotScanned)
        return m_scanState == ScanSuccess;

    m_scanState = ScanError;

    if (device()->isSequential()) {
        qWarning() << "Sequential devices are not supported";
        return false;
    }

    qint64 oldPos = device()->pos();
    device()->seek(0);

    QWebpHandler *that = const_cast<QWebpHandler *>(this);
    QByteArray header = device()->peek(sizeof(WebPBitstreamFeatures));
    if (WebPGetFeatures((const uint8_t*)header.constData(), header.size(), &(that->m_features)) == VP8_STATUS_OK)
        m_scanState = ScanSuccess;

    device()->seek(oldPos);

    return m_scanState == ScanSuccess;
}

bool QWebpHandler::read(QImage *image)
{
    if (!ensureScanned() || device()->isSequential())
        return false;

    QByteArray data = device()->readAll();
    QImage result(m_features.width, m_features.height, QImage::Format_ARGB32);
    uint8_t *output = result.bits();
    size_t output_size = result.byteCount();
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    if (!WebPDecodeBGRAInto(reinterpret_cast<const uint8_t*>(data.constData()), data.size(), output, output_size, result.bytesPerLine()))
#else
    if (!WebPDecodeARGBInto(reinterpret_cast<const uint8_t*>(data.constData()), data.size(), output, output_size, result.bytesPerLine()))
#endif
        return false;

    *image = result;
    return true;
}

static int pictureWriter(const quint8 *data, size_t data_size, const WebPPicture *const pic)
{
    QIODevice *io = reinterpret_cast<QIODevice*>(pic->custom_ptr);

    return data_size ? ((quint64)(io->write((const char*)data, data_size)) == data_size) : 1;
}

bool QWebpHandler::write(const QImage &image)
{
    if (image.isNull()) {
        qWarning() << "source image is null.";
        return false;
    }

    QImage srcImage = image;
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    if (srcImage.format() != QImage::Format_ARGB32)
        srcImage = srcImage.convertToFormat(QImage::Format_ARGB32);
#else /* Q_BIG_ENDIAN */
    if (srcImage.format() != QImage::Format_RGBA8888)
        srcImage = srcImage.convertToFormat(QImage::Format_RGBA8888);
#endif

    WebPPicture picture;
    WebPConfig config;

    if (!WebPPictureInit(&picture) || !WebPConfigInit(&config)) {
        qWarning() << "failed to init webp picture and config";
        return false;
    }

    picture.width = srcImage.width();
    picture.height = srcImage.height();
    picture.use_argb = 1;
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    if (!WebPPictureImportBGRA(&picture, srcImage.bits(), srcImage.bytesPerLine())) {
#else /* Q_BIG_ENDIAN */
    if (!WebPPictureImportRGBA(&picture, srcImage.bits(), srcImage.bytesPerLine())) {
#endif
        qWarning() << "failed to import image data to webp picture.";

        WebPPictureFree(&picture);
        return false;
    }

    config.lossless = m_lossless;
    config.quality = m_quality;
    picture.writer = pictureWriter;
    picture.custom_ptr = device();

    if (!WebPEncode(&config, &picture)) {
        qWarning() << "failed to encode webp picture, error code: " << picture.error_code;
        WebPPictureFree(&picture);
        return false;
    }

    WebPPictureFree(&picture);

    return true;
}

QVariant QWebpHandler::option(ImageOption option) const
{
    if (!supportsOption(option) || !ensureScanned())
        return QVariant();

    switch (option) {
    case Quality:
        return m_quality;
    case Size:
        return QSize(m_features.width, m_features.height);
    default:
        return QVariant();
    }
}

void QWebpHandler::setOption(ImageOption option, const QVariant &value)
{
    switch (option) {
    case Quality:
        m_quality = qBound(0, value.toInt(), 100);
        m_lossless = (m_quality >= 100);
        return;
    default:
        break;
    }
    return QImageIOHandler::setOption(option, value);
}

bool QWebpHandler::supportsOption(ImageOption option) const
{
    return option == Quality || option == Size;
}

QByteArray QWebpHandler::name() const
{
    return QByteArrayLiteral("webp");
}
