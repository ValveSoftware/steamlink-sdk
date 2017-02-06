/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

#include "canvasrenderer_p.h"
#include "teximage3d_p.h"
#include "renderjob_p.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLShader>
#include <QtGui/QOpenGLShaderProgram>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtCore/QMutexLocker>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

const int initialQueueSize = 256;
const int maxQueueSize = 1000000;

CanvasRenderer::CanvasRenderer(QObject *parent):
    QObject(parent),
    m_fboSize(0, 0),
    m_initializedSize(-1, -1),
    m_glContext(0),
    m_glContextQt(0),
    m_glContextShare(0),
    m_contextWindow(0),
    m_renderTarget(Canvas::RenderTargetOffscreenBuffer),
    m_stateStore(0),
    m_fps(0),
    m_maxSamples(0),
    m_isOpenGLES2(false),
    m_antialias(false),
    m_preserveDrawingBuffer(false),
    m_multiplyAlpha(false),
    m_alphaMultiplierProgram(0),
    m_alphaMultiplierVertexShader(0),
    m_alphaMultiplierFragmentShader(0),
    m_alphaMultiplierVertexBuffer(0),
    m_alphaMultiplierUVBuffer(0),
    m_alphaMultiplierVertexAttribute(0),
    m_alphaMultiplierUVAttribute(0),
    m_antialiasFbo(0),
    m_renderFbo(0),
    m_displayFbo(0),
    m_alphaMultiplierFbo(0),
    m_recreateFbos(false),
    m_verifyFboBinds(false),
    m_offscreenSurface(0),
    m_commandQueue(0, maxQueueSize), // command queue size will be reset when context is created.
    m_executeQueue(0),
    m_executeQueueCount(0),
    m_executeStartIndex(0),
    m_executeEndIndex(0),
    m_currentFramebufferId(0),
    m_glError(0),
    m_frameTimeMs(0),
    m_fpsFrames(0),
    m_textureFinalized(false),
    m_clearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)
{
    m_fpsTimer.start();
}

CanvasRenderer::~CanvasRenderer()
{
    shutDown();
}

void CanvasRenderer::resolveQtContext(QQuickWindow *window, const QSize &initializedSize,
                                      Canvas::RenderTarget renderTarget)
{
    m_initializedSize = initializedSize;
    m_glContextQt = window->openglContext();
    m_isOpenGLES2 = m_glContextQt->isOpenGLES();
    m_renderTarget = renderTarget;

    if (m_renderTarget != Canvas::RenderTargetOffscreenBuffer)
        m_glContext = m_glContextQt;
}

/*!
 * Creates the shared context for Canvas. On some platforms shared context must be created
 * when the 'parent' context is not active, so we set no context as current temporarily.
 * Called from the render thread.
 */
void CanvasRenderer::createContextShare()
{
    QSurfaceFormat surfaceFormat = m_glContextQt->format();
    // Some devices report wrong version, so force 2.0 on ES2
    if (m_isOpenGLES2)
        surfaceFormat.setVersion(2, 0);

    if (!m_isOpenGLES2 || surfaceFormat.majorVersion() >= 3)
        m_maxSamples = 4;
    m_glContextShare = new QOpenGLContext;
    m_glContextShare->setFormat(surfaceFormat);
    m_glContextShare->setShareContext(m_glContextQt);
    QSurface *surface = m_glContextQt->surface();
    m_glContextQt->doneCurrent();
    if (!m_glContextShare->create()) {
        qCWarning(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                               << " Failed to create share context";
    }
    if (!m_glContextQt->makeCurrent(surface)) {
        qCWarning(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                               << " Failed to make old surface current";
    }
}

void CanvasRenderer::getQtContextAttributes(CanvasContextAttributes &contextAttributes)
{
    QSurfaceFormat surfaceFormat = m_glContextQt->format();
    contextAttributes.setAlpha(surfaceFormat.alphaBufferSize() != 0);
    contextAttributes.setDepth(surfaceFormat.depthBufferSize() != 0);
    contextAttributes.setStencil(surfaceFormat.stencilBufferSize() != 0);
    contextAttributes.setAntialias(surfaceFormat.samples() != 0);
    contextAttributes.setPreserveDrawingBuffer(false);
    contextAttributes.setPremultipliedAlpha(true);
}

void CanvasRenderer::init(QQuickWindow *window, const CanvasContextAttributes &contextAttributes,
                          GLint &maxVertexAttribs, QSize &maxSize, int &contextVersion,
                          QSet<QByteArray> &extensions, bool &isCombinedDepthStencilSupported)
{
    m_antialias = contextAttributes.antialias();
    m_preserveDrawingBuffer = contextAttributes.preserveDrawingBuffer();
    m_multiplyAlpha = !contextAttributes.premultipliedAlpha() && contextAttributes.alpha();

    m_currentFramebufferId = 0;
    m_forceViewportRect = QRect();

    m_contextWindow = window;

    initializeOpenGLFunctions();

    // Get the maximum drawable size
    GLint viewportDims[2];
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewportDims);
    maxSize.setWidth(viewportDims[0]);
    maxSize.setHeight(viewportDims[1]);

    // Set the size
    if (maxSize.width() < m_initializedSize.width())
        m_initializedSize.setWidth(maxSize.width());
    if (maxSize.height() < m_initializedSize.height())
        m_initializedSize.setHeight(maxSize.height());
    setFboSize(m_initializedSize);
    m_forceViewportRect = QRect(0, 0, m_fboSize.width(), m_fboSize.height());
    glScissor(0, 0, m_fboSize.width(), m_fboSize.height());

#if !defined(QT_OPENGL_ES_2)
    if (!m_isOpenGLES2) {
        // Make it possible to change point primitive size and use textures with them in
        // the shaders. These are implicitly enabled in ES2.
        glEnable(GL_PROGRAM_POINT_SIZE);
        glEnable(GL_POINT_SPRITE);
    }
#endif

    m_commandQueue.resetQueue(initialQueueSize);
    m_executeQueue.resize(initialQueueSize);
    m_executeQueueCount = 0;
    m_executeStartIndex = 0;
    m_executeEndIndex = 0;

#if defined(Q_OS_WIN)
    // Check driver vendor. We need to do some additional checking with Intel GPUs in Windows,
    // as our FBOs can get corrupted when some unrelated Qt Quick items are
    // dynamically constructed or modified. Since this doesn't happen on any other
    // vendor's GPUs, it is likely caused by a bug in Intel drivers.
    const GLubyte *vendor = m_glContext->functions()->glGetString(GL_VENDOR);
    QByteArray vendorArray(reinterpret_cast<const char *>(vendor));
    if (vendorArray.toLower().contains("intel"))
        m_verifyFboBinds = true;
#endif
    m_glContext->functions()->glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);

    contextVersion = m_glContext->format().majorVersion();

    if (contextVersion < 3) {
        if (m_isOpenGLES2) {
            isCombinedDepthStencilSupported =
                    m_glContext->hasExtension(QByteArrayLiteral("GL_OES_packed_depth_stencil"));
        } else {
            isCombinedDepthStencilSupported =
                    m_glContext->hasExtension(QByteArrayLiteral("GL_ARB_framebuffer_object"))
                    || m_glContext->hasExtension(QByteArrayLiteral("GL_EXT_packed_depth_stencil"));
        }
    } else {
        isCombinedDepthStencilSupported = true;
    }

    extensions = m_glContext->extensions();

    if (!m_alphaMultiplierProgram) {
        m_alphaMultiplierProgram = new QOpenGLShaderProgram();
        m_alphaMultiplierVertexShader = new QOpenGLShader(QOpenGLShader::Vertex);
        m_alphaMultiplierFragmentShader = new QOpenGLShader(QOpenGLShader::Fragment);
        m_alphaMultiplierVertexShader->compileSourceCode(
                    "attribute highp vec2 aPos;"
                    "attribute highp vec2 aUV;"
                    "varying highp vec2 vUV;"
                    "void main(void) {"
                    "gl_Position = vec4(aPos, 0.0, 1.0);"
                    "vUV = aUV;"
                    "}");
        m_alphaMultiplierFragmentShader->compileSourceCode(
                    "varying highp vec2 vUV;"
                    "uniform sampler2D uSampler;"
                    "void main(void) {"
                    "lowp vec4 oc = texture2D(uSampler, vUV);"
                    "gl_FragColor = vec4(oc.rgb * oc.a, oc.a);"
                    "}");
        m_alphaMultiplierProgram->addShader(m_alphaMultiplierVertexShader);
        m_alphaMultiplierProgram->addShader(m_alphaMultiplierFragmentShader);

        if (m_alphaMultiplierProgram->bind()) {
            m_alphaMultiplierVertexAttribute =
                    GLint(m_alphaMultiplierProgram->attributeLocation("aPos"));
            m_alphaMultiplierUVAttribute =
                    GLint(m_alphaMultiplierProgram->attributeLocation("aUV"));
            m_alphaMultiplierProgram->setUniformValue("uSampler", 0);

            glGenBuffers(1, &m_alphaMultiplierVertexBuffer);
            glGenBuffers(1, &m_alphaMultiplierUVBuffer);

            glBindBuffer(GL_ARRAY_BUFFER, m_alphaMultiplierVertexBuffer);
            GLfloat vertexBuffer[] = {-1.0f, 1.0f,
                                      -1.0f, -1.0f,
                                      1.0f, 1.0f,
                                      1.0f, -1.0f};
            glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), vertexBuffer, GL_STATIC_DRAW);

            glBindBuffer(GL_ARRAY_BUFFER, m_alphaMultiplierUVBuffer);
            GLfloat uvBuffer[] = {0.0f, 1.0f,
                                  0.0f, 0.0f,
                                  1.0f, 1.0f,
                                  1.0f, 0.0f};
            glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), uvBuffer, GL_STATIC_DRAW);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
        } else {
            delete m_alphaMultiplierProgram;
            delete m_alphaMultiplierVertexShader;
            delete m_alphaMultiplierFragmentShader;
            m_alphaMultiplierProgram = 0;
            m_alphaMultiplierVertexShader = 0;
            m_alphaMultiplierFragmentShader = 0;
            m_multiplyAlpha = false;
            qCWarning(canvas3dglerrors).nospace() << "CanvasRenderer::" << __FUNCTION__
                                                  << ":Unable to initialize premultiplier shaders";
        }
    }

    if (m_renderTarget != Canvas::RenderTargetOffscreenBuffer || m_multiplyAlpha)
        m_stateStore = new GLStateStore(m_glContext, maxVertexAttribs, m_commandQueue);

    updateGlError(__FUNCTION__);
}

/*!
 * Cleans up the OpenGL resources.
 * Called from the render thread.
 */
void CanvasRenderer::shutDown()
{
    QMutexLocker locker(&m_shutdownMutex);

    if (m_glContext) {
        if (m_renderTarget == Canvas::RenderTargetOffscreenBuffer)
            m_glContext->makeCurrent(m_offscreenSurface);

        m_commandQueue.clearResourceMaps();

        deleteCommandData();
        m_executeQueue.clear();

        delete m_renderFbo;
        delete m_displayFbo;
        delete m_antialiasFbo;

        if (m_renderTarget == Canvas::RenderTargetOffscreenBuffer) {
            delete m_alphaMultiplierFbo;
            m_alphaMultiplierFbo = 0;
            glDeleteBuffers(1, &m_alphaMultiplierUVBuffer);
            glDeleteBuffers(1, &m_alphaMultiplierVertexBuffer);
            m_alphaMultiplierUVBuffer = 0;
            m_alphaMultiplierVertexBuffer = 0;
            delete m_alphaMultiplierProgram;
            delete m_alphaMultiplierVertexShader;
            delete m_alphaMultiplierFragmentShader;
            m_alphaMultiplierProgram = 0;
            m_alphaMultiplierVertexShader = 0;
            m_alphaMultiplierFragmentShader = 0;

            m_glContext->doneCurrent();
            delete m_glContext;
        }

        m_renderFbo = 0;
        m_displayFbo = 0;
        m_antialiasFbo = 0;

        // m_offscreenSurface is owned by main thread, as on some platforms that is required.
        if (m_offscreenSurface) {
            m_offscreenSurface->deleteLater();
            m_offscreenSurface = 0;
        }

        m_currentFramebufferId = 0;
        m_forceViewportRect = QRect();

        delete m_stateStore;
        m_stateStore = 0;

        m_glContext = 0;
    }

    delete m_glContextShare;

    m_glContextQt = 0;
    m_glContextShare = 0;

    m_fps = 0;
}

/*!
 * Create the context and offscreen surface using current context attributes.
 * Called from the GUI thread.
*/
bool CanvasRenderer::createContext(QQuickWindow *window,
                                   const CanvasContextAttributes &contextAttributes,
                                   GLint &maxVertexAttribs, QSize &maxSize,
                                   int &contextVersion, QSet<QByteArray> &extensions,
                                   bool &isCombinedDepthStencilSupported)
{
    // Initialize the swap buffer chain
    if (contextAttributes.depth() && contextAttributes.stencil() && !contextAttributes.antialias())
        m_fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    else if (contextAttributes.depth() && !contextAttributes.antialias())
        m_fboFormat.setAttachment(QOpenGLFramebufferObject::Depth);
    else if (contextAttributes.stencil() && !contextAttributes.antialias())
        m_fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    else
        m_fboFormat.setAttachment(QOpenGLFramebufferObject::NoAttachment);

    if (contextAttributes.antialias()) {
        m_antialiasFboFormat.setSamples(m_maxSamples);

        if (m_antialiasFboFormat.samples() != m_maxSamples) {
            qCWarning(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                                   << " Failed to use " << m_maxSamples
                                                   << " will use "
                                                   << m_antialiasFboFormat.samples();
        }

        if (contextAttributes.depth() && contextAttributes.stencil())
            m_antialiasFboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        else if (contextAttributes.depth())
            m_antialiasFboFormat.setAttachment(QOpenGLFramebufferObject::Depth);
        else
            m_antialiasFboFormat.setAttachment(QOpenGLFramebufferObject::NoAttachment);
    }

    // Create the offscreen surface
    QSurfaceFormat surfaceFormat = m_glContextShare->format();

    if (m_isOpenGLES2) {
        // Some devices report wrong version, so force 2.0 on ES2
        surfaceFormat.setVersion(2, 0);
    } else {
        surfaceFormat.setSwapBehavior(QSurfaceFormat::SingleBuffer);
        surfaceFormat.setSwapInterval(0);
    }

    if (contextAttributes.alpha())
        surfaceFormat.setAlphaBufferSize(8);
    else
        surfaceFormat.setAlphaBufferSize(0);

    if (contextAttributes.depth())
        surfaceFormat.setDepthBufferSize(24);
    else
        surfaceFormat.setDepthBufferSize(0);

    if (contextAttributes.stencil())
        surfaceFormat.setStencilBufferSize(8);
    else
        surfaceFormat.setStencilBufferSize(0);

    if (contextAttributes.antialias())
        surfaceFormat.setSamples(m_antialiasFboFormat.samples());

    QThread *contextThread = m_glContextShare->thread();

    qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                         << " Creating QOpenGLContext with surfaceFormat :"
                                         << surfaceFormat;
    m_glContext = new QOpenGLContext();
    m_glContext->setFormat(surfaceFormat);

    // Share with m_glContextShare which in turn shares with m_glContextQt.
    // In case of threaded rendering both of these live on the render
    // thread of the scenegraph. m_glContextQt may be current on that
    // thread at this point, which would fail the context creation with
    // some drivers. Hence the need for m_glContextShare.
    m_glContext->setShareContext(m_glContextShare);
    if (!m_glContext->create()) {
        qCWarning(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                               << " Failed to create OpenGL context for FBO";
        return false;
    }

    m_offscreenSurface = new QOffscreenSurface();
    m_offscreenSurface->setFormat(m_glContext->format());
    m_offscreenSurface->create();

    if (!m_glContext->makeCurrent(m_offscreenSurface)) {
        qCWarning(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                               << " Failed to make offscreen surface current";
        return false;
    }

    init(window, contextAttributes, maxVertexAttribs, maxSize, contextVersion, extensions,
         isCombinedDepthStencilSupported);

    if (m_glContext->thread() != contextThread) {
        m_glContext->doneCurrent();
        m_glContext->moveToThread(contextThread);
    }

    return true;
}

/*!
 * Renders the command queue in the correct context.
 * Called from the render thread.
 */
void CanvasRenderer::render()
{
    // If rendering to background, we need to clear the framebuffer before rendering the frame.
    // When rendering to foreground, we cannot clear the color buffer here, as that would erase
    // the rest of the scene, but we do need to clear depth and stencil buffers even in that case.
    if (m_renderTarget != Canvas::RenderTargetOffscreenBuffer) {
        if (m_renderTarget == Canvas::RenderTargetForeground)
            m_clearMask &= ~GL_COLOR_BUFFER_BIT;
        clearBackground();
    }

    // Skip render if there is no context or nothing to render
    if (m_glContext && m_executeQueueCount) {
        // Update tracked quick item textures
        int providerCount = m_commandQueue.providerCache().size();
        if (providerCount) {
            QMap<GLint, CanvasGlCommandQueue::ProviderCacheItem *>::iterator i =
                    m_commandQueue.providerCache().begin();
            while (i != m_commandQueue.providerCache().end()) {
                CanvasGlCommandQueue::ProviderCacheItem *cacheItem = i.value();
                QSGTextureProvider *texProvider = cacheItem->providerPtr.data();
                GLint id = i.key();
                QMap<GLint, CanvasGlCommandQueue::ProviderCacheItem *>::iterator prev = i;
                i++;

                if (texProvider) {
                    QSGDynamicTexture *texture =
                            qobject_cast<QSGDynamicTexture *>(texProvider->texture());
                    if (texture) {
                        texture->updateTexture();
                        int textureId = texture->textureId();
                        int currentTextureId = m_commandQueue.getGlId(id);
                        if (textureId && textureId != currentTextureId) {
                            m_commandQueue.setGlIdToMap(
                                        id, textureId,
                                        CanvasGlCommandQueue::internalClearQuickItemAsTexture);
                            emit textureIdResolved(cacheItem->quickItem);
                        }
                    }
                } else {
                    // Clean obsolete providers off the cache
                    m_commandQueue.providerCache().erase(prev);
                    delete cacheItem;
                }
            }
        }

        // Change to canvas context, if needed
        QOpenGLContext *oldContext(0);
        QSurface *oldSurface(0);
        if (m_renderTarget == Canvas::RenderTargetOffscreenBuffer) {
            oldContext = QOpenGLContext::currentContext();
            oldSurface = oldContext->surface();
            makeCanvasContextCurrent();
        }

        executeCommandQueue();

        // Restore Qt context
        if (m_renderTarget != Canvas::RenderTargetOffscreenBuffer) {
            resetQtOpenGLState();
        } else {
            if (!oldContext->makeCurrent(oldSurface)) {
                qCWarning(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                                       << " Failed to make old surface current";
            }
        }

        // Calculate the fps
        if (m_textureFinalized) {
            m_textureFinalized = false;
            ++m_fpsFrames;
            if (m_fpsTimer.elapsed() >= 500) {
                qreal avgtime = qreal(m_fpsTimer.restart()) / qreal(m_fpsFrames);
                uint avgFps = qRound(1000.0 / avgtime);
                if (avgFps != m_fps) {
                    m_fps = avgFps;
                    emit fpsChanged(avgFps);
                }
                m_fpsFrames = 0;
            }
        }
    }
}

/*!
 * Clears the render buffer to default values.
 * Called from the render thread and only when not rendering to offscreen buffer.
 */
void CanvasRenderer::clearBackground()
{
    if (m_glContext && m_clearMask) {
        if (m_clearMask & GL_COLOR_BUFFER_BIT) {
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        }
        if (m_clearMask & GL_DEPTH_BUFFER_BIT) {
            glClearDepthf(1.0f);
        }
        if (m_clearMask & GL_STENCIL_BUFFER_BIT) {
            glClearStencil(0);
        }
        glClear(m_clearMask);
    }
}

/*!
 * Fetches the GL errors and updates m_glError.
 * If onlyWhenLogging is true, errors are only updated when GL error logging is enabled
 * Called from the render thread. Context must be valid.
 * Returns true, if there are new errors.
 */
bool CanvasRenderer::updateGlError(const char *funcName)
{
    bool newErrors = false;
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        newErrors = true;
        // Merge any GL errors with internal errors so that we don't lose them
        switch (err) {
        case GL_INVALID_ENUM:
            m_glError |= CanvasContext::CANVAS_INVALID_ENUM;
            break;
        case GL_INVALID_VALUE:
            m_glError |= CanvasContext::CANVAS_INVALID_VALUE;
            break;
        case GL_INVALID_OPERATION:
            m_glError |= CanvasContext::CANVAS_INVALID_OPERATION;
            break;
        case GL_OUT_OF_MEMORY:
            m_glError |= CanvasContext::CANVAS_OUT_OF_MEMORY;
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            m_glError |= CanvasContext::CANVAS_INVALID_FRAMEBUFFER_OPERATION;
            break;
#if defined(GL_STACK_OVERFLOW)
        case GL_STACK_OVERFLOW:
            qCWarning(canvas3dglerrors).nospace() << "CanvasRenderer::" << __FUNCTION__
                                                  << ":GL_STACK_OVERFLOW error ignored";
            break;
#endif
#if defined(GL_STACK_UNDERFLOW)
        case GL_STACK_UNDERFLOW:
            qCWarning(canvas3dglerrors).nospace() << "CanvasRenderer::" << __FUNCTION__
                                                  << ": GL_CANVAS_STACK_UNDERFLOW error ignored";
            break;
#endif
        default:
            break;
        }
        qCWarning(canvas3dglerrors).nospace() << "CanvasRenderer::" << funcName
                                              << ": OpenGL ERROR: "
                                              << err;
    }

    return newErrors;
}

/*!
 * Applies alpha value to other color components to produce a permultiplied alpha output
 * into m_alphaMultiplierFbo.
 */
void CanvasRenderer::multiplyAlpha()
{
    GLuint texId = m_renderFbo->texture();
    m_alphaMultiplierFbo->bind();
    m_alphaMultiplierProgram->bind();

    glActiveTexture(GL_TEXTURE0);

    glEnableVertexAttribArray(m_alphaMultiplierVertexAttribute);
    glBindBuffer(GL_ARRAY_BUFFER, m_alphaMultiplierVertexBuffer);
    glVertexAttribPointer(m_alphaMultiplierVertexAttribute, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glEnableVertexAttribArray(m_alphaMultiplierUVAttribute);
    glBindBuffer(GL_ARRAY_BUFFER, m_alphaMultiplierUVBuffer);
    glVertexAttribPointer(m_alphaMultiplierUVAttribute, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    glColorMask(true, true, true, true);
    glClearColor(0, 0, 0, 0);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glViewport(m_forceViewportRect.x(), m_forceViewportRect.y(),
               m_forceViewportRect.width(), m_forceViewportRect.height());
    glBindTexture(GL_TEXTURE_2D, texId);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    restoreCanvasOpenGLState();
}

/*!
 * Sets framebuffer size.
 * Called from the GUI thread.
 */
void CanvasRenderer::setFboSize(const QSize &fboSize)
{
    qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                         << "(setFboSize:" << fboSize
                                         << ")";

    if (m_fboSize == fboSize && m_renderFbo != 0)
        return;

    m_fboSize = fboSize;
    if (m_fboSize.width() > 0 && m_fboSize.height() > 0)
        m_recreateFbos = true;
    else
        m_recreateFbos = false;
}

/*!
 * Creates framebuffers.
 * This method is called from the render thread and in the correct context.
 */
void CanvasRenderer::createFBOs()
{
    qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__ << "()";

    if (!m_glContext) {
        qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                             << " No OpenGL context created, returning";
        return;
    }

    if (!m_offscreenSurface) {
        qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                             << " No offscreen surface created, returning";
        return;
    }

    // Ensure context is current
    if (!m_glContext->makeCurrent(m_offscreenSurface)) {
        qCWarning(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                               << " Failed to make offscreen surface current";
        return;
    }

    // Store current clear color and the bound texture
    GLint texBinding2D;
    GLfloat clearColor[4];
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texBinding2D);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);

    // Store existing FBOs, don't delete before we've created the new ones
    // so that we get fresh texture and FBO id's for the newly created objects.
    QOpenGLFramebufferObject *displayFBO = m_displayFbo;
    QOpenGLFramebufferObject *renderFbo = m_renderFbo;
    QOpenGLFramebufferObject *antialiasFbo = m_antialiasFbo;
    QOpenGLFramebufferObject *alphaFbo = m_alphaMultiplierFbo;


    // Initialize the scissor box the first time we create FBOs
    if (!m_displayFbo)
        glScissor(0, 0, m_fboSize.width(), m_fboSize.height());

    // Create FBOs
    qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                         << " Creating front and back FBO's with"
                                         << " attachment format:" << m_fboFormat.attachment()
                                         << " and size:" << m_fboSize;
    m_displayFbo = new QOpenGLFramebufferObject(m_fboSize.width(),
                                                m_fboSize.height(),
                                                m_fboFormat);
    m_renderFbo  = new QOpenGLFramebufferObject(m_fboSize.width(),
                                                m_fboSize.height(),
                                                m_fboFormat);
    if (m_multiplyAlpha) {
        m_alphaMultiplierFbo  = new QOpenGLFramebufferObject(m_fboSize.width(),
                                                             m_fboSize.height(),
                                                             m_fboFormat);
    }

    // Clear the FBOs to prevent random junk appearing on the screen
    // Note: Viewport may not be changed automatically
    glClearColor(0,0,0,0);
    m_displayFbo->bind();
    glClear(GL_COLOR_BUFFER_BIT);
    m_renderFbo->bind();
    glClear(GL_COLOR_BUFFER_BIT);

    qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                         << " Render FBO handle:" << m_renderFbo->handle()
                                         << " isValid:" << m_renderFbo->isValid();

    if (m_antialias) {
        qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                             << "Creating MSAA buffer with "
                                             << m_antialiasFboFormat.samples() << " samples "
                                             << " and attachment format of "
                                             << m_antialiasFboFormat.attachment();
        m_antialiasFbo = new QOpenGLFramebufferObject(
                    m_fboSize.width(),
                    m_fboSize.height(),
                    m_antialiasFboFormat);
        qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                             << " Antialias FBO handle:" << m_antialiasFbo->handle()
                                             << " isValid:" << m_antialiasFbo->isValid();
        m_antialiasFbo->bind();
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // FBO ids and texture id's have been generated, we can now free the existing ones.
    delete displayFBO;
    delete renderFbo;
    delete antialiasFbo;
    delete alphaFbo;

    // Store the correct texture binding
    glBindTexture(GL_TEXTURE_2D, texBinding2D);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

    if (m_currentFramebufferId)
        bindCurrentRenderTarget();

    if (canvas3dglerrors().isDebugEnabled())
        updateGlError(__FUNCTION__);
}

/*!
 * Moves commands from command queue to execution queue.
 * Called from the render thread when the GUI thread is blocked.
 */
void CanvasRenderer::transferCommands()
{
    if (m_glContext) {
        const int count = m_commandQueue.queuedCount();
        if (count > m_executeQueue.size())
            m_executeQueue.resize(count);
        if (m_renderTarget == Canvas::RenderTargetOffscreenBuffer) {
            m_executeQueueCount = count;
            m_commandQueue.transferCommands(m_executeQueue);
        } else {
            m_clearMask = m_commandQueue.resetClearMask();
            // Use previous frame count and indices if no new commands, otherwise reset values
            if (count) {
                deleteCommandData();
                m_executeQueueCount = count;
                m_executeStartIndex = 0;
                m_executeEndIndex = 0;
                m_commandQueue.transferCommands(m_executeQueue);
            }
        }
    }
}

/*!
 * Bind the correct target framebuffer for rendering.
 * This method is called from the render thread and in the correct context.
 */
void CanvasRenderer::bindCurrentRenderTarget()
{
    qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__ << "()";

    if (m_currentFramebufferId == 0) {
        if (m_renderTarget != Canvas::RenderTargetOffscreenBuffer) {
            QOpenGLFramebufferObject::bindDefault();
        } else {
            if (m_verifyFboBinds)
                updateGlError(__FUNCTION__);

            // Bind default framebuffer
            if (m_antialiasFbo) {
                qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                                     << " Binding current FBO to antialias FBO:"
                                                     << m_antialiasFbo->handle();
                m_antialiasFbo->bind();
            } else {
                qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                                     << " Binding current FBO to render FBO:"
                                                     << m_renderFbo->handle();
                m_renderFbo->bind();
            }

            if (m_verifyFboBinds) {
                // Silently ignore sudden binding errors with our default framebuffers, as they
                // are likely result of a GPU driver bug.
                GLenum err;
                while ((err = glGetError()) != GL_NO_ERROR)
                    m_recreateFbos = true;
                if (m_recreateFbos) {
                    m_verifyFboBinds = false; // To avoid infinite loops
                    createFBOs();
                    m_recreateFbos = false;
                    bindCurrentRenderTarget();
                    m_verifyFboBinds = true;
                }
            }
        }
    } else {
        qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                             << " Binding current FBO to current Context3D FBO:"
                                             << m_currentFramebufferId;
        glBindFramebuffer(GL_FRAMEBUFFER, m_currentFramebufferId);
    }
    if (canvas3dglerrors().isDebugEnabled())
        updateGlError(__FUNCTION__);
}

/*!
 * Makes the canvas context current, if it exists.
 * Called from the render thread.
 */
void CanvasRenderer::makeCanvasContextCurrent()
{
    if (!m_glContext)
        return;

    if (!m_glContext->makeCurrent(m_offscreenSurface)) {
        qCWarning(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                               << " Failed to make offscreen surface current";
    }
}

/*!
 * Executes the command queue.
 * This method is called from the render thread and in the correct context.
 */
void CanvasRenderer::executeCommandQueue()
{
    if (!m_glContext)
        return;

    if (m_renderTarget == Canvas::RenderTargetOffscreenBuffer && m_recreateFbos) {
        createFBOs();
        m_recreateFbos = false;
    }

    qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__;

    // Bind the correct render target FBO
    bindCurrentRenderTarget();

    // Ensure we have correct viewport set in the context
    glViewport(m_forceViewportRect.x(), m_forceViewportRect.y(),
               m_forceViewportRect.width(), m_forceViewportRect.height());
    qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                         << " Viewport set to " << m_forceViewportRect;

    if (m_renderTarget != Canvas::RenderTargetOffscreenBuffer)
        restoreCanvasOpenGLState();

    GLuint u1(0); // A generic variable used in the following switch statement

    const int executeEndIndex = m_executeEndIndex ? m_executeEndIndex : m_executeQueueCount;

    bool logGlErrors = canvas3dglerrors().isDebugEnabled();

    // Execute the execution queue
    for (int i = m_executeStartIndex; i < executeEndIndex; i++) {
        GlCommand &command = m_executeQueue[i];
        switch (command.id) {
        case CanvasGlCommandQueue::glActiveTexture: {
            glActiveTexture(GLenum(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glAttachShader: {
            QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
            QOpenGLShader *shader = m_commandQueue.getShader(command.i2);
            bool success = false;
            if (program && shader)
                success = program->addShader(shader);
            if (!success) {
                m_glError |= CanvasContext::CANVAS_INVALID_VALUE;
                qCWarning(canvas3dglerrors).nospace() << "CanvasRenderer::" << __FUNCTION__
                                                      << ": Failed to attach shader "
                                                      << shader << " to program " << program;
            }
            break;
        }
        case CanvasGlCommandQueue::glBindAttribLocation: {
            QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
            if (program) {
                program->bindAttributeLocation(*command.data, command.i2);
            } else {
                m_glError |= CanvasContext::CANVAS_INVALID_VALUE;
                qCWarning(canvas3dglerrors).nospace() << "CanvasRenderer::" << __FUNCTION__
                                                      << ": Failed to bind attribute ("
                                                      << command.data << ") to program " << program;
            }
            break;
        }
        case CanvasGlCommandQueue::glBindBuffer: {
            glBindBuffer(GLenum(command.i1), m_commandQueue.getGlId(command.i2));
            break;
        }
        case CanvasGlCommandQueue::glBindFramebuffer: {
            m_currentFramebufferId = m_commandQueue.getGlId(command.i1);
            // Special logic for frame buffer binding
            bindCurrentRenderTarget();
            break;
        }
        case CanvasGlCommandQueue::glBindRenderbuffer: {
            glBindRenderbuffer(GLenum(command.i1), m_commandQueue.getGlId(command.i2));
            break;
        }
        case CanvasGlCommandQueue::glBindTexture: {
            glBindTexture(GLenum(command.i1), m_commandQueue.getGlId(command.i2));
            break;
        }
        case CanvasGlCommandQueue::glBlendColor: {
            glBlendColor(GLclampf(command.f1), GLclampf(command.f2),
                         GLclampf(command.f3), GLclampf(command.f4));
            break;
        }
        case CanvasGlCommandQueue::glBlendEquation: {
            glBlendEquation(GLenum(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glBlendEquationSeparate: {
            glBlendEquationSeparate(GLenum(command.i1), GLenum(command.i2));
            break;
        }
        case CanvasGlCommandQueue::glBlendFunc: {
            glBlendFunc(GLenum(command.i1), GLenum(command.i2));
            break;
        }
        case CanvasGlCommandQueue::glBlendFuncSeparate: {
            glBlendFuncSeparate(GLenum(command.i1), GLenum(command.i2),
                                GLenum(command.i3), GLenum(command.i4));
            break;
        }
        case CanvasGlCommandQueue::glBufferData: {
            const void *data = 0;
            if (command.data)
                data = static_cast<const void *>(command.data->constData());

            glBufferData(GLenum(command.i1), qopengl_GLsizeiptr(command.i2),
                         data, GLenum(command.i3));

            break;
        }
        case CanvasGlCommandQueue::glBufferSubData: {
            const void *data = static_cast<const void *>(command.data->constData());
            glBufferSubData(GLenum(command.i1), qopengl_GLintptr(command.i2),
                            qopengl_GLsizeiptr(command.data->size()), data);
            break;
        }
        case CanvasGlCommandQueue::glClear: {
            glClear(GLbitfield(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glClearColor: {
            glClearColor(GLclampf(command.f1), GLclampf(command.f2),
                         GLclampf(command.f3), GLclampf(command.f4));
            break;
        }
        case CanvasGlCommandQueue::glClearDepthf: {
            glClearDepthf(GLclampf(command.f1));
            break;
        }
        case CanvasGlCommandQueue::glClearStencil: {
            glClearStencil(command.i1);
            break;
        }
        case CanvasGlCommandQueue::glColorMask: {
            glColorMask(GLboolean(command.i1), GLboolean(command.i2),
                        GLboolean(command.i3), GLboolean(command.i4));
            break;
        }
        case CanvasGlCommandQueue::glCompileShader: {
            QOpenGLShader *shader = m_commandQueue.getShader(command.i1);
            bool success = false;
            if (shader)
                success = shader->compileSourceCode(*command.data);
            if (!success) {
                m_glError |= CanvasContext::CANVAS_INVALID_VALUE;
                qCWarning(canvas3dglerrors).nospace() << "CanvasRenderer::" << __FUNCTION__
                                                      << ": Failed to compile shader "
                                                      << shader;
            }
            break;
        }
        case CanvasGlCommandQueue::glCompressedTexImage2D: {
            const void *data = static_cast<const void *>(command.data->constData());
            glCompressedTexImage2D(GLenum(command.i1), command.i2, GLenum(command.i3),
                                   GLsizei(command.i4), GLsizei(command.i5), command.i6,
                                   GLsizei(command.data->size()), data);
            break;
        }
        case CanvasGlCommandQueue::glCompressedTexSubImage2D: {
            const void *data = static_cast<const void *>(command.data->constData());
            glCompressedTexSubImage2D(GLenum(command.i1), command.i2, command.i3, command.i4,
                                      GLsizei(command.i5), GLsizei(command.i6), GLenum(command.i7),
                                      GLsizei(command.data->size()), data);
            break;
        }
        case CanvasGlCommandQueue::glCopyTexImage2D: {
            glCopyTexImage2D(GLenum(command.i1), command.i2, GLenum(command.i3), command.i4,
                             command.i5, GLsizei(command.i6), GLsizei(command.i7), command.i8);
            break;
        }
        case CanvasGlCommandQueue::glCopyTexSubImage2D: {
            glCopyTexSubImage2D(GLenum(command.i1), command.i2, command.i3, command.i4, command.i5,
                                command.i6, GLsizei(command.i7), GLsizei(command.i8));
            break;
        }
        case CanvasGlCommandQueue::glCreateProgram: {
            QOpenGLShaderProgram *program = new QOpenGLShaderProgram();
            m_commandQueue.setProgramToMap(command.i1, program);
            break;
        }
        case CanvasGlCommandQueue::glCreateShader: {
            QOpenGLShader::ShaderType type = (command.i1 == GL_VERTEX_SHADER)
                    ? QOpenGLShader::Vertex : QOpenGLShader::Fragment;
            QOpenGLShader *shader = new QOpenGLShader(type);
            m_commandQueue.setShaderToMap(command.i2, shader);
            break;
        }
        case CanvasGlCommandQueue::glCullFace: {
            glCullFace(GLenum(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glDeleteBuffers: {
            u1 = m_commandQueue.takeSingleIdParam(command);
            glDeleteBuffers(1, &u1);
            break;
        }
        case CanvasGlCommandQueue::glDeleteFramebuffers: {
            u1 = m_commandQueue.takeSingleIdParam(command);
            glDeleteFramebuffers(1, &u1);
            break;
        }
        case CanvasGlCommandQueue::glDeleteProgram: {
            delete m_commandQueue.takeProgramFromMap(command.i1);
            break;
        }
        case CanvasGlCommandQueue::glDeleteRenderbuffers: {
            u1 = m_commandQueue.takeSingleIdParam(command);
            glDeleteRenderbuffers(1, &u1);
            break;
        }
        case CanvasGlCommandQueue::glDeleteShader: {
            delete m_commandQueue.takeShaderFromMap(command.i1);
            break;
        }
        case CanvasGlCommandQueue::glDeleteTextures: {
            u1 = m_commandQueue.takeSingleIdParam(command);
            glDeleteTextures(1, &u1);
            break;
        }
        case CanvasGlCommandQueue::glDepthFunc: {
            glDepthFunc(GLenum(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glDepthMask: {
            glDepthMask(GLboolean(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glDepthRangef: {
            glDepthRangef(GLclampf(command.f1), GLclampf(command.f2));
            break;
        }
        case CanvasGlCommandQueue::glDetachShader: {
            QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
            QOpenGLShader *shader = m_commandQueue.getShader(command.i2);
            if (program && shader) {
                program->removeShader(shader);
            } else {
                m_glError |= CanvasContext::CANVAS_INVALID_VALUE;
                qCWarning(canvas3dglerrors).nospace() << "CanvasRenderer::" << __FUNCTION__
                                                      << ": Failed to detach shader "
                                                      << shader << " from program " << program;
            }
            break;
        }
        case CanvasGlCommandQueue::glDisable: {
            glDisable(GLenum(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glDisableVertexAttribArray: {
            glDisableVertexAttribArray(GLuint(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glDrawArrays: {
            glDrawArrays(GLenum(command.i1), command.i2, GLsizei(command.i3));
            break;
        }
        case CanvasGlCommandQueue::glDrawElements: {
            glDrawElements(GLenum(command.i1), GLsizei(command.i2), GLenum(command.i3),
                           reinterpret_cast<GLvoid *>(command.i4));
            break;
        }
        case CanvasGlCommandQueue::glEnable: {
            glEnable(GLenum(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glEnableVertexAttribArray: {
            glEnableVertexAttribArray(GLuint(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glFlush: {
            glFlush();
            break;
        }
        case CanvasGlCommandQueue::glFramebufferRenderbuffer: {
            glFramebufferRenderbuffer(GLenum(command.i1), GLenum(command.i2), GLenum(command.i3),
                                      m_commandQueue.getGlId(command.i4));
            break;
        }
        case CanvasGlCommandQueue::glFramebufferTexture2D: {
            glFramebufferTexture2D(GLenum(command.i1), GLenum(command.i2), GLenum(command.i3),
                                   m_commandQueue.getGlId(command.i4), command.i5);
            break;
        }
        case CanvasGlCommandQueue::glFrontFace: {
            glFrontFace(GLenum(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glGenBuffers: {
            glGenBuffers(1, &u1);
            m_commandQueue.handleGenerateCommand(command, u1);
            break;
        }
        case CanvasGlCommandQueue::glGenerateMipmap: {
            glGenerateMipmap(GLenum(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glGenFramebuffers: {
            glGenFramebuffers(1, &u1);
            m_commandQueue.handleGenerateCommand(command, u1);
            break;
        }
        case CanvasGlCommandQueue::glGenRenderbuffers: {
            glGenRenderbuffers(1, &u1);
            m_commandQueue.handleGenerateCommand(command, u1);
            break;
        }
        case CanvasGlCommandQueue::glGenTextures: {
            glGenTextures(1, &u1);
            m_commandQueue.handleGenerateCommand(command, u1);
            break;
        }
        case CanvasGlCommandQueue::glGetUniformLocation: {
            QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i2);
            const GLuint glLocation = program ? program->uniformLocation(*command.data)
                                              : GLuint(-1);
            // command.i1 is the location id, so use standard handler function to generate mapping
            m_commandQueue.handleGenerateCommand(command, glLocation);
            break;
        }
        case CanvasGlCommandQueue::glHint: {
            glHint(GLenum(command.i1), GLenum(command.i2));
            break;
        }
        case CanvasGlCommandQueue::glLineWidth: {
            glLineWidth(command.f1);
            break;
        }
        case CanvasGlCommandQueue::glLinkProgram: {
            QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
            bool success = false;
            if (program)
                success = program->link();
            if (!success) {
                m_glError |= CanvasContext::CANVAS_INVALID_VALUE;
                qCWarning(canvas3dglerrors).nospace() << "CanvasRenderer::" << __FUNCTION__
                                                      << ": Failed to link program: "
                                                      << program;
            }
            break;
        }
        case CanvasGlCommandQueue::glPixelStorei: {
            glPixelStorei(GLenum(command.i1), command.i2);
            break;
        }
        case CanvasGlCommandQueue::glPolygonOffset: {
            glPolygonOffset(command.f1, command.f2);
            break;
        }
        case CanvasGlCommandQueue::glRenderbufferStorage: {
            glRenderbufferStorage(GLenum(command.i1), GLenum(command.i2),
                                  GLsizei(command.i3), GLsizei(command.i4));
            break;
        }
        case CanvasGlCommandQueue::glSampleCoverage: {
            glSampleCoverage(GLclampf(command.f1), GLboolean(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glScissor: {
            glScissor(command.i1, command.i2, GLsizei(command.i3), GLsizei(command.i4));
            break;
        }
        case CanvasGlCommandQueue::glStencilFunc: {
            glStencilFunc(GLenum(command.i1), command.i2, GLuint(command.i3));
            break;
        }
        case CanvasGlCommandQueue::glStencilFuncSeparate: {
            glStencilFuncSeparate(GLenum(command.i1), GLenum(command.i2),
                                  command.i3, GLuint(command.i4));
            break;
        }
        case CanvasGlCommandQueue::glStencilMask: {
            glStencilMask(GLuint(command.i1));
            break;
        }
        case CanvasGlCommandQueue::glStencilMaskSeparate: {
            glStencilMaskSeparate(GLenum(command.i1), GLuint(command.i2));
            break;
        }
        case CanvasGlCommandQueue::glStencilOp: {
            glStencilOp(GLenum(command.i1), GLenum(command.i2), GLenum(command.i3));
            break;
        }
        case CanvasGlCommandQueue::glStencilOpSeparate: {
            glStencilOpSeparate(GLenum(command.i1), GLenum(command.i2),
                                GLenum(command.i3), GLenum(command.i4));
            break;
        }
        case CanvasGlCommandQueue::glTexImage2D: {
            glTexImage2D(GLenum(command.i1), command.i2, command.i3, GLsizei(command.i4),
                         GLsizei(command.i5), command.i6, GLenum(command.i7), GLenum(command.i8),
                         reinterpret_cast<const GLvoid *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glTexParameterf: {
            glTexParameterf(GLenum(command.i1), GLenum(command.i2), command.f1);
            break;
        }
        case CanvasGlCommandQueue::glTexParameteri: {
            glTexParameteri(GLenum(command.i1), GLenum(command.i2), command.i3);
            break;
        }
        case CanvasGlCommandQueue::glTexSubImage2D: {
            glTexSubImage2D(GLenum(command.i1), command.i2, command.i3, command.i4,
                            GLsizei(command.i5), GLsizei(command.i6), GLenum(command.i7),
                            GLenum(command.i8),
                            reinterpret_cast<const GLvoid *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glUniform1f: {
            glUniform1f(GLint(m_commandQueue.getGlId(command.i1)), command.f1);
            break;
        }
        case CanvasGlCommandQueue::glUniform2f: {
            glUniform2f(GLint(m_commandQueue.getGlId(command.i1)), command.f1, command.f2);
            break;
        }
        case CanvasGlCommandQueue::glUniform3f: {
            glUniform3f(GLint(m_commandQueue.getGlId(command.i1)), command.f1, command.f2,
                        command.f3);
            break;
        }
        case CanvasGlCommandQueue::glUniform4f: {
            glUniform4f(GLint(m_commandQueue.getGlId(command.i1)), command.f1, command.f2,
                        command.f3, command.f4);
            break;
        }
        case CanvasGlCommandQueue::glUniform1i: {
            glUniform1i(GLint(m_commandQueue.getGlId(command.i1)), command.i2);
            break;
        }
        case CanvasGlCommandQueue::glUniform2i: {
            glUniform2i(GLint(m_commandQueue.getGlId(command.i1)), command.i2, command.i3);
            break;
        }
        case CanvasGlCommandQueue::glUniform3i: {
            glUniform3i(GLint(m_commandQueue.getGlId(command.i1)), command.i2, command.i3,
                        command.i4);
            break;
        }
        case CanvasGlCommandQueue::glUniform4i: {
            glUniform4i(GLint(m_commandQueue.getGlId(command.i1)), command.i2, command.i3,
                        command.i4, command.i5);
            break;
        }
        case CanvasGlCommandQueue::glUniform1fv: {
            glUniform1fv(GLint(m_commandQueue.getGlId(command.i1)), GLsizei(command.i2),
                         reinterpret_cast<const GLfloat *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glUniform2fv: {
            glUniform2fv(GLint(m_commandQueue.getGlId(command.i1)), GLsizei(command.i2),
                         reinterpret_cast<const GLfloat *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glUniform3fv: {
            glUniform3fv(GLint(m_commandQueue.getGlId(command.i1)), GLsizei(command.i2),
                         reinterpret_cast<const GLfloat *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glUniform4fv: {
            glUniform4fv(GLint(m_commandQueue.getGlId(command.i1)), GLsizei(command.i2),
                         reinterpret_cast<const GLfloat *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glUniform1iv: {
            glUniform1iv(GLint(m_commandQueue.getGlId(command.i1)), GLsizei(command.i2),
                         reinterpret_cast<const GLint *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glUniform2iv: {
            glUniform2iv(GLint(m_commandQueue.getGlId(command.i1)), GLsizei(command.i2),
                         reinterpret_cast<const GLint *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glUniform3iv: {
            glUniform3iv(GLint(m_commandQueue.getGlId(command.i1)), GLsizei(command.i2),
                         reinterpret_cast<const GLint *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glUniform4iv: {
            glUniform4iv(GLint(m_commandQueue.getGlId(command.i1)), GLsizei(command.i2),
                         reinterpret_cast<const GLint *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glUniformMatrix2fv: {
            glUniformMatrix2fv(GLint(m_commandQueue.getGlId(command.i1)),
                               GLsizei(command.i2), GLboolean(command.i3),
                               reinterpret_cast<const GLfloat *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glUniformMatrix3fv: {
            glUniformMatrix3fv(GLint(m_commandQueue.getGlId(command.i1)),
                               GLsizei(command.i2), GLboolean(command.i3),
                               reinterpret_cast<const GLfloat *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glUniformMatrix4fv: {
            glUniformMatrix4fv(GLint(m_commandQueue.getGlId(command.i1)),
                               GLsizei(command.i2), GLboolean(command.i3),
                               reinterpret_cast<const GLfloat *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glUseProgram: {
            QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
            if (program) {
                glUseProgram(program->programId());
            } else {
                m_glError |= CanvasContext::CANVAS_INVALID_VALUE;
                qCWarning(canvas3dglerrors).nospace() << "CanvasRenderer::" << __FUNCTION__
                                                      << ": Failed to bind program: "
                                                      << program;
            }
            break;
        }
        case CanvasGlCommandQueue::glValidateProgram: {
            QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
            if (program) {
                glValidateProgram(program->programId());
            } else {
                m_glError |= CanvasContext::CANVAS_INVALID_VALUE;
                qCWarning(canvas3dglerrors).nospace() << "CanvasRenderer::" << __FUNCTION__
                                                      << ": Failed to validate program, "
                                                      << "invalid program id";
            }
            break;
        }
        case CanvasGlCommandQueue::glVertexAttrib1f: {
            glVertexAttrib1f(GLuint(command.i1), command.f1);
            break;
        }
        case CanvasGlCommandQueue::glVertexAttrib2f: {
            glVertexAttrib2f(GLuint(command.i1), command.f1, command.f2);
            break;
        }
        case CanvasGlCommandQueue::glVertexAttrib3f: {
            glVertexAttrib3f(GLuint(command.i1), command.f1, command.f2, command.f3);
            break;
        }
        case CanvasGlCommandQueue::glVertexAttrib4f: {
            glVertexAttrib4f(GLuint(command.i1), command.f1, command.f2, command.f3, command.f4);
            break;
        }
        case CanvasGlCommandQueue::glVertexAttrib1fv: {
            glVertexAttrib1fv(GLuint(command.i1),
                              reinterpret_cast<const GLfloat *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glVertexAttrib2fv: {
            glVertexAttrib2fv(GLuint(command.i1),
                              reinterpret_cast<const GLfloat *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glVertexAttrib3fv: {
            glVertexAttrib3fv(GLuint(command.i1),
                              reinterpret_cast<const GLfloat *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glVertexAttrib4fv: {
            glVertexAttrib4fv(GLuint(command.i1),
                              reinterpret_cast<const GLfloat *>(command.data->constData()));
            break;
        }
        case CanvasGlCommandQueue::glVertexAttribPointer: {
            glVertexAttribPointer(GLuint(command.i1), command.i2, GLenum(command.i3),
                                  GLboolean(command.i4), GLsizei(command.i5),
                                  reinterpret_cast<const GLvoid *>(command.i6));
            break;
        }
        case CanvasGlCommandQueue::glViewport: {
            glViewport(command.i1, command.i2, GLsizei(command.i3), GLsizei(command.i4));
            if (command.i3 < 0 || command.i4 < 0) {
                m_forceViewportRect = QRect();
            } else {
                m_forceViewportRect.setX(command.i1);
                m_forceViewportRect.setY(command.i2);
                m_forceViewportRect.setWidth(command.i3);
                m_forceViewportRect.setHeight(command.i4);
            }
            break;
        }
        case CanvasGlCommandQueue::internalClearLocation: {
            // Used to clear a mapped uniform location from map when no longer needed
            m_commandQueue.removeResourceIdFromMap(command.i1);
            break;
        }
        case CanvasGlCommandQueue::internalBeginPaint: {
            // Remember the beginning of the latest paint
            m_executeStartIndex = i + 1;
            break;
        }
        case CanvasGlCommandQueue::internalTextureComplete: {
            finalizeTexture();
            bindCurrentRenderTarget();
            // Remember the end of latest paint for back/foreground rendering
            if (m_renderTarget != Canvas::RenderTargetOffscreenBuffer) {
                m_executeEndIndex = i + 1;
            }
            break;
        }
        case CanvasGlCommandQueue::internalClearQuickItemAsTexture: {
            // Used to clear mapped quick item texture ids when no longer needed
            m_commandQueue.removeResourceIdFromMap(command.i1);
            delete m_commandQueue.providerCache().take(command.i1);
            break;
        }
        default: {
            qWarning() << __FUNCTION__
                       << "Unsupported GL command handled:" << command.id;
            break;
        }
        }

        if (logGlErrors)
            updateGlError(__FUNCTION__);

        if (m_stateStore)
            m_stateStore->storeStateCommand(command);
    }

    // Rebind default FBO
    QOpenGLFramebufferObject::bindDefault();

    // If rendering to offscreen buffer, we  can delete command data already here so we don't
    // waste any time during sync.
    if (m_renderTarget == Canvas::RenderTargetOffscreenBuffer) {
        deleteCommandData();
        m_executeQueueCount = 0;
    }
}

/*!
 * Executes a single GlSyncCommand.
 * This method is called from the render thread and in the correct context.
 */
void CanvasRenderer::executeSyncCommand(GlSyncCommand &command)
{
    if (!m_glContext)
        return;

    // Bind the correct render target FBO
    bindCurrentRenderTarget();

    // Store pending OpenGL errors
    updateGlError(__FUNCTION__);

    switch (command.id) {
    case CanvasGlCommandQueue::glGetActiveAttrib: {
        QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
        if (program) {
            GLuint glId = program->programId();
            GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
            glGetActiveAttrib(glId, GLsizei(command.i2), GLsizei(command.i3),
                              reinterpret_cast<GLsizei *>(&retVal[0]), &retVal[1],
                              reinterpret_cast<GLenum *>(&retVal[2]),
                              reinterpret_cast<char *>(&retVal[3]));
        } else {
            m_glError |= CanvasContext::CANVAS_INVALID_OPERATION;
            command.glError = true;
        }
        break;
    }
    case CanvasGlCommandQueue::glGetActiveUniform: {
        QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
        if (program) {
            GLuint glId = program->programId();
            GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
            glGetActiveUniform(glId, GLsizei(command.i2), GLsizei(command.i3),
                               reinterpret_cast<GLsizei *>(&retVal[0]), &retVal[1],
                               reinterpret_cast<GLenum *>(&retVal[2]),
                               reinterpret_cast<char *>(&retVal[3]));
        } else {
            m_glError |= CanvasContext::CANVAS_INVALID_OPERATION;
            command.glError = true;
        }
        break;
    }
    case CanvasGlCommandQueue::glGetAttribLocation: {
        QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
        if (program) {
            GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
            *retVal = program->attributeLocation(*command.data);
        } else {
            m_glError |= CanvasContext::CANVAS_INVALID_OPERATION;
            command.glError = true;
        }
        break;
    }
    case CanvasGlCommandQueue::glGetBooleanv: {
        GLboolean *retVal = reinterpret_cast<GLboolean *>(command.returnValue);
        glGetBooleanv(GLenum(command.i1), retVal);
        break;
    }
    case CanvasGlCommandQueue::glGetBufferParameteriv: {
        GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
        glGetBufferParameteriv(GLenum(command.i1), GLenum(command.i2), retVal);
        break;
    }
    case CanvasGlCommandQueue::glCheckFramebufferStatus: {
        GLenum *value = reinterpret_cast<GLenum *>(command.returnValue);
        *value = glCheckFramebufferStatus(GLenum(command.i1));
        break;
    }
    case CanvasGlCommandQueue::glFinish: {
        glFinish();
        break;
    }
    case CanvasGlCommandQueue::glGetError: {
        int *retVal = reinterpret_cast<int *>(command.returnValue);
        *retVal |= m_glError;
        m_glError = CanvasContext::CANVAS_NO_ERRORS;
        break;
    }
    case CanvasGlCommandQueue::glGetFloatv: {
        GLfloat *retVal = reinterpret_cast<GLfloat *>(command.returnValue);
        glGetFloatv(GLenum(command.i1), retVal);
        break;
    }
    case CanvasGlCommandQueue::glGetFramebufferAttachmentParameteriv: {
        GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
        glGetFramebufferAttachmentParameteriv(GLenum(command.i1), GLenum(command.i2),
                                              GLenum(command.i3), retVal);
        break;
    }
    case CanvasGlCommandQueue::glGetIntegerv: {
        GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
        glGetIntegerv(GLenum(command.i1), retVal);
        break;
    }
    case CanvasGlCommandQueue::glGetProgramInfoLog: {
        QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
        if (program) {
            QString *logStr = reinterpret_cast<QString *>(command.returnValue);
            *logStr = program->log();
        } else {
            m_glError |= CanvasContext::CANVAS_INVALID_OPERATION;
            command.glError = true;
        }
        break;
    }
    case CanvasGlCommandQueue::glGetProgramiv: {
        QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
        if (program) {
            GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
            glGetProgramiv(program->programId(), GLenum(command.i2), retVal);
        } else {
            m_glError |= CanvasContext::CANVAS_INVALID_OPERATION;
            command.glError = true;
        }
        break;
    }
    case CanvasGlCommandQueue::glGetRenderbufferParameteriv: {
        GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
        glGetRenderbufferParameteriv(GLenum(command.i1), GLenum(command.i2), retVal);
        break;
    }
    case CanvasGlCommandQueue::glGetShaderInfoLog: {
        QOpenGLShader *shader = m_commandQueue.getShader(command.i1);
        if (shader) {
            QString *logStr = reinterpret_cast<QString *>(command.returnValue);
            *logStr = shader->log();
        } else {
            m_glError |= CanvasContext::CANVAS_INVALID_OPERATION;
            command.glError = true;
        }
        break;
    }
    case CanvasGlCommandQueue::glGetShaderiv: {
        QOpenGLShader *shader = m_commandQueue.getShader(command.i1);
        if (shader) {
            GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
            glGetShaderiv(shader->shaderId(), GLenum(command.i2), retVal);
        } else {
            m_glError |= CanvasContext::CANVAS_INVALID_OPERATION;
            command.glError = true;
        }
        break;
    }
    case CanvasGlCommandQueue::glGetShaderPrecisionFormat: {
        GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
        glGetShaderPrecisionFormat(GLenum(command.i1), GLenum(command.i2), retVal, &retVal[2]);
        break;
    }
    case CanvasGlCommandQueue::glGetTexParameteriv: {
        GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
        glGetTexParameteriv(GLenum(command.i1), GLenum(command.i2), retVal);
        break;
    }
    case CanvasGlCommandQueue::glGetUniformfv: {
        QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
        if (program) {
            GLfloat *retVal = reinterpret_cast<GLfloat *>(command.returnValue);
            GLuint programId = program->programId();
            GLint location = m_commandQueue.getGlId(command.i2);
            glGetUniformfv(programId, location, retVal);
        } else {
            m_glError |= CanvasContext::CANVAS_INVALID_OPERATION;
            command.glError = true;
        }
        break;
    }
    case CanvasGlCommandQueue::glGetUniformiv: {
        QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
        if (program) {
            GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
            GLuint programId = program->programId();
            GLint location = m_commandQueue.getGlId(command.i2);
            glGetUniformiv(programId, location, retVal);
        } else {
            m_glError |= CanvasContext::CANVAS_INVALID_OPERATION;
            command.glError = true;
        }
        break;
    }
    case CanvasGlCommandQueue::glGetVertexAttribPointerv: {
        GLuint *retVal = reinterpret_cast<GLuint *>(command.returnValue);
        glGetVertexAttribPointerv(GLuint(command.i1), GLenum(command.i2),
                                  reinterpret_cast<GLvoid **>(retVal));
        break;
    }
    case CanvasGlCommandQueue::glGetVertexAttribfv: {
        GLfloat *retVal = reinterpret_cast<GLfloat *>(command.returnValue);
        glGetVertexAttribfv(GLuint(command.i1), GLenum(command.i2), retVal);
        break;
    }
    case CanvasGlCommandQueue::glGetVertexAttribiv: {
        GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
        glGetVertexAttribiv(GLuint(command.i1), GLenum(command.i2), retVal);
        if (command.i2 == GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING) {
            // We need to return the buffer id of the context side, rather than the GL id.
            *retVal = m_commandQueue.getCanvasId(*retVal, CanvasGlCommandQueue::glGenBuffers);
        }
        break;
    }
    case CanvasGlCommandQueue::glGetString: {
        command.returnValue =
                const_cast<GLubyte *>(glGetString(GLenum(command.i1)));
        break;
    }
    case CanvasGlCommandQueue::glIsBuffer: {
        GLboolean *retVal = reinterpret_cast<GLboolean *>(command.returnValue);
        *retVal = glIsBuffer(m_commandQueue.getGlId(command.i1));
        break;
    }
    case CanvasGlCommandQueue::glIsEnabled: {
        GLboolean *retVal = reinterpret_cast<GLboolean *>(command.returnValue);
        *retVal = glIsEnabled(GLenum(command.i1));
        break;
    }
    case CanvasGlCommandQueue::glIsFramebuffer: {
        GLboolean *retVal = reinterpret_cast<GLboolean *>(command.returnValue);
        *retVal = glIsFramebuffer(m_commandQueue.getGlId(command.i1));
        break;
    }
    case CanvasGlCommandQueue::glIsProgram: {
        GLboolean *retVal = reinterpret_cast<GLboolean *>(command.returnValue);
        QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
        *retVal = program ? glIsProgram(program->programId()) : GL_FALSE;
        break;
    }
    case CanvasGlCommandQueue::glIsRenderbuffer: {
        GLboolean *retVal = reinterpret_cast<GLboolean *>(command.returnValue);
        *retVal = glIsRenderbuffer(m_commandQueue.getGlId(command.i1));
        break;
    }
    case CanvasGlCommandQueue::glIsShader: {
        GLboolean *retVal = reinterpret_cast<GLboolean *>(command.returnValue);
        QOpenGLShader *shader = m_commandQueue.getShader(command.i1);
        *retVal = shader ? glIsShader(shader->shaderId()) : GL_FALSE;
        break;
    }
    case CanvasGlCommandQueue::glIsTexture: {
        GLboolean *retVal = reinterpret_cast<GLboolean *>(command.returnValue);
        *retVal = glIsTexture(m_commandQueue.getGlId(command.i1));
        break;
    }
    case CanvasGlCommandQueue::glReadPixels: {
        if (m_renderTarget == Canvas::RenderTargetOffscreenBuffer && !m_currentFramebufferId) {
            // Check if the buffer is antialiased. If it is, we need to blit to the final buffer before
            // reading the value.
            if (m_antialias) {
                resolveMSAAFbo();
                m_renderFbo->bind();
            }
            // Similarly, we must resolve the alpha multiplications
            if (m_multiplyAlpha)
                multiplyAlpha();
        }

        GLvoid *buf = reinterpret_cast<GLvoid *>(command.returnValue);
        glReadPixels(command.i1, command.i2, GLsizei(command.i3), GLsizei(command.i4),
                     GLenum(command.i5), GLenum(command.i6), buf);
        break;
    }
    case CanvasGlCommandQueue::internalGetUniformType: {
        // The uniform type needs to be known when CanvasContext::getUniform is
        // handled. There is no easy way to determine this, as the active uniform
        // indices do not usually match the uniform locations. We must query
        // active uniforms until we hit the one we want. This is obviously
        // extremely inefficient, but luckily getUniform is not something most
        // users typically need or use.

        QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
        GLint *retVal = reinterpret_cast<GLint *>(command.returnValue);
        if (program) {
            GLuint programId = program->programId();
            const int maxCharCount = 512;
            GLsizei length;
            GLint size;
            GLenum type;
            char nameBuf[maxCharCount];
            GLint uniformCount = 0;
            glGetProgramiv(programId, GL_ACTIVE_UNIFORMS, &uniformCount);
            // Strip any [] from the uniform name, unless part of struct
            QByteArray strippedName = *command.data;
            int idx = strippedName.indexOf('[');
            if (idx >= 0) {
                // Don't truncate if part of struct
                if (strippedName.indexOf('.') == -1)
                    strippedName.truncate(idx);
            }
            for (int i = 0; i < uniformCount; i++) {
                nameBuf[0] = '\0';
                glGetActiveUniform(programId, i, maxCharCount, &length,
                                  &size, &type, nameBuf);
                QByteArray activeName(nameBuf, length);
                idx = activeName.indexOf('[');
                if (idx >= 0) {
                    // Don't truncate if part of struct
                    if (activeName.indexOf('.') == -1)
                        activeName.truncate(idx);
                }

                if (activeName == strippedName) {
                    *retVal = type;
                    break;
                }
            }
        } else {
            *retVal = -1;
        }
        break;
    }
    case CanvasGlCommandQueue::extStateDump: {
        CanvasGLStateDump *dumpObj = reinterpret_cast<CanvasGLStateDump *>(command.returnValue);
        dumpObj->doGLStateDump();
        break;
    }
    default: {
        qWarning() << "CanvasRenderer::" << __FUNCTION__
                   << "Unsupported GL command handled:" << command.id;
        break;
    }
    }

    command.glError = updateGlError(__FUNCTION__);

    // Rebind default FBO
    QOpenGLFramebufferObject::bindDefault();
}

/*!
 * This method is called once the all rendering for the frame is done to offscreen FBO.
 * It does the final blitting and swaps the buffers.
 * This method is called from the render thread and in the correct context.
 */
void CanvasRenderer::finalizeTexture()
{
    qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__ << "()";

    // Resolve MSAA
    if (m_renderTarget == Canvas::RenderTargetOffscreenBuffer && m_antialias)
        resolveMSAAFbo();

    // If the canvas content is not in premultipliedAlpha format, do an additional pass
    // to premultiply the values, as Qt Quick expects premultiplied format when blending items.
    if (m_multiplyAlpha) {
        multiplyAlpha();
        qSwap(m_renderFbo, m_alphaMultiplierFbo);
    }

    // We need to flush the contents to the FBO before posting the texture,
    // otherwise we might get unexpected results.
    glFlush();
    glFinish();

    m_textureFinalized = true;

    m_frameTimeMs = m_frameTimer.elapsed();

    if (m_renderTarget == Canvas::RenderTargetOffscreenBuffer) {
        // Swap
        qSwap(m_renderFbo, m_displayFbo);
        qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                             << " Displaying texture:"
                                             << m_displayFbo->texture()
                                             << " from FBO:" << m_displayFbo->handle();

        if (m_preserveDrawingBuffer) {
            // Copy the content of display fbo to the render fbo
            GLint texBinding2D;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texBinding2D);

            m_displayFbo->bind();
            glBindTexture(GL_TEXTURE_2D, m_renderFbo->texture());

            glCopyTexImage2D(GL_TEXTURE_2D, 0, m_displayFbo->format().internalTextureFormat(),
                             0, 0, m_fboSize.width(), m_fboSize.height(), 0);

            glBindTexture(GL_TEXTURE_2D, texBinding2D);
        }

        // Notify the render node of new texture parameters
        emit textureReady(m_displayFbo->texture(), m_fboSize);
    }
}

void CanvasRenderer::restoreCanvasOpenGLState()
{
    m_stateStore->restoreStoredState();
}

void CanvasRenderer::resetQtOpenGLState()
{
    m_contextWindow->resetOpenGLState();
}

/*!
 * Blits the antialias fbo into the final render fbo.
 * This method is called from the render thread and in the correct context.
 */
void CanvasRenderer::resolveMSAAFbo()
{
    qCDebug(canvas3drendering).nospace() << "CanvasRenderer::" << __FUNCTION__
                                         << " Resolving MSAA from FBO:"
                                         << m_antialiasFbo->handle()
                                         << " to FBO:" << m_renderFbo->handle();
    QOpenGLFramebufferObject::blitFramebuffer(m_renderFbo, m_antialiasFbo);
}

void CanvasRenderer::deleteCommandData()
{
    for (int i = 0; i < m_executeQueueCount; i++) {
        GlCommand &command = m_executeQueue[i];
        if (command.data)
            command.deleteData();
    }
}

qint64 CanvasRenderer::previousFrameTime()
{
    m_frameTimer.start();
    return m_frameTimeMs;
}

void CanvasRenderer::destroy()
{
    QMutexLocker locker(&m_shutdownMutex);

    if (m_glContext) {
        deleteLater();
    } else {
        // It is safe to delete even in another thread if we are already shut down or not yet
        // started up.
        locker.unlock();
        delete this;
    }
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
