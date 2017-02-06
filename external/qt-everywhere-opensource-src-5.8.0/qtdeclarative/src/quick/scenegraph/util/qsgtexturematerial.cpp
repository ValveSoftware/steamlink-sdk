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

#include "qsgtexturematerial_p.h"
#include "qsgtexture_p.h"
#ifndef QT_NO_OPENGL
# include <QtGui/qopenglshaderprogram.h>
# include <QtGui/qopenglfunctions.h>
#endif

QT_BEGIN_NAMESPACE

#ifndef QT_NO_OPENGL
inline static bool isPowerOfTwo(int x)
{
    // Assumption: x >= 1
    return x == (x & -x);
}
#endif

QSGMaterialType QSGOpaqueTextureMaterialShader::type;

QSGOpaqueTextureMaterialShader::QSGOpaqueTextureMaterialShader()
    : QSGMaterialShader()
{
#ifndef QT_NO_OPENGL
    setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/qt-project.org/scenegraph/shaders/opaquetexture.vert"));
    setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qt-project.org/scenegraph/shaders/opaquetexture.frag"));
#endif
}

char const *const *QSGOpaqueTextureMaterialShader::attributeNames() const
{
    static char const *const attr[] = { "qt_VertexPosition", "qt_VertexTexCoord", 0 };
    return attr;
}

void QSGOpaqueTextureMaterialShader::initialize()
{
#ifndef QT_NO_OPENGL
    m_matrix_id = program()->uniformLocation("qt_Matrix");
#endif
}

void QSGOpaqueTextureMaterialShader::updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    Q_ASSERT(oldEffect == 0 || newEffect->type() == oldEffect->type());
    QSGOpaqueTextureMaterial *tx = static_cast<QSGOpaqueTextureMaterial *>(newEffect);
    QSGOpaqueTextureMaterial *oldTx = static_cast<QSGOpaqueTextureMaterial *>(oldEffect);

    QSGTexture *t = tx->texture();

#ifndef QT_NO_DEBUG
    if (!qsg_safeguard_texture(t))
        return;
#endif

    t->setFiltering(tx->filtering());

    t->setHorizontalWrapMode(tx->horizontalWrapMode());
    t->setVerticalWrapMode(tx->verticalWrapMode());
#ifndef QT_NO_OPENGL
    bool npotSupported = const_cast<QOpenGLContext *>(state.context())
        ->functions()->hasOpenGLFeature(QOpenGLFunctions::NPOTTextureRepeat);
    if (!npotSupported) {
        QSize size = t->textureSize();
        const bool isNpot = !isPowerOfTwo(size.width()) || !isPowerOfTwo(size.height());
        if (isNpot) {
            t->setHorizontalWrapMode(QSGTexture::ClampToEdge);
            t->setVerticalWrapMode(QSGTexture::ClampToEdge);
        }
    }
#else
    Q_UNUSED(state)
#endif
    t->setMipmapFiltering(tx->mipmapFiltering());

    if (oldTx == 0 || oldTx->texture()->textureId() != t->textureId())
        t->bind();
    else
        t->updateBindOptions();
#ifndef QT_NO_OPENGL
    if (state.isMatrixDirty())
        program()->setUniformValue(m_matrix_id, state.combinedMatrix());
#endif
}


/*!
    \class QSGOpaqueTextureMaterial
    \brief The QSGOpaqueTextureMaterial class provides a convenient way of
    rendering textured geometry in the scene graph.
    \inmodule QtQuick
    \ingroup qtquick-scenegraph-materials

    \warning This utility class is only functional when running with the OpenGL
    backend of the Qt Quick scenegraph.

    The opaque textured material will fill every pixel in a geometry with
    the supplied texture. The material does not respect the opacity of the
    QSGMaterialShader::RenderState, so opacity nodes in the parent chain
    of nodes using this material, have no effect.

    The geometry to be rendered with an opaque texture material requires
    vertices in attribute location 0 and texture coordinates in attribute
    location 1. The texture coordinate is a 2-dimensional floating-point
    tuple. The QSGGeometry::defaultAttributes_TexturedPoint2D returns an
    attribute set compatible with this material.

    The texture to be rendered can be set using setTexture(). How the
    texture should be rendered can be specified using setMipmapFiltering(),
    setFiltering(), setHorizontalWrapMode() and setVerticalWrapMode().
    The rendering state is set on the texture instance just before it
    is bound.

    The opaque textured material respects the current matrix and the alpha
    channel of the texture. It will disregard the accumulated opacity in
    the scenegraph.

    A texture material must have a texture set before it is used as
    a material in the scene graph.
 */



/*!
    Creates a new QSGOpaqueTextureMaterial.

    The default mipmap filtering and filtering mode is set to
    QSGTexture::Nearest. The default wrap modes is set to
    \c QSGTexture::ClampToEdge.

 */
QSGOpaqueTextureMaterial::QSGOpaqueTextureMaterial()
    : m_texture(0)
    , m_filtering(QSGTexture::Nearest)
    , m_mipmap_filtering(QSGTexture::None)
    , m_horizontal_wrap(QSGTexture::ClampToEdge)
    , m_vertical_wrap(QSGTexture::ClampToEdge)
{
}


/*!
    \internal
 */
QSGMaterialType *QSGOpaqueTextureMaterial::type() const
{
    return &QSGOpaqueTextureMaterialShader::type;
}

/*!
    \internal
 */
QSGMaterialShader *QSGOpaqueTextureMaterial::createShader() const
{
    return new QSGOpaqueTextureMaterialShader;
}



/*!
    \fn QSGTexture *QSGOpaqueTextureMaterial::texture() const

    Returns this texture material's texture.
 */



/*!
    Sets the texture of this material to \a texture.

    The material does not take ownership of the texture.
 */

void QSGOpaqueTextureMaterial::setTexture(QSGTexture *texture)
{
    m_texture = texture;
    setFlag(Blending, m_texture ? m_texture->hasAlphaChannel() : false);
}



/*!
    \fn void QSGOpaqueTextureMaterial::setMipmapFiltering(QSGTexture::Filtering filtering)

    Sets the mipmap mode to \a filtering.

    The mipmap filtering mode is set on the texture instance just before the
    texture is bound for rendering.

    If the texture does not have mipmapping support, enabling mipmapping has no
    effect.
 */



/*!
    \fn QSGTexture::Filtering QSGOpaqueTextureMaterial::mipmapFiltering() const

    Returns this material's mipmap filtering mode.

    The default mipmap mode is \c QSGTexture::Nearest.
 */



/*!
    \fn void QSGOpaqueTextureMaterial::setFiltering(QSGTexture::Filtering filtering)

    Sets the filtering to \a filtering.

    The filtering mode is set on the texture instance just before the texture
    is bound for rendering.
 */



/*!
    \fn QSGTexture::Filtering QSGOpaqueTextureMaterial::filtering() const

    Returns this material's filtering mode.

    The default filtering is \c QSGTexture::Nearest.
 */



/*!
    \fn void QSGOpaqueTextureMaterial::setHorizontalWrapMode(QSGTexture::WrapMode mode)

    Sets the horizontal wrap mode to \a mode.

    The horizontal wrap mode is set on the texture instance just before the texture
    is bound for rendering.
 */



 /*!
     \fn QSGTexture::WrapMode QSGOpaqueTextureMaterial::horizontalWrapMode() const

     Returns this material's horizontal wrap mode.

     The default horizontal wrap mode is \c QSGTexutre::ClampToEdge.
  */



/*!
    \fn void QSGOpaqueTextureMaterial::setVerticalWrapMode(QSGTexture::WrapMode mode)

    Sets the vertical wrap mode to \a mode.

    The vertical wrap mode is set on the texture instance just before the texture
    is bound for rendering.
 */



 /*!
     \fn QSGTexture::WrapMode QSGOpaqueTextureMaterial::verticalWrapMode() const

     Returns this material's vertical wrap mode.

     The default vertical wrap mode is \c QSGTexutre::ClampToEdge.
  */



/*!
    \internal
 */

int QSGOpaqueTextureMaterial::compare(const QSGMaterial *o) const
{
    Q_ASSERT(o && type() == o->type());
    const QSGOpaqueTextureMaterial *other = static_cast<const QSGOpaqueTextureMaterial *>(o);
    if (int diff = m_texture->textureId() - other->texture()->textureId())
        return diff;
    return int(m_filtering) - int(other->m_filtering);
}



/*!
    \class QSGTextureMaterial
    \brief The QSGTextureMaterial class provides a convenient way of
    rendering textured geometry in the scene graph.
    \inmodule QtQuick
    \ingroup qtquick-scenegraph-materials

    \warning This utility class is only functional when running with the OpenGL
    backend of the Qt Quick scenegraph.

    The textured material will fill every pixel in a geometry with
    the supplied texture.

    The geometry to be rendered with a texture material requires
    vertices in attribute location 0 and texture coordinates in attribute
    location 1. The texture coordinate is a 2-dimensional floating-point
    tuple. The QSGGeometry::defaultAttributes_TexturedPoint2D returns an
    attribute set compatible with this material.

    The texture to be rendered can be set using setTexture(). How the
    texture should be rendered can be specified using setMipmapFiltering(),
    setFiltering(), setHorizontalWrapMode() and setVerticalWrapMode().
    The rendering state is set on the texture instance just before it
    is bound.

    The textured material respects the current matrix and the alpha
    channel of the texture. It will also respect the accumulated opacity
    in the scenegraph.

    A texture material must have a texture set before it is used as
    a material in the scene graph.
 */

QSGMaterialType QSGTextureMaterialShader::type;



/*!
    \internal
 */

QSGMaterialType *QSGTextureMaterial::type() const
{
    return &QSGTextureMaterialShader::type;
}



/*!
    \internal
 */

QSGMaterialShader *QSGTextureMaterial::createShader() const
{
    return new QSGTextureMaterialShader;
}

QSGTextureMaterialShader::QSGTextureMaterialShader()
    : QSGOpaqueTextureMaterialShader()
{
#ifndef QT_NO_OPENGL
    setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qt-project.org/scenegraph/shaders/texture.frag"));
#endif
}

void QSGTextureMaterialShader::updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    Q_ASSERT(oldEffect == 0 || newEffect->type() == oldEffect->type());
#ifndef QT_NO_OPENGL
    if (state.isOpacityDirty())
        program()->setUniformValue(m_opacity_id, state.opacity());
#endif
    QSGOpaqueTextureMaterialShader::updateState(state, newEffect, oldEffect);
}

void QSGTextureMaterialShader::initialize()
{
    QSGOpaqueTextureMaterialShader::initialize();
#ifndef QT_NO_OPENGL
    m_opacity_id = program()->uniformLocation("opacity");
#endif
}

QT_END_NAMESPACE
