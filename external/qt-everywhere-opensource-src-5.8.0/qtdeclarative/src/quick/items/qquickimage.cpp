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

#include "qquickimage_p.h"
#include "qquickimage_p_p.h"

#include <QtQuick/qsgtextureprovider.h>

#include <QtQuick/private/qsgcontext_p.h>
#include <private/qsgadaptationlayer_p.h>
#include <private/qnumeric_p.h>

#include <QtCore/qmath.h>
#include <QtGui/qpainter.h>
#include <QtCore/QRunnable>

QT_BEGIN_NAMESPACE

class QQuickImageTextureProvider : public QSGTextureProvider
{
    Q_OBJECT
public:
    QQuickImageTextureProvider()
        : m_texture(0)
        , m_smooth(false)
    {
    }

    void updateTexture(QSGTexture *texture) {
        if (m_texture == texture)
            return;
        m_texture = texture;
        emit textureChanged();
    }

    QSGTexture *texture() const {
        if (m_texture) {
            m_texture->setFiltering(m_smooth ? QSGTexture::Linear : QSGTexture::Nearest);
            m_texture->setMipmapFiltering(m_mipmap ? QSGTexture::Linear : QSGTexture::None);
            m_texture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
            m_texture->setVerticalWrapMode(QSGTexture::ClampToEdge);
        }
        return m_texture;
    }

    friend class QQuickImage;

    QSGTexture *m_texture;
    bool m_smooth;
    bool m_mipmap;
};

#include "qquickimage.moc"

QQuickImagePrivate::QQuickImagePrivate()
    : fillMode(QQuickImage::Stretch)
    , paintedWidth(0)
    , paintedHeight(0)
    , pixmapChanged(false)
    , mipmap(false)
    , hAlign(QQuickImage::AlignHCenter)
    , vAlign(QQuickImage::AlignVCenter)
    , provider(0)
{
}

/*!
    \qmltype Image
    \instantiates QQuickImage
    \inqmlmodule QtQuick
    \ingroup qtquick-visual
    \inherits Item
    \brief Displays an image

    The Image type displays an image.

    The source of the image is specified as a URL using the \l source property.
    Images can be supplied in any of the standard image formats supported by Qt,
    including bitmap formats such as PNG and JPEG, and vector graphics formats
    such as SVG. If you need to display animated images, use \l AnimatedSprite
    or \l AnimatedImage.

    If the \l{Item::width}{width} and \l{Item::height}{height} properties are not
    specified, the Image automatically uses the size of the loaded image.
    By default, specifying the width and height of the item causes the image
    to be scaled to that size. This behavior can be changed by setting the
    \l fillMode property, allowing the image to be stretched and tiled instead.

    \section1 Example Usage

    The following example shows the simplest usage of the Image type.

    \snippet qml/image.qml document

    \beginfloatleft
    \image declarative-qtlogo.png
    \endfloat

    \clearfloat

    \section1 Performance

    By default, locally available images are loaded immediately, and the user interface
    is blocked until loading is complete. If a large image is to be loaded, it may be
    preferable to load the image in a low priority thread, by enabling the \l asynchronous
    property.

    If the image is obtained from a network rather than a local resource, it is
    automatically loaded asynchronously, and the \l progress and \l status properties
    are updated as appropriate.

    Images are cached and shared internally, so if several Image items have the same \l source,
    only one copy of the image will be loaded.

    \b Note: Images are often the greatest user of memory in QML user interfaces.  It is recommended
    that images which do not form part of the user interface have their
    size bounded via the \l sourceSize property. This is especially important for content
    that is loaded from external sources or provided by the user.

    \sa {Qt Quick Examples - Image Elements}, QQuickImageProvider
*/

QQuickImage::QQuickImage(QQuickItem *parent)
    : QQuickImageBase(*(new QQuickImagePrivate), parent)
{
}

QQuickImage::QQuickImage(QQuickImagePrivate &dd, QQuickItem *parent)
    : QQuickImageBase(dd, parent)
{
}

QQuickImage::~QQuickImage()
{
    Q_D(QQuickImage);
    if (d->provider) {
        // We're guaranteed to have a window() here because the provider would have
        // been released in releaseResources() if we were gone from a window.
        QQuickWindowQObjectCleanupJob::schedule(window(), d->provider);
    }
}

void QQuickImagePrivate::setImage(const QImage &image)
{
    Q_Q(QQuickImage);
    pix.setImage(image);

    q->pixmapChange();
    status = pix.isNull() ? QQuickImageBase::Null : QQuickImageBase::Ready;

    q->update();
}

void QQuickImagePrivate::setPixmap(const QQuickPixmap &pixmap)
{
    Q_Q(QQuickImage);
    pix.setPixmap(pixmap);

    q->pixmapChange();
    status = pix.isNull() ? QQuickImageBase::Null : QQuickImageBase::Ready;

    q->update();
}

/*!
    \qmlproperty enumeration QtQuick::Image::fillMode

    Set this property to define what happens when the source image has a different size
    than the item.
    \list
    \li Image.Stretch - the image is scaled to fit
    \li Image.PreserveAspectFit - the image is scaled uniformly to fit without cropping
    \li Image.PreserveAspectCrop - the image is scaled uniformly to fill, cropping if necessary
    \li Image.Tile - the image is duplicated horizontally and vertically
    \li Image.TileVertically - the image is stretched horizontally and tiled vertically
    \li Image.TileHorizontally - the image is stretched vertically and tiled horizontally
    \li Image.Pad - the image is not transformed
    \endlist

    \table

    \row
    \li \image declarative-qtlogo-stretch.png
    \li Stretch (default)
    \qml
    Image {
        width: 130; height: 100
        source: "qtlogo.png"
    }
    \endqml

    \row
    \li \image declarative-qtlogo-preserveaspectfit.png
    \li PreserveAspectFit
    \qml
    Image {
        width: 130; height: 100
        fillMode: Image.PreserveAspectFit
        source: "qtlogo.png"
    }
    \endqml

    \row
    \li \image declarative-qtlogo-preserveaspectcrop.png
    \li PreserveAspectCrop
    \qml
    Image {
        width: 130; height: 100
        fillMode: Image.PreserveAspectCrop
        source: "qtlogo.png"
        clip: true
    }
    \endqml

    \row
    \li \image declarative-qtlogo-tile.png
    \li Tile
    \qml
    Image {
        width: 120; height: 120
        fillMode: Image.Tile
        horizontalAlignment: Image.AlignLeft
        verticalAlignment: Image.AlignTop
        source: "qtlogo.png"
    }
    \endqml

    \row
    \li \image declarative-qtlogo-tilevertically.png
    \li TileVertically
    \qml
    Image {
        width: 120; height: 120
        fillMode: Image.TileVertically
        verticalAlignment: Image.AlignTop
        source: "qtlogo.png"
    }
    \endqml

    \row
    \li \image declarative-qtlogo-tilehorizontally.png
    \li TileHorizontally
    \qml
    Image {
        width: 120; height: 120
        fillMode: Image.TileHorizontally
        verticalAlignment: Image.AlignLeft
        source: "qtlogo.png"
    }
    \endqml

    \endtable

    Note that \c clip is \c false by default which means that the item might
    paint outside its bounding rectangle even if the fillMode is set to \c PreserveAspectCrop.

    \sa {Qt Quick Examples - Image Elements}
*/
QQuickImage::FillMode QQuickImage::fillMode() const
{
    Q_D(const QQuickImage);
    return d->fillMode;
}

void QQuickImage::setFillMode(FillMode mode)
{
    Q_D(QQuickImage);
    if (d->fillMode == mode)
        return;
    d->fillMode = mode;
    update();
    updatePaintedGeometry();
    emit fillModeChanged();
}

/*!

    \qmlproperty real QtQuick::Image::paintedWidth
    \qmlproperty real QtQuick::Image::paintedHeight

    These properties hold the size of the image that is actually painted.
    In most cases it is the same as \c width and \c height, but when using an
     \l {fillMode}{Image.PreserveAspectFit} or an \l {fillMode}{Image.PreserveAspectCrop}
    \c paintedWidth or \c paintedHeight can be smaller or larger than
    \c width and \c height of the Image item.
*/
qreal QQuickImage::paintedWidth() const
{
    Q_D(const QQuickImage);
    return d->paintedWidth;
}

qreal QQuickImage::paintedHeight() const
{
    Q_D(const QQuickImage);
    return d->paintedHeight;
}

/*!
    \qmlproperty enumeration QtQuick::Image::status

    This property holds the status of image loading.  It can be one of:
    \list
    \li Image.Null - no image has been set
    \li Image.Ready - the image has been loaded
    \li Image.Loading - the image is currently being loaded
    \li Image.Error - an error occurred while loading the image
    \endlist

    Use this status to provide an update or respond to the status change in some way.
    For example, you could:

    \list
    \li Trigger a state change:
    \qml
        State { name: 'loaded'; when: image.status == Image.Ready }
    \endqml

    \li Implement an \c onStatusChanged signal handler:
    \qml
        Image {
            id: image
            onStatusChanged: if (image.status == Image.Ready) console.log('Loaded')
        }
    \endqml

    \li Bind to the status value:
    \qml
        Text { text: image.status == Image.Ready ? 'Loaded' : 'Not loaded' }
    \endqml
    \endlist

    \sa progress
*/

/*!
    \qmlproperty real QtQuick::Image::progress

    This property holds the progress of image loading, from 0.0 (nothing loaded)
    to 1.0 (finished).

    \sa status
*/

/*!
    \qmlproperty bool QtQuick::Image::smooth

    This property holds whether the image is smoothly filtered when scaled or
    transformed.  Smooth filtering gives better visual quality, but it may be slower
    on some hardware.  If the image is displayed at its natural size, this property has
    no visual or performance effect.

    By default, this property is set to true.

    \sa mipmap
*/

/*!
    \qmlproperty QSize QtQuick::Image::sourceSize

    This property holds the actual width and height of the loaded image.

    Unlike the \l {Item::}{width} and \l {Item::}{height} properties, which scale
    the painting of the image, this property sets the actual number of pixels
    stored for the loaded image so that large images do not use more
    memory than necessary. For example, this ensures the image in memory is no
    larger than 1024x1024 pixels, regardless of the Image's \l {Item::}{width} and
    \l {Item::}{height} values:

    \code
    Rectangle {
        width: ...
        height: ...

        Image {
           anchors.fill: parent
           source: "reallyBigImage.jpg"
           sourceSize.width: 1024
           sourceSize.height: 1024
        }
    }
    \endcode

    If the image's actual size is larger than the sourceSize, the image is scaled down.
    If only one dimension of the size is set to greater than 0, the
    other dimension is set in proportion to preserve the source image's aspect ratio.
    (The \l fillMode is independent of this.)

    If both the sourceSize.width and sourceSize.height are set the image will be scaled
    down to fit within the specified size, maintaining the image's aspect ratio.  The actual
    size of the image after scaling is available via \l Item::implicitWidth and \l Item::implicitHeight.

    If the source is an intrinsically scalable image (eg. SVG), this property
    determines the size of the loaded image regardless of intrinsic size.
    Avoid changing this property dynamically; rendering an SVG is \e slow compared
    to an image.

    If the source is a non-scalable image (eg. JPEG), the loaded image will
    be no greater than this property specifies. For some formats (currently only JPEG),
    the whole image will never actually be loaded into memory.

    sourceSize can be cleared to the natural size of the image
    by setting sourceSize to \c undefined.

    \note \e {Changing this property dynamically causes the image source to be reloaded,
    potentially even from the network, if it is not in the disk cache.}
*/

/*!
    \qmlproperty url QtQuick::Image::source

    Image can handle any image format supported by Qt, loaded from any URL scheme supported by Qt.

    The URL may be absolute, or relative to the URL of the component.

    \sa QQuickImageProvider
*/

/*!
    \qmlproperty bool QtQuick::Image::asynchronous

    Specifies that images on the local filesystem should be loaded
    asynchronously in a separate thread.  The default value is
    false, causing the user interface thread to block while the
    image is loaded.  Setting \a asynchronous to true is useful where
    maintaining a responsive user interface is more desirable
    than having images immediately visible.

    Note that this property is only valid for images read from the
    local filesystem.  Images loaded via a network resource (e.g. HTTP)
    are always loaded asynchronously.
*/

/*!
    \qmlproperty bool QtQuick::Image::cache

    Specifies whether the image should be cached. The default value is
    true. Setting \a cache to false is useful when dealing with large images,
    to make sure that they aren't cached at the expense of small 'ui element' images.
*/

/*!
    \qmlproperty bool QtQuick::Image::mirror

    This property holds whether the image should be horizontally inverted
    (effectively displaying a mirrored image).

    The default value is false.
*/

/*!
    \qmlproperty enumeration QtQuick::Image::horizontalAlignment
    \qmlproperty enumeration QtQuick::Image::verticalAlignment

    Sets the horizontal and vertical alignment of the image. By default, the image is center aligned.

    The valid values for \c horizontalAlignment are \c Image.AlignLeft, \c Image.AlignRight and \c Image.AlignHCenter.
    The valid values for \c verticalAlignment are \c Image.AlignTop, \c Image.AlignBottom
    and \c Image.AlignVCenter.
*/
void QQuickImage::updatePaintedGeometry()
{
    Q_D(QQuickImage);

    if (d->fillMode == PreserveAspectFit) {
        if (!d->pix.width() || !d->pix.height()) {
            setImplicitSize(0, 0);
            return;
        }
        qreal w = widthValid() ? width() : d->pix.width();
        qreal widthScale = w / qreal(d->pix.width());
        qreal h = heightValid() ? height() : d->pix.height();
        qreal heightScale = h / qreal(d->pix.height());
        if (widthScale <= heightScale) {
            d->paintedWidth = w;
            d->paintedHeight = widthScale * qreal(d->pix.height());
        } else if (heightScale < widthScale) {
            d->paintedWidth = heightScale * qreal(d->pix.width());
            d->paintedHeight = h;
        }
        qreal iHeight = (widthValid() && !heightValid()) ? d->paintedHeight : d->pix.height();
        qreal iWidth = (heightValid() && !widthValid()) ? d->paintedWidth : d->pix.width();
        setImplicitSize(iWidth, iHeight);

    } else if (d->fillMode == PreserveAspectCrop) {
        if (!d->pix.width() || !d->pix.height())
            return;
        qreal widthScale = width() / qreal(d->pix.width());
        qreal heightScale = height() / qreal(d->pix.height());
        if (widthScale < heightScale) {
            widthScale = heightScale;
        } else if (heightScale < widthScale) {
            heightScale = widthScale;
        }

        d->paintedHeight = heightScale * qreal(d->pix.height());
        d->paintedWidth = widthScale * qreal(d->pix.width());
    } else if (d->fillMode == Pad) {
        d->paintedWidth = d->pix.width();
        d->paintedHeight = d->pix.height();
    } else {
        d->paintedWidth = width();
        d->paintedHeight = height();
    }
    emit paintedGeometryChanged();
}

void QQuickImage::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickImageBase::geometryChanged(newGeometry, oldGeometry);
    updatePaintedGeometry();
}

QRectF QQuickImage::boundingRect() const
{
    Q_D(const QQuickImage);
    return QRectF(0, 0, qMax(width(), d->paintedWidth), qMax(height(), d->paintedHeight));
}

QSGTextureProvider *QQuickImage::textureProvider() const
{
    Q_D(const QQuickImage);

    // When Item::layer::enabled == true, QQuickItem will be a texture
    // provider. In this case we should prefer to return the layer rather
    // than the image itself. The layer will include any children and any
    // the image's wrap and fill mode.
    if (QQuickItem::isTextureProvider())
        return QQuickItem::textureProvider();

    if (!d->window || !d->sceneGraphRenderContext() || QThread::currentThread() != d->sceneGraphRenderContext()->thread()) {
        qWarning("QQuickImage::textureProvider: can only be queried on the rendering thread of an exposed window");
        return 0;
    }

    if (!d->provider) {
        QQuickImagePrivate *dd = const_cast<QQuickImagePrivate *>(d);
        dd->provider = new QQuickImageTextureProvider;
        dd->provider->m_smooth = d->smooth;
        dd->provider->m_mipmap = d->mipmap;
        dd->provider->updateTexture(d->sceneGraphRenderContext()->textureForFactory(d->pix.textureFactory(), window()));
    }

    return d->provider;
}

void QQuickImage::invalidateSceneGraph()
{
    Q_D(QQuickImage);
    delete d->provider;
    d->provider = 0;
}

void QQuickImage::releaseResources()
{
    Q_D(QQuickImage);
    if (d->provider) {
        QQuickWindowQObjectCleanupJob::schedule(window(), d->provider);
        d->provider = 0;
    }
}

QSGNode *QQuickImage::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    Q_D(QQuickImage);

    QSGTexture *texture = d->sceneGraphRenderContext()->textureForFactory(d->pix.textureFactory(), window());

    // Copy over the current texture state into the texture provider...
    if (d->provider) {
        d->provider->m_smooth = d->smooth;
        d->provider->m_mipmap = d->mipmap;
        d->provider->updateTexture(texture);
    }

    if (!texture || width() <= 0 || height() <= 0) {
        delete oldNode;
        return 0;
    }

    QSGInternalImageNode *node = static_cast<QSGInternalImageNode *>(oldNode);
    if (!node) {
        d->pixmapChanged = true;
        node = d->sceneGraphContext()->createInternalImageNode();
    }

    QRectF targetRect;
    QRectF sourceRect;
    QSGTexture::WrapMode hWrap = QSGTexture::ClampToEdge;
    QSGTexture::WrapMode vWrap = QSGTexture::ClampToEdge;

    qreal pixWidth = (d->fillMode == PreserveAspectFit) ? d->paintedWidth : d->pix.width() / d->devicePixelRatio;
    qreal pixHeight = (d->fillMode == PreserveAspectFit) ? d->paintedHeight :  d->pix.height() / d->devicePixelRatio;

    int xOffset = 0;
    if (d->hAlign == QQuickImage::AlignHCenter)
        xOffset = qCeil((width() - pixWidth) / 2.);
    else if (d->hAlign == QQuickImage::AlignRight)
        xOffset = qCeil(width() - pixWidth);

    int yOffset = 0;
    if (d->vAlign == QQuickImage::AlignVCenter)
        yOffset = qCeil((height() - pixHeight) / 2.);
    else if (d->vAlign == QQuickImage::AlignBottom)
        yOffset = qCeil(height() - pixHeight);

    switch (d->fillMode) {
    default:
    case Stretch:
        targetRect = QRectF(0, 0, width(), height());
        sourceRect = d->pix.rect();
        break;

    case PreserveAspectFit:
        targetRect = QRectF(xOffset, yOffset, d->paintedWidth, d->paintedHeight);
        sourceRect = d->pix.rect();
        break;

    case PreserveAspectCrop: {
        targetRect = QRect(0, 0, width(), height());
        qreal wscale = width() / qreal(d->pix.width());
        qreal hscale = height() / qreal(d->pix.height());

        if (wscale > hscale) {
            int src = (hscale / wscale) * qreal(d->pix.height());
            int y = 0;
            if (d->vAlign == QQuickImage::AlignVCenter)
                y = qCeil((d->pix.height() - src) / 2.);
            else if (d->vAlign == QQuickImage::AlignBottom)
                y = qCeil(d->pix.height() - src);
            sourceRect = QRectF(0, y, d->pix.width(), src);

        } else {
            int src = (wscale / hscale) * qreal(d->pix.width());
            int x = 0;
            if (d->hAlign == QQuickImage::AlignHCenter)
                x = qCeil((d->pix.width() - src) / 2.);
            else if (d->hAlign == QQuickImage::AlignRight)
                x = qCeil(d->pix.width() - src);
            sourceRect = QRectF(x, 0, src, d->pix.height());
        }
        }
        break;

    case Tile:
        targetRect = QRectF(0, 0, width(), height());
        sourceRect = QRectF(-xOffset, -yOffset, width(), height());
        hWrap = QSGTexture::Repeat;
        vWrap = QSGTexture::Repeat;
        break;

    case TileHorizontally:
        targetRect = QRectF(0, 0, width(), height());
        sourceRect = QRectF(-xOffset, 0, width(), d->pix.height());
        hWrap = QSGTexture::Repeat;
        break;

    case TileVertically:
        targetRect = QRectF(0, 0, width(), height());
        sourceRect = QRectF(0, -yOffset, d->pix.width(), height());
        vWrap = QSGTexture::Repeat;
        break;

    case Pad:
        qreal w = qMin(qreal(pixWidth), width());
        qreal h = qMin(qreal(pixHeight), height());
        qreal x = (pixWidth > width()) ? -xOffset : 0;
        qreal y = (pixHeight > height()) ? -yOffset : 0;
        targetRect = QRectF(x + xOffset, y + yOffset, w, h);
        sourceRect = QRectF(x, y, w, h);
        break;
    };

    qreal nsWidth = (hWrap == QSGTexture::Repeat || d->fillMode == Pad) ? d->pix.width() / d->devicePixelRatio : d->pix.width();
    qreal nsHeight = (vWrap == QSGTexture::Repeat || d->fillMode == Pad) ? d->pix.height() / d->devicePixelRatio : d->pix.height();
    QRectF nsrect(sourceRect.x() / nsWidth,
                  sourceRect.y() / nsHeight,
                  sourceRect.width() / nsWidth,
                  sourceRect.height() / nsHeight);

    if (targetRect.isEmpty()
        || !qt_is_finite(targetRect.width()) || !qt_is_finite(targetRect.height())
        || nsrect.isEmpty()
        || !qt_is_finite(nsrect.width()) || !qt_is_finite(nsrect.height())) {
        delete node;
        return 0;
    }

    if (d->pixmapChanged) {
        // force update the texture in the node to trigger reconstruction of
        // geometry and the likes when a atlas segment has changed.
        if (texture->isAtlasTexture() && (hWrap == QSGTexture::Repeat || vWrap == QSGTexture::Repeat || d->mipmap))
            node->setTexture(texture->removedFromAtlas());
        else
            node->setTexture(texture);
        d->pixmapChanged = false;
    }

    node->setMipmapFiltering(d->mipmap ? QSGTexture::Linear : QSGTexture::None);
    node->setHorizontalWrapMode(hWrap);
    node->setVerticalWrapMode(vWrap);
    node->setFiltering(d->smooth ? QSGTexture::Linear : QSGTexture::Nearest);

    node->setTargetRect(targetRect);
    node->setInnerTargetRect(targetRect);
    node->setSubSourceRect(nsrect);
    node->setMirror(d->mirror);
    node->setAntialiasing(d->antialiasing);
    node->update();

    return node;
}

void QQuickImage::pixmapChange()
{
    Q_D(QQuickImage);
    // PreserveAspectFit calculates the implicit size differently so we
    // don't call our superclass pixmapChange(), since that would
    // result in the implicit size being set incorrectly, then updated
    // in updatePaintedGeometry()
    if (d->fillMode != PreserveAspectFit)
        QQuickImageBase::pixmapChange();
    updatePaintedGeometry();
    d->pixmapChanged = true;

    // When the pixmap changes, such as being deleted, we need to update the textures
    update();
}

QQuickImage::VAlignment QQuickImage::verticalAlignment() const
{
    Q_D(const QQuickImage);
    return d->vAlign;
}

void QQuickImage::setVerticalAlignment(VAlignment align)
{
    Q_D(QQuickImage);
    if (d->vAlign == align)
        return;

    d->vAlign = align;
    update();
    updatePaintedGeometry();
    emit verticalAlignmentChanged(align);
}

QQuickImage::HAlignment QQuickImage::horizontalAlignment() const
{
    Q_D(const QQuickImage);
    return d->hAlign;
}

void QQuickImage::setHorizontalAlignment(HAlignment align)
{
    Q_D(QQuickImage);
    if (d->hAlign == align)
        return;

    d->hAlign = align;
    update();
    updatePaintedGeometry();
    emit horizontalAlignmentChanged(align);
}

/*!
    \qmlproperty bool QtQuick::Image::mipmap
    \since 5.3

    This property holds whether the image uses mipmap filtering when scaled or
    transformed.

    Mipmap filtering gives better visual quality when scaling down
    compared to smooth, but it may come at a performance cost (both when
    initializing the image and during rendering).

    By default, this property is set to false.

    \sa smooth
 */

bool QQuickImage::mipmap() const
{
    Q_D(const QQuickImage);
    return d->mipmap;
}

void QQuickImage::setMipmap(bool use)
{
    Q_D(QQuickImage);
    if (d->mipmap == use)
        return;
    d->mipmap = use;
    emit mipmapChanged(d->mipmap);

    d->pixmapChanged = true;
    update();
}

/*!
    \qmlproperty bool QtQuick::Image::autoTransform
    \since 5.5

    This property holds whether the image should automatically apply
    image transformation metadata such as EXIF orientation.

    By default, this property is set to false.
 */

QT_END_NAMESPACE
