/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
#include "qsgvideonode_texture.h"
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
    QSGVideoMaterialShader_Texture(QVideoFrame::PixelFormat pixelFormat)
        : QSGMaterialShader(),
          m_pixelFormat(pixelFormat)
    {
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

    virtual const char *vertexShader() const {
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

    virtual const char *fragmentShader() const {
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

        static const char *colorsSwapShader =
        "uniform sampler2D rgbTexture;"
        "uniform lowp float opacity;"
        ""
        "varying highp vec2 qt_TexCoord;"
        ""
        "void main()"
        "{"
        "    gl_FragColor =  vec4(texture2D(rgbTexture, qt_TexCoord).bgr, 1.0) * opacity;"
        "}";

        if (!QMediaOpenGLHelper::isANGLE()) {
            switch (m_pixelFormat) {
            case QVideoFrame::Format_RGB32:
            case QVideoFrame::Format_ARGB32:
                return colorsSwapShader;
            default:
                break;
            }
        }

        return shader;
    }

    virtual void initialize() {
        m_id_matrix = program()->uniformLocation("qt_Matrix");
        m_id_Texture = program()->uniformLocation("rgbTexture");
        m_id_opacity = program()->uniformLocation("opacity");
    }

    int m_id_matrix;
    int m_id_Texture;
    int m_id_opacity;
    QVideoFrame::PixelFormat m_pixelFormat;
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
        static QSGMaterialType theType;
        return &theType;
    }

    virtual QSGMaterialShader *createShader() const {
        return new QSGVideoMaterialShader_Texture(m_format.pixelFormat());
    }

    virtual int compare(const QSGMaterial *other) const {
        const QSGVideoMaterial_Texture *m = static_cast<const QSGVideoMaterial_Texture *>(other);
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

void QSGVideoNode_Texture::setCurrentFrame(const QVideoFrame &frame)
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
