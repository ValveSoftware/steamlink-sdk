/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
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

#include <private/qtquickglobal_p.h>
#include "qquickitemgrabresult.h"

#include "qquickwindow.h"
#include "qquickitem.h"
#if QT_CONFIG(quick_shadereffect)
#include "qquickshadereffectsource_p.h"
#endif

#include <QtQml/QQmlEngine>

#include <private/qquickpixmapcache_p.h>
#include <private/qquickitem_p.h>
#include <private/qsgcontext_p.h>
#include <private/qsgadaptationlayer_p.h>

QT_BEGIN_NAMESPACE

const QEvent::Type Event_Grab_Completed = static_cast<QEvent::Type>(QEvent::User + 1);

class QQuickItemGrabResultPrivate : public QObjectPrivate
{
public:
    QQuickItemGrabResultPrivate()
        : cacheEntry(0)
        , qmlEngine(0)
        , texture(0)
    {
    }

    ~QQuickItemGrabResultPrivate()
    {
        delete cacheEntry;
    }

    void ensureImageInCache() const {
        if (url.isEmpty() && !image.isNull()) {
            url.setScheme(QStringLiteral("ItemGrabber"));
            url.setPath(QVariant::fromValue(item.data()).toString());
            static uint counter = 0;
            url.setFragment(QString::number(++counter));
            cacheEntry = new QQuickPixmap(url, image);
        }
    }

    static QQuickItemGrabResult *create(QQuickItem *item, const QSize &size);

    QImage image;

    mutable QUrl url;
    mutable QQuickPixmap *cacheEntry;

    QQmlEngine *qmlEngine;
    QJSValue callback;

    QPointer<QQuickItem> item;
    QPointer<QQuickWindow> window;
    QSGLayer *texture;
    QSizeF itemSize;
    QSize textureSize;
};

/*!
 * \qmlproperty url QtQuick::ItemGrabResult::url
 *
 * This property holds a URL which can be used in conjunction with
 * URL based image consumers, such as the QtQuick::Image type.
 *
 * The URL is valid while there is a reference in QML or JavaScript
 * to the ItemGrabResult or while the image the URL references is
 * actively used.
 *
 * The URL does not represent a valid file or location to read it from, it
 * is primarily a key to access images through Qt Quick's image-based types.
 */

/*!
 * \property QQuickItemGrabResult::url
 *
 * This property holds a URL which can be used in conjunction with
 * URL based image consumers, such as the QtQuick::Image type.
 *
 * The URL is valid until the QQuickItemGrabResult object is deleted.
 *
 * The URL does not represent a valid file or location to read it from, it
 * is primarily a key to access images through Qt Quick's image-based types.
 */

/*!
 * \qmlproperty variant QtQuick::ItemGrabResult::image
 *
 * This property holds the pixel results from a grab in the
 * form of a QImage.
 */

/*!
 * \property QQuickItemGrabResult::image
 *
 * This property holds the pixel results from a grab.
 *
 * If the grab is not yet complete or if it failed,
 * an empty image is returned.
 */

/*!
    \class QQuickItemGrabResult
    \inmodule QtQuick
    \brief The QQuickItemGrabResult contains the result from QQuickItem::grabToImage().

    \sa QQuickItem::grabToImage()
 */

/*!
 * \fn void QQuickItemGrabResult::ready()
 *
 * This signal is emitted when the grab has completed.
 */

/*!
 * \qmltype ItemGrabResult
 * \instantiates QQuickItemGrabResult
 * \inherits QtObject
 * \inqmlmodule QtQuick
 * \ingroup qtquick-visual
 * \brief Contains the results from a call to Item::grabToImage().
 *
 * The ItemGrabResult is a small container used to encapsulate
 * the results from Item::grabToImage().
 *
 * \sa Item::grabToImage()
 */

QQuickItemGrabResult::QQuickItemGrabResult(QObject *parent)
    : QObject(*new QQuickItemGrabResultPrivate, parent)
{
}

/*!
 * \qmlmethod bool QtQuick::ItemGrabResult::saveToFile(fileName)
 *
 * Saves the grab result as an image to \a fileName. Returns true
 * if successful; otherwise returns false.
 */

/*!
 * Saves the grab result as an image to \a fileName. Returns true
 * if successful; otherwise returns false.
 */
bool QQuickItemGrabResult::saveToFile(const QString &fileName)
{
    Q_D(QQuickItemGrabResult);
    return d->image.save(fileName);
}

QUrl QQuickItemGrabResult::url() const
{
    Q_D(const QQuickItemGrabResult);
    d->ensureImageInCache();
    return d->url;
}

QImage QQuickItemGrabResult::image() const
{
    Q_D(const QQuickItemGrabResult);
    return d->image;
}

/*!
 * \internal
 */
bool QQuickItemGrabResult::event(QEvent *e)
{
    Q_D(QQuickItemGrabResult);
    if (e->type() == Event_Grab_Completed) {
        // JS callback
        if (d->qmlEngine && d->callback.isCallable())
            d->callback.call(QJSValueList() << d->qmlEngine->newQObject(this));
        else
            Q_EMIT ready();
        return true;
    }
    return QObject::event(e);
}

void QQuickItemGrabResult::setup()
{
    Q_D(QQuickItemGrabResult);
    if (!d->item) {
        disconnect(d->window.data(), &QQuickWindow::beforeSynchronizing, this, &QQuickItemGrabResult::setup);
        disconnect(d->window.data(), &QQuickWindow::afterRendering, this, &QQuickItemGrabResult::render);
        QCoreApplication::postEvent(this, new QEvent(Event_Grab_Completed));
        return;
    }

    QSGRenderContext *rc = QQuickWindowPrivate::get(d->window.data())->context;
    d->texture = rc->sceneGraphContext()->createLayer(rc);
    d->texture->setItem(QQuickItemPrivate::get(d->item)->itemNode());
    d->itemSize = QSizeF(d->item->width(), d->item->height());
}

void QQuickItemGrabResult::render()
{
    Q_D(QQuickItemGrabResult);
    if (!d->texture)
        return;

    d->texture->setRect(QRectF(0, d->itemSize.height(), d->itemSize.width(), -d->itemSize.height()));
    const QSize minSize = QQuickWindowPrivate::get(d->window.data())->context->sceneGraphContext()->minimumFBOSize();
    d->texture->setSize(QSize(qMax(minSize.width(), d->textureSize.width()),
                              qMax(minSize.height(), d->textureSize.height())));
    d->texture->scheduleUpdate();
    d->texture->updateTexture();
    d->image =  d->texture->toImage();

    delete d->texture;
    d->texture = 0;

    disconnect(d->window.data(), &QQuickWindow::beforeSynchronizing, this, &QQuickItemGrabResult::setup);
    disconnect(d->window.data(), &QQuickWindow::afterRendering, this, &QQuickItemGrabResult::render);
    QCoreApplication::postEvent(this, new QEvent(Event_Grab_Completed));
}

QQuickItemGrabResult *QQuickItemGrabResultPrivate::create(QQuickItem *item, const QSize &targetSize)
{
    QSize size = targetSize;
    if (size.isEmpty())
        size = QSize(item->width(), item->height());

    if (size.width() < 1 || size.height() < 1) {
        qWarning("Item::grabToImage: item has invalid dimensions");
        return 0;
    }

    if (!item->window()) {
        qWarning("Item::grabToImage: item is not attached to a window");
        return 0;
    }

    if (!item->window()->isVisible()) {
        qWarning("Item::grabToImage: item's window is not visible");
        return 0;
    }

    QQuickItemGrabResult *result = new QQuickItemGrabResult();
    QQuickItemGrabResultPrivate *d = result->d_func();
    d->item = item;
    d->window = item->window();
    d->textureSize = size;

    QQuickItemPrivate::get(item)->refFromEffectItem(false);

    // trigger sync & render
    item->window()->update();

    return result;
}

/*!
 * Grabs the item into an in-memory image.
 *
 * The grab happens asynchronously and the signal QQuickItemGrabResult::ready()
 * is emitted when the grab has been completed.
 *
 * Use \a targetSize to specify the size of the target image. By default, the
 * result will have the same size as item.
 *
 * If the grab could not be initiated, the function returns \c null.
 *
 * \note This function will render the item to an offscreen surface and
 * copy that surface from the GPU's memory into the CPU's memory, which can
 * be quite costly. For "live" preview, use \l {QtQuick::Item::layer.enabled} {layers}
 * or ShaderEffectSource.
 *
 * \sa QQuickWindow::grabWindow()
 */
QSharedPointer<QQuickItemGrabResult> QQuickItem::grabToImage(const QSize &targetSize)
{
    QQuickItemGrabResult *result = QQuickItemGrabResultPrivate::create(this, targetSize);
    if (!result)
        return QSharedPointer<QQuickItemGrabResult>();

    connect(window(), &QQuickWindow::beforeSynchronizing, result, &QQuickItemGrabResult::setup, Qt::DirectConnection);
    connect(window(), &QQuickWindow::afterRendering, result, &QQuickItemGrabResult::render, Qt::DirectConnection);

    return QSharedPointer<QQuickItemGrabResult>(result);
}

/*!
 * \qmlmethod bool QtQuick::Item::grabToImage(callback, targetSize)
 *
 * Grabs the item into an in-memory image.
 *
 * The grab happens asynchronously and the JavaScript function \a callback is
 * invoked when the grab is completed. The callback takes one argument, which
 * is the result of the grab operation; an \l ItemGrabResult object.
 *
 * Use \a targetSize to specify the size of the target image. By default, the result
 * will have the same size as the item.
 *
 * If the grab could not be initiated, the function returns \c false.
 *
 * The following snippet shows how to grab an item and store the results to
 * a file.
 *
 * \snippet qml/itemGrab.qml grab-source
 * \snippet qml/itemGrab.qml grab-to-file
 *
 * The following snippet shows how to grab an item and use the results in
 * another image element.
 *
 * \snippet qml/itemGrab.qml grab-image-target
 * \snippet qml/itemGrab.qml grab-to-cache
 *
 * \note This function will render the item to an offscreen surface and
 * copy that surface from the GPU's memory into the CPU's memory, which can
 * be quite costly. For "live" preview, use \l {QtQuick::Item::layer.enabled} {layers}
 * or ShaderEffectSource.
 */

/*!
 * \internal
 * Only visible from QML.
 */
bool QQuickItem::grabToImage(const QJSValue &callback, const QSize &targetSize)
{
    QQmlEngine *engine = qmlEngine(this);
    if (!engine) {
        qWarning("Item::grabToImage: no QML Engine");
        return false;
    }

    if (!callback.isCallable()) {
        qWarning("Item::grabToImage: 'callback' is not a function");
        return false;
    }

    QSize size = targetSize;
    if (size.isEmpty())
        size = QSize(width(), height());

    if (size.width() < 1 || size.height() < 1) {
        qWarning("Item::grabToImage: item has invalid dimensions");
        return false;
    }

    if (!window()) {
        qWarning("Item::grabToImage: item is not attached to a window");
        return false;
    }

    QQuickItemGrabResult *result = QQuickItemGrabResultPrivate::create(this, size);
    if (!result)
        return false;

    connect(window(), &QQuickWindow::beforeSynchronizing, result, &QQuickItemGrabResult::setup, Qt::DirectConnection);
    connect(window(), &QQuickWindow::afterRendering, result, &QQuickItemGrabResult::render, Qt::DirectConnection);

    QQuickItemGrabResultPrivate *d = result->d_func();
    d->qmlEngine = engine;
    d->callback = callback;
    return true;
}

QT_END_NAMESPACE
