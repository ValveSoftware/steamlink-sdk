/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsgvideonode_egl.h"

#include <QtMultimedia/qvideosurfaceformat.h>

#include <GLES2/gl2ext.h>

QT_BEGIN_NAMESPACE

class QSGVideoMaterial_EGLShader : public QSGMaterialShader
{
public:
    void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect);
    char const *const *attributeNames() const;

    static QSGMaterialType type;

protected:
    void initialize();

    const char *vertexShader() const;
    const char *fragmentShader() const;

private:
    int id_matrix;
    int id_opacity;
    int id_texture;
};

void QSGVideoMaterial_EGLShader::updateState(
        const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    QSGVideoMaterial_EGL *material = static_cast<QSGVideoMaterial_EGL *>(newEffect);
    if (!oldEffect) {
        program()->setUniformValue(id_texture, 0);
    }

    if (state.isMatrixDirty()) {
        program()->setUniformValue(id_matrix, state.combinedMatrix());
    }

    if (state.isOpacityDirty()) {
        program()->setUniformValue(id_opacity, state.opacity());
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, material->m_textureId);
}

char const *const *QSGVideoMaterial_EGLShader::attributeNames() const
{
    static char const *const attr[] = { "position", "texcoord", 0 };
    return attr;
}

QSGMaterialType QSGVideoMaterial_EGLShader::type;

void QSGVideoMaterial_EGLShader::initialize()
{
    id_matrix = program()->uniformLocation("matrix");
    id_opacity = program()->uniformLocation("opacity");
    id_texture = program()->uniformLocation("texture");
}

const char *QSGVideoMaterial_EGLShader::vertexShader() const
{
    return  "\n uniform highp mat4 matrix;"
            "\n attribute highp vec4 position;"
            "\n attribute highp vec2 texcoord;"
            "\n varying highp vec2 frag_tx;"
            "\n void main(void)"
            "\n {"
            "\n     gl_Position = matrix * position;"
            "\n     frag_tx = texcoord;"
            "\n }";
}

const char *QSGVideoMaterial_EGLShader::fragmentShader() const
{
    return  "\n #extension GL_OES_EGL_image_external : require"
            "\n uniform samplerExternalOES texture;"
            "\n varying highp vec2 frag_tx;"
            "\n void main(void)"
            "\n {"
            "\n     gl_FragColor = texture2D(texture, frag_tx.st);"
            "\n }";
}

QSGVideoMaterial_EGL::QSGVideoMaterial_EGL()
    : m_image(0)
    , m_textureId(0)
{
}

QSGVideoMaterial_EGL::~QSGVideoMaterial_EGL()
{
    if (m_textureId) {
        glDeleteTextures(1, &m_textureId);
        m_textureId = 0;
    }
}

QSGMaterialShader *QSGVideoMaterial_EGL::createShader() const
{
    return new QSGVideoMaterial_EGLShader;
}

QSGMaterialType *QSGVideoMaterial_EGL::type() const
{
    return &QSGVideoMaterial_EGLShader::type;
}

int QSGVideoMaterial_EGL::compare(const QSGMaterial *other) const
{
    return m_textureId - static_cast<const QSGVideoMaterial_EGL *>(other)->m_textureId;
}

void QSGVideoMaterial_EGL::setImage(EGLImageKHR image)
{
    static const PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES
            = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));

    if (m_image == image || !glEGLImageTargetTexture2DOES)
        return;

    m_image = image;

    if (!m_image) {
        if (m_textureId) {
            glDeleteTextures(1, &m_textureId);
            m_textureId = 0;
        }
    } else {
        if (!m_textureId) {
            glGenTextures(1, &m_textureId);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_textureId);
            glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        } else {
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_textureId);
        }
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, m_image);
    }
}

QSGVideoNode_EGL::QSGVideoNode_EGL(const QVideoSurfaceFormat &format)
    : m_pixelFormat(format.pixelFormat())
{
    setMaterial(&m_material);
}

QSGVideoNode_EGL::~QSGVideoNode_EGL()
{
}

void QSGVideoNode_EGL::setCurrentFrame(const QVideoFrame &frame)
{
    EGLImageKHR image = frame.handle().value<void *>();
    m_material.setImage(image);
    markDirty(DirtyMaterial);
}

QVideoFrame::PixelFormat QSGVideoNode_EGL::pixelFormat() const
{
    return m_pixelFormat;
}

static bool isExtensionSupported()
{
    static const bool supported = eglGetProcAddress("glEGLImageTargetTexture2DOES");
    return supported;
}

QList<QVideoFrame::PixelFormat> QSGVideoNodeFactory_EGL::supportedPixelFormats(
        QAbstractVideoBuffer::HandleType handleType) const
{
    if (handleType != QAbstractVideoBuffer::EGLImageHandle || !isExtensionSupported())
        return QList<QVideoFrame::PixelFormat>();

    return QList<QVideoFrame::PixelFormat>()
            << QVideoFrame::Format_Invalid
            << QVideoFrame::Format_YV12
            << QVideoFrame::Format_UYVY
            << QVideoFrame::Format_NV21
            << QVideoFrame::Format_YUYV
            << QVideoFrame::Format_RGB32
            << QVideoFrame::Format_BGR32
            << QVideoFrame::Format_RGB24
            << QVideoFrame::Format_BGR24
            << QVideoFrame::Format_RGB565
            << QVideoFrame::Format_BGR565;
}

QSGVideoNode *QSGVideoNodeFactory_EGL::createNode(const QVideoSurfaceFormat &format)
{
    return format.handleType() == QAbstractVideoBuffer::EGLImageHandle && isExtensionSupported()
            ? new QSGVideoNode_EGL(format)
            : 0;
}

QT_END_NAMESPACE
