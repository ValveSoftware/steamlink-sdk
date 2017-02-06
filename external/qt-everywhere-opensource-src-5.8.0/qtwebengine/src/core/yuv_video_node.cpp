/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

// Based on cc/output/gl_renderer.cc and cc/output/shader.cc:
// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "yuv_video_node.h"

#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>
#include <QtQuick/qsgtexture.h>

namespace QtWebEngineCore {

class YUVVideoMaterialShader : public QSGMaterialShader
{
public:
    virtual void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) Q_DECL_OVERRIDE;

    virtual char const *const *attributeNames() const Q_DECL_OVERRIDE {
        static const char *names[] = {
            "a_position",
            "a_texCoord",
            0
        };
        return names;
    }

protected:
    virtual const char *vertexShader() const Q_DECL_OVERRIDE {
        // Keep in sync with cc::VertexShaderPosTexYUVStretchOffset
        const char *shader =
        "attribute highp vec4 a_position;\n"
        "attribute mediump vec2 a_texCoord;\n"
        "uniform highp mat4 matrix;\n"
        "varying mediump vec2 v_yaTexCoord;\n"
        "varying mediump vec2 v_uvTexCoord;\n"
        "uniform mediump vec2 yaTexScale;\n"
        "uniform mediump vec2 yaTexOffset;\n"
        "uniform mediump vec2 uvTexScale;\n"
        "uniform mediump vec2 uvTexOffset;\n"
        "void main() {\n"
        "  gl_Position = matrix * a_position;\n"
        "  v_yaTexCoord = a_texCoord * yaTexScale + yaTexOffset;\n"
        "  v_uvTexCoord = a_texCoord * uvTexScale + uvTexOffset;\n"
        "}";
        return shader;
    }

    virtual const char *fragmentShader() const Q_DECL_OVERRIDE {
        // Keep in sync with cc::FragmentShaderYUVVideo
        static const char *shader =
        "varying mediump vec2 v_yaTexCoord;\n"
        "varying mediump vec2 v_uvTexCoord;\n"
        "uniform sampler2D y_texture;\n"
        "uniform sampler2D u_texture;\n"
        "uniform sampler2D v_texture;\n"
        "uniform mediump float alpha;\n"
        "uniform mediump vec3 yuv_adj;\n"
        "uniform mediump mat3 yuv_matrix;\n"
        "uniform mediump vec4 ya_clamp_rect;\n"
        "uniform mediump vec4 uv_clamp_rect;\n"
        "void main() {\n"
        "  mediump vec2 ya_clamped =\n"
        "      max(ya_clamp_rect.xy, min(ya_clamp_rect.zw, v_yaTexCoord));\n"
        "  mediump float y_raw = texture2D(y_texture, ya_clamped).x;\n"
        "  mediump vec2 uv_clamped =\n"
        "      max(uv_clamp_rect.xy, min(uv_clamp_rect.zw, v_uvTexCoord));\n"
        "  mediump float u_unsigned = texture2D(u_texture, uv_clamped).x;\n"
        "  mediump float v_unsigned = texture2D(v_texture, uv_clamped).x;\n"
        "  mediump vec3 yuv = vec3(y_raw, u_unsigned, v_unsigned) + yuv_adj;\n"
        "  mediump vec3 rgb = yuv_matrix * yuv;\n"
        "  gl_FragColor = vec4(rgb, 1.0) * alpha;\n"
        "}";
        return shader;
    }

    virtual void initialize() Q_DECL_OVERRIDE {
        m_id_matrix = program()->uniformLocation("matrix");
        m_id_yaTexScale = program()->uniformLocation("yaTexScale");
        m_id_uvTexScale = program()->uniformLocation("uvTexScale");
        m_id_yaTexOffset = program()->uniformLocation("yaTexOffset");
        m_id_uvTexOffset = program()->uniformLocation("uvTexOffset");
        m_id_yaClampRect = program()->uniformLocation("ya_clamp_rect");
        m_id_uvClampRect = program()->uniformLocation("uv_clamp_rect");
        m_id_yTexture = program()->uniformLocation("y_texture");
        m_id_uTexture = program()->uniformLocation("u_texture");
        m_id_vTexture = program()->uniformLocation("v_texture");
        m_id_yuvMatrix = program()->uniformLocation("yuv_matrix");
        m_id_yuvAdjust = program()->uniformLocation("yuv_adj");
        m_id_opacity = program()->uniformLocation("alpha");
    }

    int m_id_matrix;
    int m_id_yaTexScale;
    int m_id_uvTexScale;
    int m_id_yaTexOffset;
    int m_id_uvTexOffset;
    int m_id_yaClampRect;
    int m_id_uvClampRect;
    int m_id_yTexture;
    int m_id_uTexture;
    int m_id_vTexture;
    int m_id_yuvMatrix;
    int m_id_yuvAdjust;
    int m_id_opacity;
};

class YUVAVideoMaterialShader : public YUVVideoMaterialShader
{
    virtual void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) Q_DECL_OVERRIDE;

protected:
    virtual const char *fragmentShader() const Q_DECL_OVERRIDE {
        // Keep in sync with cc::FragmentShaderYUVAVideo
        static const char *shader =
        "varying mediump vec2 v_yaTexCoord;\n"
        "varying mediump vec2 v_uvTexCoord;\n"
        "uniform sampler2D y_texture;\n"
        "uniform sampler2D u_texture;\n"
        "uniform sampler2D v_texture;\n"
        "uniform sampler2D a_texture;\n"
        "uniform mediump float alpha;\n"
        "uniform mediump vec3 yuv_adj;\n"
        "uniform mediump mat3 yuv_matrix;\n"
        "uniform mediump vec4 ya_clamp_rect;\n"
        "uniform mediump vec4 uv_clamp_rect;\n"
        "void main() {\n"
        "  mediump vec2 ya_clamped =\n"
        "      max(ya_clamp_rect.xy, min(ya_clamp_rect.zw, v_yaTexCoord));\n"
        "  mediump float y_raw = texture2D(y_texture, ya_clamped).x;\n"
        "  mediump vec2 uv_clamped =\n"
        "      max(uv_clamp_rect.xy, min(uv_clamp_rect.zw, v_uvTexCoord));\n"
        "  mediump float u_unsigned = texture2D(u_texture, uv_clamped).x;\n"
        "  mediump float v_unsigned = texture2D(v_texture, uv_clamped).x;\n"
        "  mediump float a_raw = texture2D(a_texture, ya_clamped).x;\n"
        "  mediump vec3 yuv = vec3(y_raw, u_unsigned, v_unsigned) + yuv_adj;\n"
        "  mediump vec3 rgb = yuv_matrix * yuv;\n"
        "  gl_FragColor = vec4(rgb, 1.0) * (alpha * a_raw);\n"
        "}";
        return shader;
    }

    virtual void initialize() Q_DECL_OVERRIDE {
        // YUVVideoMaterialShader has a subset of the uniforms.
        YUVVideoMaterialShader::initialize();
        m_id_aTexture = program()->uniformLocation("a_texture");
    }

    int m_id_aTexture;
};

void YUVVideoMaterialShader::updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
    Q_UNUSED(oldMaterial);

    // Keep logic in sync with logic in GLRenderer::DrawYUVVideoQuad:

    YUVVideoMaterial *mat = static_cast<YUVVideoMaterial *>(newMaterial);
    program()->setUniformValue(m_id_yTexture, 0);
    program()->setUniformValue(m_id_uTexture, 1);
    program()->setUniformValue(m_id_vTexture, 2);

    QOpenGLFunctions glFuncs(QOpenGLContext::currentContext());

    glFuncs.glActiveTexture(GL_TEXTURE1);
    mat->m_uTexture->bind();
    glFuncs.glActiveTexture(GL_TEXTURE2);
    mat->m_vTexture->bind();
    glFuncs.glActiveTexture(GL_TEXTURE0); // Finish with 0 as default texture unit
    mat->m_yTexture->bind();

    const QSizeF yaSizeScale(1.0f / mat->m_yaTexSize.width(), 1.0f / mat->m_yaTexSize.height());
    const QSizeF uvSizeScale(1.0f / mat->m_uvTexSize.width(), 1.0f / mat->m_uvTexSize.height());

    const QPointF yaTexOffset(mat->m_yaTexCoordRect.left() * yaSizeScale.width(), mat->m_yaTexCoordRect.top() * yaSizeScale.height());
    const QPointF uvTexOffset(mat->m_uvTexCoordRect.left() * uvSizeScale.width(), mat->m_uvTexCoordRect.top() * uvSizeScale.height());
    const QSizeF yaTexScale(mat->m_yaTexCoordRect.width() * yaSizeScale.width(), mat->m_yaTexCoordRect.height() * yaSizeScale.height());
    const QSizeF uvTexScale(mat->m_uvTexCoordRect.width() * uvSizeScale.width(), mat->m_uvTexCoordRect.height() * uvSizeScale.height());
    program()->setUniformValue(m_id_yaTexOffset, yaTexOffset);
    program()->setUniformValue(m_id_uvTexOffset, uvTexOffset);
    program()->setUniformValue(m_id_yaTexScale, yaTexScale);
    program()->setUniformValue(m_id_uvTexScale, uvTexScale);
    QRectF yaClampRect(yaTexOffset, yaTexScale);
    QRectF uvClampRect(uvTexOffset, uvTexScale);
    yaClampRect = yaClampRect.marginsRemoved(QMarginsF(yaSizeScale.width() * 0.5f, yaSizeScale.height() * 0.5f,
                                                       yaSizeScale.width() * 0.5f, yaSizeScale.height() * 0.5f));
    uvClampRect = uvClampRect.marginsRemoved(QMarginsF(uvSizeScale.width() * 0.5f, uvSizeScale.height() * 0.5f,
                                                       uvSizeScale.width() * 0.5f, uvSizeScale.height() * 0.5f));

    const QVector4D yaClampV(yaClampRect.left(), yaClampRect.top(), yaClampRect.right(), yaClampRect.bottom());
    const QVector4D uvClampV(uvClampRect.left(), uvClampRect.top(), uvClampRect.right(), uvClampRect.bottom());
    program()->setUniformValue(m_id_yaClampRect, yaClampV);
    program()->setUniformValue(m_id_uvClampRect, uvClampV);

    // These values are magic numbers that are used in the transformation from YUV
    // to RGB color values.  They are taken from the following webpage:
    // http://www.fourcc.org/fccyvrgb.php
    const float yuv_to_rgb_rec601[9] = {
        1.164f, 0.0f, 1.596f,
        1.164f, -.391f, -.813f,
        1.164f, 2.018f, 0.0f,
    };
    const float yuv_to_rgb_rec709[9] = {
        1.164f, 0.0f, 1.793f,
        1.164f, -0.213f, -0.533f,
        1.164f, 2.112f, 0.0f,
    };
    const float yuv_to_rgb_jpeg[9] = {
        1.f, 0.0f, 1.402f,
        1.f, -.34414f, -.71414f,
        1.f, 1.772f, 0.0f,
    };

    // These values map to 16, 128, and 128 respectively, and are computed
    // as a fraction over 256 (e.g. 16 / 256 = 0.0625).
    // They are used in the YUV to RGBA conversion formula:
    //   Y - 16   : Gives 16 values of head and footroom for overshooting
    //   U - 128  : Turns unsigned U into signed U [-128,127]
    //   V - 128  : Turns unsigned V into signed V [-128,127]
    const float yuv_adjust_constrained[3] = {
        -0.0625f, -0.5f, -0.5f,
    };

    // Same as above, but without the head and footroom.
    const float yuv_adjust_full[3] = {
        0.0f, -0.5f, -0.5f,
    };

    const float *yuv_to_rgb = 0;
    const float *yuv_adjust = 0;

    switch (mat->m_colorSpace) {
      case YUVVideoMaterial::REC_601:
        yuv_to_rgb = yuv_to_rgb_rec601;
        yuv_adjust = yuv_adjust_constrained;
        break;
      case YUVVideoMaterial::REC_709:
        yuv_to_rgb = yuv_to_rgb_rec709;
        yuv_adjust = yuv_adjust_constrained;
        break;
      case YUVVideoMaterial::JPEG:
        yuv_to_rgb = yuv_to_rgb_jpeg;
        yuv_adjust = yuv_adjust_full;
        break;
    }

    float yuv_to_rgb_multiplied[9];
    float yuv_adjust_with_offset[3];

    for (int i = 0; i < 9; ++i)
      yuv_to_rgb_multiplied[i] = yuv_to_rgb[i] * mat->m_resourceMultiplier;

    for (int i = 0; i < 3; ++i)
      yuv_adjust_with_offset[i] =
          yuv_adjust[i] / mat->m_resourceMultiplier - mat->m_resourceOffset;


    program()->setUniformValue(m_id_yuvMatrix, QMatrix3x3(yuv_to_rgb_multiplied));
    program()->setUniformValue(m_id_yuvAdjust, QVector3D(yuv_adjust_with_offset[0], yuv_adjust_with_offset[1], yuv_adjust_with_offset[2]));

    if (state.isOpacityDirty())
        program()->setUniformValue(m_id_opacity, state.opacity());

    if (state.isMatrixDirty())
        program()->setUniformValue(m_id_matrix, state.combinedMatrix());
}

void YUVAVideoMaterialShader::updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
    YUVVideoMaterialShader::updateState(state, newMaterial, oldMaterial);

    YUVAVideoMaterial *mat = static_cast<YUVAVideoMaterial *>(newMaterial);
    program()->setUniformValue(m_id_aTexture, 3);

    QOpenGLFunctions glFuncs(QOpenGLContext::currentContext());

    glFuncs.glActiveTexture(GL_TEXTURE3);
    mat->m_aTexture->bind();

    // Reset the default texture unit.
    glFuncs.glActiveTexture(GL_TEXTURE0);
}


YUVVideoMaterial::YUVVideoMaterial(QSGTexture *yTexture, QSGTexture *uTexture, QSGTexture *vTexture,
                                   const QRectF &yaTexCoordRect, const QRectF &uvTexCoordRect, const QSizeF &yaTexSize, const QSizeF &uvTexSize,
                                   YUVVideoMaterial::ColorSpace colorspace,
                                   float rMul, float rOff)
    : m_yTexture(yTexture)
    , m_uTexture(uTexture)
    , m_vTexture(vTexture)
    , m_yaTexCoordRect(yaTexCoordRect)
    , m_uvTexCoordRect(uvTexCoordRect)
    , m_yaTexSize(yaTexSize)
    , m_uvTexSize(uvTexSize)
    , m_colorSpace(colorspace)
    , m_resourceMultiplier(rMul)
    , m_resourceOffset(rOff)
{
}

QSGMaterialShader *YUVVideoMaterial::createShader() const
{
    return new YUVVideoMaterialShader;
}

int YUVVideoMaterial::compare(const QSGMaterial *other) const
{
    const YUVVideoMaterial *m = static_cast<const YUVVideoMaterial *>(other);
    if (int diff = m_yTexture->textureId() - m->m_yTexture->textureId())
        return diff;
    if (int diff = m_uTexture->textureId() - m->m_uTexture->textureId())
        return diff;
    return m_vTexture->textureId() - m->m_vTexture->textureId();
}

YUVAVideoMaterial::YUVAVideoMaterial(QSGTexture *yTexture, QSGTexture *uTexture, QSGTexture *vTexture, QSGTexture *aTexture,
                                     const QRectF &yaTexCoordRect, const QRectF &uvTexCoordRect, const QSizeF &yaTexSize, const QSizeF &uvTexSize,
                                     YUVVideoMaterial::ColorSpace colorspace,
                                     float rMul, float rOff)
    : YUVVideoMaterial(yTexture, uTexture, vTexture, yaTexCoordRect, uvTexCoordRect, yaTexSize, uvTexSize, colorspace, rMul, rOff)
    , m_aTexture(aTexture)
{
    setFlag(Blending, aTexture);
}

QSGMaterialShader *YUVAVideoMaterial::createShader() const
{
    return new YUVAVideoMaterialShader;
}

int YUVAVideoMaterial::compare(const QSGMaterial *other) const
{
    if (int diff = YUVVideoMaterial::compare(other))
        return diff;
    const YUVAVideoMaterial *m = static_cast<const YUVAVideoMaterial *>(other);
    return (m_aTexture ? m_aTexture->textureId() : 0) - (m->m_aTexture ? m->m_aTexture->textureId() : 0);
}

YUVVideoNode::YUVVideoNode(QSGTexture *yTexture, QSGTexture *uTexture, QSGTexture *vTexture, QSGTexture *aTexture,
                           const QRectF &yaTexCoordRect, const QRectF &uvTexCoordRect, const QSizeF &yaTexSize, const QSizeF &uvTexSize,
                           YUVVideoMaterial::ColorSpace colorspace, float rMul, float rOff)
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    setGeometry(&m_geometry);
    setFlag(QSGNode::OwnsMaterial);
    if (aTexture)
        m_material = new YUVAVideoMaterial(yTexture, uTexture, vTexture, aTexture, yaTexCoordRect, uvTexCoordRect, yaTexSize, uvTexSize, colorspace, rMul, rOff);
    else
        m_material = new YUVVideoMaterial(yTexture, uTexture, vTexture, yaTexCoordRect, uvTexCoordRect, yaTexSize, uvTexSize, colorspace, rMul, rOff);
    setMaterial(m_material);
}

void YUVVideoNode::setRect(const QRectF &rect)
{
    QSGGeometry::updateTexturedRectGeometry(geometry(), rect, QRectF(0, 0, 1, 1));
}

} // namespace
