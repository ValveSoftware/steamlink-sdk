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

#include "stream_video_node.h"

#include <QtQuick/qsgtexture.h>

class StreamVideoMaterialShader : public QSGMaterialShader
{
public:
    virtual void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial);

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
        // Keep in sync with cc::VertexShaderVideoTransform
        const char *shader =
        "attribute highp vec4 a_position;\n"
        "attribute mediump vec2 a_texCoord;\n"
        "uniform highp mat4 matrix;\n"
        "uniform highp mat4 texMatrix;\n"
        "varying mediump vec2 v_texCoord;\n"
        "void main() {\n"
        "  gl_Position = matrix * a_position;\n"
        "  v_texCoord = vec4(texMatrix * vec4(a_texCoord.x, 1.0 - a_texCoord.y, 0.0, 1.0)).xy;\n"
        "}";
        return shader;
    }

    virtual const char *fragmentShader() const Q_DECL_OVERRIDE {
        // Keep in sync with cc::FragmentShaderRGBATexAlpha
        static const char *shader =
        "#extension GL_OES_EGL_image_external : require\n"
        "varying mediump vec2 v_texCoord;\n"
        "uniform samplerExternalOES s_texture;\n"
        "uniform lowp float alpha;\n"
        "void main() {\n"
        "  lowp vec4 texColor = texture2D(s_texture, v_texCoord);\n"
        "  gl_FragColor = texColor * alpha;\n"
        "}";
        return shader;
    }

    virtual void initialize() {
        m_id_matrix = program()->uniformLocation("matrix");
        m_id_sTexture = program()->uniformLocation("s_texture");
        m_id_texMatrix = program()->uniformLocation("texMatrix");
        m_id_opacity = program()->uniformLocation("alpha");
    }

    int m_id_matrix;
    int m_id_texMatrix;
    int m_id_sTexture;
    int m_id_opacity;
};

void StreamVideoMaterialShader::updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
    Q_UNUSED(oldMaterial);

    StreamVideoMaterial *mat = static_cast<StreamVideoMaterial *>(newMaterial);
    program()->setUniformValue(m_id_sTexture, 0);

    mat->m_texture->bind();

    if (state.isOpacityDirty())
        program()->setUniformValue(m_id_opacity, state.opacity());

    if (state.isMatrixDirty())
        program()->setUniformValue(m_id_matrix, state.combinedMatrix());

    program()->setUniformValue(m_id_texMatrix, mat->m_texMatrix);
}

StreamVideoMaterial::StreamVideoMaterial(QSGTexture *texture)
    : m_texture(texture)
{
}

QSGMaterialShader *StreamVideoMaterial::createShader() const
{
    return new StreamVideoMaterialShader;
}

StreamVideoNode::StreamVideoNode(QSGTexture *texture)
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    setGeometry(&m_geometry);
    setFlag(QSGNode::OwnsMaterial);
    m_material = new StreamVideoMaterial(texture);
    setMaterial(m_material);
}

void StreamVideoNode::setRect(const QRectF &rect)
{
    QSGGeometry::updateTexturedRectGeometry(geometry(), rect, QRectF(0, 0, 1, 1));
}

void StreamVideoNode::setTextureMatrix(const QMatrix4x4 &matrix)
{
    m_material->m_texMatrix = matrix;
}
