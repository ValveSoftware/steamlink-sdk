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
#include "qsgvideonode_texture_p.h"
#include <QtQuick/qsgtexturematerial.h>
#include <QtQuick/qsgmaterial.h>
#include <QtCore/qmutex.h>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLShaderProgram>
#include <QtMultimedia/private/qmediaopenglhelper_p.h>

QT_BEGIN_NAMESPACE

QList<QVideoFrame::PixelFormat> QSGVideoNodeFactory_Texture::supportedPixelFormats(
                                        QAbstractVideoBuffer::HandleType handleType) const
{
    QList<QVideoFrame::PixelFormat> pixelFormats;

    if (handleType == QAbstractVideoBuffer::GLTextureHandle) {
        pixelFormats.append(QVideoFrame::Format_RGB565);
        pixelFormats.append(QVideoFrame::Format_RGB32);
        pixelFormats.append(QVideoFrame::Format_ARGB32);
        pixelFormats.append(QVideoFrame::Format_BGR32);
        pixelFormats.append(QVideoFrame::Format_BGRA32);
    }

    return pixelFormats;
}

QSGVideoNode *QSGVideoNodeFactory_Texture::createNode(const QVideoSurfaceFormat &format)
{
    if (supportedPixelFormats(format.handleType()).contains(format.pixelFormat()))
        return new QSGVideoNode_Texture(format);

    return 0;
}


class QSGVideoMaterialShader_Texture : public QSGMaterialShader
{
public:
    QSGVideoMaterialShader_Texture()
        : QSGMaterialShader()
    {
        setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/qtmultimediaquicktools/shaders/monoplanarvideo.vert"));
        setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qtmultimediaquicktools/shaders/rgbvideo.frag"));
    }

    void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial);

    virtual char const *const *attributeNames() const {
        static const char *names[] = {
            "qt_VertexPosition",
            "qt_VertexTexCoord",
            0
        };
        return names;
    }

protected:
    virtual void initialize() {
        m_id_matrix = program()->uniformLocation("qt_Matrix");
        m_id_Texture = program()->uniformLocation("rgbTexture");
        m_id_opacity = program()->uniformLocation("opacity");
    }

    int m_id_matrix;
    int m_id_Texture;
    int m_id_opacity;
};

class QSGVideoMaterialShader_Texture_swizzle : public QSGVideoMaterialShader_Texture
{
public:
    QSGVideoMaterialShader_Texture_swizzle()
        : QSGVideoMaterialShader_Texture()
    {
        setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qtmultimediaquicktools/shaders/rgbvideo_swizzle.frag"));
    }
};


class QSGVideoMaterial_Texture : public QSGMaterial
{
public:
    QSGVideoMaterial_Texture(const QVideoSurfaceFormat &format) :
        m_format(format),
        m_textureId(0),
        m_opacity(1.0)
    {
        setFlag(Blending, false);
    }

    ~QSGVideoMaterial_Texture()
    {
        m_frame = QVideoFrame();
    }

    virtual QSGMaterialType *type() const {
        static QSGMaterialType normalType, swizzleType;
        return needsSwizzling() ? &swizzleType : &normalType;
    }

    virtual QSGMaterialShader *createShader() const {
        return needsSwizzling() ? new QSGVideoMaterialShader_Texture_swizzle
                                : new QSGVideoMaterialShader_Texture;
    }

    virtual int compare(const QSGMaterial *other) const {
        const QSGVideoMaterial_Texture *m = static_cast<const QSGVideoMaterial_Texture *>(other);

        if (!m_textureId)
            return 1;

        int diff = m_textureId - m->m_textureId;
        if (diff)
            return diff;

        diff = m_format.pixelFormat() - m->m_format.pixelFormat();
        if (diff)
            return diff;

        return (m_opacity > m->m_opacity) ? 1 : -1;
    }

    void updateBlending() {
        setFlag(Blending, qFuzzyCompare(m_opacity, qreal(1.0)) ? false : true);
    }

    void setVideoFrame(const QVideoFrame &frame) {
        QMutexLocker lock(&m_frameMutex);
        m_frame = frame;
    }

    void bind()
    {
        QMutexLocker lock(&m_frameMutex);
        if (m_frame.isValid()) {
            m_textureId = m_frame.handle().toUInt();
            QOpenGLFunctions *functions = QOpenGLContext::currentContext()->functions();
            functions->glBindTexture(GL_TEXTURE_2D, m_textureId);

            functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        } else {
            m_textureId = 0;
        }
    }

    QVideoFrame m_frame;
    QMutex m_frameMutex;
    QSize m_textureSize;
    QVideoSurfaceFormat m_format;
    GLuint m_textureId;
    qreal m_opacity;

private:
    bool needsSwizzling() const {
        return !QMediaOpenGLHelper::isANGLE()
                && (m_format.pixelFormat() == QVideoFrame::Format_RGB32
                    || m_format.pixelFormat() == QVideoFrame::Format_ARGB32);
    }
};


QSGVideoNode_Texture::QSGVideoNode_Texture(const QVideoSurfaceFormat &format) :
    m_format(format)
{
    setFlag(QSGNode::OwnsMaterial);
    m_material = new QSGVideoMaterial_Texture(format);
    setMaterial(m_material);
}

QSGVideoNode_Texture::~QSGVideoNode_Texture()
{
}

void QSGVideoNode_Texture::setCurrentFrame(const QVideoFrame &frame, FrameFlags)
{
    m_material->setVideoFrame(frame);
    markDirty(DirtyMaterial);
}

void QSGVideoMaterialShader_Texture::updateState(const RenderState &state,
                                                QSGMaterial *newMaterial,
                                                QSGMaterial *oldMaterial)
{
    Q_UNUSED(oldMaterial);
    QSGVideoMaterial_Texture *mat = static_cast<QSGVideoMaterial_Texture *>(newMaterial);
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

QT_END_NAMESPACE
