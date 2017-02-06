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

#ifndef QT3DRENDER_RENDER_GLTEXTURE_H
#define QT3DRENDER_RENDER_GLTEXTURE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qtextureimagedata.h>
#include <Qt3DRender/qtexturegenerator.h>
#include <Qt3DRender/private/backendnode_p.h>
#include <Qt3DRender/private/handle_types_p.h>
#include <Qt3DRender/private/texture_p.h>
#include <QOpenGLContext>
#include <QFlags>
#include <QMutex>
#include <QSize>

QT_BEGIN_NAMESPACE

class QOpenGLTexture;

namespace Qt3DRender {
namespace Render {

class TextureImageManager;
class TextureDataManager;
class TextureImageDataManager;

/**
 * @brief
 *   Actual implementation of the OpenGL texture object. Makes sure the
 *   QOpenGLTexture is up-to-date with the generators, properties and parameters
 *   that were set for this GLTexture.
 *
 *   Can be shared among multiple QTexture backend nodes through the
 *   GLTextureManager, which will make sure that there are no two GLTextures
 *   sharing the same texture data.
 *
 *   A GLTexture can be unique though. In that case, it will not be shared
 *   between QTextures, but private to one QTexture only.
 */
class Q_AUTOTEST_EXPORT GLTexture
{
public:
    GLTexture(TextureDataManager *texDataMgr,
              TextureImageDataManager *texImgDataMgr,
              const QTextureGeneratorPtr &texGen,
              bool unique);

    ~GLTexture();

    /**
     * Helper class to hold the defining properties of TextureImages
     */
    struct Image {
        QTextureImageDataGeneratorPtr generator;
        int layer;
        int mipLevel;
        QAbstractTexture::CubeMapFace face;

        inline bool operator==(const Image &o) const {
            bool sameGenerators = (generator == o.generator)
                    || (!generator.isNull() && !o.generator.isNull() && *generator == *o.generator);
            return sameGenerators && layer == o.layer && mipLevel == o.mipLevel && face == o.face;
        }
        inline bool operator!=(const Image &o) const { return !(*this == o); }
    };

    inline bool isUnique() const { return m_unique; }

    inline TextureProperties properties() const { return m_properties; }
    inline TextureParameters parameters() const { return m_parameters; }
    inline QTextureGeneratorPtr textureGenerator() const { return m_dataFunctor; }
    inline QVector<Image> images() const { return m_images; }

    inline QSize size() const { return QSize(m_properties.width, m_properties.height); }
    inline QOpenGLTexture *getGLTexture() const { return m_gl; }

    /**
     * @brief
     *   Returns the QOpenGLTexture for this GLTexture. If necessary,
     *   the GL texture will be created from the TextureImageDatas associated
     *   with the texture and image functors. If no functors are provided,
     *   the texture will be created without images.
     *
     *   If the texture properties or parameters have changed, these changes
     *   will be applied to the resulting OpenGL texture.
     */
    QOpenGLTexture* getOrCreateGLTexture();

    /**
     * @brief Make sure to call this before calling the dtor
     */
    void destroyGLTexture();

    // Called by TextureDataManager when it has new texture data from
    // a generator that needs to be uploaded.
    void requestUpload()
    {
        QMutexLocker locker(&m_dirtyFlagMutex);
        m_dirty |= TextureData;
    }

protected:

    template<class APITexture, class APITextureImage>
    friend class APITextureManager;

    /*
     * These methods are to be accessed from the GLTextureManager.
     * The renderer and the texture backend nodes can only modify Textures
     * through the GLTextureManager.
     *
     * The methods should only be called for unique textures, or textures
     * that are not shared between multiple nodes.
     */
    void setParameters(const TextureParameters &params);
    void setProperties(const TextureProperties &props);
    void setImages(const QVector<Image> &images);

private:

    enum DirtyFlag {
        TextureData  = 0x01,     // one or more generators have been executed, data needs uploading to GPU
        Properties   = 0x02,     // texture needs to be (re-)created
        Parameters   = 0x04      // texture parameters need to be (re-)set

    };
    Q_DECLARE_FLAGS(DirtyFlags, DirtyFlag)

    QOpenGLTexture *buildGLTexture();
    void uploadGLTextureData();
    void updateGLTextureParameters();
    void destroyResources();

    bool m_unique;
    DirtyFlags m_dirty;
    QMutex m_dirtyFlagMutex;
    QOpenGLTexture *m_gl;

    TextureDataManager *m_textureDataManager;
    TextureImageDataManager *m_textureImageDataManager;

    TextureProperties m_properties;
    TextureParameters m_parameters;

    QTextureGeneratorPtr m_dataFunctor;
    QVector<Image> m_images;

    // cache actual image data generated by the functors
    QTextureDataPtr m_textureData;
    QVector<QTextureImageDataPtr> m_imageData;
};

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // QT3DRENDER_RENDER_GLTEXTURE_H
