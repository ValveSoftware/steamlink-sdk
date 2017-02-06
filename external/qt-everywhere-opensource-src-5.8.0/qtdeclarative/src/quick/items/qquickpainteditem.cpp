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

#include "qquickpainteditem.h"
#include <private/qquickpainteditem_p.h>

#include <QtQuick/private/qsgdefaultpainternode_p.h>
#include <QtQuick/private/qsgcontext_p.h>
#include <private/qsgadaptationlayer_p.h>
#include <qsgtextureprovider.h>

#include <qmath.h>

QT_BEGIN_NAMESPACE

class QQuickPaintedItemTextureProvider : public QSGTextureProvider
{
public:
    QSGPainterNode *node;
    QSGTexture *texture() const { return node ? node->texture() : 0; }
    void fireTextureChanged() { emit textureChanged(); }
};

/*!
    \class QQuickPaintedItem
    \brief The QQuickPaintedItem class provides a way to use the QPainter API in the
    QML Scene Graph.

    \inmodule QtQuick

    The QQuickPaintedItem makes it possible to use the QPainter API with the
    QML Scene Graph. It sets up a textured rectangle in the Scene Graph and
    uses a QPainter to paint onto the texture. The render target can be either
    a QImage or, when OpenGL is in use, a QOpenGLFramebufferObject. When the
    render target is a QImage, QPainter first renders into the image then the
    content is uploaded to the texture. When a QOpenGLFramebufferObject is
    used, QPainter paints directly onto the texture. Call update() to trigger a
    repaint.

    To enable QPainter to do anti-aliased rendering, use setAntialiasing().

    To write your own painted item, you first create a subclass of
    QQuickPaintedItem, and then start by implementing its only pure virtual
    public function: paint(), which implements the actual painting. The
    painting will be inside the rectangle spanning from 0,0 to
    width(),height().

    \note It important to understand the performance implications such items
    can incur. See QQuickPaintedItem::RenderTarget and
    QQuickPaintedItem::renderTarget.
*/

/*!
    \enum QQuickPaintedItem::RenderTarget

    This enum describes QQuickPaintedItem's render targets. The render target is the
    surface QPainter paints onto before the item is rendered on screen.

    \value Image The default; QPainter paints into a QImage using the raster paint engine.
    The image's content needs to be uploaded to graphics memory afterward, this operation
    can potentially be slow if the item is large. This render target allows high quality
    anti-aliasing and fast item resizing.

    \value FramebufferObject QPainter paints into a QOpenGLFramebufferObject using the GL
    paint engine. Painting can be faster as no texture upload is required, but anti-aliasing
    quality is not as good as if using an image. This render target allows faster rendering
    in some cases, but you should avoid using it if the item is resized often.

    \value InvertedYFramebufferObject Exactly as for FramebufferObject above, except once
    the painting is done, prior to rendering the painted image is flipped about the
    x-axis so that the top-most pixels are now at the bottom.  Since this is done with the
    OpenGL texture coordinates it is a much faster way to achieve this effect than using a
    painter transform.

    \sa setRenderTarget()
*/

/*!
    \enum QQuickPaintedItem::PerformanceHint

    This enum describes flags that you can enable to improve rendering
    performance in QQuickPaintedItem. By default, none of these flags are set.

    \value FastFBOResizing Resizing an FBO can be a costly operation on a few
    OpenGL driver implementations. To work around this, one can set this flag
    to let the QQuickPaintedItem allocate one large framebuffer object and
    instead draw into a subregion of it. This saves the resize at the cost of
    using more memory. Please note that this is not a common problem.

*/

/*!
    \internal
*/
QQuickPaintedItemPrivate::QQuickPaintedItemPrivate()
    : QQuickItemPrivate()
    , contentsScale(1.0)
    , fillColor(Qt::transparent)
    , renderTarget(QQuickPaintedItem::Image)
    , performanceHints(0)
    , opaquePainting(false)
    , antialiasing(false)
    , mipmap(false)
    , textureProvider(0)
    , node(0)
{
}

/*!
    Constructs a QQuickPaintedItem with the given \a parent item.
 */
QQuickPaintedItem::QQuickPaintedItem(QQuickItem *parent)
    : QQuickItem(*(new QQuickPaintedItemPrivate), parent)
{
    setFlag(ItemHasContents);
}

/*!
    \internal
*/
QQuickPaintedItem::QQuickPaintedItem(QQuickPaintedItemPrivate &dd, QQuickItem *parent)
    : QQuickItem(dd, parent)
{
    setFlag(ItemHasContents);
}

/*!
    Destroys the QQuickPaintedItem.
*/
QQuickPaintedItem::~QQuickPaintedItem()
{
    Q_D(QQuickPaintedItem);
    if (d->textureProvider)
        QQuickWindowQObjectCleanupJob::schedule(window(), d->textureProvider);
}

/*!
    Schedules a redraw of the area covered by \a rect in this item. You can call this function
    whenever your item needs to be redrawn, such as if it changes appearance or size.

    This function does not cause an immediate paint; instead it schedules a paint request that
    is processed by the QML Scene Graph when the next frame is rendered. The item will only be
    redrawn if it is visible.

    \sa paint()
*/
void QQuickPaintedItem::update(const QRect &rect)
{
    Q_D(QQuickPaintedItem);

    if (rect.isNull() && !d->dirtyRect.isNull())
        d->dirtyRect = contentsBoundingRect().toAlignedRect();
    else
        d->dirtyRect |= (contentsBoundingRect() & rect).toAlignedRect();
    QQuickItem::update();
}

/*!
    Returns true if this item is opaque; otherwise, false is returned.

    By default, painted items are not opaque.

    \sa setOpaquePainting()
*/
bool QQuickPaintedItem::opaquePainting() const
{
    Q_D(const QQuickPaintedItem);
    return d->opaquePainting;
}

/*!
    If \a opaque is true, the item is opaque; otherwise, it is considered as translucent.

    Opaque items are not blended with the rest of the scene, you should set this to true
    if the content of the item is opaque to speed up rendering.

    By default, painted items are not opaque.

    \sa opaquePainting()
*/
void QQuickPaintedItem::setOpaquePainting(bool opaque)
{
    Q_D(QQuickPaintedItem);

    if (d->opaquePainting == opaque)
        return;

    d->opaquePainting = opaque;
    QQuickItem::update();
}

/*!
    Returns true if antialiased painting is enabled; otherwise, false is returned.

    By default, antialiasing is not enabled.

    \sa setAntialiasing()
*/
bool QQuickPaintedItem::antialiasing() const
{
    Q_D(const QQuickPaintedItem);
    return d->antialiasing;
}

/*!
    If \a enable is true, antialiased painting is enabled.

    By default, antialiasing is not enabled.

    \sa antialiasing()
*/
void QQuickPaintedItem::setAntialiasing(bool enable)
{
    Q_D(QQuickPaintedItem);

    if (d->antialiasing == enable)
        return;

    d->antialiasing = enable;
    update();
}

/*!
    Returns true if mipmaps are enabled; otherwise, false is returned.

    By default, mipmapping is not enabled.

    \sa setMipmap()
*/
bool QQuickPaintedItem::mipmap() const
{
    Q_D(const QQuickPaintedItem);
    return d->mipmap;
}

/*!
    If \a enable is true, mipmapping is enabled on the associated texture.

    Mipmapping increases rendering speed and reduces aliasing artifacts when the item is
    scaled down.

    By default, mipmapping is not enabled.

    \sa mipmap()
*/
void QQuickPaintedItem::setMipmap(bool enable)
{
    Q_D(QQuickPaintedItem);

    if (d->mipmap == enable)
        return;

    d->mipmap = enable;
    update();
}

/*!
    Returns the performance hints.

    By default, no performance hint is enabled.

    \sa setPerformanceHint(), setPerformanceHints()
*/
QQuickPaintedItem::PerformanceHints QQuickPaintedItem::performanceHints() const
{
    Q_D(const QQuickPaintedItem);
    return d->performanceHints;
}

/*!
    Sets the given performance \a hint on the item if \a enabled is true;
    otherwise clears the performance hint.

    By default, no performance hint is enabled/

    \sa setPerformanceHints(), performanceHints()
*/
void QQuickPaintedItem::setPerformanceHint(QQuickPaintedItem::PerformanceHint hint, bool enabled)
{
    Q_D(QQuickPaintedItem);
    PerformanceHints oldHints = d->performanceHints;
    if (enabled)
        d->performanceHints |= hint;
    else
        d->performanceHints &= ~hint;
    if (oldHints != d->performanceHints)
       update();
}

/*!
    Sets the performance hints to \a hints

    By default, no performance hint is enabled/

    \sa setPerformanceHint(), performanceHints()
*/
void QQuickPaintedItem::setPerformanceHints(QQuickPaintedItem::PerformanceHints hints)
{
    Q_D(QQuickPaintedItem);
    if (d->performanceHints == hints)
        return;
    d->performanceHints = hints;
    update();
}

QSize QQuickPaintedItem::textureSize() const
{
    Q_D(const QQuickPaintedItem);
    return d->textureSize;
}

/*!
    \property QQuickPaintedItem::textureSize

    \brief Defines the size of the texture.

    Changing the texture's size does not affect the coordinate system used in
    paint(). A scale factor is instead applied so painting should still happen
    inside 0,0 to width(),height().

    By default, the texture size will have the same size as this item.

    \note If the item is on a window with a device pixel ratio different from
    1, this scale factor will be implicitly applied to the texture size.

 */
void QQuickPaintedItem::setTextureSize(const QSize &size)
{
    Q_D(QQuickPaintedItem);
    if (d->textureSize == size)
        return;
    d->textureSize = size;
    emit textureSizeChanged();
}

#if QT_VERSION >= 0x060000
#warning "Remove: QQuickPaintedItem::contentsBoundingRect, contentsScale, contentsSize. Also remove them from qsgadaptationlayer_p.h and qsgdefaultpainternode.h/cpp."
#endif

/*!
    \obsolete

    This function is provided for compatibility, use size in combination
    with textureSize to decide the size of what you are drawing.

    \sa width(), height(), textureSize()
*/
QRectF QQuickPaintedItem::contentsBoundingRect() const
{
    Q_D(const QQuickPaintedItem);

    qreal w = d->width;
    QSizeF sz = d->contentsSize * d->contentsScale;
    if (w < sz.width())
        w = sz.width();
    qreal h = d->height;
    if (h < sz.height())
        h = sz.height();

    return QRectF(0, 0, w, h);
}

/*!
    \property QQuickPaintedItem::contentsSize
    \brief Obsolete method for setting the contents size.
    \obsolete

    This function is provided for compatibility, use size in combination
    with textureSize to decide the size of what you are drawing.

    \sa width(), height(), textureSize()
*/
QSize QQuickPaintedItem::contentsSize() const
{
    Q_D(const QQuickPaintedItem);
    return d->contentsSize;
}

void QQuickPaintedItem::setContentsSize(const QSize &size)
{
    Q_D(QQuickPaintedItem);

    if (d->contentsSize == size)
        return;

    d->contentsSize = size;
    update();

    emit contentsSizeChanged();
}

/*!
    \obsolete
    This convenience function is equivalent to calling setContentsSize(QSize()).
*/
void QQuickPaintedItem::resetContentsSize()
{
    setContentsSize(QSize());
}

/*!
    \property QQuickPaintedItem::contentsScale
    \brief Obsolete method for scaling the contents.
    \obsolete

    This function is provided for compatibility, use size() in combination
    with textureSize() to decide the size of what you are drawing.

    \sa width(), height(), textureSize()
*/
qreal QQuickPaintedItem::contentsScale() const
{
    Q_D(const QQuickPaintedItem);
    return d->contentsScale;
}

void QQuickPaintedItem::setContentsScale(qreal scale)
{
    Q_D(QQuickPaintedItem);

    if (d->contentsScale == scale)
        return;

    d->contentsScale = scale;
    update();

    emit contentsScaleChanged();
}

/*!
    \property QQuickPaintedItem::fillColor
    \brief The item's background fill color.

    By default, the fill color is set to Qt::transparent.
*/
QColor QQuickPaintedItem::fillColor() const
{
    Q_D(const QQuickPaintedItem);
    return d->fillColor;
}

void QQuickPaintedItem::setFillColor(const QColor &c)
{
    Q_D(QQuickPaintedItem);

    if (d->fillColor == c)
        return;

    d->fillColor = c;
    update();

    emit fillColorChanged();
}

/*!
    \property QQuickPaintedItem::renderTarget
    \brief The item's render target.

    This property defines which render target the QPainter renders into, it can be either
    QQuickPaintedItem::Image, QQuickPaintedItem::FramebufferObject or QQuickPaintedItem::InvertedYFramebufferObject.

    Each has certain benefits, typically performance versus quality. Using a framebuffer
    object avoids a costly upload of the image contents to the texture in graphics memory,
    while using an image enables high quality anti-aliasing.

    \warning Resizing a framebuffer object is a costly operation, avoid using
    the QQuickPaintedItem::FramebufferObject render target if the item gets resized often.

    By default, the render target is QQuickPaintedItem::Image.

    \note Some Qt Quick backends may not support all render target options. For
    example, it is likely that non-OpenGL backends will lack support for
    QQuickPaintedItem::FramebufferObject and
    QQuickPaintedItem::InvertedYFramebufferObject. Requesting these will then
    be ignored.
*/
QQuickPaintedItem::RenderTarget QQuickPaintedItem::renderTarget() const
{
    Q_D(const QQuickPaintedItem);
    return d->renderTarget;
}

void QQuickPaintedItem::setRenderTarget(RenderTarget target)
{
    Q_D(QQuickPaintedItem);

    if (d->renderTarget == target)
        return;

    d->renderTarget = target;
    update();

    emit renderTargetChanged();
}

/*!
    \fn virtual void QQuickPaintedItem::paint(QPainter *painter) = 0

    This function, which is usually called by the QML Scene Graph, paints the
    contents of an item in local coordinates.

    The underlying texture will have a size defined by textureSize when set,
    or the item's size, multiplied by the window's device pixel ratio.

    The function is called after the item has been filled with the fillColor.

    Reimplement this function in a QQuickPaintedItem subclass to provide the
    item's painting implementation, using \a painter.

    \note The QML Scene Graph uses two separate threads, the main thread does things such as
    processing events or updating animations while a second thread does the actual OpenGL rendering.
    As a consequence, paint() is not called from the main GUI thread but from the GL enabled
    renderer thread. At the moment paint() is called, the GUI thread is blocked and this is
    therefore thread-safe.

    \warning Extreme caution must be used when creating QObjects, emitting signals, starting
    timers and similar inside this function as these will have affinity to the rendering thread.

    \sa width(), height(), textureSize
*/

/*!
    \reimp
*/
QSGNode *QQuickPaintedItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    Q_UNUSED(data);
    Q_D(QQuickPaintedItem);

    if (width() <= 0 || height() <= 0) {
        delete oldNode;
        if (d->textureProvider) {
            d->textureProvider->node = 0;
            d->textureProvider->fireTextureChanged();
        }
        return 0;
    }

    QSGPainterNode *node = static_cast<QSGPainterNode *>(oldNode);
    if (!node) {
        node = d->sceneGraphContext()->createPainterNode(this);
        d->node = node;
    }

    bool hasTextureSize = d->textureSize.width() > 0 && d->textureSize.height() > 0;

    // Use the compat mode if any of the compat things are set and
    // textureSize is 0x0.
    if (!hasTextureSize
        && (d->contentsScale != 1
            || (d->contentsSize.width() > 0 && d->contentsSize.height() > 0))) {
        QRectF br = contentsBoundingRect();
        node->setContentsScale(d->contentsScale);
        QSize size = QSize(qRound(br.width()), qRound(br.height()));
        node->setSize(size);
        node->setTextureSize(size);
    } else {
        // The default, use textureSize or item's size * device pixel ratio
        node->setContentsScale(1);
        QSize itemSize(qRound(width()), qRound(height()));
        node->setSize(itemSize);
        QSize textureSize = (hasTextureSize ? d->textureSize : itemSize)
                            * window()->effectiveDevicePixelRatio();
        node->setTextureSize(textureSize);
    }

    node->setPreferredRenderTarget(d->renderTarget);
    node->setFastFBOResizing(d->performanceHints & FastFBOResizing);
    node->setSmoothPainting(d->antialiasing);
    node->setLinearFiltering(d->smooth);
    node->setMipmapping(d->mipmap);
    node->setOpaquePainting(d->opaquePainting);
    node->setFillColor(d->fillColor);
    node->setDirty(d->dirtyRect);
    node->update();

    d->dirtyRect = QRect();

    if (d->textureProvider) {
        d->textureProvider->node = node;
        d->textureProvider->fireTextureChanged();
    }

    return node;
}

/*!
   \reimp
*/
void QQuickPaintedItem::releaseResources()
{
    Q_D(QQuickPaintedItem);
    if (d->textureProvider) {
        QQuickWindowQObjectCleanupJob::schedule(window(), d->textureProvider);
        d->textureProvider = 0;
    }
    d->node = 0; // Managed by the scene graph, just clear the pointer.
}

void QQuickPaintedItem::invalidateSceneGraph()
{
    Q_D(QQuickPaintedItem);
    delete d->textureProvider;
    d->textureProvider = 0;
    d->node = 0; // Managed by the scene graph, just clear the pointer
}

/*!
   \reimp
*/
bool QQuickPaintedItem::isTextureProvider() const
{
    return true;
}

/*!
   \reimp
*/
QSGTextureProvider *QQuickPaintedItem::textureProvider() const
{
    // When Item::layer::enabled == true, QQuickItem will be a texture
    // provider. In this case we should prefer to return the layer rather
    // than the image itself. The layer will include any children and any
    // the image's wrap and fill mode.
    if (QQuickItem::isTextureProvider())
        return QQuickItem::textureProvider();

    Q_D(const QQuickPaintedItem);
#ifndef QT_NO_OPENGL
    QQuickWindow *w = window();
    if (!w || !w->openglContext() || QThread::currentThread() != w->openglContext()->thread()) {
        qWarning("QQuickPaintedItem::textureProvider: can only be queried on the rendering thread of an exposed window");
        return 0;
    }
#endif
    if (!d->textureProvider)
        d->textureProvider = new QQuickPaintedItemTextureProvider();
    d->textureProvider->node = d->node;
    return d->textureProvider;
}

void QQuickPaintedItem::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemDevicePixelRatioHasChanged)
        update();
    QQuickItem::itemChange(change, value);
}

QT_END_NAMESPACE
