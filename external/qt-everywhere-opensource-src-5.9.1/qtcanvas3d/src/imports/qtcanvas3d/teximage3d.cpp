/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

#include "teximage3d_p.h"
#include "canvas3dcommon_p.h"

#include <QJSValueIterator>
#include <QtQml/QQmlEngine>

QT_BEGIN_NAMESPACE

QT_CANVAS3D_BEGIN_NAMESPACE

static QMap<QQmlEngine *,CanvasTextureImageFactory *>m_qmlEngineToImageFactoryMap;
static ulong m_texId = 0;

class StaticFactoryMapDeleter
{
public:
    StaticFactoryMapDeleter() {}
    ~StaticFactoryMapDeleter() {
        qDeleteAll(m_qmlEngineToImageFactoryMap);
    }
};
static StaticFactoryMapDeleter staticFactoryMapDeleter;

CanvasTextureImageFactory::CanvasTextureImageFactory(QQmlEngine *engine, QObject *parent) :
    QObject(parent)
{
    m_qmlEngine = engine;

    QObject::connect(engine, &QObject::destroyed,
                     this, &CanvasTextureImageFactory::deleteLater);
}

CanvasTextureImageFactory::~CanvasTextureImageFactory()
{
    m_qmlEngineToImageFactoryMap.remove(m_qmlEngine);
}

/*!
 * \qmltype TextureImageFactory
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \brief Create TextureImage elements.
 *
 * This static QML type is used for creating TextureImage instances by calling the
 * TextureImageFactory::newTexImage() function.
 *
 * \sa TextureImage
 */
QObject *CanvasTextureImageFactory::texture_image_factory_provider(QQmlEngine *engine,
                                                                   QJSEngine *scriptEngine)
{
    Q_UNUSED(scriptEngine)
    return factory(engine);
}

CanvasTextureImageFactory *CanvasTextureImageFactory::factory(QQmlEngine *engine)
{
    if (m_qmlEngineToImageFactoryMap.contains(engine))
        return m_qmlEngineToImageFactoryMap[engine];

    CanvasTextureImageFactory *factory = new CanvasTextureImageFactory(engine);
    m_qmlEngineToImageFactoryMap[engine] = factory;
    return factory;
}

void CanvasTextureImageFactory::handleImageLoadingStarted(CanvasTextureImage *image)
{
    if (m_loadingImagesList.contains(image))
        return;

    m_loadingImagesList << image;
}

void CanvasTextureImageFactory::notifyLoadedImages()
{
    if (!m_loadingImagesList.size())
        return;

    auto hasLoadingFinishedOrFailed = [](CanvasTextureImage *image) -> bool {
        if (image->imageState() == CanvasTextureImage::LOADING_FINISHED) {
            image->emitImageLoaded();
            return true;
        } else if (image->imageState() == CanvasTextureImage::LOADING_ERROR) {
            image->emitImageLoadingError();
            return true;
        }
        return false;
    };

    m_loadingImagesList.erase(std::remove_if(m_loadingImagesList.begin(), m_loadingImagesList.end(),
                                             hasLoadingFinishedOrFailed),
                              m_loadingImagesList.end());
}

/*!
 * \qmlmethod TextureImage TextureImageFactory::newTexImage()
 * Returns a new empty TextureImage.
 */
QJSValue CanvasTextureImageFactory::newTexImage()
{
    CanvasTextureImage *newImg = new CanvasTextureImage(this, m_qmlEngine);
    return m_qmlEngine->newQObject(newImg);
}

void CanvasTextureImageFactory::handleImageDestroyed(CanvasTextureImage *image)
{
    m_loadingImagesList.removeOne(image);
}

/*!
 * \qmltype TextureImage
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \brief Contains a texture image.
 *
 * An uncreatable QML type that contains a texture image created by calling
 * TextureImageFactory::newTexImage() and settings the \c src of the image.
 *
 * \sa TextureImageFactory
 */
CanvasTextureImage::CanvasTextureImage(CanvasTextureImageFactory *parent, QQmlEngine *engine) :
    CanvasAbstractObject(0, 0),
    m_engine(engine),
    m_networkAccessManager(m_engine->networkAccessManager()),
    m_networkReply(0),
    m_state(INITIALIZED),
    m_errorString(""),
    m_pixelCache(0),
    m_pixelCacheFormat(CanvasContext::NONE),
    m_pixelCacheFlipY(false),
    m_parentFactory(parent)
{
}

CanvasTextureImage::CanvasTextureImage(const QImage &source,
                                       int width, int height,
                                       CanvasTextureImageFactory *parent,
                                       QQmlEngine *engine) :
    CanvasAbstractObject(0, 0),
    m_engine(engine),
    m_networkAccessManager(m_engine->networkAccessManager()),
    m_networkReply(0),
    m_state(INITIALIZED),
    m_errorString(""),
    m_pixelCache(0),
    m_pixelCacheFormat(CanvasContext::NONE),
    m_pixelCacheFlipY(false),
    m_parentFactory(parent)
{
    m_image = source.scaled(width, height);
    setImageState(LOADING_FINISHED);
}

void CanvasTextureImage::cleanupNetworkReply()
{
    if (m_networkReply) {
        QObject::disconnect(m_networkReply, &QNetworkReply::finished,
                            this, &CanvasTextureImage::handleReply);
        m_networkReply->abort();
        m_networkReply->deleteLater();
        m_networkReply = 0;
    }
}

CanvasTextureImage::~CanvasTextureImage()
{
    if (!m_parentFactory.isNull())
        m_parentFactory->handleImageDestroyed(this);
    cleanupNetworkReply();
    delete[] m_pixelCache;
}

/*!
 * \qmlproperty url TextureImage::src()
 * Contains the url source where the image data is loaded from.
 */
const QUrl &CanvasTextureImage::src() const
{
    return m_source;
}

void CanvasTextureImage::setSrc(const QUrl &url)
{
    if (url == m_source)
        return;

    m_source = url;
    emit srcChanged(m_source);

    load();
}

/*!
 * \qmlmethod int TextureImage::id()
 * Contains the object id.
 */
ulong CanvasTextureImage::id()
{
    return m_texId++;
}

void CanvasTextureImage::emitImageLoaded()
{
    emit imageLoaded(this);
}

void CanvasTextureImage::emitImageLoadingError()
{
    emit imageLoadingFailed(this);
}

void CanvasTextureImage::load()
{
    if (m_source.isEmpty()) {
        QByteArray array;
        m_image.loadFromData(array);
        m_glImage = m_image.convertToFormat(QImage::Format_RGBA8888);
        setImageState(LOADING_FINISHED);
        return;
    }

    if (m_state == LOADING)
        return;

    setImageState(LOADING);
    if (!m_parentFactory.isNull())
        m_parentFactory->handleImageLoadingStarted(this);
    emit imageLoadingStarted(this);

    QNetworkRequest request(m_source);
    m_networkReply = m_networkAccessManager->get(request);
    QObject::connect(m_networkReply, &QNetworkReply::finished,
                     this, &CanvasTextureImage::handleReply);
}

/*!
 * \qmlproperty string TextureImage::errorString()
 * Contains the error string if an error happened during image creation.
 */
QString CanvasTextureImage::errorString() const
{
    return m_errorString;
}

void CanvasTextureImage::handleReply()
{
    if (m_networkReply) {
        if (m_networkReply->error() != QNetworkReply::NoError) {
            m_errorString = m_networkReply->errorString();
            emit errorStringChanged(m_errorString);
            setImageState(LOADING_ERROR);
        } else {
            m_image.loadFromData(m_networkReply->readAll());
            setImageState(LOADING_FINISHED);
        }
        cleanupNetworkReply();
    }
}

QImage &CanvasTextureImage::getImage()
{
    return m_image;
}

QVariant *CanvasTextureImage::anything() const
{
    return m_anyValue;
}

void CanvasTextureImage::setAnything(QVariant *value)
{
    if (m_anyValue == value)
        return;
    m_anyValue = value;
    emit anythingChanged(value);
}

/*!
 * \qmlproperty TextureImageState TextureImage::imageState()
 * Contains the texture image state. It is one of \c{TextureImage.INITIALIZED},
 * \c{TextureImage.LOAD_PENDING}, \c{TextureImage.LOADING}, \c{TextureImage.LOADING_FINISHED} or
 * \c{TextureImage.LOADING_ERROR}.
 */
CanvasTextureImage::TextureImageState CanvasTextureImage::imageState() const
{
    return m_state;
}

void CanvasTextureImage::setImageState(TextureImageState state)
{
    if (state == m_state)
        return;
    m_state = state;
    emit imageStateChanged(state);
}

/*!
 * \qmlproperty int TextureImage::width()
 * Contains the texture image width.
 */
int CanvasTextureImage::width() const
{
    if (m_state != LOADING_FINISHED)
        return 0;

    return m_image.width();
}

/*!
 * \qmlproperty int TextureImage::height()
 * Contains the texture image height.
 */
int CanvasTextureImage::height() const
{
    if (m_state != LOADING_FINISHED)
        return 0;

    return m_image.height();
}

uchar *CanvasTextureImage::convertToFormat(CanvasContext::glEnums format,
                                           bool flipY, bool premultipliedAlpha)
{
    if (m_pixelCacheFormat == format && m_pixelCacheFlipY == flipY)
        return m_pixelCache;

    // Destroy the pixel cache
    delete[] m_pixelCache;
    m_pixelCache = 0;
    m_pixelCacheFormat = CanvasContext::NONE;

    // Flip the image if needed
    if (m_pixelCacheFlipY != flipY) {
        m_image = m_image.mirrored(false, true);
        m_pixelCacheFlipY = flipY;
    }

    if (premultipliedAlpha)
        m_glImage = m_image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    else
        m_glImage = m_image.convertToFormat(QImage::Format_RGBA8888);

    // Get latest data for the conversion
    uchar *origPixels = m_glImage.bits();
    int width = m_glImage.width();
    int height = m_glImage.height();

    // Handle format conversions if needed
    switch (format) {
    case CanvasContext::UNSIGNED_BYTE: {
        return origPixels;
        break;
    }
    case CanvasContext::UNSIGNED_SHORT_5_6_5: {
        ushort *pixels = new ushort[width * height];
        ushort pixel;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int srcIdx = y * width * 4 + x * 4;
                pixel =  ((origPixels[srcIdx++] >> 3) & 0x1F) << 11;
                pixel |= ((origPixels[srcIdx++] >> 2) & 0x3F) << 5;
                pixel |= ((origPixels[srcIdx++] >> 3) & 0x1F) << 0;
                pixels[y * width + x] = pixel;
            }
        }
        m_pixelCacheFormat = CanvasContext::UNSIGNED_SHORT_5_6_5;
        m_pixelCache = (uchar *)pixels;
        return m_pixelCache;
    }
    case CanvasContext::UNSIGNED_SHORT_4_4_4_4: {
        ushort *pixels = new ushort[width * height];
        ushort pixel;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int srcIdx = y * width * 4 + x * 4;
                pixel =  ((origPixels[srcIdx++] >> 4) & 0x0F) << 12;
                pixel |= ((origPixels[srcIdx++] >> 4) & 0x0F) << 8;
                pixel |= ((origPixels[srcIdx++] >> 4) & 0x0F) << 4;
                pixel |= ((origPixels[srcIdx++] >> 4) & 0x0F) << 0;
                pixels[y * width + x] = pixel;
            }
        }
        m_pixelCacheFormat = CanvasContext::UNSIGNED_SHORT_4_4_4_4;
        m_pixelCache = (uchar *)pixels;
        return m_pixelCache;
    }
    case CanvasContext::UNSIGNED_SHORT_5_5_5_1: {
        ushort *pixels = new ushort[width * height];
        ushort pixel;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int srcIdx = y * width * 4 + x * 4;
                pixel =  ((origPixels[srcIdx++] >> 3) & 0x1F) << 11;
                pixel |= ((origPixels[srcIdx++] >> 3) & 0x1F) << 6;
                pixel |= ((origPixels[srcIdx++] >> 3) & 0x1F) << 1;
                pixel |= ((origPixels[srcIdx++] >> 7) & 0x01) << 0;
                pixels[y * width + x] = pixel;
            }
        }
        m_pixelCacheFormat = CanvasContext::UNSIGNED_SHORT_5_5_5_1;
        m_pixelCache = (uchar *)pixels;
        return m_pixelCache;
    }
    default: {
        qDebug() << "TexImage3D::" << __FUNCTION__ << ":INVALID_ENUM Invalid type enum";
        break;
    }
    }

    return 0;
}

/*!
 * \qmlmethod TextureImage TextureImage::create()
 * Returns a new texture image.
 */
QJSValue CanvasTextureImage::create()
{
    return m_engine->newQObject(new CanvasTextureImage(m_parentFactory, m_engine));
}

/*!
 * \qmlmethod TextureImage TextureImage::resize(int width, int height)
 * Returns a copy of the texture image resized to the given \a width and \a height.
 */
QJSValue CanvasTextureImage::resize(int width, int height)
{
    if (m_state != LOADING_FINISHED)
        return 0;

    return m_engine->newQObject(new CanvasTextureImage(m_image,
                                                       width, height,
                                                       m_parentFactory, m_engine));
}

QDebug operator<<(QDebug dbg, const CanvasTextureImage *texImage)
{
    if (texImage)
        dbg.nospace() << "TexImage3D("<< ((void*) texImage) << texImage->name() << ")";
    else
        dbg.nospace() << "TexImage3D("<< ((void*) texImage) <<")";
    return dbg.maybeSpace();
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
