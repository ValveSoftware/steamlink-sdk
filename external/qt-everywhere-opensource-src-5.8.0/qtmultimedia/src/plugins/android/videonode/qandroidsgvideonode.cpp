/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "qandroidsgvideonode.h"

#include <qsgmaterial.h>
#include <qmutex.h>

QT_BEGIN_NAMESPACE

class QAndroidSGVideoNodeMaterialShader : public QSGMaterialShader
{
public:
    void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial);

    char const *const *attributeNames() const {
        static const char *names[] = {
            "qt_VertexPosition",
            "qt_VertexTexCoord",
            0
        };
        return names;
    }

protected:

    const char *vertexShader() const {
        const char *shader =
        "uniform highp mat4 qt_Matrix;                      \n"
        "attribute highp vec4 qt_VertexPosition;            \n"
        "attribute highp vec2 qt_VertexTexCoord;            \n"
        "varying highp vec2 qt_TexCoord;                    \n"
        "void main() {                                      \n"
        "    qt_TexCoord = qt_VertexTexCoord;               \n"
        "    gl_Position = qt_Matrix * qt_VertexPosition;   \n"
        "}";
        return shader;
    }

    const char *fragmentShader() const {
        static const char *shader =
        "uniform sampler2D rgbTexture;"
        "uniform lowp float opacity;"
        ""
        "varying highp vec2 qt_TexCoord;"
        ""
        "void main()"
        "{"
        "    gl_FragColor = texture2D(rgbTexture, qt_TexCoord) * opacity;"
        "}";
        return shader;
    }

    void initialize() {
        m_id_matrix = program()->uniformLocation("qt_Matrix");
        m_id_Texture = program()->uniformLocation("rgbTexture");
        m_id_opacity = program()->uniformLocation("opacity");
    }

    int m_id_matrix;
    int m_id_Texture;
    int m_id_opacity;
};

class QAndroidSGVideoNodeMaterial : public QSGMaterial
{
public:
    QAndroidSGVideoNodeMaterial()
        : m_textureId(0)
        , m_textureUpdated(false)
        , m_opacity(1.0)
    {
        setFlag(Blending, false);
    }

    QSGMaterialType *type() const {
        static QSGMaterialType theType;
        return &theType;
    }

    QSGMaterialShader *createShader() const {
        return new QAndroidSGVideoNodeMaterialShader;
    }

    int compare(const QSGMaterial *other) const {
        const QAndroidSGVideoNodeMaterial *m = static_cast<const QAndroidSGVideoNodeMaterial *>(other);
        int diff = m_textureId - m->m_textureId;
        if (diff)
            return diff;

        return (m_opacity > m->m_opacity) ? 1 : -1;
    }

    void updateBlending() {
        setFlag(Blending, qFuzzyCompare(m_opacity, qreal(1.0)) ? false : true);
    }

    void updateTexture(GLuint id, const QSize &size) {
        if (m_textureId != id || m_textureSize != size) {
            m_textureId = id;
            m_textureSize = size;
            m_textureUpdated = true;
        }
    }

    void bind()
    {
        glBindTexture(GL_TEXTURE_2D, m_textureId);
        if (m_textureUpdated) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            m_textureUpdated = false;
        }
    }

    QSize m_textureSize;
    GLuint m_textureId;
    bool m_textureUpdated;
    qreal m_opacity;
};


QAndroidSGVideoNode::QAndroidSGVideoNode(const QVideoSurfaceFormat &format)
    : m_format(format)
{
    setFlags(OwnsMaterial | UsePreprocess);
    m_material = new QAndroidSGVideoNodeMaterial;
    setMaterial(m_material);
}

QAndroidSGVideoNode::~QAndroidSGVideoNode()
{
    m_frame = QVideoFrame();
}

void QAndroidSGVideoNode::setCurrentFrame(const QVideoFrame &frame, FrameFlags)
{
    QMutexLocker lock(&m_frameMutex);
    m_frame = frame;
    markDirty(DirtyMaterial);
}

void QAndroidSGVideoNodeMaterialShader::updateState(const RenderState &state,
                                                    QSGMaterial *newMaterial,
                                                    QSGMaterial *oldMaterial)
{
    Q_UNUSED(oldMaterial);
    QAndroidSGVideoNodeMaterial *mat = static_cast<QAndroidSGVideoNodeMaterial *>(newMaterial);
    program()->setUniformValue(m_id_Texture, 0);

    mat->bind();

    if (state.isOpacityDirty()) {
        mat->m_opacity = state.opacity();
        mat->updateBlending();
        program()->setUniformValue(m_id_opacity, GLfloat(mat->m_opacity));
    }

    if (state.isMatrixDirty())
        program()->setUniformValue(m_id_matrix, state.combinedMatrix());
}

void QAndroidSGVideoNode::preprocess()
{
    QMutexLocker lock(&m_frameMutex);

    GLuint texId = 0;
    if (m_frame.isValid())
        texId = m_frame.handle().toUInt();

    m_material->updateTexture(texId, m_frame.size());
}

QT_END_NAMESPACE
