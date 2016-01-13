/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "yuv_video_node.h"

#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>
#include <QtQuick/qsgtexture.h>

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
        "varying mediump vec2 v_texCoord;\n"
        "uniform mediump vec2 texScale;\n"
        "uniform mediump vec2 texOffset;\n"
        "void main() {\n"
        "  gl_Position = matrix * a_position;\n"
        "  v_texCoord = a_texCoord * texScale + texOffset;\n"
        "}";
        return shader;
    }

    virtual const char *fragmentShader() const Q_DECL_OVERRIDE {
        // Keep in sync with cc::FragmentShaderYUVVideo
        static const char *shader =
        "varying mediump vec2 v_texCoord;\n"
        "uniform sampler2D y_texture;\n"
        "uniform sampler2D u_texture;\n"
        "uniform sampler2D v_texture;\n"
        "uniform lowp float alpha;\n"
        "uniform lowp vec3 yuv_adj;\n"
        "uniform lowp mat3 yuv_matrix;\n"
        "void main() {\n"
        "  lowp float y_raw = texture2D(y_texture, v_texCoord).x;\n"
        "  lowp float u_unsigned = texture2D(u_texture, v_texCoord).x;\n"
        "  lowp float v_unsigned = texture2D(v_texture, v_texCoord).x;\n"
        "  lowp vec3 yuv = vec3(y_raw, u_unsigned, v_unsigned) + yuv_adj;\n"
        "  lowp vec3 rgb = yuv_matrix * yuv;\n"
        "  gl_FragColor = vec4(rgb, 1.0) * alpha;\n"
        "}";
        return shader;
    }

    virtual void initialize() Q_DECL_OVERRIDE {
        m_id_matrix = program()->uniformLocation("matrix");
        m_id_texScale = program()->uniformLocation("texScale");
        m_id_texOffset = program()->uniformLocation("texOffset");
        m_id_yTexture = program()->uniformLocation("y_texture");
        m_id_uTexture = program()->uniformLocation("u_texture");
        m_id_vTexture = program()->uniformLocation("v_texture");
        m_id_yuvMatrix = program()->uniformLocation("yuv_matrix");
        m_id_yuvAdjust = program()->uniformLocation("yuv_adj");
        m_id_opacity = program()->uniformLocation("alpha");
    }

    int m_id_matrix;
    int m_id_texScale;
    int m_id_texOffset;
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
        "varying mediump vec2 v_texCoord;\n"
        "uniform sampler2D y_texture;\n"
        "uniform sampler2D u_texture;\n"
        "uniform sampler2D v_texture;\n"
        "uniform sampler2D a_texture;\n"
        "uniform lowp float alpha;\n"
        "uniform lowp vec3 yuv_adj;\n"
        "uniform lowp mat3 yuv_matrix;\n"
        "void main() {\n"
        "  lowp float y_raw = texture2D(y_texture, v_texCoord).x;\n"
        "  lowp float u_unsigned = texture2D(u_texture, v_texCoord).x;\n"
        "  lowp float v_unsigned = texture2D(v_texture, v_texCoord).x;\n"
        "  lowp float a_raw = texture2D(a_texture, v_texCoord).x;\n"
        "  lowp vec3 yuv = vec3(y_raw, u_unsigned, v_unsigned) + yuv_adj;\n"
        "  lowp vec3 rgb = yuv_matrix * yuv;\n"
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

    program()->setUniformValue(m_id_texOffset, mat->m_texCoordRect.topLeft());
    program()->setUniformValue(m_id_texScale, mat->m_texCoordRect.size());

    // These values are magic numbers that are used in the transformation from YUV
    // to RGB color values.  They are taken from the following webpage:
    // http://www.fourcc.org/fccyvrgb.php
    const float yuv_to_rgb[9] = {
        1.164f, 0.0f, 1.596f,
        1.164f, -.391f, -.813f,
        1.164f, 2.018f, 0.0f,
    };
    const QMatrix3x3 yuvMatrix(yuv_to_rgb);

    // These values map to 16, 128, and 128 respectively, and are computed
    // as a fraction over 256 (e.g. 16 / 256 = 0.0625).
    // They are used in the YUV to RGBA conversion formula:
    //   Y - 16   : Gives 16 values of head and footroom for overshooting
    //   U - 128  : Turns unsigned U into signed U [-128,127]
    //   V - 128  : Turns unsigned V into signed V [-128,127]
    const QVector3D yuvAdjust(-0.0625f, -0.5f, -0.5f);
    program()->setUniformValue(m_id_yuvMatrix, yuvMatrix);
    program()->setUniformValue(m_id_yuvAdjust, yuvAdjust);

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


YUVVideoMaterial::YUVVideoMaterial(QSGTexture *yTexture, QSGTexture *uTexture, QSGTexture *vTexture, const QRectF &texCoordRect)
    : m_yTexture(yTexture)
    , m_uTexture(uTexture)
    , m_vTexture(vTexture)
    , m_texCoordRect(texCoordRect)
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

YUVAVideoMaterial::YUVAVideoMaterial(QSGTexture *yTexture, QSGTexture *uTexture, QSGTexture *vTexture, QSGTexture *aTexture, const QRectF &texCoordRect)
    : YUVVideoMaterial(yTexture, uTexture, vTexture, texCoordRect)
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

YUVVideoNode::YUVVideoNode(QSGTexture *yTexture, QSGTexture *uTexture, QSGTexture *vTexture, QSGTexture *aTexture, const QRectF &texCoordRect)
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    setGeometry(&m_geometry);
    setFlag(QSGNode::OwnsMaterial);
    if (aTexture)
        m_material = new YUVAVideoMaterial(yTexture, uTexture, vTexture, aTexture, texCoordRect);
    else
        m_material = new YUVVideoMaterial(yTexture, uTexture, vTexture, texCoordRect);
    setMaterial(m_material);
}

void YUVVideoNode::setRect(const QRectF &rect)
{
    QSGGeometry::updateTexturedRectGeometry(geometry(), rect, QRectF(0, 0, 1, 1));
}
