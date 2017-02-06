/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickimagebase_p.h"
#include "qquickimagebase_p_p.h"

#include <QtGui/qguiapplication.h>
#include <QtGui/qscreen.h>
#include <QtGui/qicon.h>

#include <QtQml/qqmlinfo.h>
#include <QtQml/qqmlfile.h>

QT_BEGIN_NAMESPACE

QQuickImageBase::QQuickImageBase(QQuickItem *parent)
: QQuickImplicitSizeItem(*(new QQuickImageBasePrivate), parent)
{
    setFlag(ItemHasContents);
}

QQuickImageBase::QQuickImageBase(QQuickImageBasePrivate &dd, QQuickItem *parent)
: QQuickImplicitSizeItem(dd, parent)
{
    setFlag(ItemHasContents);
}

QQuickImageBase::~QQuickImageBase()
{
}

QQuickImageBase::Status QQuickImageBase::status() const
{
    Q_D(const QQuickImageBase);
    return d->status;
}


qreal QQuickImageBase::progress() const
{
    Q_D(const QQuickImageBase);
    return d->progress;
}


bool QQuickImageBase::asynchronous() const
{
    Q_D(const QQuickImageBase);
    return d->async;
}

void QQuickImageBase::setAsynchronous(bool async)
{
    Q_D(QQuickImageBase);
    if (d->async != async) {
        d->async = async;
        emit asynchronousChanged();
    }
}

QUrl QQuickImageBase::source() const
{
    Q_D(const QQuickImageBase);
    return d->url;
}

void QQuickImageBase::setSource(const QUrl &url)
{
    Q_D(QQuickImageBase);

    if (url == d->url)
        return;

    d->url = url;
    emit sourceChanged(d->url);

    if (isComponentComplete())
        load();
}

void QQuickImageBase::setSourceSize(const QSize& size)
{
    Q_D(QQuickImageBase);
    if (d->sourcesize == size)
        return;

    d->sourcesize = size;
    emit sourceSizeChanged();
    if (isComponentComplete())
        load();
}

QSize QQuickImageBase::sourceSize() const
{
    Q_D(const QQuickImageBase);

    int width = d->sourcesize.width();
    int height = d->sourcesize.height();
    return QSize(width != -1 ? width : d->pix.width(), height != -1 ? height : d->pix.height());
}

void QQuickImageBase::resetSourceSize()
{
    setSourceSize(QSize());
}

bool QQuickImageBase::cache() const
{
    Q_D(const QQuickImageBase);
    return d->cache;
}

void QQuickImageBase::setCache(bool cache)
{
    Q_D(QQuickImageBase);
    if (d->cache == cache)
        return;

    d->cache = cache;
    emit cacheChanged();
    if (isComponentComplete())
        load();
}

QImage QQuickImageBase::image() const
{
    Q_D(const QQuickImageBase);
    return d->pix.image();
}

void QQuickImageBase::setMirror(bool mirror)
{
    Q_D(QQuickImageBase);
    if (mirror == d->mirror)
        return;

    d->mirror = mirror;

    if (isComponentComplete())
        update();

    emit mirrorChanged();
}

bool QQuickImageBase::mirror() const
{
    Q_D(const QQuickImageBase);
    return d->mirror;
}

void QQuickImageBase::load()
{
    Q_D(QQuickImageBase);

    if (d->url.isEmpty()) {
        d->pix.clear(this);
        if (d->progress != 0.0) {
            d->progress = 0.0;
            emit progressChanged(d->progress);
        }
        pixmapChange();
        d->status = Null;
        emit statusChanged(d->status);

        if (sourceSize() != d->oldSourceSize) {
            d->oldSourceSize = sourceSize();
            emit sourceSizeChanged();
        }
        if (autoTransform() != d->oldAutoTransform) {
            d->oldAutoTransform = autoTransform();
            emitAutoTransformBaseChanged();
        }
        update();

    } else {
        QQuickPixmap::Options options;
        if (d->async)
            options |= QQuickPixmap::Asynchronous;
        if (d->cache)
            options |= QQuickPixmap::Cache;
        d->pix.clear(this);

        const qreal targetDevicePixelRatio = (window() ? window()->effectiveDevicePixelRatio() : qApp->devicePixelRatio());
        d->devicePixelRatio = 1.0;

        QUrl loadUrl = d->url;

        // QQuickImageProvider and SVG can generate a high resolution image when
        // sourceSize is set. If sourceSize is not set then the provider default size
        // will be used, as usual.
        bool setDevicePixelRatio = false;
        if (d->sourcesize.isValid()) {
            if (loadUrl.scheme() == QLatin1String("image")) {
                setDevicePixelRatio = true;
            } else {
                QString stringUrl = loadUrl.path(QUrl::PrettyDecoded);
                if (stringUrl.endsWith(QLatin1String("svg")) ||
                    stringUrl.endsWith(QLatin1String("svgz"))) {
                    setDevicePixelRatio = true;
                }
            }

            if (setDevicePixelRatio)
                d->devicePixelRatio = targetDevicePixelRatio;
        }

        if (!setDevicePixelRatio) {
            // (possible) local file: loadUrl and d->devicePixelRatio will be modified if
            // an "@2x" file is found.
            resolve2xLocalFile(d->url, targetDevicePixelRatio, &loadUrl, &d->devicePixelRatio);
        }

        d->pix.load(qmlEngine(this), loadUrl, d->sourcesize * d->devicePixelRatio, options, d->autoTransform);

        if (d->pix.isLoading()) {
            if (d->progress != 0.0) {
                d->progress = 0.0;
                emit progressChanged(d->progress);
            }
            if (d->status != Loading) {
                d->status = Loading;
                emit statusChanged(d->status);
            }

            static int thisRequestProgress = -1;
            static int thisRequestFinished = -1;
            if (thisRequestProgress == -1) {
                thisRequestProgress =
                    QQuickImageBase::staticMetaObject.indexOfSlot("requestProgress(qint64,qint64)");
                thisRequestFinished =
                    QQuickImageBase::staticMetaObject.indexOfSlot("requestFinished()");
            }

            d->pix.connectFinished(this, thisRequestFinished);
            d->pix.connectDownloadProgress(this, thisRequestProgress);
            update(); //pixmap may have invalidated texture, updatePaintNode needs to be called before the next repaint
        } else {
            requestFinished();
        }
    }
}

void QQuickImageBase::requestFinished()
{
    Q_D(QQuickImageBase);

    if (d->pix.isError()) {
        qmlInfo(this) << d->pix.error();
        d->pix.clear(this);
        d->status = Error;
        if (d->progress != 0.0) {
            d->progress = 0.0;
            emit progressChanged(d->progress);
        }
    } else {
        d->status = Ready;
        if (d->progress != 1.0) {
            d->progress = 1.0;
            emit progressChanged(d->progress);
        }
    }
    pixmapChange();
    emit statusChanged(d->status);
    if (sourceSize() != d->oldSourceSize) {
        d->oldSourceSize = sourceSize();
        emit sourceSizeChanged();
    }
    if (autoTransform() != d->oldAutoTransform) {
        d->oldAutoTransform = autoTransform();
        emitAutoTransformBaseChanged();
    }
    update();
}

void QQuickImageBase::requestProgress(qint64 received, qint64 total)
{
    Q_D(QQuickImageBase);
    if (d->status == Loading && total > 0) {
        d->progress = qreal(received)/total;
        emit progressChanged(d->progress);
    }
}

void QQuickImageBase::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemSceneChange && value.window)
        connect(value.window, &QQuickWindow::screenChanged, this, &QQuickImageBase::handleScreenChanged);
    QQuickItem::itemChange(change, value);
}

void QQuickImageBase::handleScreenChanged(QScreen* screen)
{
    // Screen DPI might have changed, reload images on screen change.
    if (qmlEngine(this) && screen && isComponentComplete())
        load();
}

void QQuickImageBase::componentComplete()
{
    Q_D(QQuickImageBase);
    QQuickItem::componentComplete();
    if (d->url.isValid())
        load();
}

void QQuickImageBase::pixmapChange()
{
    Q_D(QQuickImageBase);
    setImplicitSize(d->pix.width() / d->devicePixelRatio, d->pix.height() / d->devicePixelRatio);
}

void QQuickImageBase::resolve2xLocalFile(const QUrl &url, qreal targetDevicePixelRatio, QUrl *sourceUrl, qreal *sourceDevicePixelRatio)
{
    Q_ASSERT(sourceUrl);
    Q_ASSERT(sourceDevicePixelRatio);

    // Bail out if "@2x" image loading is disabled, don't change the source url or devicePixelRatio.
    static const bool disable2xImageLoading = !qEnvironmentVariableIsEmpty("QT_HIGHDPI_DISABLE_2X_IMAGE_LOADING");
    if (disable2xImageLoading)
        return;

    const QString localFile = QQmlFile::urlToLocalFileOrQrc(url);

    // Non-local file path: @2x loading is not supported.
    if (localFile.isEmpty())
        return;

    // Special case: the url in the QML source refers directly to an "@2x" file.
    int atLocation = localFile.lastIndexOf(QLatin1Char('@'));
    if (atLocation > 0 && atLocation + 3 < localFile.size()) {
        if (localFile[atLocation + 1].isDigit()
                && localFile[atLocation + 2] == QLatin1Char('x')
                && localFile[atLocation + 3] == QLatin1Char('.')) {
            *sourceDevicePixelRatio = localFile[atLocation + 1].digitValue();
            return;
        }
    }

    // Look for an @2x version
    QString localFileX = qt_findAtNxFile(localFile, targetDevicePixelRatio, sourceDevicePixelRatio);
    if (localFileX != localFile)
        *sourceUrl = QUrl::fromLocalFile(localFileX);
}

bool QQuickImageBase::autoTransform() const
{
    Q_D(const QQuickImageBase);
    if (d->autoTransform == UsePluginDefault)
        return d->pix.autoTransform() == ApplyTransform;
    return d->autoTransform == ApplyTransform;
}

void QQuickImageBase::setAutoTransform(bool transform)
{
    Q_D(QQuickImageBase);
    if (d->autoTransform != UsePluginDefault && transform == (d->autoTransform == ApplyTransform))
        return;
    d->autoTransform = transform ? ApplyTransform : DoNotApplyTransform;
    emitAutoTransformBaseChanged();
}

QT_END_NAMESPACE
