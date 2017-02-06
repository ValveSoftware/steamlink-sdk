/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include <QtCore/qhash.h>
#include "gltexture_p.h"

#include <QDebug>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QOpenGLPixelTransferOptions>
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qtexturedata.h>
#include <Qt3DRender/qtextureimagedata.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/texturedatamanager_p.h>
#include <Qt3DRender/private/qabstracttexture_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

GLTexture::GLTexture(TextureDataManager *texDataMgr,
                     TextureImageDataManager *texImgDataMgr,
                     const QTextureGeneratorPtr &texGen,
                     bool unique)
    : m_unique(unique)
    , m_gl(nullptr)
    , m_textureDataManager(texDataMgr)
    , m_textureImageDataManager(texImgDataMgr)
    , m_dataFunctor(texGen)
{
    // make sure texture generator is executed
    // this is needed when Texture have the TargetAutomatic
    // to ensure they are loaded before trying to instantiate the QOpenGLTexture
    if (!texGen.isNull())
        m_textureDataManager->requestData(texGen, this);
}

GLTexture::~GLTexture()
{
    destroyGLTexture();
}

void GLTexture::destroyResources()
{
    // release texture data
    for (const Image &img : qAsConst(m_images))
        m_textureImageDataManager->releaseData(img.generator, this);

    if (m_dataFunctor)
        m_textureDataManager->releaseData(m_dataFunctor, this);
}

void GLTexture::destroyGLTexture()
{
    delete m_gl;
    m_gl = nullptr;
    QMutexLocker locker(&m_dirtyFlagMutex);
    m_dirty = 0;

    destroyResources();
}

QOpenGLTexture* GLTexture::getOrCreateGLTexture()
{
    QMutexLocker locker(&m_dirtyFlagMutex);
    bool needUpload = false;
    bool texturedDataInvalid = false;

    // on the first invocation in the render thread, make sure to
    // evaluate the texture data generator output
    // (this might change some property values)
    if (m_dataFunctor && !m_textureData) {
        m_textureData = m_textureDataManager->getData(m_dataFunctor);

        // if there is a texture generator, most properties will be defined by it
        if (m_textureData) {
            if (m_properties.target != QAbstractTexture::TargetAutomatic)
                qWarning() << "[Qt3DRender::GLTexture] When a texture provides a generator, it's target is expected to be TargetAutomatic";

            m_properties.target = m_textureData->target();
            m_properties.width = m_textureData->width();
            m_properties.height = m_textureData->height();
            m_properties.depth = m_textureData->depth();
            m_properties.layers = m_textureData->layers();
            m_properties.format = m_textureData->format();

            const QVector<QTextureImageDataPtr> imageData = m_textureData->imageData();

            if (imageData.size() > 0) {
                // Set the mips level based on the first image if autoMipMapGeneration is disabled
                if (!m_properties.generateMipMaps)
                    m_properties.mipLevels = imageData.first()->mipLevels();
            }

            m_dirty |= Properties;
            needUpload = true;
        } else {
            qWarning() << "[Qt3DRender::GLTexture] No QTextureData generated from Texture Generator";
            texturedDataInvalid = true;
        }
    }

    // additional texture images may be defined through image data generators
    if (m_dirty.testFlag(TextureData)) {
        m_imageData.clear();
        needUpload = true;

        for (const Image &img : qAsConst(m_images)) {
            const QTextureImageDataPtr imgData = m_textureImageDataManager->getData(img.generator);

            if (imgData) {
                m_imageData << imgData;

                // If the texture doesn't have a texture generator, we will
                // derive some properties from the first TextureImage (layer=0, miplvl=0, face=0)
                if (!m_textureData && img.layer == 0 && img.mipLevel == 0 && img.face == QAbstractTexture::CubeMapPositiveX) {
                    if (imgData->width() != -1 && imgData->height() != -1 && imgData->depth() != -1) {
                        m_properties.width = imgData->width();
                        m_properties.height = imgData->height();
                        m_properties.depth = imgData->depth();
                    }

                    // Set the format of the texture if the texture format is set to Automatic
                    if (m_properties.format == QAbstractTexture::Automatic) {
                        m_properties.format = static_cast<QAbstractTexture::TextureFormat>(imgData->format());
                    }

                    m_dirty |= Properties;
                }
            } else {
                qWarning() << "[Qt3DRender::GLTexture] No QTextureImageData generated from functor";
                texturedDataInvalid = true;
            }
        }
    }


    // if the properties changed, we need to re-allocate the texture
    if (m_dirty.testFlag(Properties)) {
        delete m_gl;
        m_gl = nullptr;
    }

    if (!m_gl) {
        m_gl = buildGLTexture();
        m_gl->allocateStorage();
        if (!m_gl->isStorageAllocated()) {
            qWarning() << Q_FUNC_INFO << "texture storage allocation failed";
            return nullptr;
        }
    }

    // need to (re-)upload texture data?
    if (needUpload && !texturedDataInvalid) {
        uploadGLTextureData();
    }

    // need to set texture parameters?
    if (m_dirty.testFlag(Properties) || m_dirty.testFlag(Parameters)) {
        updateGLTextureParameters();
    }

    m_dirty = 0;

    return m_gl;
}

void GLTexture::setParameters(const TextureParameters &params)
{
    if (m_parameters != params) {
        m_parameters = params;
        QMutexLocker locker(&m_dirtyFlagMutex);
        m_dirty |= Parameters;
    }
}

void GLTexture::setProperties(const TextureProperties &props)
{
    if (m_properties != props) {
        m_properties = props;
        QMutexLocker locker(&m_dirtyFlagMutex);
        m_dirty |= Properties;
    }
}

void GLTexture::setImages(const QVector<Image> &images)
{
    // check if something has changed at all
    bool same = (images.size() == m_images.size());
    if (same) {
        for (int i = 0; i < images.size(); i++) {
            if (images[i] != m_images[i])
                same = false;
        }
    }

    // de-reference all old texture image generators that will no longer be used.
    // we need to check all generators against each other to make sure we don't
    // de-ref a texture that would still be in use, thus maybe causing it to
    // be deleted
    if (!same) {
        for (const Image &oldImg : qAsConst(m_images)) {
            bool stillHaveThatImage = false;

            for (const Image &newImg : images) {
                if (oldImg.generator == newImg.generator) {
                    stillHaveThatImage = true;
                    break;
                }
            }

            if (!stillHaveThatImage)
                m_textureImageDataManager->releaseData(oldImg.generator, this);
        }

        m_images = images;

        // Don't mark the texture data as dirty yet. We defer this until the
        // generators have been executed and the data is made available to the
        // TextureDataManager.

        // make sure the generators are executed
        bool newEntriesCreated = false;
        for (const Image& img : qAsConst(images)) {
            newEntriesCreated |= m_textureImageDataManager->requestData(img.generator, this);
        }

        if (!newEntriesCreated) {
            // request a data upload (very important in case the image data already
            // exists and wouldn't trigger an update)
            requestUpload();
        }
    }
}

QOpenGLTexture *GLTexture::buildGLTexture()
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        qWarning() << Q_FUNC_INFO << "requires an OpenGL context";
        return nullptr;
    }

    if (m_properties.target == QAbstractTexture::TargetAutomatic) {
        qWarning() << Q_FUNC_INFO << "something went wrong, target shouldn't be automatic at this point";
        return nullptr;
    }

    QOpenGLTexture* glTex = new QOpenGLTexture(static_cast<QOpenGLTexture::Target>(m_properties.target));

    // m_format may not be ES2 compatible. Now it's time to convert it, if necessary.
    QAbstractTexture::TextureFormat format = m_properties.format;
    if (ctx->isOpenGLES() && ctx->format().majorVersion() < 3) {
        switch (m_properties.format) {
        case QOpenGLTexture::RGBA8_UNorm:
        case QOpenGLTexture::RGBAFormat:
            format = QAbstractTexture::RGBAFormat;
            break;
        case QOpenGLTexture::RGB8_UNorm:
        case QOpenGLTexture::RGBFormat:
            format = QAbstractTexture::RGBFormat;
            break;
        case QOpenGLTexture::DepthFormat:
            format = QAbstractTexture::DepthFormat;
            break;
        default:
            qWarning() << Q_FUNC_INFO << "could not find a matching OpenGL ES 2.0 unsized texture format";
            break;
        }
    }

    // Map ETC1 to ETC2 when supported. This allows using features like
    // immutable storage as ETC2 is standard in GLES 3.0, while the ETC1 extension
    // is written against GLES 1.0.
    if (m_properties.format == QAbstractTexture::RGB8_ETC1) {
        if ((ctx->isOpenGLES() && ctx->format().majorVersion() >= 3)
                || ctx->hasExtension(QByteArrayLiteral("GL_OES_compressed_ETC2_RGB8_texture"))
                || ctx->hasExtension(QByteArrayLiteral("GL_ARB_ES3_compatibility")))
            format = m_properties.format = QAbstractTexture::RGB8_ETC2;
    }

    glTex->setFormat(m_properties.format == QAbstractTexture::Automatic ?
                         QOpenGLTexture::NoFormat :
                         static_cast<QOpenGLTexture::TextureFormat>(format));
    glTex->setSize(m_properties.width, m_properties.height, m_properties.depth);
    // Set layers count if texture array
    if (m_properties.target == QAbstractTexture::Target1DArray ||
            m_properties.target == QAbstractTexture::Target2DArray ||
            m_properties.target == QAbstractTexture::Target3D ||
            m_properties.target == QAbstractTexture::Target2DMultisampleArray ||
            m_properties.target == QAbstractTexture::TargetCubeMapArray) {
        glTex->setLayers(m_properties.layers);
    }

    if (m_properties.target == QAbstractTexture::Target2DMultisample ||
            m_properties.target == QAbstractTexture::Target2DMultisampleArray) {
        // Set samples count if multisampled texture
        // (multisampled textures don't have mipmaps)
        glTex->setSamples(m_properties.samples);
    } else if (m_properties.generateMipMaps) {
        glTex->setMipLevels(glTex->maximumMipLevels());
    } else {
        glTex->setAutoMipMapGenerationEnabled(false);
        glTex->setMipBaseLevel(0);
        glTex->setMipMaxLevel(m_properties.mipLevels - 1);
        glTex->setMipLevels(m_properties.mipLevels);
    }

    if (!glTex->create()) {
        qWarning() << Q_FUNC_INFO << "creating QOpenGLTexture failed";
        return nullptr;
    }

    return glTex;
}

static void uploadGLData(QOpenGLTexture *glTex,
                         int level, int layer, QOpenGLTexture::CubeMapFace face,
                         const QByteArray &bytes, const QTextureImageDataPtr &data)
{
    if (data->isCompressed()) {
        glTex->setCompressedData(level, layer, face, bytes.size(), bytes.constData());
    } else {
        QOpenGLPixelTransferOptions uploadOptions;
        uploadOptions.setAlignment(1);
        glTex->setData(level, layer, face, data->pixelFormat(), data->pixelType(), bytes.constData(), &uploadOptions);
    }
}

void GLTexture::uploadGLTextureData()
{
    // Upload all QTexImageData set by the QTextureGenerator
    if (m_textureData) {
        const QVector<QTextureImageDataPtr> imgData = m_textureData->imageData();

        for (const QTextureImageDataPtr &data : imgData) {
            const int mipLevels = m_properties.generateMipMaps ? 1 : data->mipLevels();

            for (int layer = 0; layer < data->layers(); layer++) {
                for (int face = 0; face < data->faces(); face++) {
                    for (int level = 0; level < mipLevels; level++) {
                        // ensure we don't accidently cause a detach / copy of the raw bytes
                        const QByteArray bytes(data->data(layer, face, level));
                        uploadGLData(m_gl, level, layer,
                                     static_cast<QOpenGLTexture::CubeMapFace>(QOpenGLTexture::CubeMapPositiveX + face),
                                     bytes, data);
                    }
                }
            }
        }
    }

    // Upload all QTexImageData references by the TextureImages
    for (int i = 0; i < m_images.size(); i++) {
        const QTextureImageDataPtr &imgData = m_imageData.at(i);

        // ensure we don't accidently cause a detach / copy of the raw bytes
        const QByteArray bytes(imgData->data());
        uploadGLData(m_gl, m_images[i].mipLevel, m_images[i].layer,
                     static_cast<QOpenGLTexture::CubeMapFace>(m_images[i].face),
                     bytes, imgData);
    }
}

void GLTexture::updateGLTextureParameters()
{
    m_gl->setWrapMode(QOpenGLTexture::DirectionS, static_cast<QOpenGLTexture::WrapMode>(m_parameters.wrapModeX));
    if (m_properties.target != QAbstractTexture::Target1D &&
            m_properties.target != QAbstractTexture::Target1DArray &&
            m_properties.target != QAbstractTexture::TargetBuffer)
        m_gl->setWrapMode(QOpenGLTexture::DirectionT, static_cast<QOpenGLTexture::WrapMode>(m_parameters.wrapModeY));
    if (m_properties.target == QAbstractTexture::Target3D)
        m_gl->setWrapMode(QOpenGLTexture::DirectionR, static_cast<QOpenGLTexture::WrapMode>(m_parameters.wrapModeZ));
    m_gl->setMinMagFilters(static_cast<QOpenGLTexture::Filter>(m_parameters.minificationFilter),
                           static_cast<QOpenGLTexture::Filter>(m_parameters.magnificationFilter));
    if (m_gl->hasFeature(QOpenGLTexture::AnisotropicFiltering))
        m_gl->setMaximumAnisotropy(m_parameters.maximumAnisotropy);
    if (m_gl->hasFeature(QOpenGLTexture::TextureComparisonOperators)) {
        m_gl->setComparisonFunction(static_cast<QOpenGLTexture::ComparisonFunction>(m_parameters.comparisonFunction));
        m_gl->setComparisonMode(static_cast<QOpenGLTexture::ComparisonMode>(m_parameters.comparisonMode));
    }
}


} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
