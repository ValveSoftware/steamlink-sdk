/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickborderimage_p.h"
#include "qquickborderimage_p_p.h"

#include <QtQml/qqmlinfo.h>
#include <QtQml/qqmlfile.h>
#include <QtQml/qqmlengine.h>
#include <QtNetwork/qnetworkreply.h>
#include <QtCore/qfile.h>
#include <QtCore/qmath.h>

#include <private/qqmlglobal_p.h>

QT_BEGIN_NAMESPACE


/*!
    \qmltype BorderImage
    \instantiates QQuickBorderImage
    \inqmlmodule QtQuick
    \brief Paints a border based on an image
    \inherits Item
    \ingroup qtquick-visual

    The BorderImage type is used to create borders out of images by scaling or tiling
    parts of each image.

    A BorderImage breaks a source image, specified using the \l source property,
    into 9 regions, as shown below:

    \image declarative-scalegrid.png

    When the image is scaled, regions of the source image are scaled or tiled to
    create the displayed border image in the following way:

    \list
    \li The corners (regions 1, 3, 7, and 9) are not scaled at all.
    \li Regions 2 and 8 are scaled according to
       \l{BorderImage::horizontalTileMode}{horizontalTileMode}.
    \li Regions 4 and 6 are scaled according to
       \l{BorderImage::verticalTileMode}{verticalTileMode}.
    \li The middle (region 5) is scaled according to both
       \l{BorderImage::horizontalTileMode}{horizontalTileMode} and
       \l{BorderImage::verticalTileMode}{verticalTileMode}.
    \endlist

    The regions of the image are defined using the \l border property group, which
    describes the distance from each edge of the source image to use as a border.

    \section1 Example Usage

    The following examples show the effects of the different modes on an image.
    Guide lines are overlaid onto the image to show the different regions of the
    image as described above.

    \beginfloatleft
    \image qml-borderimage-normal-image.png
    \endfloat

    An unscaled image is displayed using an Image. The \l border property is
    used to determine the parts of the image that will lie inside the unscaled corner
    areas and the parts that will be stretched horizontally and vertically.

    \snippet qml/borderimage/normal-image.qml normal image

    \clearfloat
    \beginfloatleft
    \image qml-borderimage-scaled.png
    \endfloat

    A BorderImage is used to display the image, and it is given a size that is
    larger than the original image. Since the \l horizontalTileMode property is set to
    \l{BorderImage::horizontalTileMode}{BorderImage.Stretch}, the parts of image in
    regions 2 and 8 are stretched horizontally. Since the \l verticalTileMode property
    is set to \l{BorderImage::verticalTileMode}{BorderImage.Stretch}, the parts of image
    in regions 4 and 6 are stretched vertically.

    \snippet qml/borderimage/borderimage-scaled.qml scaled border image

    \clearfloat
    \beginfloatleft
    \image qml-borderimage-tiled.png
    \endfloat

    Again, a large BorderImage is used to display the image. With the
    \l horizontalTileMode property set to \l{BorderImage::horizontalTileMode}{BorderImage.Repeat},
    the parts of image in regions 2 and 8 are tiled so that they fill the space at the
    top and bottom of the item. Similarly, the \l verticalTileMode property is set to
    \l{BorderImage::verticalTileMode}{BorderImage.Repeat}, the parts of image in regions
    4 and 6 are tiled so that they fill the space at the left and right of the item.

    \snippet qml/borderimage/borderimage-tiled.qml tiled border image

    \clearfloat
    In some situations, the width of regions 2 and 8 may not be an exact multiple of the width
    of the corresponding regions in the source image. Similarly, the height of regions 4 and 6
    may not be an exact multiple of the height of the corresponding regions. It can be useful
    to use \l{BorderImage::horizontalTileMode}{BorderImage.Round} instead of
    \l{BorderImage::horizontalTileMode}{BorderImage.Repeat} in cases like these.

    The Border Image example in \l{Qt Quick Examples - Image Elements} shows how a BorderImage
    can be used to simulate a shadow effect on a rectangular item.

    \section1 Image Loading

    The source image may not be loaded instantaneously, depending on its original location.
    Loading progress can be monitored with the \l progress property.

    \sa Image, AnimatedImage
 */

/*!
    \qmlproperty bool QtQuick::BorderImage::asynchronous

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
QQuickBorderImage::QQuickBorderImage(QQuickItem *parent)
: QQuickImageBase(*(new QQuickBorderImagePrivate), parent)
{
}

QQuickBorderImage::~QQuickBorderImage()
{
    Q_D(QQuickBorderImage);
    if (d->sciReply)
        d->sciReply->deleteLater();
}

/*!
    \qmlproperty enumeration QtQuick::BorderImage::status

    This property describes the status of image loading.  It can be one of:

    \list
    \li BorderImage.Null - no image has been set
    \li BorderImage.Ready - the image has been loaded
    \li BorderImage.Loading - the image is currently being loaded
    \li BorderImage.Error - an error occurred while loading the image
    \endlist

    \sa progress
*/

/*!
    \qmlproperty real QtQuick::BorderImage::progress

    This property holds the progress of image loading, from 0.0 (nothing loaded)
    to 1.0 (finished).

    \sa status
*/

/*!
    \qmlproperty bool QtQuick::BorderImage::smooth

    This property holds whether the image is smoothly filtered when scaled or
    transformed.  Smooth filtering gives better visual quality, but it may be slower
    on some hardware.  If the image is displayed at its natural size, this property
    has no visual or performance effect.

    By default, this property is set to true.
*/

/*!
    \qmlproperty bool QtQuick::BorderImage::cache

    Specifies whether the image should be cached. The default value is
    true. Setting \a cache to false is useful when dealing with large images,
    to make sure that they aren't cached at the expense of small 'ui element' images.
*/

/*!
    \qmlproperty bool QtQuick::BorderImage::mirror

    This property holds whether the image should be horizontally inverted
    (effectively displaying a mirrored image).

    The default value is false.
*/

/*!
    \qmlproperty url QtQuick::BorderImage::source

    This property holds the URL that refers to the source image.

    BorderImage can handle any image format supported by Qt, loaded from any
    URL scheme supported by Qt.

    This property can also be used to refer to .sci files, which are
    written in a QML-specific, text-based format that specifies the
    borders, the image file and the tile rules for a given border image.

    The following .sci file sets the borders to 10 on each side for the
    image \c picture.png:

    \code
    border.left: 10
    border.top: 10
    border.bottom: 10
    border.right: 10
    source: "picture.png"
    \endcode

    The URL may be absolute, or relative to the URL of the component.

    \sa QQuickImageProvider
*/

/*!
    \qmlproperty QSize QtQuick::BorderImage::sourceSize

    This property holds the actual width and height of the loaded image.

    In BorderImage, this property is read-only.

    \sa Image::sourceSize
*/
void QQuickBorderImage::setSource(const QUrl &url)
{
    Q_D(QQuickBorderImage);

    if (url == d->url)
        return;

    if (d->sciReply) {
        d->sciReply->deleteLater();
        d->sciReply = 0;
    }

    d->url = url;
    d->sciurl = QUrl();
    emit sourceChanged(d->url);

    if (isComponentComplete())
        load();
}

void QQuickBorderImage::load()
{
    Q_D(QQuickBorderImage);

    if (d->url.isEmpty()) {
        d->pix.clear(this);
        d->status = Null;
        setImplicitSize(0, 0);
        emit statusChanged(d->status);
        if (d->progress != 0.0) {
            d->progress = 0.0;
            emit progressChanged(d->progress);
        }
        if (sourceSize() != d->oldSourceSize) {
            d->oldSourceSize = sourceSize();
            emit sourceSizeChanged();
        }
        pixmapChange();
        return;
    } else {
        if (d->url.path().endsWith(QLatin1String("sci"))) {
            QString lf = QQmlFile::urlToLocalFileOrQrc(d->url);
            if (!lf.isEmpty()) {
                QFile file(lf);
                file.open(QIODevice::ReadOnly);
                setGridScaledImage(QQuickGridScaledImage(&file));
                return;
            } else {
                if (d->progress != 0.0) {
                    d->progress = 0.0;
                    emit progressChanged(d->progress);
                }
                d->status = Loading;
                QNetworkRequest req(d->url);
                d->sciReply = qmlEngine(this)->networkAccessManager()->get(req);
                qmlobject_connect(d->sciReply, QNetworkReply, SIGNAL(finished()),
                                  this, QQuickBorderImage, SLOT(sciRequestFinished()))
            }
        } else {
            QQuickPixmap::Options options;
            if (d->async)
                options |= QQuickPixmap::Asynchronous;
            if (d->cache)
                options |= QQuickPixmap::Cache;
            d->pix.clear(this);
            d->pix.load(qmlEngine(this), d->url, options);

            if (d->pix.isLoading()) {
                if (d->progress != 0.0) {
                    d->progress = 0.0;
                    emit progressChanged(d->progress);
                }
                d->status = Loading;
                d->pix.connectFinished(this, SLOT(requestFinished()));
                d->pix.connectDownloadProgress(this, SLOT(requestProgress(qint64,qint64)));
            } else {
                requestFinished();
                return;
            }
        }
    }

    emit statusChanged(d->status);
}

/*!
    \qmlpropertygroup QtQuick::BorderImage::border
    \qmlproperty int QtQuick::BorderImage::border.left
    \qmlproperty int QtQuick::BorderImage::border.right
    \qmlproperty int QtQuick::BorderImage::border.top
    \qmlproperty int QtQuick::BorderImage::border.bottom

    The 4 border lines (2 horizontal and 2 vertical) break the image into 9 sections,
    as shown below:

    \image declarative-scalegrid.png

    Each border line (left, right, top, and bottom) specifies an offset in pixels
    from the respective edge of the source image. By default, each border line has
    a value of 0.

    For example, the following definition sets the bottom line 10 pixels up from
    the bottom of the image:

    \qml
    BorderImage {
        border.bottom: 10
        // ...
    }
    \endqml

    The border lines can also be specified using a
    \l {BorderImage::source}{.sci file}.
*/

QQuickScaleGrid *QQuickBorderImage::border()
{
    Q_D(QQuickBorderImage);
    return d->getScaleGrid();
}

/*!
    \qmlproperty enumeration QtQuick::BorderImage::horizontalTileMode
    \qmlproperty enumeration QtQuick::BorderImage::verticalTileMode

    This property describes how to repeat or stretch the middle parts of the border image.

    \list
    \li BorderImage.Stretch - Scales the image to fit to the available area.
    \li BorderImage.Repeat - Tile the image until there is no more space. May crop the last image.
    \li BorderImage.Round - Like Repeat, but scales the images down to ensure that the last image is not cropped.
    \endlist

    The default tile mode for each property is BorderImage.Stretch.
*/
QQuickBorderImage::TileMode QQuickBorderImage::horizontalTileMode() const
{
    Q_D(const QQuickBorderImage);
    return d->horizontalTileMode;
}

void QQuickBorderImage::setHorizontalTileMode(TileMode t)
{
    Q_D(QQuickBorderImage);
    if (t != d->horizontalTileMode) {
        d->horizontalTileMode = t;
        emit horizontalTileModeChanged();
        update();
    }
}

QQuickBorderImage::TileMode QQuickBorderImage::verticalTileMode() const
{
    Q_D(const QQuickBorderImage);
    return d->verticalTileMode;
}

void QQuickBorderImage::setVerticalTileMode(TileMode t)
{
    Q_D(QQuickBorderImage);
    if (t != d->verticalTileMode) {
        d->verticalTileMode = t;
        emit verticalTileModeChanged();
        update();
    }
}

void QQuickBorderImage::setGridScaledImage(const QQuickGridScaledImage& sci)
{
    Q_D(QQuickBorderImage);
    if (!sci.isValid()) {
        d->status = Error;
        emit statusChanged(d->status);
    } else {
        QQuickScaleGrid *sg = border();
        sg->setTop(sci.gridTop());
        sg->setBottom(sci.gridBottom());
        sg->setLeft(sci.gridLeft());
        sg->setRight(sci.gridRight());
        d->horizontalTileMode = sci.horizontalTileRule();
        d->verticalTileMode = sci.verticalTileRule();

        d->sciurl = d->url.resolved(QUrl(sci.pixmapUrl()));

        QQuickPixmap::Options options;
        if (d->async)
            options |= QQuickPixmap::Asynchronous;
        if (d->cache)
            options |= QQuickPixmap::Cache;
        d->pix.clear(this);
        d->pix.load(qmlEngine(this), d->sciurl, options);

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
                    QQuickBorderImage::staticMetaObject.indexOfSlot("requestProgress(qint64,qint64)");
                thisRequestFinished =
                    QQuickBorderImage::staticMetaObject.indexOfSlot("requestFinished()");
            }

            d->pix.connectFinished(this, thisRequestFinished);
            d->pix.connectDownloadProgress(this, thisRequestProgress);

        } else {
            requestFinished();
        }
    }
}

void QQuickBorderImage::requestFinished()
{
    Q_D(QQuickBorderImage);

    QSize impsize = d->pix.implicitSize();
    if (d->pix.isError()) {
        d->status = Error;
        qmlInfo(this) << d->pix.error();
        if (d->progress != 0) {
            d->progress = 0;
            emit progressChanged(d->progress);
        }
    } else {
        d->status = Ready;
        if (d->progress != 1.0) {
            d->progress = 1.0;
            emit progressChanged(d->progress);
        }
    }

    setImplicitSize(impsize.width(), impsize.height());
    emit statusChanged(d->status);
    if (sourceSize() != d->oldSourceSize) {
        d->oldSourceSize = sourceSize();
        emit sourceSizeChanged();
    }

    pixmapChange();
}

#define BORDERIMAGE_MAX_REDIRECT 16

void QQuickBorderImage::sciRequestFinished()
{
    Q_D(QQuickBorderImage);

    d->redirectCount++;
    if (d->redirectCount < BORDERIMAGE_MAX_REDIRECT) {
        QVariant redirect = d->sciReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirect.isValid()) {
            QUrl url = d->sciReply->url().resolved(redirect.toUrl());
            setSource(url);
            return;
        }
    }
    d->redirectCount=0;

    if (d->sciReply->error() != QNetworkReply::NoError) {
        d->status = Error;
        d->sciReply->deleteLater();
        d->sciReply = 0;
        emit statusChanged(d->status);
    } else {
        QQuickGridScaledImage sci(d->sciReply);
        d->sciReply->deleteLater();
        d->sciReply = 0;
        setGridScaledImage(sci);
    }
}

void QQuickBorderImage::doUpdate()
{
    update();
}

QSGNode *QQuickBorderImage::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    Q_D(QQuickBorderImage);

    QSGTexture *texture = d->sceneGraphRenderContext()->textureForFactory(d->pix.textureFactory(), window());

    if (!texture || width() <= 0 || height() <= 0) {
        delete oldNode;
        return 0;
    }

    QSGImageNode *node = static_cast<QSGImageNode *>(oldNode);

    bool updatePixmap = d->pixmapChanged;
    d->pixmapChanged = false;
    if (!node) {
        node = d->sceneGraphContext()->createImageNode();
        updatePixmap = true;
    }

    if (updatePixmap)
        node->setTexture(texture);

    // Don't implicitly create the scalegrid in the rendering thread...
    QRectF innerSourceRect(0, 0, 1, 1);
    QRectF targetRect(0, 0, width(), height());
    QRectF innerTargetRect = targetRect;
    if (d->border) {
        const QQuickScaleGrid *border = d->getScaleGrid();
        innerSourceRect = QRectF(border->left() / qreal(d->pix.width()),
                                 border->top() / qreal(d->pix.height()),
                                 qMax<qreal>(0, d->pix.width() - border->right() - border->left()) / d->pix.width(),
                                 qMax<qreal>(0, d->pix.height() - border->bottom() - border->top()) / d->pix.height());
        innerTargetRect = QRectF(border->left(),
                                 border->top(),
                                 qMax<qreal>(0, width() - border->right() - border->left()),
                                 qMax<qreal>(0, height() - border->bottom() - border->top()));
    }
    qreal hTiles = 1;
    qreal vTiles = 1;
    if (innerSourceRect.width() != 0) {
        switch (d->horizontalTileMode) {
        case QQuickBorderImage::Repeat:
            hTiles = innerTargetRect.width() / qreal(innerSourceRect.width() * d->pix.width());
            break;
        case QQuickBorderImage::Round:
            hTiles = qCeil(innerTargetRect.width() / qreal(innerSourceRect.width() * d->pix.width()));
            break;
        default:
            break;
        }
    }
    if (innerSourceRect.height() != 0) {
        switch (d->verticalTileMode) {
        case QQuickBorderImage::Repeat:
            vTiles = innerTargetRect.height() / qreal(innerSourceRect.height() * d->pix.height());
            break;
        case QQuickBorderImage::Round:
            vTiles = qCeil(innerTargetRect.height() / qreal(innerSourceRect.height() * d->pix.height()));
            break;
        default:
            break;
        }
    }

    node->setTargetRect(targetRect);
    node->setInnerSourceRect(innerSourceRect);
    node->setInnerTargetRect(innerTargetRect);
    node->setSubSourceRect(QRectF(0, 0, hTiles, vTiles));
    node->setMirror(d->mirror);

    node->setMipmapFiltering(QSGTexture::None);
    node->setFiltering(d->smooth ? QSGTexture::Linear : QSGTexture::Nearest);
    if (innerSourceRect == QRectF(0, 0, 1, 1) && (vTiles > 1 || hTiles > 1)) {
        node->setHorizontalWrapMode(QSGTexture::Repeat);
        node->setVerticalWrapMode(QSGTexture::Repeat);
    } else {
        node->setHorizontalWrapMode(QSGTexture::ClampToEdge);
        node->setVerticalWrapMode(QSGTexture::ClampToEdge);
    }
    node->setAntialiasing(d->antialiasing);
    node->update();

    return node;
}

void QQuickBorderImage::pixmapChange()
{
    Q_D(QQuickBorderImage);
    d->pixmapChanged = true;
    update();
}

QT_END_NAMESPACE
