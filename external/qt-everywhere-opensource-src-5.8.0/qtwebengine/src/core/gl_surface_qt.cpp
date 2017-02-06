/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gl_surface_qt.h"

#if !defined(OS_MACOSX)

#include <QGuiApplication>
#include "gl_context_qt.h"
#include "qtwebenginecoreglobal_p.h"

#include "base/logging.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_initializer.h"
#include "ui/gl/init/gl_factory.h"

#if defined(USE_X11)
#include "ui/gl/gl_surface_glx.h"

extern "C" {
#include <X11/Xlib.h>
}
#endif

#if defined(OS_WIN)
#include "ui/gl/gl_surface_wgl.h"
#include "ui/gl/gl_context_wgl.h"
#include "ui/gl/vsync_provider_win.h"
#endif

// From ANGLE's egl/eglext.h.
#ifndef EGL_ANGLE_surface_d3d_texture_2d_share_handle
#define EGL_ANGLE_surface_d3d_texture_2d_share_handle 1
#define EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE 0x3200
#endif

using ui::GetLastEGLErrorString;

namespace gl {

namespace {

void* g_config;
void* g_display;

const char* g_extensions = NULL;

bool g_egl_surfaceless_context_supported = false;

bool g_initializedEGL = false;

}  // namespace


class GLSurfaceQtEGL: public GLSurfaceQt {
public:
    explicit GLSurfaceQtEGL(const gfx::Size& size);

    static bool InitializeOneOff();

    virtual bool Initialize() Q_DECL_OVERRIDE;
    virtual void Destroy() Q_DECL_OVERRIDE;
    virtual void* GetHandle() Q_DECL_OVERRIDE;
    virtual bool Resize(const gfx::Size& size, float scale_factor, bool has_alpha) Q_DECL_OVERRIDE;

protected:
    ~GLSurfaceQtEGL();

private:
    EGLSurface m_surfaceBuffer;
    DISALLOW_COPY_AND_ASSIGN(GLSurfaceQtEGL);
};

// The following comment is cited from chromium/ui/gl/gl_surface_egl.cc:
// SurfacelessEGL is used as Offscreen surface when platform supports
// KHR_surfaceless_context and GL_OES_surfaceless_context. This would avoid the
// need to create a dummy EGLsurface in case we render to client API targets.
class GLSurfacelessQtEGL : public GLSurfaceQt {
public:
    explicit GLSurfacelessQtEGL(const gfx::Size& size);

 public:
    bool Initialize() Q_DECL_OVERRIDE;
    void Destroy() Q_DECL_OVERRIDE;
    bool IsSurfaceless() const Q_DECL_OVERRIDE;
    bool Resize(const gfx::Size& size, float scale_factor, bool has_alpha) Q_DECL_OVERRIDE;
    EGLSurface GetHandle() Q_DECL_OVERRIDE;
    void* GetShareHandle() Q_DECL_OVERRIDE;

private:
    DISALLOW_COPY_AND_ASSIGN(GLSurfacelessQtEGL);
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

    return m_surfaceBuffer->Initialize(gl::GLSurface::SURFACE_DEFAULT);
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

    g_display = GLContextHelper::getEGLDisplay();
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

    g_egl_surfaceless_context_supported = ExtensionsContain(g_extensions, "EGL_KHR_surfaceless_context");
    if (g_egl_surfaceless_context_supported) {
        scoped_refptr<GLSurface> surface = new GLSurfacelessQtEGL(gfx::Size(1, 1));
        scoped_refptr<GLContext> context = init::CreateGLContext(
            NULL, surface.get(), PreferIntegratedGpu);

        if (!context->MakeCurrent(surface.get()))
            g_egl_surfaceless_context_supported = false;

        // Ensure context supports GL_OES_surfaceless_context.
        if (g_egl_surfaceless_context_supported) {
            g_egl_surfaceless_context_supported = context->HasExtension(
                "GL_OES_surfaceless_context");
            context->ReleaseCurrent(surface.get());
        }
    }

    initialized = true;
    return true;
}

EGLDisplay GLSurfaceEGL::GetHardwareDisplay()
{
    return static_cast<EGLDisplay>(g_display);
}

bool GLSurfaceEGL::IsCreateContextRobustnessSupported()
{
    return false;
}

const char* GLSurfaceEGL::GetEGLExtensions()
{
    return g_extensions;
}

bool GLSurfaceEGL::HasEGLExtension(const char* name)
{
    return ExtensionsContain(GetEGLExtensions(), name);
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
        LOG(ERROR) << "eglCreatePbufferSurface failed with error " << GetLastEGLErrorString();
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

gfx::SwapResult GLSurfaceQt::SwapBuffers()
{
    LOG(ERROR) << "Attempted to call SwapBuffers on a pbuffer.";
    Q_UNREACHABLE();
    return gfx::SwapResult::SWAP_FAILED;
}

gfx::Size GLSurfaceQt::GetSize()
{
    return m_size;
}


bool GLSurfaceQtEGL::Resize(const gfx::Size& size, float scale_factor, bool has_alpha)
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

void* GLSurfaceQt::GetDisplay()
{
    return g_display;
}

void* GLSurfaceQt::GetConfig()
{
    return g_config;
}

GLSurfacelessQtEGL::GLSurfacelessQtEGL(const gfx::Size& size)
    : GLSurfaceQt(size)
{
}

bool GLSurfacelessQtEGL::Initialize()
{
    return true;
}

void GLSurfacelessQtEGL::Destroy()
{
}

bool GLSurfacelessQtEGL::IsSurfaceless() const
{
    return true;
}

bool GLSurfacelessQtEGL::Resize(const gfx::Size& size, float scale_factor, bool has_alpha)
{
    m_size = size;
    return true;
}

EGLSurface GLSurfacelessQtEGL::GetHandle()
{
    return EGL_NO_SURFACE;
}

void* GLSurfacelessQtEGL::GetShareHandle()
{
    return NULL;
}

namespace init {

bool InitializeGLOneOffPlatform()
{
#if defined(OS_WIN)
    VSyncProviderWin::InitializeOneOff();
#endif

    if (GetGLImplementation() == kGLImplementationOSMesaGL)
        return false;

    if (GetGLImplementation() == kGLImplementationEGLGLES2)
        return GLSurfaceQtEGL::InitializeOneOff();

    if (GetGLImplementation() == kGLImplementationDesktopGL) {
#if defined(OS_WIN)
        return GLSurfaceQtWGL::InitializeOneOff();
#elif defined(USE_X11)
        if (GLSurfaceQtGLX::InitializeOneOff())
            return true;
#endif
        // Fallback to trying EGL with desktop GL.
        if (GLSurfaceQtEGL::InitializeOneOff()) {
            g_initializedEGL = true;
            return true;
        }
    }

    return false;
}

scoped_refptr<GLSurface>
CreateOffscreenGLSurface(const gfx::Size& size)
{
    scoped_refptr<GLSurface> surface;
    switch (GetGLImplementation()) {
    case kGLImplementationDesktopGLCoreProfile:
    case kGLImplementationDesktopGL: {
#if defined(OS_WIN)
        surface = new GLSurfaceQtWGL(size);
        if (surface->Initialize())
            return surface;
        break;
#elif defined(USE_X11)
        if (!g_initializedEGL) {
            surface = new GLSurfaceQtGLX(size);
            if (surface->Initialize())
                return surface;
        }
        // no break
#endif
    }
    case kGLImplementationEGLGLES2: {
        surface = new GLSurfaceQtEGL(size);
        if (surface->Initialize())
            return surface;

        // Surfaceless context will be used ONLY if pseudo surfaceless context
        // is not available since some implementations of surfaceless context
        // have problems. (e.g. QTBUG-57290)
        if (g_egl_surfaceless_context_supported) {
            surface = new GLSurfacelessQtEGL(size);
            if (surface->Initialize())
                return surface;
        }

        break;
    }
    default:
        break;
    }
    LOG(ERROR) << "Requested OpenGL platform is not supported.";
    Q_UNREACHABLE();
    return NULL;
}

scoped_refptr<GLSurface>
CreateViewGLSurface(gfx::AcceleratedWidget window)
{
    QT_NOT_USED
    return NULL;
}

} // namespace init

std::string DriverEGL::GetPlatformExtensions()
{
    EGLDisplay display = GLContextHelper::getEGLDisplay();
    if (display == EGL_NO_DISPLAY)
        return "";

    DCHECK(g_driver_egl.fn.eglQueryStringFn);
    const char* str = g_driver_egl.fn.eglQueryStringFn(display, EGL_EXTENSIONS);
    return str ? std::string(str) : "";
}

}  // namespace gl

namespace gpu {
class GpuCommandBufferStub;
class GpuChannelManager;
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeSurface(GpuChannelManager*, GpuCommandBufferStub*,
                                                                        SurfaceHandle, gl::GLSurface::Format)
{
    QT_NOT_USED
    return scoped_refptr<gl::GLSurface>();
}
}

#endif // !defined(OS_MACOSX)
