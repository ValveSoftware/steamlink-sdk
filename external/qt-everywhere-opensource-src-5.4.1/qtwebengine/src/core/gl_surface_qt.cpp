/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gl_surface_qt.h"

#if !defined(OS_MACOSX)

#include <QGuiApplication>
#include "gl_context_qt.h"
#include "qtwebenginecoreglobal.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/image_transport_surface.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"

#if defined(USE_X11)
#include "ui/gl/gl_surface_glx.h"

extern "C" {
#include <X11/Xlib.h>
}
#endif

#if defined(OS_WIN)
#include "ui/gl/gl_surface_wgl.h"
#include "ui/gl/gl_context_wgl.h"
#endif

using ui::GetLastEGLErrorString;

namespace gfx {

namespace {

void* g_config;
void* g_display;

const char* g_extensions = NULL;

}  // namespace


class GLSurfaceQtEGL: public GLSurfaceQt {
public:
    explicit GLSurfaceQtEGL(const gfx::Size& size);

    static bool InitializeOneOff();

    virtual bool Initialize() Q_DECL_OVERRIDE;
    virtual void Destroy() Q_DECL_OVERRIDE;
    virtual void* GetHandle() Q_DECL_OVERRIDE;
    virtual void* GetShareHandle() Q_DECL_OVERRIDE;
    virtual bool Resize(const gfx::Size &size) Q_DECL_OVERRIDE;

protected:
    ~GLSurfaceQtEGL();

private:
    EGLSurface m_surfaceBuffer;
    DISALLOW_COPY_AND_ASSIGN(GLSurfaceQtEGL);
};


GLSurfaceQt::~GLSurfaceQt()
{
}

GLSurfaceQtEGL::~GLSurfaceQtEGL()
{
    Destroy();
}

#if defined(USE_X11)
class GLSurfaceQtGLX: public GLSurfaceQt {
public:
    explicit GLSurfaceQtGLX(const gfx::Size& size);

    static bool InitializeOneOff();

    virtual bool Initialize() Q_DECL_OVERRIDE;
    virtual void Destroy() Q_DECL_OVERRIDE;
    virtual void* GetHandle() Q_DECL_OVERRIDE;

protected:
    ~GLSurfaceQtGLX();

private:
    XID m_surfaceBuffer;
    DISALLOW_COPY_AND_ASSIGN(GLSurfaceQtGLX);
};

GLSurfaceQtGLX::~GLSurfaceQtGLX()
{
    Destroy();
}

bool GLSurfaceGLX::IsCreateContextSupported()
{
    return ExtensionsContain(g_extensions, "GLX_ARB_create_context");
}

bool GLSurfaceGLX::HasGLXExtension(const char *name)
{
    return ExtensionsContain(g_extensions, name);
}

bool GLSurfaceGLX::IsTextureFromPixmapSupported()
{
    return ExtensionsContain(g_extensions, "GLX_EXT_texture_from_pixmap");
}

const char* GLSurfaceGLX::GetGLXExtensions()
{
    return g_extensions;
}

bool GLSurfaceGLX::IsCreateContextRobustnessSupported()
{
    return false;
}

bool GLSurfaceQtGLX::InitializeOneOff()
{
    static bool initialized = false;
    if (initialized)
        return true;

    // http://crbug.com/245466
    qputenv("force_s3tc_enable", "true");

    XInitThreads();

    g_display = GLContextHelper::getXDisplay();
    if (!g_display) {
        LOG(ERROR) << "GLContextHelper::getXDisplay() failed.";
        return false;
    }

    g_config = GLContextHelper::getXConfig();
    if (!g_config) {
        LOG(ERROR) << "GLContextHelper::getXConfig() failed.";
        return false;
    }

    Display* display = static_cast<Display*>(g_display);
    int major, minor;
    if (!glXQueryVersion(display, &major, &minor)) {
        LOG(ERROR) << "glxQueryVersion failed.";
        return false;
    }

    if (major == 1 && minor < 3) {
        LOG(ERROR) << "GLX 1.3 or later is required.";
        return false;
    }

    g_extensions = glXQueryExtensionsString(display, 0);
    initialized = true;
    return true;
}

bool GLSurfaceQtGLX::Initialize()
{
    Q_ASSERT(!m_surfaceBuffer);

    Display* display = static_cast<Display*>(g_display);
    const int pbuffer_attributes[] = {
        GLX_PBUFFER_WIDTH, m_size.width(),
        GLX_PBUFFER_HEIGHT, m_size.height(),
        GLX_LARGEST_PBUFFER, False,
        GLX_PRESERVED_CONTENTS, False,
        GLX_NONE
    };

    m_surfaceBuffer = glXCreatePbuffer(display, static_cast<GLXFBConfig>(g_config), pbuffer_attributes);

    if (!m_surfaceBuffer) {
        Destroy();
        LOG(ERROR) << "glXCreatePbuffer failed.";
        return false;
    }
    return true;
}

void GLSurfaceQtGLX::Destroy()
{
    if (m_surfaceBuffer) {
        glXDestroyPbuffer(static_cast<Display*>(g_display), m_surfaceBuffer);
        m_surfaceBuffer = 0;
    }
}

GLSurfaceQtGLX::GLSurfaceQtGLX(const gfx::Size& size)
    : GLSurfaceQt(size),
      m_surfaceBuffer(0)
{
}

void* GLSurfaceQtGLX::GetHandle()
{
    return reinterpret_cast<void*>(m_surfaceBuffer);
}

#elif defined(OS_WIN)

class GLSurfaceQtWGL: public GLSurfaceQt {
public:
    explicit GLSurfaceQtWGL(const gfx::Size& size);

    static bool InitializeOneOff();

    virtual bool Initialize() Q_DECL_OVERRIDE;
    virtual void Destroy() Q_DECL_OVERRIDE;
    virtual void* GetHandle() Q_DECL_OVERRIDE;
    virtual void* GetDisplay() Q_DECL_OVERRIDE;
    virtual void* GetConfig() Q_DECL_OVERRIDE;

protected:
    ~GLSurfaceQtWGL();

private:
    scoped_refptr<PbufferGLSurfaceWGL> m_surfaceBuffer;
    DISALLOW_COPY_AND_ASSIGN(GLSurfaceQtWGL);
};

GLSurfaceQtWGL::GLSurfaceQtWGL(const gfx::Size& size)
    : GLSurfaceQt(size),
      m_surfaceBuffer(0)
{
}

GLSurfaceQtWGL::~GLSurfaceQtWGL()
{
    Destroy();
}

bool GLSurfaceQtWGL::InitializeOneOff()
{
    return GLSurfaceWGL::InitializeOneOff();
}

bool GLSurfaceQtWGL::Initialize()
{
    m_surfaceBuffer = new PbufferGLSurfaceWGL(m_size);

    return m_surfaceBuffer->Initialize();
}

void GLSurfaceQtWGL::Destroy()
{
    m_surfaceBuffer = 0;
}

void *GLSurfaceQtWGL::GetHandle()
{
    return m_surfaceBuffer->GetHandle();
}

void *GLSurfaceQtWGL::GetDisplay()
{
    return m_surfaceBuffer->GetDisplay();
}

void *GLSurfaceQtWGL::GetConfig()
{
    return m_surfaceBuffer->GetConfig();
}

#endif // defined(OS_WIN)

GLSurfaceQt::GLSurfaceQt()
{
}

bool GLSurfaceQtEGL::InitializeOneOff()
{
    static bool initialized = false;
    if (initialized)
        return true;

#if defined(USE_X11)
    EGLNativeDisplayType nativeDisplay = reinterpret_cast<EGLNativeDisplayType>(GLContextHelper::getXDisplay());
    g_display = eglGetDisplay(nativeDisplay);
#else
    g_display = GLContextHelper::getEGLDisplay();
#endif

    if (!g_display) {
        LOG(ERROR) << "GLContextHelper::getEGLDisplay() failed.";
        return false;
    }

    g_config = GLContextHelper::getEGLConfig();
    if (!g_config) {
        LOG(ERROR) << "GLContextHelper::getEGLConfig() failed.";
        return false;
    }

    g_extensions = eglQueryString(g_display, EGL_EXTENSIONS);
    if (!eglInitialize(g_display, NULL, NULL)) {
        LOG(ERROR) << "eglInitialize failed with error " << GetLastEGLErrorString();
        return false;
    }

    initialized = true;
    return true;
}

bool GLSurface::InitializeOneOffInternal()
{
    if (GetGLImplementation() == kGLImplementationOSMesaGL)
        return false;

    if (GetGLImplementation() == kGLImplementationEGLGLES2)
        return GLSurfaceQtEGL::InitializeOneOff();

    if (GetGLImplementation() == kGLImplementationDesktopGL)
#if defined(USE_X11)
        return GLSurfaceQtGLX::InitializeOneOff();
#elif defined(OS_WIN)
        return GLSurfaceQtWGL::InitializeOneOff();
#endif

    return false;
}

EGLDisplay GLSurfaceEGL::GetHardwareDisplay()
{
    return static_cast<EGLDisplay>(g_display);
}

bool GLSurfaceEGL::IsCreateContextRobustnessSupported()
{
    return false;
}

GLSurfaceQt::GLSurfaceQt(const gfx::Size& size)
    : m_size(size)
{
    // Some implementations of Pbuffer do not support having a 0 size. For such
    // cases use a (1, 1) surface.
    if (m_size.GetArea() == 0)
        m_size.SetSize(1, 1);
}

bool GLSurfaceQt::HasEGLExtension(const char* name)
{
    return ExtensionsContain(g_extensions, name);
}

GLSurfaceQtEGL::GLSurfaceQtEGL(const gfx::Size& size)
    : GLSurfaceQt(size),
      m_surfaceBuffer(0)
{
}

bool GLSurfaceQtEGL::Initialize()
{
    Q_ASSERT(!m_surfaceBuffer);

    EGLDisplay display = g_display;
    if (!display) {
        LOG(ERROR) << "Trying to create surface with invalid display.";
        return false;
    }

    const EGLint pbuffer_attributes[] = {
        EGL_WIDTH, m_size.width(),
        EGL_HEIGHT, m_size.height(),
        EGL_LARGEST_PBUFFER, EGL_FALSE,
        EGL_NONE
    };

    m_surfaceBuffer = eglCreatePbufferSurface(display,
                                        g_config,
                                        pbuffer_attributes);
    if (!m_surfaceBuffer) {
        LOG(ERROR) << "eglCreatePbufferSurface failed with error ", GetLastEGLErrorString();
        Destroy();
        return false;
    }

    return true;
}

void GLSurfaceQtEGL::Destroy()
{
    if (m_surfaceBuffer) {
        if (!eglDestroySurface(g_display, m_surfaceBuffer))
            LOG(ERROR) << "eglDestroySurface failed with error " << GetLastEGLErrorString();

        m_surfaceBuffer = 0;
    }
}

bool GLSurfaceQt::IsOffscreen()
{
    return true;
}

bool GLSurfaceQt::SwapBuffers()
{
    LOG(ERROR) << "Attempted to call SwapBuffers on a pbuffer.";
    Q_UNREACHABLE();
    return false;
}

gfx::Size GLSurfaceQt::GetSize()
{
    return m_size;
}

bool GLSurfaceQtEGL::Resize(const gfx::Size& size)
{
    if (size == m_size)
        return true;

    GLContext *currentContext = GLContext::GetCurrent();
    bool wasCurrent = currentContext && currentContext->IsCurrent(this);
    if (wasCurrent)
        currentContext->ReleaseCurrent(this);

    Destroy();

    m_size = size;

    if (!Initialize()) {
        LOG(ERROR) << "Failed to resize pbuffer.";
        return false;
    }

    if (wasCurrent)
        return currentContext->MakeCurrent(this);

    return true;
}

void* GLSurfaceQtEGL::GetHandle()
{
    return reinterpret_cast<void*>(m_surfaceBuffer);
}

void* GLSurfaceQtEGL::GetShareHandle()
{
#if defined(OS_ANDROID)
    Q_UNREACHABLE();
    return NULL;
#else
    if (!gfx::g_driver_egl.ext.b_EGL_ANGLE_query_surface_pointer)
        return NULL;

    if (!gfx::g_driver_egl.ext.b_EGL_ANGLE_surface_d3d_texture_2d_share_handle)
        return NULL;

    void* handle;
    if (!eglQuerySurfacePointerANGLE(g_display,
                                     GetHandle(),
                                     EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
                                     &handle)) {
        return NULL;
    }

    return handle;
#endif
}

void* GLSurfaceQt::GetDisplay()
{
    return g_display;
}

void* GLSurfaceQt::GetConfig()
{
    return g_config;
}

// static
scoped_refptr<GLSurface>
GLSurface::CreateOffscreenGLSurface(const gfx::Size& size)
{
    switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL: {
#if defined(USE_X11)
        scoped_refptr<GLSurface> surface = new GLSurfaceQtGLX(size);
        if (!surface->Initialize())
            return NULL;
        return surface;
#elif defined(OS_WIN)
        scoped_refptr<GLSurface> surface = new GLSurfaceQtWGL(size);
        if (!surface->Initialize())
            return NULL;
        return surface;
#else
        LOG(ERROR) << "Desktop GL is not supported on this platform.";
        Q_UNREACHABLE();
        return NULL;
#endif
    }
    case kGLImplementationEGLGLES2: {
        scoped_refptr<GLSurface> surface = new GLSurfaceQtEGL(size);
        if (!surface->Initialize())
            return NULL;
        return surface;
    }
    default:
        Q_UNREACHABLE();
        return NULL;
    }
}

// static
scoped_refptr<GLSurface>
GLSurface::CreateViewGLSurface(gfx::AcceleratedWidget window)
{
    QT_NOT_USED
    return NULL;
}

}  // namespace gfx

namespace content {
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateNativeSurface(GpuChannelManager* manager, GpuCommandBufferStub* stub, const gfx::GLSurfaceHandle& handle)
{
    QT_NOT_USED
    return scoped_refptr<gfx::GLSurface>();
}
}

#endif // !defined(OS_MACOSX)
