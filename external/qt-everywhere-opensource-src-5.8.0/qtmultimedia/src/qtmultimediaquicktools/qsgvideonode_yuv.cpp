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
#include "qsgvideonode_yuv_p.h"
#include <QtCore/qmutex.h>
#include <QtQuick/qsgtexturematerial.h>
#include <QtQuick/qsgmaterial.h>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLShaderProgram>

QT_BEGIN_NAMESPACE

QList<QVideoFrame::PixelFormat> QSGVideoNodeFactory_YUV::supportedPixelFormats(
                                        QAbstractVideoBuffer::HandleType handleType) const
{
    QList<QVideoFrame::PixelFormat> formats;

    if (handleType == QAbstractVideoBuffer::NoHandle) {
        formats << QVideoFrame::Format_YUV420P << QVideoFrame::Format_YV12
                << QVideoFrame::Format_NV12 << QVideoFrame::Format_NV21
                << QVideoFrame::Format_UYVY << QVideoFrame::Format_YUYV;
    }

    return formats;
}

QSGVideoNode *QSGVideoNodeFactory_YUV::createNode(const QVideoSurfaceFormat &format)
{
    if (supportedPixelFormats(format.handleType()).contains(format.pixelFormat()))
        return new QSGVideoNode_YUV(format);

    return 0;
}


class QSGVideoMaterialShader_YUV_BiPlanar : public QSGMaterialShader
{
public:
    QSGVideoMaterialShader_YUV_BiPlanar()
        : QSGMaterialShader()
    {
        setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/qtmultimediaquicktools/shaders/biplanaryuvvideo.vert"));
        setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qtmultimediaquicktools/shaders/biplanaryuvvideo.frag"));
    }

    virtual void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial);

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
        m_id_plane1Width = program()->uniformLocation("plane1Width");
        m_id_plane2Width = program()->uniformLocation("plane2Width");
        m_id_plane1Texture = program()->uniformLocation("plane1Texture");
        m_id_plane2Texture = program()->uniformLocation("plane2Texture");
        m_id_colorMatrix = program()->uniformLocation("colorMatrix");
        m_id_opacity = program()->uniformLocation("opacity");
    }

    int m_id_matrix;
    int m_id_plane1Width;
    int m_id_plane2Width;
    int m_id_plane1Texture;
    int m_id_plane2Texture;
    int m_id_colorMatrix;
    int m_id_opacity;
};

class QSGVideoMaterialShader_UYVY : public QSGMaterialShader
{
public:
    QSGVideoMaterialShader_UYVY()
        : QSGMaterialShader()
    {
        setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/qtmultimediaquicktools/shaders/monoplanarvideo.vert"));
        setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qtmultimediaquicktools/shaders/uyvyvideo.frag"));
    }

    void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) Q_DECL_OVERRIDE;

    char const *const *attributeNames() const Q_DECL_OVERRIDE {
        static const char *names[] = {
            "qt_VertexPosition",
            "qt_VertexTexCoord",
            0
        };
        return names;
    }

protected:
    void initialize() Q_DECL_OVERRIDE {
        m_id_matrix = program()->uniformLocation("qt_Matrix");
        m_id_yuvtexture = program()->uniformLocation("yuvTexture");
        m_id_imageWidth = program()->uniformLocation("imageWidth");
        m_id_colorMatrix = program()->uniformLocation("colorMatrix");
        m_id_opacity = program()->uniformLocation("opacity");
        QSGMaterialShader::initialize();
    }

    int m_id_matrix;
    int m_id_yuvtexture;
    int m_id_imageWidth;
    int m_id_colorMatrix;
    int m_id_opacity;
};


class QSGVideoMaterialShader_YUYV : public QSGVideoMaterialShader_UYVY
{
public:
    QSGVideoMaterialShader_YUYV()
        : QSGVideoMaterialShader_UYVY()
    {
        setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qtmultimediaquicktools/shaders/yuyvvideo.frag"));
    }
};


class QSGVideoMaterialShader_YUV_BiPlanar_swizzle : public QSGVideoMaterialShader_YUV_BiPlanar
{
public:
    QSGVideoMaterialShader_YUV_BiPlanar_swizzle()
        : QSGVideoMaterialShader_YUV_BiPlanar()
    {
        setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qtmultimediaquicktools/shaders/biplanaryuvvideo_swizzle.frag"));
    }
};


class QSGVideoMaterialShader_YUV_TriPlanar : public QSGVideoMaterialShader_YUV_BiPlanar
{
public:
    QSGVideoMaterialShader_YUV_TriPlanar()
        : QSGVideoMaterialShader_YUV_BiPlanar()
    {
        setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/qtmultimediaquicktools/shaders/triplanaryuvvideo.vert"));
        setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qtmultimediaquicktools/shaders/triplanaryuvvideo.frag"));
    }

    virtual void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial);

protected:
    virtual void initialize() {
        m_id_plane3Width = program()->uniformLocation("plane3Width");
        m_id_plane3Texture = program()->uniformLocation("plane3Texture");
        QSGVideoMaterialShader_YUV_BiPlanar::initialize();
    }

    int m_id_plane3Width;
    int m_id_plane3Texture;
};


class QSGVideoMaterial_YUV : public QSGMaterial
{
public:
    QSGVideoMaterial_YUV(const QVideoSurfaceFormat &format);
    ~QSGVideoMaterial_YUV();

    virtual QSGMaterialType *type() const {
        static QSGMaterialType biPlanarType, biPlanarSwizzleType, triPlanarType, uyvyType, yuyvType;

        switch (m_format.pixelFormat()) {
        case QVideoFrame::Format_NV12:
            return &biPlanarType;
        case QVideoFrame::Format_NV21:
            return &biPlanarSwizzleType;
        case QVideoFrame::Format_UYVY:
            return &uyvyType;
        case QVideoFrame::Format_YUYV:
            return &yuyvType;
        default: // Currently: YUV420P and YV12
            return &triPlanarType;
        }
    }

    virtual QSGMaterialShader *createShader() const {
        switch (m_format.pixelFormat()) {
        case QVideoFrame::Format_NV12:
            return new QSGVideoMaterialShader_YUV_BiPlanar;
        case QVideoFrame::Format_NV21:
            return new QSGVideoMaterialShader_YUV_BiPlanar_swizzle;
        case QVideoFrame::Format_UYVY:
            return new QSGVideoMaterialShader_UYVY;
        case QVideoFrame::Format_YUYV:
            return new QSGVideoMaterialShader_YUYV;
        default: // Currently: YUV420P and YV12
            return new QSGVideoMaterialShader_YUV_TriPlanar;
        }
    }

    virtual int compare(const QSGMaterial *other) const {
        const QSGVideoMaterial_YUV *m = static_cast<const QSGVideoMaterial_YUV *>(other);
        if (!m_textureIds[0])
            return 1;

        int d = m_textureIds[0] - m->m_textureIds[0];
        if (d)
            return d;
        else if ((d = m_textureIds[1] - m->m_textureIds[1]) != 0)
            return d;
        else
            return m_textureIds[2] - m->m_textureIds[2];
    }

    void updateBlending() {
        setFlag(Blending, qFuzzyCompare(m_opacity, qreal(1.0)) ? false : true);
    }

    void setCurrentFrame(const QVideoFrame &frame) {
        QMutexLocker lock(&m_frameMutex);
        m_frame = frame;
    }

    void bind();
    void bindTexture(int id, int w, int h, const uchar *bits, GLenum format);

    QVideoSurfaceFormat m_format;
    QSize m_textureSize;
    int m_planeCount;

    GLuint m_textureIds[3];
    GLfloat m_planeWidth[3];

    qreal m_opacity;
    QMatrix4x4 m_colorMatrix;

    QVideoFrame m_frame;
    QMutex m_frameMutex;
};

QSGVideoMaterial_YUV::QSGVideoMaterial_YUV(const QVideoSurfaceFormat &format) :
    m_format(format),
    m_opacity(1.0)
{
    memset(m_textureIds, 0, sizeof(m_textureIds));

    switch (format.pixelFormat()) {
    case QVideoFrame::Format_NV12:
    case QVideoFrame::Format_NV21:
        m_planeCount = 2;
        break;
    case QVideoFrame::Format_YUV420P:
    case QVideoFrame::Format_YV12:
        m_planeCount = 3;
        break;
    case QVideoFrame::Format_UYVY:
    case QVideoFrame::Format_YUYV:
    default:
        m_planeCount = 1;
        break;
    }

    switch (format.yCbCrColorSpace()) {
    case QVideoSurfaceFormat::YCbCr_JPEG:
        m_colorMatrix = QMatrix4x4(
                    1.0f,  0.000f,  1.402f, -0.701f,
                    1.0f, -0.344f, -0.714f,  0.529f,
                    1.0f,  1.772f,  0.000f, -0.886f,
                    0.0f,  0.000f,  0.000f,  1.0000f);
        break;
    case QVideoSurfaceFormat::YCbCr_BT709:
    case QVideoSurfaceFormat::YCbCr_xvYCC709:
        m_colorMatrix = QMatrix4x4(
                    1.164f,  0.000f,  1.793f, -0.5727f,
                    1.164f, -0.534f, -0.213f,  0.3007f,
                    1.164f,  2.115f,  0.000f, -1.1302f,
                    0.0f,    0.000f,  0.000f,  1.0000f);
        break;
    default: //BT 601:
        m_colorMatrix = QMatrix4x4(
                    1.164f,  0.000f,  1.596f, -0.8708f,
                    1.164f, -0.392f, -0.813f,  0.5296f,
                    1.164f,  2.017f,  0.000f, -1.081f,
                    0.0f,    0.000f,  0.000f,  1.0000f);
    }

    setFlag(Blending, false);
}

QSGVideoMaterial_YUV::~QSGVideoMaterial_YUV()
{
    if (!m_textureSize.isEmpty()) {
        if (QOpenGLContext *current = QOpenGLContext::currentContext())
            current->functions()->glDeleteTextures(m_planeCount, m_textureIds);
        else
            qWarning() << "QSGVideoMaterial_YUV: Cannot obtain GL context, unable to delete textures";
    }
}

void QSGVideoMaterial_YUV::bind()
{
    QOpenGLFunctions *functions = QOpenGLContext::currentContext()->functions();
    QMutexLocker lock(&m_frameMutex);
    if (m_frame.isValid()) {
        if (m_frame.map(QAbstractVideoBuffer::ReadOnly)) {
            int fw = m_frame.width();
            int fh = m_frame.height();

            // Frame has changed size, recreate textures...
            if (m_textureSize != m_frame.size()) {
                if (!m_textureSize.isEmpty())
                    functions->glDeleteTextures(m_planeCount, m_textureIds);
                functions->glGenTextures(m_planeCount, m_textureIds);
                m_textureSize = m_frame.size();
            }

            GLint previousAlignment;
            functions->glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
            functions->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

             if (m_format.pixelFormat() == QVideoFrame::Format_UYVY
              || m_format.pixelFormat() == QVideoFrame::Format_YUYV) {
                int fw = m_frame.width() / 2;
                m_planeWidth[0] = fw;

                functions->glActiveTexture(GL_TEXTURE0);
                bindTexture(m_textureIds[0], fw, m_frame.height(), m_frame.bits(), GL_RGBA);

            } else if (m_format.pixelFormat() == QVideoFrame::Format_NV12
                    || m_format.pixelFormat() == QVideoFrame::Format_NV21) {
                const int y = 0;
                const int uv = 1;

                m_planeWidth[0] = m_planeWidth[1] = qreal(fw) / m_frame.bytesPerLine(y);

                functions->glActiveTexture(GL_TEXTURE1);
                bindTexture(m_textureIds[1], m_frame.bytesPerLine(uv) / 2, fh / 2, m_frame.bits(uv), GL_LUMINANCE_ALPHA);
                functions->glActiveTexture(GL_TEXTURE0); // Finish with 0 as default texture unit
                bindTexture(m_textureIds[0], m_frame.bytesPerLine(y), fh, m_frame.bits(y), GL_LUMINANCE);

            } else { // YUV420P || YV12
                const int y = 0;
                const int u = m_frame.pixelFormat() == QVideoFrame::Format_YUV420P ? 1 : 2;
                const int v = m_frame.pixelFormat() == QVideoFrame::Format_YUV420P ? 2 : 1;

                m_planeWidth[0] = qreal(fw) / m_frame.bytesPerLine(y);
                m_planeWidth[1] = m_planeWidth[2] = qreal(fw) / (2 * m_frame.bytesPerLine(u));

                functions->glActiveTexture(GL_TEXTURE1);
                bindTexture(m_textureIds[1], m_frame.bytesPerLine(u), fh / 2, m_frame.bits(u), GL_LUMINANCE);
                functions->glActiveTexture(GL_TEXTURE2);
                bindTexture(m_textureIds[2], m_frame.bytesPerLine(v), fh / 2, m_frame.bits(v), GL_LUMINANCE);
                functions->glActiveTexture(GL_TEXTURE0); // Finish with 0 as default texture unit
                bindTexture(m_textureIds[0], m_frame.bytesPerLine(y), fh, m_frame.bits(y), GL_LUMINANCE);
            }

            functions->glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
            m_frame.unmap();
        }

        m_frame = QVideoFrame();
    } else {
        // Go backwards to finish with GL_TEXTURE0
        for (int i = m_planeCount - 1; i >= 0; --i) {
            functions->glActiveTexture(GL_TEXTURE0 + i);
            functions->glBindTexture(GL_TEXTURE_2D, m_textureIds[i]);
        }
    }
}

void QSGVideoMaterial_YUV::bindTexture(int id, int w, int h, const uchar *bits, GLenum format)
{
    QOpenGLFunctions *functions = QOpenGLContext::currentContext()->functions();
    functions->glBindTexture(GL_TEXTURE_2D, id);
    functions->glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, bits);
    functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

QSGVideoNode_YUV::QSGVideoNode_YUV(const QVideoSurfaceFormat &format) :
    m_format(format)
{
    setFlag(QSGNode::OwnsMaterial);
    m_material = new QSGVideoMaterial_YUV(format);
    setMaterial(m_material);
}

QSGVideoNode_YUV::~QSGVideoNode_YUV()
{
}

void QSGVideoNode_YUV::setCurrentFrame(const QVideoFrame &frame, FrameFlags)
{
    m_material->setCurrentFrame(frame);
    markDirty(DirtyMaterial);
}

void QSGVideoMaterialShader_YUV_BiPlanar::updateState(const RenderState &state,
                                                      QSGMaterial *newMaterial,
                                                      QSGMaterial *oldMaterial)
{
    Q_UNUSED(oldMaterial);

    QSGVideoMaterial_YUV *mat = static_cast<QSGVideoMaterial_YUV *>(newMaterial);
    program()->setUniformValue(m_id_plane1Texture, 0);
    program()->setUniformValue(m_id_plane2Texture, 1);

    mat->bind();

    program()->setUniformValue(m_id_colorMatrix, mat->m_colorMatrix);
    program()->setUniformValue(m_id_plane1Width, mat->m_planeWidth[0]);
    program()->setUniformValue(m_id_plane2Width, mat->m_planeWidth[1]);
    if (state.isOpacityDirty()) {
        mat->m_opacity = state.opacity();
        program()->setUniformValue(m_id_opacity, GLfloat(mat->m_opacity));
    }
    if (state.isMatrixDirty())
        program()->setUniformValue(m_id_matrix, state.combinedMatrix());
}

void QSGVideoMaterialShader_YUV_TriPlanar::updateState(const RenderState &state,
                                                       QSGMaterial *newMaterial,
                                                       QSGMaterial *oldMaterial)
{
    QSGVideoMaterialShader_YUV_BiPlanar::updateState(state, newMaterial, oldMaterial);

    QSGVideoMaterial_YUV *mat = static_cast<QSGVideoMaterial_YUV *>(newMaterial);
    program()->setUniformValue(m_id_plane3Texture, 2);
    program()->setUniformValue(m_id_plane3Width, mat->m_planeWidth[2]);
}

void QSGVideoMaterialShader_UYVY::updateState(const RenderState &state,
                                                       QSGMaterial *newMaterial,
                                                       QSGMaterial *oldMaterial)
{
    Q_UNUSED(oldMaterial);

    QSGVideoMaterial_YUV *mat = static_cast<QSGVideoMaterial_YUV *>(newMaterial);

    program()->setUniformValue(m_id_yuvtexture, 0);

    mat->bind();

    program()->setUniformValue(m_id_colorMatrix, mat->m_colorMatrix);
    program()->setUniformValue(m_id_imageWidth, mat->m_frame.width());

    if (state.isOpacityDirty()) {
        mat->m_opacity = state.opacity();
        program()->setUniformValue(m_id_opacity, GLfloat(mat->m_opacity));
    }

    if (state.isMatrixDirty())
        program()->setUniformValue(m_id_matrix, state.combinedMatrix());
}

QT_END_NAMESPACE
