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

#ifndef QT3DRENDER_RENDER_APITEXTUREMANAGER_P_H
#define QT3DRENDER_RENDER_APITEXTUREMANAGER_P_H

#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/texturedatamanager_p.h>
#include <Qt3DRender/private/textureimage_p.h>
#include <Qt3DRender/private/texture_p.h>
#include <Qt3DRender/qtexturegenerator.h>
#include <Qt3DRender/qtextureimagedatagenerator.h>

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

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

// Manages instances of APITexture. This includes sharing between multiple
// Texture nodes, updating APITextures and deleting abandoned instances.
template <class APITexture, class APITextureImage>
class APITextureManager
{
public:

    explicit APITextureManager(TextureImageManager *textureImageManager,
                               TextureDataManager *textureDataManager,
                               TextureImageDataManager *textureImageDataManager)
        : m_textureImageManager(textureImageManager)
        , m_textureDataManager(textureDataManager)
        , m_textureImageDataManager(textureImageDataManager)
    {
    }

    ~APITextureManager()
    {
        qDeleteAll(activeResources());
        m_nodeIdToGLTexture.clear();
        m_sharedTextures.clear();
        m_updatedTextures.clear();
    }

    // Used to retrieve all resources that needs to be destroyed on the GPU
    QVector<APITexture *> activeResources() const
    {
        // Active Resources are
        // all shared textures
        // all unique textures
        // all textures that haven't yet been destroyed
        // Note: updatedTextures only referenced textures in one of these 3 vectors
        return m_sharedTextures.keys().toVector() + m_uniqueTextures + m_abandonedTextures;
    }

    APITexture *lookupResource(Qt3DCore::QNodeId textureId)
    {
        return m_nodeIdToGLTexture.value(textureId);
    }

    // Returns a APITexture that matches the given QTexture node. Will make sure
    // that texture data generator jobs are launched, if necessary. The APITexture
    // may be shared between multiple texture backend nodes
    APITexture *getOrCreateShared(const Texture *node)
    {
        Q_ASSERT(node);

        APITexture *shared = findMatchingShared(node);

        // no matching shared texture was found; create a new one:
        if (shared == nullptr)
            shared = createTexture(node, false);

        // store texture node to shared texture relationship
        adoptShared(shared, node);

        return shared;
    }

    // Store that the shared texture references node
    void adoptShared(APITexture *sharedApiTexture, const Texture *node)
    {
        if (!m_sharedTextures[sharedApiTexture].contains(node->peerId())) {
            m_sharedTextures[sharedApiTexture].push_back(node->peerId());
            m_nodeIdToGLTexture.insert(node->peerId(), sharedApiTexture);
        }
    }

    // If there is already a shared texture with the properties of the given
    // texture node, return this instance, else NULL.
    // Note: the reference to the texture node is added if the shared texture
    // wasn't referencing it already
    APITexture *findMatchingShared(const Texture *node)
    {
        Q_ASSERT(node);

        // search for existing texture
        const auto end = m_sharedTextures.end();
        for (auto it = m_sharedTextures.begin(); it != end; ++it)
            if (isSameTexture(it.key(), node))
                return it.key();
        return nullptr;
    }

    // Returns a APITexture that matches the given QTexture node. Will make sure
    // that texture data generator jobs are launched, if necessary.
    APITexture *createUnique(const Texture *node)
    {
        Q_ASSERT(node);
        APITexture *uniqueTex = createTexture(node, true);
        m_uniqueTextures.push_back(uniqueTex);
        m_nodeIdToGLTexture.insert(node->peerId(), uniqueTex);
        return uniqueTex;
    }

    // De-associate the given APITexture from the backend node. If the texture
    // is no longer referenced by any other node, it will be deleted.
    void abandon(APITexture *tex, const Texture *node)
    {
        APITexture *apiTexture = m_nodeIdToGLTexture.take(node->peerId());
        Q_ASSERT(tex == apiTexture);

        if (Q_UNLIKELY(!apiTexture)) {
            qWarning() << "[Qt3DRender::TextureManager] abandon: could not find Texture";
            return;
        }

        if (tex->isUnique()) {
            m_uniqueTextures.removeAll(apiTexture);
            m_abandonedTextures.push_back(apiTexture);
        } else {
            QVector<Qt3DCore::QNodeId> &referencedTextureNodes = m_sharedTextures[apiTexture];
            referencedTextureNodes.removeAll(node->peerId());

            // If no texture nodes is referencing the shared APITexture, remove it
            if (referencedTextureNodes.empty()) {
                m_abandonedTextures.push_back(apiTexture);
                m_sharedTextures.remove(apiTexture);
                tex->destroyResources();
            }
        }
    }

    // Change the properties of the given texture, if it is a non-shared texture
    // Returns true, if it was changed successfully, false otherwise
    bool setProperties(APITexture *tex, const TextureProperties &props)
    {
        Q_ASSERT(tex);

        if (isShared(tex))
            return false;

        tex->setProperties(props);
        m_updatedTextures.push_back(tex);

        return true;
    }

    // Change the parameters of the given texture, if it is a non-shared texture
    // Returns true, if it was changed successfully, false otherwise
    bool setParameters(APITexture *tex, const TextureParameters &params)
    {
        Q_ASSERT(tex);

        if (isShared(tex))
            return false;

        tex->setParameters(params);
        m_updatedTextures.push_back(tex);

        return true;
    }

    // Change the texture images of the given texture, if it is a non-shared texture
    // Return true, if it was changed successfully, false otherwise
    bool setImages(APITexture *tex, const QVector<HTextureImage> &images)
    {
        Q_ASSERT(tex);

        if (isShared(tex))
            return false;

        // create Image structs
        QVector<APITextureImage> texImgs = texImgsFromNodes(images);
        if (texImgs.size() != images.size())
            return false;

        tex->setImages(texImgs);
        m_updatedTextures.push_back(tex);

        return true;
    }

    // Change the texture data generator for given texture, if it is a non-shared texture
    // Return true, if it was changed successfully, false otherwise
    bool setGenerator(APITexture *tex, const QTextureGeneratorPtr &generator)
    {
        Q_ASSERT(tex);

        if (isShared(tex))
            return false;

        tex->setGenerator(generator);
        m_updatedTextures.push_back(tex);

        return true;
    }

    // Retrieves abandoned textures. This should be regularly called from the OpenGL thread
    // to make sure needed GL resources are de-allocated.
    QVector<APITexture*> takeAbandonedTextures()
    {
        return std::move(m_abandonedTextures);
    }

    // Retrieves textures that have been modified
    QVector<APITexture*> takeUpdatedTextures()
    {
        return std::move(m_updatedTextures);
    }

    // Returns whether the given APITexture is shared between multiple TextureNodes
    bool isShared(APITexture *impl)
    {
        Q_ASSERT(impl);

        if (impl->isUnique())
            return false;

        auto it = m_sharedTextures.constFind(impl);
        if (it == m_sharedTextures.cend())
            return false;

        return it.value().size() > 1;
    }

private:

    // Check if the given APITexture matches the TextureNode
    bool isSameTexture(const APITexture *tex, const Texture *texNode)
    {
        // make sure there either are no texture generators, or the two are the same
        if (tex->textureGenerator().isNull() != texNode->dataGenerator().isNull())
            return false;
        if (!tex->textureGenerator().isNull() && !(*tex->textureGenerator() == *texNode->dataGenerator()))
            return false;

        // make sure the image generators are the same
        const QVector<APITextureImage> texImgGens = tex->images();
        const QVector<HTextureImage> texImgs = texNode->textureImages();
        if (texImgGens.size() != texImgs.size())
            return false;
        for (int i = 0; i < texImgGens.size(); ++i) {
            const TextureImage *img = m_textureImageManager->data(texImgs[i]);
            Q_ASSERT(img != nullptr);
            if (!(*img->dataGenerator() == *texImgGens[i].generator)
                    || img->layer() != texImgGens[i].layer
                    || img->face() != texImgGens[i].face
                    || img->mipLevel() != texImgGens[i].mipLevel)
                return false;
        }

        // if the texture has a texture generator, this generator will mostly determine
        // the properties of the texture.
        if (!tex->textureGenerator().isNull())
            return (tex->properties().generateMipMaps == texNode->properties().generateMipMaps
                    && tex->parameters() == texNode->parameters());

        // if it doesn't have a texture generator, but texture image generators,
        // few more properties will influence the texture type
        if (!texImgGens.empty())
            return (tex->properties().target == texNode->properties().target
                    && tex->properties().format == texNode->properties().format
                    && tex->properties().generateMipMaps == texNode->properties().generateMipMaps
                    && tex->parameters() == texNode->parameters());

        // texture without images
        return tex->properties() == texNode->properties()
                && tex->parameters() == texNode->parameters();
    }

    // Create APITexture from given TextureNode. Also make sure the generators
    // will be executed by jobs soon.
    APITexture *createTexture(const Texture *node, bool unique)
    {
        // create Image structs
        const QVector<APITextureImage> texImgs = texImgsFromNodes(node->textureImages());
        if (texImgs.empty() && !node->textureImages().empty())
            return nullptr;

        // no matching shared texture was found, create a new one
        APITexture *newTex = new APITexture(m_textureDataManager, m_textureImageDataManager, node->dataGenerator(), unique);
        newTex->setProperties(node->properties());
        newTex->setParameters(node->parameters());
        newTex->setImages(texImgs);

        m_updatedTextures.push_back(newTex);

        return newTex;
    }

    QVector<APITextureImage> texImgsFromNodes(const QVector<HTextureImage> &images) const
    {
        QVector<APITextureImage> ret;
        ret.resize(images.size());

        for (int i = 0; i < images.size(); ++i) {
            const TextureImage *img = m_textureImageManager->data(images[i]);
            if (!img) {
                qWarning() << "[Qt3DRender::TextureManager] invalid TextureImage handle";
                return QVector<APITextureImage>();
            }

            ret[i].generator = img->dataGenerator();
            ret[i].face = img->face();
            ret[i].layer = img->layer();
            ret[i].mipLevel = img->mipLevel();
        }

        return ret;
    }

    TextureImageManager *m_textureImageManager;
    TextureDataManager *m_textureDataManager;
    TextureImageDataManager *m_textureImageDataManager;

    /* each non-unique texture is associated with a number of Texture nodes referencing it */
    QHash<APITexture*, QVector<Qt3DCore::QNodeId>> m_sharedTextures;

    // Texture id -> APITexture (both shared and unique ones)
    QHash<Qt3DCore::QNodeId, APITexture *> m_nodeIdToGLTexture;

    QVector<APITexture*> m_uniqueTextures;
    QVector<APITexture*> m_abandonedTextures;
    QVector<APITexture*> m_updatedTextures;
};


} // Render

} // Qt3DRender

QT_END_NAMESPACE


#endif // QT3DRENDER_RENDER_APITEXTUREMANAGER_P_H
