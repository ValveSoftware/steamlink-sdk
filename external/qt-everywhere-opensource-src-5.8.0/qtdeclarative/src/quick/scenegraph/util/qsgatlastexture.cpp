/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qsgatlastexture_p.h"

#include <QtCore/QVarLengthArray>
#include <QtCore/QElapsedTimer>
#include <QtCore/QtMath>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtGui/QSurface>
#include <QtGui/QWindow>
#include <QtGui/qpa/qplatformnativeinterface.h>

#include <private/qsgtexture_p.h>

#include <private/qquickprofiler_p.h>

QT_BEGIN_NAMESPACE

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

int qt_sg_envInt(const char *name, int defaultValue);

static QElapsedTimer qsg_renderer_timer;

namespace QSGAtlasTexture
{

Manager::Manager()
    : m_atlas(0)
{
    QOpenGLContext *gl = QOpenGLContext::currentContext();
    Q_ASSERT(gl);
    QSurface *surface = gl->surface();
    QSize surfaceSize = surface->size();
    int max;
    gl->functions()->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);

    int w = qMin(max, qt_sg_envInt("QSG_ATLAS_WIDTH", qMax(512U, qNextPowerOfTwo(surfaceSize.width() - 1))));
    int h = qMin(max, qt_sg_envInt("QSG_ATLAS_HEIGHT", qMax(512U, qNextPowerOfTwo(surfaceSize.height() - 1))));

    if (surface->surfaceClass() == QSurface::Window) {
        QWindow *window = static_cast<QWindow *>(surface);
        // Coverwindows, optimize for memory rather than speed
        if ((window->type() & Qt::CoverWindow) == Qt::CoverWindow) {
            w /= 2;
            h /= 2;
        }
    }

    m_atlas_size_limit = qt_sg_envInt("QSG_ATLAS_SIZE_LIMIT", qMax(w, h) / 2);
    m_atlas_size = QSize(w, h);

    qCDebug(QSG_LOG_INFO, "texture atlas dimensions: %dx%d", w, h);
}


Manager::~Manager()
{
    Q_ASSERT(m_atlas == 0);
}

void Manager::invalidate()
{
    if (m_atlas) {
        m_atlas->invalidate();
        m_atlas->deleteLater();
        m_atlas = 0;
    }
}

QSGTexture *Manager::create(const QImage &image, bool hasAlphaChannel)
{
    Texture *t = 0;
    if (image.width() < m_atlas_size_limit && image.height() < m_atlas_size_limit) {
        if (!m_atlas)
            m_atlas = new Atlas(m_atlas_size);
        // t may be null for atlas allocation failure
        t = m_atlas->create(image);
        if (t && !hasAlphaChannel && t->hasAlphaChannel())
            t->setHasAlphaChannel(false);
    }
    return t;
}

Atlas::Atlas(const QSize &size)
    : m_allocator(size)
    , m_texture_id(0)
    , m_size(size)
    , m_atlas_transient_image_threshold(0)
    , m_allocated(false)
{

    m_internalFormat = GL_RGBA;
    m_externalFormat = GL_BGRA;

#ifndef QT_OPENGL_ES
    if (QOpenGLContext::currentContext()->isOpenGLES()) {
#endif

#if defined(Q_OS_ANDROID)
    QString *deviceName =
            static_cast<QString *>(QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("AndroidDeviceName"));
    static bool wrongfullyReportsBgra8888Support = deviceName != 0
                                                    && (deviceName->compare(QLatin1String("samsung SM-T211"), Qt::CaseInsensitive) == 0
                                                        || deviceName->compare(QLatin1String("samsung SM-T210"), Qt::CaseInsensitive) == 0
                                                        || deviceName->compare(QLatin1String("samsung SM-T215"), Qt::CaseInsensitive) == 0);
#else
    static bool wrongfullyReportsBgra8888Support = false;
    // The Raspberry Pi (both 1 and 2) GPU refuses framebuffers with BGRA color attachments.
    const GLubyte *renderer = QOpenGLContext::currentContext()->functions()->glGetString(GL_RENDERER);
    if (renderer && strstr((const char *) renderer, "VideoCore IV"))
        wrongfullyReportsBgra8888Support = true;
#endif // ANDROID

    if (qEnvironmentVariableIsSet("QSG_ATLAS_NO_BGRA_WORKAROUNDS"))
        wrongfullyReportsBgra8888Support = false;

    const char *ext = (const char *) QOpenGLContext::currentContext()->functions()->glGetString(GL_EXTENSIONS);
    if (ext && !wrongfullyReportsBgra8888Support
            && (strstr(ext, "GL_EXT_bgra")
                || strstr(ext, "GL_EXT_texture_format_BGRA8888")
                || strstr(ext, "GL_IMG_texture_format_BGRA8888"))) {
        m_internalFormat = m_externalFormat = GL_BGRA;
#if defined(Q_OS_DARWIN) && !defined(Q_OS_OSX)
    } else if (ext && strstr(ext, "GL_APPLE_texture_format_BGRA8888")) {
        m_internalFormat = GL_RGBA;
        m_externalFormat = GL_BGRA;
#endif // IOS || TVOS
    } else {
        m_internalFormat = m_externalFormat = GL_RGBA;
    }

#ifndef QT_OPENGL_ES
    }
#endif

    m_use_bgra_fallback = qEnvironmentVariableIsSet("QSG_ATLAS_USE_BGRA_FALLBACK");
    m_debug_overlay = qEnvironmentVariableIsSet("QSG_ATLAS_OVERLAY");

    // images smaller than this will retain their QImage.
    // by default no images are retained (favoring memory)
    // set to a very large value to retain all images (allowing quick removal from the atlas)
    m_atlas_transient_image_threshold = qt_sg_envInt("QSG_ATLAS_TRANSIENT_IMAGE_THRESHOLD", 0);
}

Atlas::~Atlas()
{
    Q_ASSERT(!m_texture_id);
}

void Atlas::invalidate()
{
    if (m_texture_id && QOpenGLContext::currentContext())
        QOpenGLContext::currentContext()->functions()->glDeleteTextures(1, &m_texture_id);
    m_texture_id = 0;
}

Texture *Atlas::create(const QImage &image)
{
    // No need to lock, as manager already locked it.
    QRect rect = m_allocator.allocate(QSize(image.width() + 2, image.height() + 2));
    if (rect.width() > 0 && rect.height() > 0) {
        Texture *t = new Texture(this, rect, image);
        m_pending_uploads << t;
        return t;
    }
    return 0;
}


int Atlas::textureId() const
{
    if (!m_texture_id) {
        Q_ASSERT(QOpenGLContext::currentContext());
        QOpenGLContext::currentContext()->functions()->glGenTextures(1, &const_cast<Atlas *>(this)->m_texture_id);
    }

    return m_texture_id;
}

static void swizzleBGRAToRGBA(QImage *image)
{
    const int width = image->width();
    const int height = image->height();
    uint *p = (uint *) image->bits();
    int stride = image->bytesPerLine() / 4;
    for (int i = 0; i < height; ++i) {
        for (int x = 0; x < width; ++x)
            p[x] = ((p[x] << 16) & 0xff0000) | ((p[x] >> 16) & 0xff) | (p[x] & 0xff00ff00);
        p += stride;
    }
}

void Atlas::upload(Texture *texture)
{
    const QImage &image = texture->image();
    const QRect &r = texture->atlasSubRect();

    QImage tmp(r.width(), r.height(), QImage::Format_ARGB32_Premultiplied);
    {
        QPainter p(&tmp);
        p.setCompositionMode(QPainter::CompositionMode_Source);

        int w = r.width();
        int h = r.height();
        int iw = image.width();
        int ih = image.height();

        p.drawImage(1, 1, image);
        p.drawImage(1, 0, image, 0, 0, iw, 1);
        p.drawImage(1, h - 1, image, 0, ih - 1, iw, 1);
        p.drawImage(0, 1, image, 0, 0, 1, ih);
        p.drawImage(w - 1, 1, image, iw - 1, 0, 1, ih);
        p.drawImage(0, 0, image, 0, 0, 1, 1);
        p.drawImage(0, h - 1, image, 0, ih - 1, 1, 1);
        p.drawImage(w - 1, 0, image, iw - 1, 0, 1, 1);
        p.drawImage(w - 1, h - 1, image, iw - 1, ih - 1, 1, 1);
        if (m_debug_overlay) {
            p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
            p.fillRect(0, 0, iw, ih, QBrush(QColor::fromRgbF(1, 0, 1, 0.5)));
        }
    }

    if (m_externalFormat == GL_RGBA)
        swizzleBGRAToRGBA(&tmp);
    QOpenGLContext::currentContext()->functions()->glTexSubImage2D(GL_TEXTURE_2D, 0,
                                                                   r.x(), r.y(), r.width(), r.height(),
                                                                   m_externalFormat, GL_UNSIGNED_BYTE, tmp.constBits());
}

void Atlas::uploadBgra(Texture *texture)
{
    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    const QRect &r = texture->atlasSubRect();
    QImage image = texture->image();

    if (image.isNull())
        return;

    if (image.format() != QImage::Format_ARGB32_Premultiplied
            && image.format() != QImage::Format_RGB32) {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    if (m_debug_overlay) {
        QPainter p(&image);
        p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        p.fillRect(0, 0, image.width(), image.height(), QBrush(QColor::fromRgbF(0, 1, 1, 0.5)));
    }

    QVarLengthArray<quint32, 512> tmpBits(qMax(image.width() + 2, image.height() + 2));
    int iw = image.width();
    int ih = image.height();
    int bpl = image.bytesPerLine() / 4;
    const quint32 *src = (const quint32 *) image.constBits();
    quint32 *dst = tmpBits.data();

    // top row, padding corners
    dst[0] = src[0];
    memcpy(dst + 1, src, iw * sizeof(quint32));
    dst[1 + iw] = src[iw-1];
    funcs->glTexSubImage2D(GL_TEXTURE_2D, 0, r.x(), r.y(), iw + 2, 1, m_externalFormat, GL_UNSIGNED_BYTE, dst);

    // bottom row, padded corners
    const quint32 *lastRow = src + bpl * (ih - 1);
    dst[0] = lastRow[0];
    memcpy(dst + 1, lastRow, iw * sizeof(quint32));
    dst[1 + iw] = lastRow[iw-1];
    funcs->glTexSubImage2D(GL_TEXTURE_2D, 0, r.x(), r.y() + ih + 1, iw + 2, 1, m_externalFormat, GL_UNSIGNED_BYTE, dst);

    // left column
    for (int i=0; i<ih; ++i)
        dst[i] = src[i * bpl];
    funcs->glTexSubImage2D(GL_TEXTURE_2D, 0, r.x(), r.y() + 1, 1, ih, m_externalFormat, GL_UNSIGNED_BYTE, dst);

    // right column
    for (int i=0; i<ih; ++i)
        dst[i] = src[i * bpl + iw - 1];
    funcs->glTexSubImage2D(GL_TEXTURE_2D, 0, r.x() + iw + 1, r.y() + 1, 1, ih, m_externalFormat, GL_UNSIGNED_BYTE, dst);

    // Inner part of the image....
    if (bpl != iw) {
        int sy = r.y() + 1;
        int ey = sy + r.height() - 2;
        for (int y = sy; y < ey; ++y) {
            funcs->glTexSubImage2D(GL_TEXTURE_2D, 0, r.x() + 1, y, r.width() - 2, 1, m_externalFormat, GL_UNSIGNED_BYTE, src);
            src += bpl;
        }
    } else {
        funcs->glTexSubImage2D(GL_TEXTURE_2D, 0, r.x() + 1, r.y() + 1, r.width() - 2, r.height() - 2, m_externalFormat, GL_UNSIGNED_BYTE, src);
    }
}

void Atlas::bind(QSGTexture::Filtering filtering)
{
    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    if (!m_allocated) {
        m_allocated = true;

        while (funcs->glGetError() != GL_NO_ERROR) ;

        funcs->glGenTextures(1, &m_texture_id);
        funcs->glBindTexture(GL_TEXTURE_2D, m_texture_id);
        funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if !defined(QT_OPENGL_ES_2)
        if (!QOpenGLContext::currentContext()->isOpenGLES())
            funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
#endif
        funcs->glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_size.width(), m_size.height(), 0, m_externalFormat, GL_UNSIGNED_BYTE, 0);

#if 0
        QImage pink(m_size.width(), m_size.height(), QImage::Format_ARGB32_Premultiplied);
        pink.fill(0);
        QPainter p(&pink);
        QLinearGradient redGrad(0, 0, m_size.width(), 0);
        redGrad.setColorAt(0, Qt::black);
        redGrad.setColorAt(1, Qt::red);
        p.fillRect(0, 0, m_size.width(), m_size.height(), redGrad);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        QLinearGradient blueGrad(0, 0, 0, m_size.height());
        blueGrad.setColorAt(0, Qt::black);
        blueGrad.setColorAt(1, Qt::blue);
        p.fillRect(0, 0, m_size.width(), m_size.height(), blueGrad);
        p.end();

        funcs->glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_size.width(), m_size.height(), 0, m_externalFormat, GL_UNSIGNED_BYTE, pink.constBits());
#endif

        GLenum errorCode = funcs->glGetError();
        if (errorCode == GL_OUT_OF_MEMORY) {
            qDebug("QSGTextureAtlas: texture atlas allocation failed, out of memory");
            funcs->glDeleteTextures(1, &m_texture_id);
            m_texture_id = 0;
        } else if (errorCode != GL_NO_ERROR) {
            qDebug("QSGTextureAtlas: texture atlas allocation failed, code=%x", errorCode);
            funcs->glDeleteTextures(1, &m_texture_id);
            m_texture_id = 0;
        }
    } else {
        funcs->glBindTexture(GL_TEXTURE_2D, m_texture_id);
    }

    if (m_texture_id == 0)
        return;

    // Upload all pending images..
    for (int i=0; i<m_pending_uploads.size(); ++i) {

        bool profileFrames = QSG_LOG_TIME_TEXTURE().isDebugEnabled();
        if (profileFrames)
            qsg_renderer_timer.start();

        Q_QUICK_SG_PROFILE_START(QQuickProfiler::SceneGraphTexturePrepare);

        // Skip bind, convert, swizzle; they're irrelevant
        Q_QUICK_SG_PROFILE_SKIP(QQuickProfiler::SceneGraphTexturePrepare,
                                QQuickProfiler::SceneGraphTexturePrepareStart, 3);

        Texture *t = m_pending_uploads.at(i);
        if (m_externalFormat == GL_BGRA &&
                !m_use_bgra_fallback) {
            uploadBgra(t);
        } else {
            upload(t);
        }
        const QSize textureSize = t->textureSize();
        if (textureSize.width() > m_atlas_transient_image_threshold ||
                textureSize.height() > m_atlas_transient_image_threshold)
            t->releaseImage();

        qCDebug(QSG_LOG_TIME_TEXTURE).nospace() << "atlastexture uploaded in: " << qsg_renderer_timer.elapsed()
                                           << "ms (" << t->textureSize().width() << "x"
                                           << t->textureSize().height() << ")";

        Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphTexturePrepare,
                                  QQuickProfiler::SceneGraphTexturePrepareUpload);

        // Skip mipmap; unused
        Q_QUICK_SG_PROFILE_SKIP(QQuickProfiler::SceneGraphTexturePrepare,
                                QQuickProfiler::SceneGraphTexturePrepareUpload, 1);
        Q_QUICK_SG_PROFILE_REPORT(QQuickProfiler::SceneGraphTexturePrepare,
                                  QQuickProfiler::SceneGraphTexturePrepareMipmap);
    }

    GLenum f = filtering == QSGTexture::Nearest ? GL_NEAREST : GL_LINEAR;
    funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);

    m_pending_uploads.clear();
}

void Atlas::remove(Texture *t)
{
    QRect atlasRect = t->atlasSubRect();
    m_allocator.deallocate(atlasRect);
    m_pending_uploads.removeOne(t);
}



Texture::Texture(Atlas *atlas, const QRect &textureRect, const QImage &image)
    : QSGTexture()
    , m_allocated_rect(textureRect)
    , m_image(image)
    , m_atlas(atlas)
    , m_nonatlas_texture(0)
    , m_has_alpha(image.hasAlphaChannel())
{
    float w = atlas->size().width();
    float h = atlas->size().height();
    QRect nopad = atlasSubRectWithoutPadding();
    m_texture_coords_rect = QRectF(nopad.x() / w,
                                   nopad.y() / h,
                                   nopad.width() / w,
                                   nopad.height() / h);
}

Texture::~Texture()
{
    m_atlas->remove(this);
    if (m_nonatlas_texture)
        delete m_nonatlas_texture;
}

void Texture::bind()
{
    m_atlas->bind(filtering());
}

QSGTexture *Texture::removedFromAtlas() const
{
    if (m_nonatlas_texture) {
        m_nonatlas_texture->setMipmapFiltering(mipmapFiltering());
        m_nonatlas_texture->setFiltering(filtering());
        return m_nonatlas_texture;
    }

    if (!m_image.isNull()) {
        m_nonatlas_texture = new QSGPlainTexture();
        m_nonatlas_texture->setImage(m_image);
        m_nonatlas_texture->setFiltering(filtering());

    } else {
        QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
        // bind the atlas texture as an fbo and extract the texture..

        // First extract the currently bound fbo so we can restore it later.
        GLint currentFbo;
        f->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFbo);

        // Create an FBO and bind the atlas texture into it.
        GLuint fbo;
        f->glGenFramebuffers(1, &fbo);
        f->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_atlas->textureId(), 0);

        // Create the target texture, QSGPlainTexture below will deal with the texparams, so we don't
        // need to worry about those here.
        GLuint texture;
        f->glGenTextures(1, &texture);
        f->glBindTexture(GL_TEXTURE_2D, texture);
        QRect r = atlasSubRectWithoutPadding();
        // and copy atlas into our texture.
        while (f->glGetError() != GL_NO_ERROR) ;
        f->glCopyTexImage2D(GL_TEXTURE_2D, 0, m_atlas->internalFormat(), r.x(), r.y(), r.width(), r.height(), 0);
        // BGRA may have been rejected by some GLES implementations
        if (f->glGetError() != GL_NO_ERROR)
            f->glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r.x(), r.y(), r.width(), r.height(), 0);

        m_nonatlas_texture = new QSGPlainTexture();
        m_nonatlas_texture->setTextureId(texture);
        m_nonatlas_texture->setOwnsTexture(true);
        m_nonatlas_texture->setHasAlphaChannel(m_has_alpha);
        m_nonatlas_texture->setTextureSize(r.size());

        // cleanup: unbind our atlas from the fbo, rebind the old default and delete the fbo.
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        f->glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) currentFbo);
        f->glDeleteFramebuffers(1, &fbo);
    }

    m_nonatlas_texture->setMipmapFiltering(mipmapFiltering());
    m_nonatlas_texture->setFiltering(filtering());
    return m_nonatlas_texture;
}

}

QT_END_NAMESPACE
