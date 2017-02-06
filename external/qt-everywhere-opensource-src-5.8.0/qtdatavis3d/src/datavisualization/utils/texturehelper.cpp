/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "texturehelper_p.h"
#include "utils_p.h"

#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtCore/QTime>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

// Defined in shaderhelper.cpp
extern void discardDebugMsgs(QtMsgType type, const QMessageLogContext &context, const QString &msg);

TextureHelper::TextureHelper()
{
    initializeOpenGLFunctions();
#if !defined(QT_OPENGL_ES_2)
    if (!Utils::isOpenGLES()) {
        // Discard warnings about deprecated functions
        QtMessageHandler handler = qInstallMessageHandler(discardDebugMsgs);

        m_openGlFunctions_2_1 =
                QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_2_1>();
        m_openGlFunctions_2_1->initializeOpenGLFunctions();

        // Restore original message handler
        qInstallMessageHandler(handler);
    }
#endif
}

TextureHelper::~TextureHelper()
{
}

GLuint TextureHelper::create2DTexture(const QImage &image, bool useTrilinearFiltering,
                                      bool convert, bool smoothScale, bool clampY)
{
    if (image.isNull())
        return 0;

    QImage texImage = image;

    if (Utils::isOpenGLES()) {
        GLuint imageWidth = Utils::getNearestPowerOfTwo(image.width());
        GLuint imageHeight = Utils::getNearestPowerOfTwo(image.height());
        if (smoothScale) {
            texImage = image.scaled(imageWidth, imageHeight, Qt::IgnoreAspectRatio,
                                    Qt::SmoothTransformation);
        } else {
            texImage = image.scaled(imageWidth, imageHeight, Qt::IgnoreAspectRatio);
        }
    }

    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    if (convert)
        texImage = convertToGLFormat(texImage);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texImage.width(), texImage.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, texImage.bits());
    if (smoothScale)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    else
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (useTrilinearFiltering) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
    if (clampY)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureId;
}

GLuint TextureHelper::create3DTexture(const QVector<uchar> *data, int width, int height, int depth,
                                      QImage::Format dataFormat)
{
    if (Utils::isOpenGLES() || !width || !height || !depth)
        return 0;

    GLuint textureId = 0;
#if defined(QT_OPENGL_ES_2)
    Q_UNUSED(dataFormat)
    Q_UNUSED(data)
#else
    glEnable(GL_TEXTURE_3D);

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_3D, textureId);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    GLenum status = glGetError();
    // glGetError docs advise to call glGetError in loop to clear all error flags
    while (status)
        status = glGetError();

    GLint internalFormat = 4;
    GLint format = GL_BGRA;
    if (dataFormat == QImage::Format_Indexed8) {
        internalFormat = 1;
        format = GL_RED;
        // Align width to 32bits
        width = width + width % 4;
    }
    m_openGlFunctions_2_1->glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, width, height, depth, 0,
                                        format, GL_UNSIGNED_BYTE, data->constData());
    status = glGetError();
    if (status)
        qWarning() << __FUNCTION__ << "3D texture creation failed:" << status;

    glBindTexture(GL_TEXTURE_3D, 0);
    glDisable(GL_TEXTURE_3D);
#endif
    return textureId;
}

GLuint TextureHelper::createCubeMapTexture(const QImage &image, bool useTrilinearFiltering)
{
    if (image.isNull())
        return 0;

    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureId);
    QImage glTexture = convertToGLFormat(image);
    glTexImage2D(GL_TEXTURE_CUBE_MAP, 0, GL_RGBA, glTexture.width(), glTexture.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, glTexture.bits());
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (useTrilinearFiltering) {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    } else {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureId;
}

GLuint TextureHelper::createSelectionTexture(const QSize &size, GLuint &frameBuffer,
                                             GLuint &depthBuffer)
{
    GLuint textureid;

    // Create texture for the selection buffer
    glGenTextures(1, &textureid);
    glBindTexture(GL_TEXTURE_2D, textureid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create render buffer
    if (depthBuffer)
        glDeleteRenderbuffers(1, &depthBuffer);

    glGenRenderbuffers(1, &depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    GLenum status = glGetError();
    // glGetError docs advise to call glGetError in loop to clear all error flags
    while (status)
        status = glGetError();
    if (Utils::isOpenGLES())
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, size.width(), size.height());
    else
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, size.width(), size.height());

    status = glGetError();
    if (status) {
        qCritical() << "Selection texture render buffer creation failed:" << status;
        glDeleteTextures(1, &textureid);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        return 0;
    }
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Create frame buffer
    if (!frameBuffer)
        glGenFramebuffers(1, &frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

    // Attach texture to color attachment
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureid, 0);
    // Attach renderbuffer to depth attachment
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

    // Verify that the frame buffer is complete
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        qCritical() << "Selection texture frame buffer creation failed:" << status;
        glDeleteTextures(1, &textureid);
        textureid = 0;
    }

    // Restore the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return textureid;
}

GLuint TextureHelper::createCursorPositionTexture(const QSize &size, GLuint &frameBuffer)
{
    GLuint textureid;
    glGenTextures(1, &textureid);
    glBindTexture(GL_TEXTURE_2D, textureid);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           textureid, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        qCritical() << "Cursor position mapper frame buffer creation failed:" << status;
        glDeleteTextures(1, &textureid);
        textureid = 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return textureid;
}

GLuint TextureHelper::createUniformTexture(const QColor &color)
{
    QImage image(QSize(int(uniformTextureWidth), int(uniformTextureHeight)),
                 QImage::Format_RGB32);
    QPainter pmp(&image);
    pmp.setBrush(QBrush(color));
    pmp.setPen(Qt::NoPen);
    pmp.drawRect(0, 0, int(uniformTextureWidth), int(uniformTextureHeight));

    return create2DTexture(image, false, true, false, true);
}

GLuint TextureHelper::createGradientTexture(const QLinearGradient &gradient)
{
    QImage image(QSize(int(gradientTextureWidth), int(gradientTextureHeight)),
                 QImage::Format_RGB32);
    QPainter pmp(&image);
    pmp.setBrush(QBrush(gradient));
    pmp.setPen(Qt::NoPen);
    pmp.drawRect(0, 0, int(gradientTextureWidth), int(gradientTextureHeight));

    return create2DTexture(image, false, true, false, true);
}

GLuint TextureHelper::createDepthTexture(const QSize &size, GLuint textureSize)
{
    GLuint depthtextureid = 0;
#if defined(QT_OPENGL_ES_2)
    Q_UNUSED(size)
    Q_UNUSED(textureSize)
#else
    if (!Utils::isOpenGLES()) {
        // Create depth texture for the shadow mapping
        glGenTextures(1, &depthtextureid);
        glBindTexture(GL_TEXTURE_2D, depthtextureid);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, size.width() * textureSize,
                     size.height() * textureSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
#endif
    return depthtextureid;
}

GLuint TextureHelper::createDepthTextureFrameBuffer(const QSize &size, GLuint &frameBuffer,
                                                    GLuint textureSize)
{
    GLuint depthtextureid = createDepthTexture(size, textureSize);
#if defined(QT_OPENGL_ES_2)
    Q_UNUSED(frameBuffer)
#else
    if (!Utils::isOpenGLES()) {
        // Create frame buffer
        if (!frameBuffer)
            glGenFramebuffers(1, &frameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

        // Attach texture to depth attachment
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthtextureid, 0);

        m_openGlFunctions_2_1->glDrawBuffer(GL_NONE);
        m_openGlFunctions_2_1->glReadBuffer(GL_NONE);

        // Verify that the frame buffer is complete
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            qCritical() << "Depth texture frame buffer creation failed" << status;
            glDeleteTextures(1, &depthtextureid);
            depthtextureid = 0;
        }

        // Restore the default framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
#endif
    return depthtextureid;
}

void TextureHelper::deleteTexture(GLuint *texture)
{
    if (texture && *texture) {
        if (QOpenGLContext::currentContext())
            glDeleteTextures(1, texture);
        *texture = 0;
    }
}

QImage TextureHelper::convertToGLFormat(const QImage &srcImage)
{
    QImage res(srcImage.size(), QImage::Format_ARGB32);
    convertToGLFormatHelper(res, srcImage.convertToFormat(QImage::Format_ARGB32), GL_RGBA);
    return res;
}

void TextureHelper::convertToGLFormatHelper(QImage &dstImage, const QImage &srcImage,
                                            GLenum texture_format)
{
    Q_ASSERT(dstImage.depth() == 32);
    Q_ASSERT(srcImage.depth() == 32);

    if (dstImage.size() != srcImage.size()) {
        int target_width = dstImage.width();
        int target_height = dstImage.height();
        float sx = target_width / float(srcImage.width());
        float sy = target_height / float(srcImage.height());

        quint32 *dest = (quint32 *) dstImage.scanLine(0); // NB! avoid detach here
        uchar *srcPixels = (uchar *) srcImage.scanLine(srcImage.height() - 1);
        int sbpl = srcImage.bytesPerLine();
        int dbpl = dstImage.bytesPerLine();

        int ix = int(0x00010000 / sx);
        int iy = int(0x00010000 / sy);

        quint32 basex = int(0.5 * ix);
        quint32 srcy = int(0.5 * iy);

        // scale, swizzle and mirror in one loop
        while (target_height--) {
            const uint *src = (const quint32 *) (srcPixels - (srcy >> 16) * sbpl);
            int srcx = basex;
            for (int x=0; x<target_width; ++x) {
                dest[x] = qt_gl_convertToGLFormatHelper(src[srcx >> 16], texture_format);
                srcx += ix;
            }
            dest = (quint32 *)(((uchar *) dest) + dbpl);
            srcy += iy;
        }
    } else {
        const int width = srcImage.width();
        const int height = srcImage.height();
        const uint *p = (const uint*) srcImage.scanLine(srcImage.height() - 1);
        uint *q = (uint*) dstImage.scanLine(0);

#if !defined(QT_OPENGL_ES_2)
        if (texture_format == GL_BGRA) {
#else
        if (texture_format == GL_BGRA8_EXT) {
#endif
            if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
                // mirror + swizzle
                for (int i=0; i < height; ++i) {
                    const uint *end = p + width;
                    while (p < end) {
                        *q = ((*p << 24) & 0xff000000)
                                | ((*p >> 24) & 0x000000ff)
                                | ((*p << 8) & 0x00ff0000)
                                | ((*p >> 8) & 0x0000ff00);
                        p++;
                        q++;
                    }
                    p -= 2 * width;
                }
            } else {
                const uint bytesPerLine = srcImage.bytesPerLine();
                for (int i=0; i < height; ++i) {
                    memcpy(q, p, bytesPerLine);
                    q += width;
                    p -= width;
                }
            }
        } else {
            if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
                for (int i=0; i < height; ++i) {
                    const uint *end = p + width;
                    while (p < end) {
                        *q = (*p << 8) | ((*p >> 24) & 0xff);
                        p++;
                        q++;
                    }
                    p -= 2 * width;
                }
            } else {
                for (int i=0; i < height; ++i) {
                    const uint *end = p + width;
                    while (p < end) {
                        *q = ((*p << 16) & 0xff0000) | ((*p >> 16) & 0xff) | (*p & 0xff00ff00);
                        p++;
                        q++;
                    }
                    p -= 2 * width;
                }
            }
        }
    }
}

QRgb TextureHelper::qt_gl_convertToGLFormatHelper(QRgb src_pixel, GLenum texture_format)
{
#if !defined(QT_OPENGL_ES_2)
    if (texture_format == GL_BGRA) {
#else
    if (texture_format == GL_BGRA8_EXT) {
#endif
        if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
            return ((src_pixel << 24) & 0xff000000)
                    | ((src_pixel >> 24) & 0x000000ff)
                    | ((src_pixel << 8) & 0x00ff0000)
                    | ((src_pixel >> 8) & 0x0000ff00);
        } else {
            return src_pixel;
        }
    } else {  // GL_RGBA
        if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
            return (src_pixel << 8) | ((src_pixel >> 24) & 0xff);
        } else {
            return ((src_pixel << 16) & 0xff0000)
                    | ((src_pixel >> 16) & 0xff)
                    | (src_pixel & 0xff00ff00);
        }
    }
}

QT_END_NAMESPACE_DATAVISUALIZATION
