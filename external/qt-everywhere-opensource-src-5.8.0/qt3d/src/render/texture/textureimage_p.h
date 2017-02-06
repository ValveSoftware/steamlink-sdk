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

#ifndef QT3DRENDER_RENDER_TEXTUREIMAGE_H
#define QT3DRENDER_RENDER_TEXTUREIMAGE_H

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

#include <Qt3DRender/private/backendnode_p.h>
#include <Qt3DRender/private/handle_types_p.h>
#include <Qt3DRender/qabstracttexture.h>
#include <Qt3DRender/qtextureimagedatagenerator.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

class TextureManager;
class TextureImageManager;
class TextureDataManager;

/**
 * Backend class for QAbstractTextureImage.
 * Will only hold the generator and some info values.
 */
class Q_AUTOTEST_EXPORT TextureImage : public BackendNode
{
public:
    TextureImage();
    ~TextureImage();

    void cleanup();

    void setTextureManager(TextureManager *manager);

    void sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e) Q_DECL_OVERRIDE;

    inline int layer() const { return m_layer; }
    inline int mipLevel() const { return m_mipLevel; }
    inline QAbstractTexture::CubeMapFace face() const { return m_face; }
    inline QTextureImageDataGeneratorPtr dataGenerator() const { return m_generator; }

private:
    void initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change) Q_DECL_FINAL;

    int m_layer;
    int m_mipLevel;
    QAbstractTexture::CubeMapFace m_face;
    QTextureImageDataGeneratorPtr m_generator;

    TextureManager *m_textureManager;
    HTexture m_textureProvider;
};

class TextureImageFunctor : public Qt3DCore::QBackendNodeMapper
{
public:
    explicit TextureImageFunctor(AbstractRenderer *renderer,
                                 TextureManager *textureManager,
                                 TextureImageManager *textureImageManager);

    Qt3DCore::QBackendNode *create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const Q_DECL_FINAL;
    Qt3DCore::QBackendNode *get(Qt3DCore::QNodeId id) const Q_DECL_FINAL;
    void destroy(Qt3DCore::QNodeId id) const Q_DECL_FINAL;

private:
    AbstractRenderer *m_renderer;
    TextureManager *m_textureManager;
    TextureImageManager *m_textureImageManager;
};


} // namespace Render

} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // QT3DRENDER_RENDER_TEXTUREIMAGE_H
