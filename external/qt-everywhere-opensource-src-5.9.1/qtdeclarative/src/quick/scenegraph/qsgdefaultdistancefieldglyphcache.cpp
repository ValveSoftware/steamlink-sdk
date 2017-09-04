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

#include "qsgdefaultdistancefieldglyphcache_p.h"

#include <QtGui/private/qdistancefield_p.h>
#include <QtGui/private/qopenglcontext_p.h>
#include <QtQml/private/qqmlglobal_p.h>
#include <QtQuick/private/qsgdistancefieldutil_p.h>
#include <qopenglfunctions.h>
#include <qopenglframebufferobject.h>
#include <qmath.h>

#if !defined(QT_OPENGL_ES_2)
#include <QtGui/qopenglfunctions_3_2_core.h>
#endif

QT_BEGIN_NAMESPACE

DEFINE_BOOL_CONFIG_OPTION(qmlUseGlyphCacheWorkaround, QML_USE_GLYPHCACHE_WORKAROUND)
DEFINE_BOOL_CONFIG_OPTION(qsgPreferFullSizeGlyphCacheTextures, QSG_PREFER_FULLSIZE_GLYPHCACHE_TEXTURES)

#if !defined(QSG_DEFAULT_DISTANCEFIELD_GLYPH_CACHE_PADDING)
#  define QSG_DEFAULT_DISTANCEFIELD_GLYPH_CACHE_PADDING 2
#endif

QSGDefaultDistanceFieldGlyphCache::QSGDefaultDistanceFieldGlyphCache(QSGDistanceFieldGlyphCacheManager *man, QOpenGLContext *c, const QRawFont &font)
    : QSGDistanceFieldGlyphCache(man, c, font)
    , m_maxTextureSize(0)
    , m_maxTextureCount(3)
    , m_blitProgram(0)
    , m_blitBuffer(QOpenGLBuffer::VertexBuffer)
    , m_fboGuard(0)
    , m_funcs(c->functions())
#if !defined(QT_OPENGL_ES_2)
    , m_coreFuncs(0)
#endif
{
    m_blitBuffer.create();
    m_blitBuffer.bind();
    static GLfloat buffer[16] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f,
                                 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    m_blitBuffer.allocate(buffer, sizeof(buffer));
    m_blitBuffer.release();

    m_areaAllocator = new QSGAreaAllocator(QSize(maxTextureSize(), m_maxTextureCount * maxTextureSize()));
}

QSGDefaultDistanceFieldGlyphCache::~QSGDefaultDistanceFieldGlyphCache()
{
    for (int i = 0; i < m_textures.count(); ++i)
        m_funcs->glDeleteTextures(1, &m_textures[i].texture);

    if (m_fboGuard != 0)
        m_fboGuard->free();

    delete m_blitProgram;
    delete m_areaAllocator;
}

void QSGDefaultDistanceFieldGlyphCache::requestGlyphs(const QSet<glyph_t> &glyphs)
{
    QList<GlyphPosition> glyphPositions;
    QVector<glyph_t> glyphsToRender;

    for (QSet<glyph_t>::const_iterator it = glyphs.constBegin(); it != glyphs.constEnd() ; ++it) {
        glyph_t glyphIndex = *it;

        int padding = QSG_DEFAULT_DISTANCEFIELD_GLYPH_CACHE_PADDING;
        QRectF boundingRect = glyphData(glyphIndex).boundingRect;
        int glyphWidth = qCeil(boundingRect.width()) + distanceFieldRadius() * 2;
        int glyphHeight = qCeil(boundingRect.height()) + distanceFieldRadius() * 2;
        QSize glyphSize(glyphWidth + padding * 2, glyphHeight + padding * 2);
        QRect alloc = m_areaAllocator->allocate(glyphSize);

        if (alloc.isNull()) {
            // Unallocate unused glyphs until we can allocated the new glyph
            while (alloc.isNull() && !m_unusedGlyphs.isEmpty()) {
                glyph_t unusedGlyph = *m_unusedGlyphs.constBegin();

                TexCoord unusedCoord = glyphTexCoord(unusedGlyph);
                QRectF unusedGlyphBoundingRect = glyphData(unusedGlyph).boundingRect;
                int unusedGlyphWidth = qCeil(unusedGlyphBoundingRect.width()) + distanceFieldRadius() * 2;
                int unusedGlyphHeight = qCeil(unusedGlyphBoundingRect.height())  + distanceFieldRadius() * 2;
                m_areaAllocator->deallocate(QRect(unusedCoord.x - padding,
                                                  unusedCoord.y - padding,
                                                  padding * 2 + unusedGlyphWidth,
                                                  padding * 2 + unusedGlyphHeight));

                m_unusedGlyphs.remove(unusedGlyph);
                m_glyphsTexture.remove(unusedGlyph);
                removeGlyph(unusedGlyph);

                alloc = m_areaAllocator->allocate(glyphSize);
            }

            // Not enough space left for this glyph... skip to the next one
            if (alloc.isNull())
                continue;
        }

        TextureInfo *tex = textureInfo(alloc.y() / maxTextureSize());
        alloc = QRect(alloc.x(), alloc.y() % maxTextureSize(), alloc.width(), alloc.height());

        tex->allocatedArea |= alloc;
        Q_ASSERT(tex->padding == padding || tex->padding < 0);
        tex->padding = padding;

        GlyphPosition p;
        p.glyph = glyphIndex;
        p.position = alloc.topLeft() + QPoint(padding, padding);

        glyphPositions.append(p);
        glyphsToRender.append(glyphIndex);
        m_glyphsTexture.insert(glyphIndex, tex);
    }

    setGlyphsPosition(glyphPositions);
    markGlyphsToRender(glyphsToRender);
}

void QSGDefaultDistanceFieldGlyphCache::storeGlyphs(const QList<QDistanceField> &glyphs)
{
    typedef QHash<TextureInfo *, QVector<glyph_t> > GlyphTextureHash;
    typedef GlyphTextureHash::const_iterator GlyphTextureHashConstIt;

    GlyphTextureHash glyphTextures;

    GLint alignment = 4; // default value
    m_funcs->glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);

    // Distance field data is always tightly packed
    m_funcs->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (int i = 0; i < glyphs.size(); ++i) {
        QDistanceField glyph = glyphs.at(i);
        glyph_t glyphIndex = glyph.glyph();
        TexCoord c = glyphTexCoord(glyphIndex);
        TextureInfo *texInfo = m_glyphsTexture.value(glyphIndex);

        resizeTexture(texInfo, texInfo->allocatedArea.width(), texInfo->allocatedArea.height());
        m_funcs->glBindTexture(GL_TEXTURE_2D, texInfo->texture);

        glyphTextures[texInfo].append(glyphIndex);

        int padding = texInfo->padding;
        int expectedWidth = qCeil(c.width + c.xMargin * 2);
        glyph = glyph.copy(-padding, -padding,
                           expectedWidth + padding  * 2, glyph.height() + padding * 2);

        if (useTextureResizeWorkaround()) {
            uchar *inBits = glyph.scanLine(0);
            uchar *outBits = texInfo->image.scanLine(int(c.y) - padding) + int(c.x) - padding;
            for (int y = 0; y < glyph.height(); ++y) {
                memcpy(outBits, inBits, glyph.width());
                inBits += glyph.width();
                outBits += texInfo->image.width();
            }
        }

#if !defined(QT_OPENGL_ES_2)
        const GLenum format = isCoreProfile() ? GL_RED : GL_ALPHA;
#else
        const GLenum format = GL_ALPHA;
#endif
        if (useTextureUploadWorkaround()) {
            for (int i = 0; i < glyph.height(); ++i) {
                m_funcs->glTexSubImage2D(GL_TEXTURE_2D, 0,
                                         c.x - padding, c.y + i - padding, glyph.width(),1,
                                         format, GL_UNSIGNED_BYTE,
                                         glyph.scanLine(i));
            }
        } else {
            m_funcs->glTexSubImage2D(GL_TEXTURE_2D, 0,
                                     c.x - padding, c.y - padding, glyph.width(), glyph.height(),
                                     format, GL_UNSIGNED_BYTE,
                                     glyph.constBits());
        }
    }

    // restore to previous alignment
    m_funcs->glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);

    for (GlyphTextureHashConstIt i = glyphTextures.constBegin(), cend = glyphTextures.constEnd(); i != cend; ++i) {
        Texture t;
        t.textureId = i.key()->texture;
        t.size = i.key()->size;
        setGlyphsTexture(i.value(), t);
    }
}

void QSGDefaultDistanceFieldGlyphCache::referenceGlyphs(const QSet<glyph_t> &glyphs)
{
    m_unusedGlyphs -= glyphs;
}

void QSGDefaultDistanceFieldGlyphCache::releaseGlyphs(const QSet<glyph_t> &glyphs)
{
    m_unusedGlyphs += glyphs;
}

void QSGDefaultDistanceFieldGlyphCache::createTexture(TextureInfo *texInfo, int width, int height)
{
    if (useTextureResizeWorkaround() && texInfo->image.isNull())
        texInfo->image = QDistanceField(width, height);

    while (m_funcs->glGetError() != GL_NO_ERROR) { }

    m_funcs->glGenTextures(1, &texInfo->texture);
    m_funcs->glBindTexture(GL_TEXTURE_2D, texInfo->texture);

    m_funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    m_funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    m_funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if !defined(QT_OPENGL_ES_2)
    if (!QOpenGLContext::currentContext()->isOpenGLES())
        m_funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    const GLint internalFormat = isCoreProfile() ? GL_R8 : GL_ALPHA;
    const GLenum format = isCoreProfile() ? GL_RED : GL_ALPHA;
#else
    const GLint internalFormat = GL_ALPHA;
    const GLenum format = GL_ALPHA;
#endif

    QByteArray zeroBuf(width * height, 0);
    m_funcs->glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, zeroBuf.constData());

    texInfo->size = QSize(width, height);

    GLuint error = m_funcs->glGetError();
    if (error != GL_NO_ERROR) {
        m_funcs->glBindTexture(GL_TEXTURE_2D, 0);
        m_funcs->glDeleteTextures(1, &texInfo->texture);
        texInfo->texture = 0;
    }

}

static void freeFramebufferFunc(QOpenGLFunctions *funcs, GLuint id)
{
    funcs->glDeleteFramebuffers(1, &id);
}

void QSGDefaultDistanceFieldGlyphCache::resizeTexture(TextureInfo *texInfo, int width, int height)
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    Q_ASSERT(ctx);

    int oldWidth = texInfo->size.width();
    int oldHeight = texInfo->size.height();
    if (width == oldWidth && height == oldHeight)
        return;

    GLuint oldTexture = texInfo->texture;
    createTexture(texInfo, width, height);

    if (!oldTexture)
        return;

    updateTexture(oldTexture, texInfo->texture, texInfo->size);

#if !defined(QT_OPENGL_ES_2)
    if (isCoreProfile() && !useTextureResizeWorkaround()) {
        // For an OpenGL Core Profile we can use http://www.opengl.org/wiki/Framebuffer#Blitting
        // to efficiently copy the contents of the old texture to the new texture
        // TODO: Use ARB_copy_image if available of if we have >=4.3 context
        if (!m_coreFuncs) {
            m_coreFuncs = ctx->versionFunctions<QOpenGLFunctions_3_2_Core>();
            Q_ASSERT(m_coreFuncs);
            m_coreFuncs->initializeOpenGLFunctions();
        }

        // Create a framebuffer object to which we can attach our old and new textures (to
        // the first two color buffer attachment points)
        if (!m_fboGuard) {
            GLuint fbo;
            m_coreFuncs->glGenFramebuffers(1, &fbo);
            m_fboGuard = new QOpenGLSharedResourceGuard(ctx, fbo, freeFramebufferFunc);
        }

        // Bind the FBO to both the GL_READ_FRAMEBUFFER? and GL_DRAW_FRAMEBUFFER targets
        m_coreFuncs->glBindFramebuffer(GL_FRAMEBUFFER, m_fboGuard->id());

        // Bind the old texture to GL_COLOR_ATTACHMENT0
        m_coreFuncs->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                        GL_TEXTURE_2D, oldTexture, 0);

        // Bind the new texture to GL_COLOR_ATTACHMENT1
        m_coreFuncs->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                                        GL_TEXTURE_2D, texInfo->texture, 0);

        // Set the source and destination buffers
        m_coreFuncs->glReadBuffer(GL_COLOR_ATTACHMENT0);
        m_coreFuncs->glDrawBuffer(GL_COLOR_ATTACHMENT1);

        // Do the blit
        m_coreFuncs->glBlitFramebuffer(0, 0, oldWidth, oldHeight,
                                   0, 0, oldWidth, oldHeight,
                                   GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // Reset the default framebuffer
        QOpenGLFramebufferObject::bindDefault();

        return;
    } else if (useTextureResizeWorkaround()) {
#else
    if (useTextureResizeWorkaround()) {
#endif
        GLint alignment = 4; // default value
        m_funcs->glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        m_funcs->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

#if !defined(QT_OPENGL_ES_2)
        const GLenum format = isCoreProfile() ? GL_RED : GL_ALPHA;
#else
        const GLenum format = GL_ALPHA;
#endif

        if (useTextureUploadWorkaround()) {
            for (int i = 0; i < texInfo->image.height(); ++i) {
                m_funcs->glTexSubImage2D(GL_TEXTURE_2D, 0,
                                         0, i, oldWidth, 1,
                                         format, GL_UNSIGNED_BYTE,
                                         texInfo->image.scanLine(i));
            }
        } else {
            m_funcs->glTexSubImage2D(GL_TEXTURE_2D, 0,
                                     0, 0, oldWidth, oldHeight,
                                     format, GL_UNSIGNED_BYTE,
                                     texInfo->image.constBits());
        }

        m_funcs->glPixelStorei(GL_UNPACK_ALIGNMENT, alignment); // restore to previous value

        texInfo->image = texInfo->image.copy(0, 0, width, height);
        m_funcs->glDeleteTextures(1, &oldTexture);
        return;
    }

    if (!m_blitProgram)
        createBlitProgram();

    Q_ASSERT(m_blitProgram);

    if (!m_fboGuard) {
        GLuint fbo;
        m_funcs->glGenFramebuffers(1, &fbo);
        m_fboGuard = new QOpenGLSharedResourceGuard(ctx, fbo, freeFramebufferFunc);
    }
    m_funcs->glBindFramebuffer(GL_FRAMEBUFFER, m_fboGuard->id());

    GLuint tmp_texture;
    m_funcs->glGenTextures(1, &tmp_texture);
    m_funcs->glBindTexture(GL_TEXTURE_2D, tmp_texture);
    m_funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    m_funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    m_funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if !defined(QT_OPENGL_ES_2)
    if (!ctx->isOpenGLES())
        m_funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
#endif
    m_funcs->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, oldWidth, oldHeight, 0,
                          GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    m_funcs->glBindTexture(GL_TEXTURE_2D, 0);
    m_funcs->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                    GL_TEXTURE_2D, tmp_texture, 0);

    m_funcs->glActiveTexture(GL_TEXTURE0);
    m_funcs->glBindTexture(GL_TEXTURE_2D, oldTexture);

    // save current render states
    GLboolean stencilTestEnabled;
    GLboolean depthTestEnabled;
    GLboolean scissorTestEnabled;
    GLboolean blendEnabled;
    GLint viewport[4];
    GLint oldProgram;
    m_funcs->glGetBooleanv(GL_STENCIL_TEST, &stencilTestEnabled);
    m_funcs->glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
    m_funcs->glGetBooleanv(GL_SCISSOR_TEST, &scissorTestEnabled);
    m_funcs->glGetBooleanv(GL_BLEND, &blendEnabled);
    m_funcs->glGetIntegerv(GL_VIEWPORT, &viewport[0]);
    m_funcs->glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);

    m_funcs->glDisable(GL_STENCIL_TEST);
    m_funcs->glDisable(GL_DEPTH_TEST);
    m_funcs->glDisable(GL_SCISSOR_TEST);
    m_funcs->glDisable(GL_BLEND);

    m_funcs->glViewport(0, 0, oldWidth, oldHeight);

    const bool vaoInit = m_vao.isCreated();
    if (isCoreProfile()) {
        if ( !vaoInit )
            m_vao.create();
        m_vao.bind();
    }
    m_blitProgram->bind();
    if (!vaoInit || !isCoreProfile()) {
        m_blitBuffer.bind();

        m_blitProgram->enableAttributeArray(int(QT_VERTEX_COORDS_ATTR));
        m_blitProgram->enableAttributeArray(int(QT_TEXTURE_COORDS_ATTR));
        m_blitProgram->setAttributeBuffer(int(QT_VERTEX_COORDS_ATTR), GL_FLOAT, 0, 2);
        m_blitProgram->setAttributeBuffer(int(QT_TEXTURE_COORDS_ATTR), GL_FLOAT, 32, 2);
    }
    m_blitProgram->disableAttributeArray(int(QT_OPACITY_ATTR));
    m_blitProgram->setUniformValue("imageTexture", GLuint(0));

    m_funcs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    m_funcs->glBindTexture(GL_TEXTURE_2D, texInfo->texture);

    if (useTextureUploadWorkaround()) {
        for (int i = 0; i < oldHeight; ++i)
            m_funcs->glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, i, 0, i, oldWidth, 1);
    } else {
        m_funcs->glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, oldWidth, oldHeight);
    }

    m_funcs->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_RENDERBUFFER, 0);
    m_funcs->glDeleteTextures(1, &tmp_texture);
    m_funcs->glDeleteTextures(1, &oldTexture);

    QOpenGLFramebufferObject::bindDefault();

    // restore render states
    if (stencilTestEnabled)
        m_funcs->glEnable(GL_STENCIL_TEST);
    if (depthTestEnabled)
        m_funcs->glEnable(GL_DEPTH_TEST);
    if (scissorTestEnabled)
        m_funcs->glEnable(GL_SCISSOR_TEST);
    if (blendEnabled)
        m_funcs->glEnable(GL_BLEND);
    m_funcs->glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    m_funcs->glUseProgram(oldProgram);

    m_blitProgram->disableAttributeArray(int(QT_VERTEX_COORDS_ATTR));
    m_blitProgram->disableAttributeArray(int(QT_TEXTURE_COORDS_ATTR));
    if (isCoreProfile())
        m_vao.release();
}

bool QSGDefaultDistanceFieldGlyphCache::useTextureResizeWorkaround() const
{
    static bool set = false;
    static bool useWorkaround = false;
    if (!set) {
        QOpenGLContextPrivate *ctx_p = static_cast<QOpenGLContextPrivate *>(QOpenGLContextPrivate::get(QOpenGLContext::currentContext()));
        useWorkaround = ctx_p->workaround_brokenFBOReadBack
                || qmlUseGlyphCacheWorkaround(); // on some hardware the workaround is faster (see QTBUG-29264)
        set = true;
    }
    return useWorkaround;
}

bool QSGDefaultDistanceFieldGlyphCache::useTextureUploadWorkaround() const
{
    static bool set = false;
    static bool useWorkaround = false;
    if (!set) {
        useWorkaround = qstrcmp(reinterpret_cast<const char*>(m_funcs->glGetString(GL_RENDERER)),
                                "Mali-400 MP") == 0;
        set = true;
    }
    return useWorkaround;
}

bool QSGDefaultDistanceFieldGlyphCache::createFullSizeTextures() const
{
    return qsgPreferFullSizeGlyphCacheTextures() && glyphCount() > QT_DISTANCEFIELD_HIGHGLYPHCOUNT();
}

int QSGDefaultDistanceFieldGlyphCache::maxTextureSize() const
{
    if (!m_maxTextureSize)
        m_funcs->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);
    return m_maxTextureSize;
}

QT_END_NAMESPACE
