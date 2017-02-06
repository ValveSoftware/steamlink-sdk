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

#include "qsgdefaultrendercontext_p.h"

#include <QtGui/QOpenGLFramebufferObject>

#include <QtQuick/private/qsgbatchrenderer_p.h>
#include <QtQuick/private/qsgrenderer_p.h>
#include <QtQuick/private/qsgatlastexture_p.h>
#include <QtQuick/private/qsgdefaultdistancefieldglyphcache_p.h>
#include <QtQuick/private/qsgdistancefieldutil_p.h>

QT_BEGIN_NAMESPACE

#define QSG_RENDERCONTEXT_PROPERTY "_q_sgrendercontext"

QSGDefaultRenderContext::QSGDefaultRenderContext(QSGContext *context)
    : QSGRenderContext(context)
    , m_gl(nullptr)
    , m_depthStencilManager(nullptr)
    , m_maxTextureSize(0)
    , m_brokenIBOs(false)
    , m_serializedRender(false)
    , m_attachToGLContext(true)
    , m_atlasManager(nullptr)
{

}

/*!
    Initializes the scene graph render context with the GL context \a context. This also
    emits the ready() signal so that the QML graph can start building scene graph nodes.
 */
void QSGDefaultRenderContext::initialize(void *context)
{
    if (!m_sg)
        return;

    QOpenGLContext *openglContext = static_cast<QOpenGLContext *>(context);

    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    funcs->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);

    // Sanity check the surface format, in case it was overridden by the application
    QSurfaceFormat requested = m_sg->defaultSurfaceFormat();
    QSurfaceFormat actual = openglContext->format();
    if (requested.depthBufferSize() > 0 && actual.depthBufferSize() <= 0)
        qWarning("QSGContext::initialize: depth buffer support missing, expect rendering errors");
    if (requested.stencilBufferSize() > 0 && actual.stencilBufferSize() <= 0)
        qWarning("QSGContext::initialize: stencil buffer support missing, expect rendering errors");

    if (!m_atlasManager)
        m_atlasManager = new QSGAtlasTexture::Manager();

    Q_ASSERT_X(!m_gl, "QSGRenderContext::initialize", "already initialized!");
    m_gl = openglContext;
    if (m_attachToGLContext) {
        Q_ASSERT(!openglContext->property(QSG_RENDERCONTEXT_PROPERTY).isValid());
        openglContext->setProperty(QSG_RENDERCONTEXT_PROPERTY, QVariant::fromValue(this));
    }
    m_sg->renderContextInitialized(this);

#ifdef Q_OS_LINUX
    const char *vendor = (const char *) funcs->glGetString(GL_VENDOR);
    if (vendor && strstr(vendor, "nouveau"))
        m_brokenIBOs = true;
    const char *renderer = (const char *) funcs->glGetString(GL_RENDERER);
    if (renderer && strstr(renderer, "llvmpipe"))
        m_serializedRender = true;
    if (vendor && renderer && strstr(vendor, "Hisilicon Technologies") && strstr(renderer, "Immersion.16"))
        m_brokenIBOs = true;
#endif

    emit initialized();
}


void QSGDefaultRenderContext::invalidate()
{
    if (!m_gl)
        return;

    qDeleteAll(m_texturesToDelete);
    m_texturesToDelete.clear();

    qDeleteAll(m_textures);
    m_textures.clear();

    /* The cleanup of the atlas textures is a bit intriguing.
       As part of the cleanup in the threaded render loop, we
       do:
       1. call this function
       2. call QCoreApp::sendPostedEvents() to immediately process
          any pending deferred deletes.
       3. delete the GL context.

       As textures need the atlas manager while cleaning up, the
       manager needs to be cleaned up after the textures, so
       we post a deleteLater here at the very bottom so it gets
       deferred deleted last.

       Another alternative would be to use a QPointer in
       QSGAtlasTexture::Texture, but this seemed simpler.
     */
    m_atlasManager->invalidate();
    m_atlasManager->deleteLater();
    m_atlasManager = nullptr;

    // The following piece of code will read/write to the font engine's caches,
    // potentially from different threads. However, this is safe because this
    // code is only called from QQuickWindow's shutdown which is called
    // only when the GUI is blocked, and multiple threads will call it in
    // sequence. (see qsgdefaultglyphnode_p.cpp's init())
    for (QSet<QFontEngine *>::const_iterator it = m_fontEnginesToClean.constBegin(),
         end = m_fontEnginesToClean.constEnd(); it != end; ++it) {
        (*it)->clearGlyphCache(m_gl);
        if (!(*it)->ref.deref())
            delete *it;
    }
    m_fontEnginesToClean.clear();

    delete m_depthStencilManager;
    m_depthStencilManager = 0;

    delete m_distanceFieldCacheManager;
    m_distanceFieldCacheManager = 0;

    if (m_gl->property(QSG_RENDERCONTEXT_PROPERTY) == QVariant::fromValue(this))
        m_gl->setProperty(QSG_RENDERCONTEXT_PROPERTY, QVariant());
    m_gl = 0;

    if (m_sg)
        m_sg->renderContextInvalidated(this);
    emit invalidated();
}

static QBasicMutex qsg_framerender_mutex;

void QSGDefaultRenderContext::renderNextFrame(QSGRenderer *renderer, uint fboId)
{
    if (m_serializedRender)
        qsg_framerender_mutex.lock();

    renderer->renderScene(fboId);

    if (m_serializedRender)
        qsg_framerender_mutex.unlock();
}

/*!
    Returns a shared pointer to a depth stencil buffer that can be used with \a fbo.
*/
QSharedPointer<QSGDepthStencilBuffer> QSGDefaultRenderContext::depthStencilBufferForFbo(QOpenGLFramebufferObject *fbo)
{
    if (!m_gl)
        return QSharedPointer<QSGDepthStencilBuffer>();
    QSGDepthStencilBufferManager *manager = depthStencilBufferManager();
    QSGDepthStencilBuffer::Format format;
    format.size = fbo->size();
    format.samples = fbo->format().samples();
    format.attachments = QSGDepthStencilBuffer::DepthAttachment | QSGDepthStencilBuffer::StencilAttachment;
    QSharedPointer<QSGDepthStencilBuffer> buffer = manager->bufferForFormat(format);
    if (buffer.isNull()) {
        buffer = QSharedPointer<QSGDepthStencilBuffer>(new QSGDefaultDepthStencilBuffer(m_gl, format));
        manager->insertBuffer(buffer);
    }
    return buffer;
}

/*!
    Returns a pointer to the context's depth/stencil buffer manager. This is useful for custom
    implementations of \l depthStencilBufferForFbo().
*/
QSGDepthStencilBufferManager *QSGDefaultRenderContext::depthStencilBufferManager()
{
    if (!m_gl)
        return 0;
    if (!m_depthStencilManager)
        m_depthStencilManager = new QSGDepthStencilBufferManager(m_gl);
    return m_depthStencilManager;
}

QSGTexture *QSGDefaultRenderContext::createTexture(const QImage &image, uint flags) const
{
    bool atlas = flags & CreateTexture_Atlas;
    bool mipmap = flags & CreateTexture_Mipmap;
    bool alpha = flags & CreateTexture_Alpha;

    // The atlas implementation is only supported from the render thread and
    // does not support mipmaps.
    if (!mipmap && atlas && openglContext() && QThread::currentThread() == openglContext()->thread()) {
        QSGTexture *t = m_atlasManager->create(image, alpha);
        if (t)
            return t;
    }

    QSGPlainTexture *texture = new QSGPlainTexture();
    texture->setImage(image);
    if (texture->hasAlphaChannel() && !alpha)
        texture->setHasAlphaChannel(false);

    return texture;
}

QSGRenderer *QSGDefaultRenderContext::createRenderer()
{
    return new QSGBatchRenderer::Renderer(this);
}

/*!
    Compile \a shader, optionally using \a vertexCode and \a fragmentCode as
    replacement for the source code supplied by \a shader.

    If \a vertexCode or \a fragmentCode is supplied, the caller is responsible
    for setting up attribute bindings.

    \a material is supplied in case the implementation needs to take the
    material flags into account.
 */
void QSGDefaultRenderContext::compileShader(QSGMaterialShader *shader, QSGMaterial *material, const char *vertexCode, const char *fragmentCode)
{
    Q_UNUSED(material);
    if (vertexCode || fragmentCode) {
        Q_ASSERT_X((material->flags() & QSGMaterial::CustomCompileStep) == 0,
                   "QSGRenderContext::compile()",
                   "materials with custom compile step cannot have custom vertex/fragment code");
        QOpenGLShaderProgram *p = shader->program();
        p->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexCode ? vertexCode : shader->vertexShader());
        p->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentCode ? fragmentCode : shader->fragmentShader());
        p->link();
        if (!p->isLinked())
            qWarning() << "shader compilation failed:" << endl << p->log();
    } else {
        shader->compile();
    }
}

void QSGDefaultRenderContext::initializeShader(QSGMaterialShader *shader)
{
    shader->program()->bind();
    shader->initialize();
}

void QSGDefaultRenderContext::setAttachToGraphicsContext(bool attach)
{
    Q_ASSERT(!isValid());
    m_attachToGLContext = attach;
}

QSGDefaultRenderContext *QSGDefaultRenderContext::from(QOpenGLContext *context)
{
    return qobject_cast<QSGDefaultRenderContext *>(context->property(QSG_RENDERCONTEXT_PROPERTY).value<QObject *>());
}

QT_END_NAMESPACE


QSGDistanceFieldGlyphCache *QSGDefaultRenderContext::distanceFieldGlyphCache(const QRawFont &font)
{
    if (!m_distanceFieldCacheManager)
        m_distanceFieldCacheManager = new QSGDistanceFieldGlyphCacheManager;

    QSGDistanceFieldGlyphCache *cache = m_distanceFieldCacheManager->cache(font);
    if (!cache) {
        cache = new QSGDefaultDistanceFieldGlyphCache(m_distanceFieldCacheManager, openglContext(), font);
        m_distanceFieldCacheManager->insertCache(font, cache);
    }

    return cache;
}
