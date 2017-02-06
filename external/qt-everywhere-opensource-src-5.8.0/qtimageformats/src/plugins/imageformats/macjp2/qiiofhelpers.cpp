/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the MacJp2 plugin in the Qt ImageFormats module.
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

#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#include <QBuffer>
#include <QImageIOHandler>
#include <QImage>
#include <private/qcore_mac_p.h>

#include "qiiofhelpers_p.h"

#include <ImageIO/ImageIO.h>

QT_BEGIN_NAMESPACE

// Callbacks for sequential data provider & consumer:

static size_t cbGetBytes(void *info, void *buffer, size_t count)
{
    QIODevice *dev = static_cast<QIODevice *>(info);
    if (!dev || !buffer)
        return 0;
    qint64 res = dev->read(static_cast<char *>(buffer), count);
    return qMax(qint64(0), res);
}

static off_t cbSkipForward(void *info, off_t count)
{
    QIODevice *dev = static_cast<QIODevice *>(info);
    if (!dev || !count)
        return 0;
    qint64 res = 0;
    if (!dev->isSequential()) {
        qint64 prevPos = dev->pos();
        dev->seek(prevPos + count);
        res = dev->pos() - prevPos;
    } else {
        char *buf = new char[count];
        res = dev->read(buf, count);
        delete[] buf;
    }
    return qMax(qint64(0), res);
}

static void cbRewind(void *)
{
    // Ignore this; we do not want the Qt device to be rewound after reading the image
}

static size_t cbPutBytes(void *info, const void *buffer, size_t count)
{
    QIODevice *dev = static_cast<QIODevice *>(info);
    if (!dev || !buffer)
        return 0;
    qint64 res = dev->write(static_cast<const char *>(buffer), count);
    return qMax(qint64(0), res);
}


// QImage <-> CGImage conversion functions
typedef QImage (*cgImageToQImagePtr)(CGImageRef image);
typedef CGImageRef (*qImageToCGImagePtr)(const QImage &image);

bool QIIOFHelpers::readImage(QImageIOHandler *q_ptr, QImage *out)
{
    static const CGDataProviderSequentialCallbacks cgCallbacks = { 0, &cbGetBytes, &cbSkipForward, &cbRewind, nullptr };
    static cgImageToQImagePtr cgImageToQImageFn = nullptr;
    if (!cgImageToQImageFn) {
        if (QPlatformNativeInterface *pni = QGuiApplication::platformNativeInterface())
            cgImageToQImageFn = reinterpret_cast<cgImageToQImagePtr>(pni->nativeResourceFunctionForIntegration(QByteArrayLiteral("cgImageToQImage")));
    }

    if (!q_ptr || !q_ptr->device() || !out || !cgImageToQImageFn)
        return false;

    QCFType<CGDataProviderRef> cgDataProvider;
    if (QBuffer *b = qobject_cast<QBuffer *>(q_ptr->device())) {
        // do direct access to avoid data copy
        const void *rawData = b->data().constData() + b->pos();
        cgDataProvider = CGDataProviderCreateWithData(nullptr, rawData, b->data().size() - b->pos(), nullptr);
    } else {
        cgDataProvider = CGDataProviderCreateSequential(q_ptr->device(), &cgCallbacks);
    }

    QCFType<CGImageSourceRef> cgImageSource = CGImageSourceCreateWithDataProvider(cgDataProvider, nullptr);
    if (!cgImageSource)
        return false;

    QCFType<CGImageRef> cgImage = CGImageSourceCreateImageAtIndex(cgImageSource, 0, nullptr);
    if (!cgImage)
        return false;

    *out = cgImageToQImageFn(cgImage);
    return !out->isNull();
}


bool QIIOFHelpers::writeImage(QImageIOHandler *q_ptr, const QImage &in, const QString &uti)
{
    static const CGDataConsumerCallbacks cgCallbacks = { &cbPutBytes, nullptr };
    static qImageToCGImagePtr qImageToCGImageFn = nullptr;
    if (!qImageToCGImageFn) {
        if (QPlatformNativeInterface *pni = QGuiApplication::platformNativeInterface())
            qImageToCGImageFn = reinterpret_cast<qImageToCGImagePtr>(pni->nativeResourceFunctionForIntegration(QByteArrayLiteral("qImageToCGImage")));
    }

    if (!q_ptr || !q_ptr->device() || in.isNull() || !qImageToCGImageFn)
        return false;

    QCFType<CGImageRef> cgImage = qImageToCGImageFn(in);
    QCFType<CGDataConsumerRef> cgDataConsumer = CGDataConsumerCreate(q_ptr->device(), &cgCallbacks);
    QCFType<CFStringRef> cfUti = uti.toCFString();
    QCFType<CGImageDestinationRef> cgImageDest = CGImageDestinationCreateWithDataConsumer(cgDataConsumer, cfUti, 1, nullptr);
    if (!cgImageDest || !cgImage)
        return false;

    QCFType<CFNumberRef> cfVal;
    QCFType<CFDictionaryRef> cfProps;
    if (q_ptr->supportsOption(QImageIOHandler::Quality)) {
        bool ok = false;
        int writeQuality = q_ptr->option(QImageIOHandler::Quality).toInt(&ok);
        // If quality is unset, default to 75%
        float quality = (ok && writeQuality >= 0 ? (qMin(writeQuality, 100)) : 75) / 100.0;
        cfVal = CFNumberCreate(nullptr, kCFNumberFloatType, &quality);
        cfProps = CFDictionaryCreate(nullptr, (const void **)&kCGImageDestinationLossyCompressionQuality, (const void **)&cfVal, 1,
                                     &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    CGImageDestinationAddImage(cgImageDest, cgImage, cfProps);
    return CGImageDestinationFinalize(cgImageDest);
}

QT_END_NAMESPACE
