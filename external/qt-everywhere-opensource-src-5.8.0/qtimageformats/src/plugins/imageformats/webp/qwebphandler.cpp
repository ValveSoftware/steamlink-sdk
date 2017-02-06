/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the WebP plugins in the Qt ImageFormats module.
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

#include "qwebphandler_p.h"
#include "webp/encode.h"
#include <qcolor.h>
#include <qimage.h>
#include <qdebug.h>
#include <qpainter.h>
#include <qvariant.h>

static const int riffHeaderSize = 12; // RIFF_HEADER_SIZE from webp/format_constants.h

QWebpHandler::QWebpHandler() :
    m_lossless(false),
    m_quality(75),
    m_scanState(ScanNotScanned),
    m_features(),
    m_loop(0),
    m_frameCount(0),
    m_demuxer(NULL),
    m_composited(NULL)
{
    memset(&m_iter, 0, sizeof(m_iter));
}

QWebpHandler::~QWebpHandler()
{
    WebPDemuxReleaseIterator(&m_iter);
    WebPDemuxDelete(m_demuxer);
    delete m_composited;
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
    if (WebPGetFeatures((const uint8_t*)header.constData(), header.size(), &(that->m_features)) == VP8_STATUS_OK) {
        if (m_features.has_animation) {
            // For animation, we have to read and scan whole file to determine loop count and images count
            device()->seek(oldPos);

            if (that->ensureDemuxer()) {
                that->m_loop = WebPDemuxGetI(m_demuxer, WEBP_FF_LOOP_COUNT);
                that->m_frameCount = WebPDemuxGetI(m_demuxer, WEBP_FF_FRAME_COUNT);
                that->m_bgColor = QColor::fromRgba(QRgb(WebPDemuxGetI(m_demuxer, WEBP_FF_BACKGROUND_COLOR)));

                that->m_composited = new QImage(that->m_features.width, that->m_features.height, QImage::Format_ARGB32);

                // We do not reset device position since we have read in all data
                m_scanState = ScanSuccess;
                return true;
            }
        } else {
            m_scanState = ScanSuccess;
        }
    }

    device()->seek(oldPos);

    return m_scanState == ScanSuccess;
}

bool QWebpHandler::ensureDemuxer()
{
    if (m_demuxer)
        return true;

    m_rawData = device()->readAll();
    m_webpData.bytes = reinterpret_cast<const uint8_t *>(m_rawData.constData());
    m_webpData.size = m_rawData.size();

    m_demuxer = WebPDemux(&m_webpData);
    if (m_demuxer == NULL)
        return false;

    return true;
}

bool QWebpHandler::read(QImage *image)
{
    if (!ensureScanned() || device()->isSequential() || !ensureDemuxer())
        return false;

    if (m_iter.frame_num == 0) {
        // Go to first frame
        if (!WebPDemuxGetFrame(m_demuxer, 1, &m_iter))
            return false;
    } else {
        // Go to next frame
        if (!WebPDemuxNextFrame(&m_iter))
            return false;
    }

    WebPBitstreamFeatures features;
    VP8StatusCode status = WebPGetFeatures(m_iter.fragment.bytes, m_iter.fragment.size, &features);
    if (status != VP8_STATUS_OK)
        return false;

    QImage frame(m_iter.width, m_iter.height, QImage::Format_ARGB32);
    uint8_t *output = frame.bits();
    size_t output_size = frame.byteCount();
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    if (!WebPDecodeBGRAInto(
        reinterpret_cast<const uint8_t*>(m_iter.fragment.bytes), m_iter.fragment.size,
        output, output_size, frame.bytesPerLine()))
#else
    if (!WebPDecodeARGBInto(
        reinterpret_cast<const uint8_t*>(m_iter.fragment.bytes), m_iter.fragment.size,
        output, output_size, frame.bytesPerLine()))
#endif
        return false;

    if (!m_features.has_animation) {
        // Single image
        *image = frame;
    } else {
        // Animation
        QPainter painter(m_composited);
        painter.drawImage(currentImageRect(), frame);

        *image = *m_composited;
    }

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
    case Animation:
        return m_features.has_animation;
    case BackgroundColor:
        return m_bgColor;
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
    return option == Quality
        || option == Size
        || option == Animation
        || option == BackgroundColor;
}

QByteArray QWebpHandler::name() const
{
    return QByteArrayLiteral("webp");
}

int QWebpHandler::imageCount() const
{
    if (!ensureScanned())
        return 0;

    if (!m_features.has_animation)
        return 1;

    return m_frameCount;
}

int QWebpHandler::currentImageNumber() const
{
    if (!ensureScanned() || !m_features.has_animation)
        return 0;

    // Frame number in WebP starts from 1
    return m_iter.frame_num - 1;
}

QRect QWebpHandler::currentImageRect() const
{
    if (!ensureScanned())
        return QRect();

    return QRect(m_iter.x_offset, m_iter.y_offset, m_iter.width, m_iter.height);
}

int QWebpHandler::loopCount() const
{
    if (!ensureScanned() || !m_features.has_animation)
        return 0;

    // Loop count in WebP starts from 0
    return m_loop - 1;
}

int QWebpHandler::nextImageDelay() const
{
    if (!ensureScanned() || !m_features.has_animation)
        return 0;

    return m_iter.duration;
}
