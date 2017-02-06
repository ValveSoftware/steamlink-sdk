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

#include "textureimage_p.h"
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DRender/qtextureimage.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/qabstracttextureimage_p.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

TextureImage::TextureImage()
    : BackendNode(ReadWrite)
    , m_layer(0)
    , m_mipLevel(0)
    , m_face(QAbstractTexture::CubeMapPositiveX)
    , m_textureManager(nullptr)
{
}

TextureImage::~TextureImage()
{
}

void TextureImage::cleanup()
{
}

void TextureImage::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QAbstractTextureImageData>>(change);
    const auto &data = typedChange->data;
    m_mipLevel = data.mipLevel;
    m_layer = data.layer;
    m_face = data.face;
    m_generator = data.generator;

    if (!change->parentId()) {
        qWarning() << "No QAbstractTexture parent found";
    } else {
        const QNodeId id = change->parentId();
        m_textureProvider = m_textureManager->lookupHandle(id);
        Texture *texture = m_textureManager->data(m_textureProvider);
        Q_ASSERT(texture);
        // Notify the Texture that it has a new TextureImage and needs an update
        texture->addTextureImage(peerId());
    }
}

void TextureImage::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);

    if (e->type() == PropertyUpdated) {
        if (propertyChange->propertyName() == QByteArrayLiteral("layer")) {
            m_layer = propertyChange->value().toInt();
        } else if (propertyChange->propertyName() == QByteArrayLiteral("mipLevel")) {
            m_mipLevel = propertyChange->value().toInt();
        } else if (propertyChange->propertyName() == QByteArrayLiteral("face")) {
            m_face = static_cast<QAbstractTexture::CubeMapFace>(propertyChange->value().toInt());
        } else if (propertyChange->propertyName() == QByteArrayLiteral("dataGenerator")) {
            m_generator = propertyChange->value().value<QTextureImageDataGeneratorPtr>();
        }

        // Notify the Texture that we were updated and request it to schedule an update job
        Texture *txt = m_textureManager->data(m_textureProvider);
        if (txt != nullptr)
            txt->addDirtyFlag(Texture::DirtyGenerators);
    }

    markDirty(AbstractRenderer::AllDirty);
    BackendNode::sceneChangeEvent(e);
}

void TextureImage::setTextureManager(TextureManager *manager)
{
    m_textureManager = manager;
}

TextureImageFunctor::TextureImageFunctor(AbstractRenderer *renderer,
                                         TextureManager *textureManager,
                                         TextureImageManager *textureImageManager)
    : m_renderer(renderer)
    , m_textureManager(textureManager)
    , m_textureImageManager(textureImageManager)
{
}

Qt3DCore::QBackendNode *TextureImageFunctor::create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const
{
    TextureImage *backend = m_textureImageManager->getOrCreateResource(change->subjectId());
    backend->setTextureManager(m_textureManager);
    backend->setRenderer(m_renderer);
    return backend;
}

Qt3DCore::QBackendNode *TextureImageFunctor::get(Qt3DCore::QNodeId id) const
{
    return m_textureImageManager->lookupResource(id);
}

void TextureImageFunctor::destroy(Qt3DCore::QNodeId id) const
{
    m_textureImageManager->releaseResource(id);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
