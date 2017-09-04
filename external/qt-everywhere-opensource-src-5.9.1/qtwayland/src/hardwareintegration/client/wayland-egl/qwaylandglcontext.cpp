/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qwaylandglcontext.h"

#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QtWaylandClient/private/qwaylandsubsurface_p.h>
#include <QtWaylandClient/private/qwaylandabstractdecoration_p.h>
#include <QtWaylandClient/private/qwaylandintegration_p.h>
#include "qwaylandeglwindow.h"

#include <QDebug>
#include <QtEglSupport/private/qeglconvenience_p.h>
#include <QtGui/private/qopenglcontext_p.h>
#include <QtGui/private/qopengltexturecache_p.h>
#include <QtGui/private/qguiapplication_p.h>

#include <qpa/qplatformopenglcontext.h>
#include <QtGui/QSurfaceFormat>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLFunctions>

#include <QtCore/qmutex.h>

#include <dlfcn.h>

// Constants from EGL_KHR_create_context
#ifndef EGL_CONTEXT_MINOR_VERSION_KHR
#define EGL_CONTEXT_MINOR_VERSION_KHR 0x30FB
#endif
#ifndef EGL_CONTEXT_FLAGS_KHR
#define EGL_CONTEXT_FLAGS_KHR 0x30FC
#endif
#ifndef EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR 0x30FD
#endif
#ifndef EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR 0x00000001
#endif
#ifndef EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR 0x00000002
#endif
#ifndef EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR 0x00000001
#endif
#ifndef EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR 0x00000002
#endif

// Constants for OpenGL which are not available in the ES headers.
#ifndef GL_CONTEXT_FLAGS
#define GL_CONTEXT_FLAGS 0x821E
#endif
#ifndef GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT
#define GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT 0x0001
#endif
#ifndef GL_CONTEXT_FLAG_DEBUG_BIT
#define GL_CONTEXT_FLAG_DEBUG_BIT 0x00000002
#endif
#ifndef GL_CONTEXT_PROFILE_MASK
#define GL_CONTEXT_PROFILE_MASK 0x9126
#endif
#ifndef GL_CONTEXT_CORE_PROFILE_BIT
#define GL_CONTEXT_CORE_PROFILE_BIT 0x00000001
#endif
#ifndef GL_CONTEXT_COMPATIBILITY_PROFILE_BIT
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT 0x00000002
#endif

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

class DecorationsBlitter : public QOpenGLFunctions
{
public:
    DecorationsBlitter(QWaylandGLContext *context)
        : m_context(context)
    {
        initializeOpenGLFunctions();
        m_blitProgram = new QOpenGLShaderProgram();
        m_blitProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, "attribute vec4 position;\n\
                                                                    attribute vec4 texCoords;\n\
                                                                    varying vec2 outTexCoords;\n\
                                                                    void main()\n\
                                                                    {\n\
                                                                        gl_Position = position;\n\
                                                                        outTexCoords = texCoords.xy;\n\
                                                                    }");
        m_blitProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, "varying highp vec2 outTexCoords;\n\
                                                                        uniform sampler2D texture;\n\
                                                                        void main()\n\
                                                                        {\n\
                                                                            gl_FragColor = texture2D(texture, outTexCoords);\n\
                                                                        }");

        m_blitProgram->bindAttributeLocation("position", 0);
        m_blitProgram->bindAttributeLocation("texCoords", 1);

        if (!m_blitProgram->link()) {
            qDebug() << "Shader Program link failed.";
            qDebug() << m_blitProgram->log();
        }
    }
    ~DecorationsBlitter()
    {
        delete m_blitProgram;
    }
    void blit(QWaylandEglWindow *window)
    {
        QOpenGLTextureCache *cache = QOpenGLTextureCache::cacheForContext(m_context->context());

        QRect windowRect = window->window()->frameGeometry();
        int scale = window->scale() ;
        glViewport(0, 0, windowRect.width() * scale, windowRect.height() * scale);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glDepthMask(GL_FALSE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        m_context->mUseNativeDefaultFbo = true;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_context->mUseNativeDefaultFbo = false;

        static const GLfloat squareVertices[] = {
            -1.f, -1.f,
            1.0f, -1.f,
            -1.f,  1.0f,
            1.0f,  1.0f
        };

        static const GLfloat inverseSquareVertices[] = {
            -1.f, 1.f,
            1.f, 1.f,
            -1.f, -1.f,
            1.f, -1.f
        };

        static const GLfloat textureVertices[] = {
            0.0f,  0.0f,
            1.0f,  0.0f,
            0.0f,  1.0f,
            1.0f,  1.0f,
        };

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_blitProgram->bind();

        m_blitProgram->enableAttributeArray(0);
        m_blitProgram->enableAttributeArray(1);
        m_blitProgram->setAttributeArray(1, textureVertices, 2);

        glActiveTexture(GL_TEXTURE0);

        //Draw Decoration
        m_blitProgram->setAttributeArray(0, inverseSquareVertices, 2);
        QImage decorationImage = window->decoration()->contentImage();
        cache->bindTexture(m_context->context(), decorationImage);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (m_context->context()->functions()->hasOpenGLFeature(QOpenGLFunctions::NPOTTextureRepeat)) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        //Draw Content
        m_blitProgram->setAttributeArray(0, squareVertices, 2);
        glBindTexture(GL_TEXTURE_2D, window->contentTexture());
        QRect r = window->contentsRect();
        glViewport(r.x() * scale, r.y() * scale, r.width() * scale, r.height() * scale);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        //Cleanup
        m_blitProgram->disableAttributeArray(0);
        m_blitProgram->disableAttributeArray(1);
    }

    QOpenGLShaderProgram *m_blitProgram;
    QWaylandGLContext *m_context;
};



QWaylandGLContext::QWaylandGLContext(EGLDisplay eglDisplay, QWaylandDisplay *display, const QSurfaceFormat &format, QPlatformOpenGLContext *share)
    : QPlatformOpenGLContext()
    , m_eglDisplay(eglDisplay)
    , m_display(display)
    , m_blitter(0)
    , mUseNativeDefaultFbo(false)
    , mSupportNonBlockingSwap(true)
{
    QSurfaceFormat fmt = format;
    if (static_cast<QWaylandIntegration *>(QGuiApplicationPrivate::platformIntegration())->display()->supportsWindowDecoration())
        fmt.setAlphaBufferSize(8);
    m_config = q_configFromGLFormat(m_eglDisplay, fmt);
    m_format = q_glFormatFromConfig(m_eglDisplay, m_config);
    m_shareEGLContext = share ? static_cast<QWaylandGLContext *>(share)->eglContext() : EGL_NO_CONTEXT;

    QVector<EGLint> eglContextAttrs;
    eglContextAttrs.append(EGL_CONTEXT_CLIENT_VERSION);
    eglContextAttrs.append(format.majorVersion());
    const bool hasKHRCreateContext = q_hasEglExtension(m_eglDisplay, "EGL_KHR_create_context");
    if (hasKHRCreateContext) {
        eglContextAttrs.append(EGL_CONTEXT_MINOR_VERSION_KHR);
        eglContextAttrs.append(format.minorVersion());
        int flags = 0;
        // The debug bit is supported both for OpenGL and OpenGL ES.
        if (format.testOption(QSurfaceFormat::DebugContext))
            flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
        // The fwdcompat bit is only for OpenGL 3.0+.
        if (m_format.renderableType() == QSurfaceFormat::OpenGL
            && format.majorVersion() >= 3
            && !format.testOption(QSurfaceFormat::DeprecatedFunctions))
            flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;
        if (flags) {
            eglContextAttrs.append(EGL_CONTEXT_FLAGS_KHR);
            eglContextAttrs.append(flags);
        }
        // Profiles are OpenGL only and mandatory in 3.2+. The value is silently ignored for < 3.2.
        if (m_format.renderableType() == QSurfaceFormat::OpenGL) {
            eglContextAttrs.append(EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR);
            eglContextAttrs.append(format.profile() == QSurfaceFormat::CoreProfile
                                ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR
                                : EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR);
        }
    }
    eglContextAttrs.append(EGL_NONE);

    switch (m_format.renderableType()) {
    case QSurfaceFormat::OpenVG:
        m_api = EGL_OPENVG_API;
        break;
#ifdef EGL_VERSION_1_4
#  if !defined(QT_OPENGL_ES_2)
    case QSurfaceFormat::DefaultRenderableType:
#  endif
    case QSurfaceFormat::OpenGL:
        m_api = EGL_OPENGL_API;
        break;
#endif
    case QSurfaceFormat::OpenGLES:
    default:
        m_api = EGL_OPENGL_ES_API;
        break;
    }
    eglBindAPI(m_api);

    m_context = eglCreateContext(m_eglDisplay, m_config, m_shareEGLContext, eglContextAttrs.constData());

    if (m_context == EGL_NO_CONTEXT) {
        m_context = eglCreateContext(m_eglDisplay, m_config, EGL_NO_CONTEXT, eglContextAttrs.constData());
        m_shareEGLContext = EGL_NO_CONTEXT;
    }

    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qWarning("QWaylandGLContext: failed to create EGLContext, error=%x", error);
        return;
    }

    EGLint a = EGL_MIN_SWAP_INTERVAL;
    EGLint b = EGL_MAX_SWAP_INTERVAL;
    if (!eglGetConfigAttrib(m_eglDisplay, m_config, a, &a) ||
        !eglGetConfigAttrib(m_eglDisplay, m_config, b, &b) ||
        a > 0) {
       mSupportNonBlockingSwap = false;
    }
    if (!mSupportNonBlockingSwap) {
        qWarning() << "Non-blocking swap buffers not supported. Subsurface rendering can be affected.";
    }

    updateGLFormat();
}

void QWaylandGLContext::updateGLFormat()
{
    // Have to save & restore to prevent QOpenGLContext::currentContext() from becoming
    // inconsistent after QOpenGLContext::create().
    EGLDisplay prevDisplay = eglGetCurrentDisplay();
    if (prevDisplay == EGL_NO_DISPLAY) // when no context is current
        prevDisplay = m_eglDisplay;
    EGLContext prevContext = eglGetCurrentContext();
    EGLSurface prevSurfaceDraw = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface prevSurfaceRead = eglGetCurrentSurface(EGL_READ);

    wl_surface *wlSurface = m_display->createSurface(Q_NULLPTR);
    wl_egl_window *eglWindow = wl_egl_window_create(wlSurface, 1, 1);
    EGLSurface eglSurface = eglCreateWindowSurface(m_eglDisplay, m_config, eglWindow, 0);

    if (eglMakeCurrent(m_eglDisplay, eglSurface, eglSurface, m_context)) {
        if (m_format.renderableType() == QSurfaceFormat::OpenGL
            || m_format.renderableType() == QSurfaceFormat::OpenGLES) {
            const GLubyte *s = glGetString(GL_VERSION);
            if (s) {
                QByteArray version = QByteArray(reinterpret_cast<const char *>(s));
                int major, minor;
                if (QPlatformOpenGLContext::parseOpenGLVersion(version, major, minor)) {
                    m_format.setMajorVersion(major);
                    m_format.setMinorVersion(minor);
                }
            }
            m_format.setProfile(QSurfaceFormat::NoProfile);
            m_format.setOptions(QSurfaceFormat::FormatOptions());
            if (m_format.renderableType() == QSurfaceFormat::OpenGL) {
                // Check profile and options.
                if (m_format.majorVersion() < 3) {
                    m_format.setOption(QSurfaceFormat::DeprecatedFunctions);
                } else {
                    GLint value = 0;
                    glGetIntegerv(GL_CONTEXT_FLAGS, &value);
                    if (!(value & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT))
                        m_format.setOption(QSurfaceFormat::DeprecatedFunctions);
                    if (value & GL_CONTEXT_FLAG_DEBUG_BIT)
                        m_format.setOption(QSurfaceFormat::DebugContext);
                    if (m_format.version() >= qMakePair(3, 2)) {
                        value = 0;
                        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &value);
                        if (value & GL_CONTEXT_CORE_PROFILE_BIT)
                            m_format.setProfile(QSurfaceFormat::CoreProfile);
                        else if (value & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)
                            m_format.setProfile(QSurfaceFormat::CompatibilityProfile);
                    }
                }
            }
        }
        eglMakeCurrent(prevDisplay, prevSurfaceDraw, prevSurfaceRead, prevContext);
    }
    eglDestroySurface(m_eglDisplay, eglSurface);
    wl_egl_window_destroy(eglWindow);
    wl_surface_destroy(wlSurface);
}

QWaylandGLContext::~QWaylandGLContext()
{
    delete m_blitter;
    eglDestroyContext(m_eglDisplay, m_context);
}

bool QWaylandGLContext::makeCurrent(QPlatformSurface *surface)
{
    // in QWaylandGLContext() we called eglBindAPI with the correct value. However,
    // eglBindAPI's documentation says:
    // "eglBindAPI defines the current rendering API for EGL in the thread it is called from"
    // Since makeCurrent() can be called from a different thread than the one we created the
    // context in make sure to call eglBindAPI in the correct thread.
    if (eglQueryAPI() != m_api) {
        eglBindAPI(m_api);
    }

    QWaylandEglWindow *window = static_cast<QWaylandEglWindow *>(surface);
    EGLSurface eglSurface = window->eglSurface();

    if (!window->needToUpdateContentFBO() && (eglSurface != EGL_NO_SURFACE && eglGetCurrentContext() == m_context && eglGetCurrentSurface(EGL_DRAW) == eglSurface))
        return true;

    window->setCanResize(false);
    // Core profiles mandate the use of VAOs when rendering. We would then need to use one
    // in DecorationsBlitter, but for that we would need a QOpenGLFunctions_3_2_Core instead
    // of the QOpenGLFunctions we use, but that would break when using a lower version context.
    // Instead of going crazy, just disable decorations for core profiles until we use
    // subsurfaces for them.
    if (m_format.profile() != QSurfaceFormat::CoreProfile && !window->decoration())
        window->createDecoration();

    if (eglSurface == EGL_NO_SURFACE) {
        window->updateSurface(true);
        eglSurface = window->eglSurface();
    }

    if (!eglMakeCurrent(m_eglDisplay, eglSurface, eglSurface, m_context)) {
        qWarning("QWaylandGLContext::makeCurrent: eglError: %x, this: %p \n", eglGetError(), this);
        window->setCanResize(true);
        return false;
    }

    window->bindContentFBO();

    return true;
}

void QWaylandGLContext::doneCurrent()
{
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

#define STATE_GUARD_VERTEX_ATTRIB_COUNT 2

class StateGuard
{
public:
    StateGuard() {
        QOpenGLFunctions glFuncs(QOpenGLContext::currentContext());

        glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *) &m_program);
        glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint *) &m_activeTextureUnit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *) &m_texture);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *) &m_fbo);
        glGetIntegerv(GL_VIEWPORT, m_viewport);
        glGetIntegerv(GL_DEPTH_WRITEMASK, &m_depthWriteMask);
        glGetIntegerv(GL_COLOR_WRITEMASK, m_colorWriteMask);
        m_blend = glIsEnabled(GL_BLEND);
        m_depth = glIsEnabled(GL_DEPTH_TEST);
        m_cull = glIsEnabled(GL_CULL_FACE);
        m_scissor = glIsEnabled(GL_SCISSOR_TEST);
        for (int i = 0; i < STATE_GUARD_VERTEX_ATTRIB_COUNT; ++i) {
            glFuncs.glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, (GLint *) &m_vertexAttribs[i].enabled);
            glFuncs.glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, (GLint *) &m_vertexAttribs[i].arrayBuffer);
            glFuncs.glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &m_vertexAttribs[i].size);
            glFuncs.glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &m_vertexAttribs[i].stride);
            glFuncs.glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint *) &m_vertexAttribs[i].type);
            glFuncs.glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, (GLint *) &m_vertexAttribs[i].normalized);
            glFuncs.glGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &m_vertexAttribs[i].pointer);
        }
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint *) &m_minFilter);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint *) &m_magFilter);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint *) &m_wrapS);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint *) &m_wrapT);
    }

    ~StateGuard() {
        QOpenGLFunctions glFuncs(QOpenGLContext::currentContext());

        glFuncs.glUseProgram(m_program);
        glActiveTexture(m_activeTextureUnit);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glFuncs.glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
        glDepthMask(m_depthWriteMask);
        glColorMask(m_colorWriteMask[0], m_colorWriteMask[1], m_colorWriteMask[2], m_colorWriteMask[3]);
        if (m_blend)
            glEnable(GL_BLEND);
        if (m_depth)
            glEnable(GL_DEPTH_TEST);
        if (m_cull)
            glEnable(GL_CULL_FACE);
        if (m_scissor)
            glEnable(GL_SCISSOR_TEST);
        for (int i = 0; i < STATE_GUARD_VERTEX_ATTRIB_COUNT; ++i) {
            if (m_vertexAttribs[i].enabled)
                glFuncs.glEnableVertexAttribArray(i);
            GLuint prevBuf;
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint *) &prevBuf);
            glFuncs.glBindBuffer(GL_ARRAY_BUFFER, m_vertexAttribs[i].arrayBuffer);
            glFuncs.glVertexAttribPointer(i, m_vertexAttribs[i].size, m_vertexAttribs[i].type,
                                          m_vertexAttribs[i].normalized, m_vertexAttribs[i].stride,
                                          m_vertexAttribs[i].pointer);
            glFuncs.glBindBuffer(GL_ARRAY_BUFFER, prevBuf);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_minFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_magFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapT);
    }

private:
    GLuint m_program;
    GLenum m_activeTextureUnit;
    GLuint m_texture;
    GLuint m_fbo;
    GLint m_depthWriteMask;
    GLint m_colorWriteMask[4];
    GLboolean m_blend;
    GLboolean m_depth;
    GLboolean m_cull;
    GLboolean m_scissor;
    GLint m_viewport[4];
    struct VertexAttrib {
        bool enabled;
        GLuint arrayBuffer;
        GLint size;
        GLint stride;
        GLenum type;
        bool normalized;
        void *pointer;
    } m_vertexAttribs[STATE_GUARD_VERTEX_ATTRIB_COUNT];
    GLenum m_minFilter;
    GLenum m_magFilter;
    GLenum m_wrapS;
    GLenum m_wrapT;
};

void QWaylandGLContext::swapBuffers(QPlatformSurface *surface)
{
    QWaylandEglWindow *window = static_cast<QWaylandEglWindow *>(surface);

    EGLSurface eglSurface = window->eglSurface();

    if (window->decoration()) {
        makeCurrent(surface);

        // Must save & restore all state. Applications are usually not prepared
        // for random context state changes in a swapBuffers() call.
        StateGuard stateGuard;

        if (!m_blitter)
            m_blitter = new DecorationsBlitter(this);
        m_blitter->blit(window);
    }


    QWaylandSubSurface *sub = window->subSurfaceWindow();
    if (sub) {
        QMutexLocker l(sub->syncMutex());

        int si = (sub->isSync() && mSupportNonBlockingSwap) ? 0 : m_format.swapInterval();

        eglSwapInterval(m_eglDisplay, si);
        eglSwapBuffers(m_eglDisplay, eglSurface);
    } else {
        eglSwapInterval(m_eglDisplay, m_format.swapInterval());
        eglSwapBuffers(m_eglDisplay, eglSurface);
    }


    window->setCanResize(true);
}

GLuint QWaylandGLContext::defaultFramebufferObject(QPlatformSurface *surface) const
{
    if (mUseNativeDefaultFbo)
        return 0;

    return static_cast<QWaylandEglWindow *>(surface)->contentFBO();
}

bool QWaylandGLContext::isSharing() const
{
    return m_shareEGLContext != EGL_NO_CONTEXT;
}

bool QWaylandGLContext::isValid() const
{
    return m_context != EGL_NO_CONTEXT;
}

QFunctionPointer QWaylandGLContext::getProcAddress(const char *procName)
{
    QFunctionPointer proc = (QFunctionPointer) eglGetProcAddress(procName);
    if (!proc)
        proc = (QFunctionPointer) dlsym(RTLD_DEFAULT, procName);
    return proc;
}

EGLConfig QWaylandGLContext::eglConfig() const
{
    return m_config;
}

}

QT_END_NAMESPACE
