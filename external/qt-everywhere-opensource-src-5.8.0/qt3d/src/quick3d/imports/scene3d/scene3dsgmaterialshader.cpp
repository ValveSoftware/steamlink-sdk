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

#include "scene3dsgmaterialshader_p.h"
#include "scene3dsgmaterial_p.h"

#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>
#include <QtGui/qsurfaceformat.h>

QT_BEGIN_NAMESPACE

namespace {

inline bool isPowerOfTwo(int x)
{
    // Assumption: x >= 1
    return x == (x & -x);
}

}

namespace Qt3DRender {

/*!
    \class Qt3DCore::SCene3DMaterialShader
    \internal

    \brief The Qt3DRender::Scene3DSGMaterialShader class is a custom
    QSGMaterialShader subclass instantiated by a Qt3DRender::Scene3DSGMateria1

    The Qt3DRender::Scene3DSGMaterialShader provides a shader that renders a texture
    using premultiplied alpha.
 */

QSGMaterialType Scene3DSGMaterialShader::type;

const char * const *Scene3DSGMaterialShader::attributeNames() const
{
    static char const *const attr[] = { "qt_VertexPosition", "qt_VertexTexCoord", 0 };
    return attr;
}

const char *Scene3DSGMaterialShader::vertexShader() const
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx->format().version() >= qMakePair(3, 2) && ctx->format().profile() == QSurfaceFormat::CoreProfile) {
        return ""
               "#version 150 core                                   \n"
               "uniform mat4 qt_Matrix;                             \n"
               "in vec4 qt_VertexPosition;                          \n"
               "in vec2 qt_VertexTexCoord;                          \n"
               "out vec2 qt_TexCoord;                               \n"
               "void main() {                                       \n"
               "   qt_TexCoord = qt_VertexTexCoord;                 \n"
               "   gl_Position = qt_Matrix * qt_VertexPosition;     \n"
               "}";
    } else {
        return ""
               "uniform highp mat4 qt_Matrix;                       \n"
               "attribute highp vec4 qt_VertexPosition;             \n"
               "attribute highp vec2 qt_VertexTexCoord;             \n"
               "varying highp vec2 qt_TexCoord;                     \n"
               "void main() {                                       \n"
               "   qt_TexCoord = qt_VertexTexCoord;                 \n"
               "   gl_Position = qt_Matrix * qt_VertexPosition;     \n"
               "}";
    }
}

const char *Scene3DSGMaterialShader::fragmentShader() const
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx->format().version() >= qMakePair(3, 2) && ctx->format().profile() == QSurfaceFormat::CoreProfile) {
        return ""
               "#version 150 core                                   \n"
               "uniform sampler2D source;                           \n"
               "uniform float qt_Opacity;                           \n"
               "in vec2 qt_TexCoord;                                \n"
               "out vec4 fragColor;                                 \n"
               "void main() {                                       \n"
               "   vec4 p = texture(source, qt_TexCoord);         \n"
               "   fragColor = vec4(p.rgb * p.a, qt_Opacity * p.a); \n"
               "}";
    } else {
        return ""
               "uniform highp sampler2D source;                         \n"
               "uniform highp float qt_Opacity;                         \n"
               "varying highp vec2 qt_TexCoord;                         \n"
               "void main() {                                           \n"
               "   highp vec4 p = texture2D(source, qt_TexCoord);       \n"
               "   gl_FragColor = vec4(p.rgb * p.a, qt_Opacity * p.a);  \n"
               "}";
    }
}

void Scene3DSGMaterialShader::initialize()
{
    m_matrixId = program()->uniformLocation("qt_Matrix");
    m_opacityId = program()->uniformLocation("qt_Opacity");
}

void Scene3DSGMaterialShader::updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    Q_ASSERT(oldEffect == 0 || newEffect->type() == oldEffect->type());
    Scene3DSGMaterial *tx = static_cast<Scene3DSGMaterial *>(newEffect);
    Scene3DSGMaterial *oldTx = static_cast<Scene3DSGMaterial *>(oldEffect);

    QSGTexture *t = tx->texture();

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

    if (oldTx == 0 || oldTx->texture()->textureId() != t->textureId())
        t->bind();
    else
        t->updateBindOptions();

    if (state.isMatrixDirty())
        program()->setUniformValue(m_matrixId, state.combinedMatrix());

    if (state.isOpacityDirty())
        program()->setUniformValue(m_opacityId, state.opacity());
}

} // namespace Qt3DRender

QT_END_NAMESPACE
