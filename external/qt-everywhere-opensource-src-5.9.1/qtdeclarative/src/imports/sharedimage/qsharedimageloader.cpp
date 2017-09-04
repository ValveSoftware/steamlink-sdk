/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qsharedimageloader_p.h"
#include <private/qobject_p.h>
#include <private/qimage_p.h>
#include <QSharedMemory>


QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcSharedImage, "qt.quick.sharedimage");

struct SharedImageHeader {
    quint8 magic;
    quint8 version;
    quint16 offset;
    qint32 width;
    qint32 height;
    qint32 bpl;
    QImage::Format format;
};
Q_STATIC_ASSERT(sizeof(SharedImageHeader) % 4 == 0);

#if QT_CONFIG(sharedmemory)
struct SharedImageInfo {
    QString path;
    QPointer<QSharedMemory> shmp;
};

void cleanupSharedImage(void *cleanupInfo)
{
    if (!cleanupInfo)
        return;
    SharedImageInfo *sii = static_cast<SharedImageInfo *>(cleanupInfo);
    qCDebug(lcSharedImage) << "Cleanup called for" << sii->path;
    if (sii->shmp.isNull()) {
        qCDebug(lcSharedImage) << "shm is 0 for" << sii->path;
        return;
    }
    QSharedMemory *shm = sii->shmp.data();
    sii->shmp.clear();
    delete shm;    // destructor detaches
    delete sii;
}
#else
void cleanupSharedImage(void *) {}
#endif

class QSharedImageLoaderPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QSharedImageLoader)

public:
    QSharedImageLoaderPrivate()
        : QObjectPrivate()
    {}

    QImage load(const QString &path, QSharedImageLoader::ImageParameters *params);

    void storeImageToMem(void *data, const QImage &img);

    bool verifyMem(const void *data, int size);

    QImage createImageFromMem(const void *data, void *cleanupInfo);

};


void QSharedImageLoaderPrivate::storeImageToMem(void *data, const QImage &img)
{
    Q_ASSERT(data && !img.isNull());

    SharedImageHeader *h = static_cast<SharedImageHeader *>(data);
    h->magic = 'Q';
    h->version = 1;
    h->offset = sizeof(SharedImageHeader);
    h->width = img.width();
    h->height = img.height();
    h->bpl = img.bytesPerLine();
    h->format = img.format();

    uchar *p = static_cast<uchar *>(data) + sizeof(SharedImageHeader);
    memcpy(p, img.constBits(), img.byteCount());
}


bool QSharedImageLoaderPrivate::verifyMem(const void *data, int size)
{
    if (!data || size < int(sizeof(SharedImageHeader)))
        return false;

    const SharedImageHeader *h = static_cast<const SharedImageHeader *>(data);
    if ((h->magic != 'Q')
        || (h->version < 1)
        || (h->offset < sizeof(SharedImageHeader))
        || (h->width <= 0)
        || (h->height <= 0)
        || (h->bpl <= 0)
        || (h->format <= QImage::Format_Invalid)
        || (h->format >= QImage::NImageFormats)) {
        return false;
    }

    int availSize = size - h->offset;
    if (h->height * h->bpl > availSize)
        return false;
    if ((qt_depthForFormat(h->format) * h->width * h->height) > (8 * availSize))
        return false;

    return true;
}


QImage QSharedImageLoaderPrivate::createImageFromMem(const void *data, void *cleanupInfo)
{
    const SharedImageHeader *h = static_cast<const SharedImageHeader *>(data);
    const uchar *p = static_cast<const uchar *>(data) + h->offset;

    QImage img(p, h->width, h->height, h->bpl, h->format, cleanupSharedImage, cleanupInfo);
    return img;
}


QImage QSharedImageLoaderPrivate::load(const QString &path, QSharedImageLoader::ImageParameters *params)
{
#if QT_CONFIG(sharedmemory)
    Q_Q(QSharedImageLoader);

    QImage nil;
    if (path.isEmpty())
        return nil;

    QScopedPointer<QSharedMemory> shm(new QSharedMemory(q->key(path, params)));
    bool locked = false;

    if (!shm->attach(QSharedMemory::ReadOnly)) {
        QImage img = q->loadFile(path, params);
        if (img.isNull())
            return nil;
        int size = sizeof(SharedImageHeader) + img.byteCount();
        if (shm->create(size)) {
            qCDebug(lcSharedImage) << "Created new shm segment of size" << size << "for image" << path;
            if (!shm->lock()) {
                qCDebug(lcSharedImage) << "Lock1 failed!?" << shm->errorString();
                return nil;
            }
            locked = true;
            storeImageToMem(shm->data(), img);
        } else if (shm->error() == QSharedMemory::AlreadyExists) {
            // race handling: other process may have created the share while
            // we loaded the image, so try again to just attach
            if (!shm->attach(QSharedMemory::ReadOnly)) {
                qCDebug(lcSharedImage) << "Attach to existing failed?" << shm->errorString();
                return nil;
            }
        } else {
            qCDebug(lcSharedImage) << "Create failed?" << shm->errorString();
            return nil;
        }
    }

    Q_ASSERT(shm->isAttached());

    if (!locked) {
        if (!shm->lock()) {
            qCDebug(lcSharedImage) << "Lock2 failed!?" << shm->errorString();
            return nil;
        }
        locked = true;
    }

    if (!verifyMem(shm->constData(), shm->size())) {
        qCDebug(lcSharedImage) << "Verifymem failed!?";
        shm->unlock();
        return nil;
    }

    QSharedMemory *shmp = shm.take();
    SharedImageInfo *sii = new SharedImageInfo;
    sii->path = path;
    sii->shmp = shmp;
    QImage shImg = createImageFromMem(shmp->constData(), sii);

    if (!shmp->unlock()) {
        qCDebug(lcSharedImage) << "UnLock failed!?";
    }

    return shImg;
#else
    Q_UNUSED(path);
    Q_UNUSED(params);
    return QImage();
#endif
}


QSharedImageLoader::QSharedImageLoader(QObject *parent)
    : QObject(*new QSharedImageLoaderPrivate, parent)
{
}

QSharedImageLoader::~QSharedImageLoader()
{
}

QImage QSharedImageLoader::load(const QString &path, ImageParameters *params)
{
    Q_D(QSharedImageLoader);

    return d->load(path, params);
}

QImage QSharedImageLoader::loadFile(const QString &path, ImageParameters *params)
{
    Q_UNUSED(params);

    return QImage(path);
}

QString QSharedImageLoader::key(const QString &path, ImageParameters *params)
{
    Q_UNUSED(params);

    return path;
}


QT_END_NAMESPACE
