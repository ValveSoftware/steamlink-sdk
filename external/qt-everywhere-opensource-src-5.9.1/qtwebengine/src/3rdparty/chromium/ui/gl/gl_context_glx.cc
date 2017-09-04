// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_context_glx.h"

extern "C" {
#include <X11/Xlib.h>
}
#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/GL/glextchromium.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_glx.h"

namespace gl {

namespace {

static int IgnoreX11Errors(Display*, XErrorEvent*) {
  return 0;
}

using GLVersion = std::pair<int, int>;

GLXContext CreateContextAttribs(Display* display,
                                GLXFBConfig config,
                                GLXContext share,
                                GLVersion version,
                                int profile_mask) {
  std::vector<int> attribs;

  if (GLSurfaceGLX::IsCreateContextRobustnessSupported()) {
    attribs.push_back(GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);
    attribs.push_back(GLX_LOSE_CONTEXT_ON_RESET_ARB);
  }

  if (version.first != 0 || version.second != 0) {
    attribs.push_back(GLX_CONTEXT_MAJOR_VERSION_ARB);
    attribs.push_back(version.first);

    attribs.push_back(GLX_CONTEXT_MINOR_VERSION_ARB);
    attribs.push_back(version.second);
  }

  if (profile_mask != 0 && GLSurfaceGLX::IsCreateContextProfileSupported()) {
    attribs.push_back(GLX_CONTEXT_PROFILE_MASK_ARB);
    attribs.push_back(profile_mask);
  }

  attribs.push_back(0);

  // When creating a context with glXCreateContextAttribsARB, a variety of X11
  // errors can be generated. To prevent these errors from crashing our process,
  // we simply ignore them and only look if the GLXContext was created.
  // Sync to ensure any errors generated are processed.
  XSync(display, False);
  auto old_error_handler = XSetErrorHandler(IgnoreX11Errors);
  GLXContext context =
      glXCreateContextAttribsARB(display, config, share, True, attribs.data());
  XSetErrorHandler(old_error_handler);

  return context;
}

GLXContext CreateHighestVersionContext(Display* display,
                                       GLXFBConfig config,
                                       GLXContext share) {
  // It is commonly assumed that glXCreateContextAttrib will create a context
  // of the highest version possible but it is not specified in the spec and
  // is not true on the Mesa drivers. On Mesa, Instead we try to create a
  // context per GL version until we succeed, starting from newer version.
  // On both Mesa and other drivers we try to create a desktop context and fall
  // back to ES context.
  // The code could be simpler if the Mesa code path was used for all drivers,
  // however the cost of failing a context creation can be high (3 milliseconds
  // for the NVIDIA driver). The good thing is that failed context creation only
  // takes 0.1 milliseconds on Mesa.

  struct ContextCreationInfo {
    int profileFlag;
    GLVersion version;
  };

  // clang-format off
  // For regular drivers we try to create a compatibility, core, then ES
  // context. Without requiring any specific version.
  const ContextCreationInfo contexts_to_try[] = {
      { 0, GLVersion(0, 0) },
      { GLX_CONTEXT_CORE_PROFILE_BIT_ARB, GLVersion(0, 0) },
      { GLX_CONTEXT_ES2_PROFILE_BIT_EXT, GLVersion(0, 0) },
  };

  // On Mesa we try to create a core context, except for versions below 3.2
  // where it is not applicable. (and fallback to ES as well)
  const ContextCreationInfo mesa_contexts_to_try[] = {
      { GLX_CONTEXT_CORE_PROFILE_BIT_ARB, { GLVersion(4, 5) } },
      { GLX_CONTEXT_CORE_PROFILE_BIT_ARB, { GLVersion(4, 4) } },
      { GLX_CONTEXT_CORE_PROFILE_BIT_ARB, { GLVersion(4, 3) } },
      { GLX_CONTEXT_CORE_PROFILE_BIT_ARB, { GLVersion(4, 2) } },
      { GLX_CONTEXT_CORE_PROFILE_BIT_ARB, { GLVersion(4, 1) } },
      { GLX_CONTEXT_CORE_PROFILE_BIT_ARB, { GLVersion(4, 0) } },
      { GLX_CONTEXT_CORE_PROFILE_BIT_ARB, { GLVersion(3, 3) } },
      // Do not try to create OpenGL context versions between 3.0 and
      // 3.2 because of compatibility problems. crbug.com/659030
      { 0, { GLVersion(2, 0) } },
      { 0, { GLVersion(1, 5) } },
      { 0, { GLVersion(1, 4) } },
      { 0, { GLVersion(1, 3) } },
      { 0, { GLVersion(1, 2) } },
      { 0, { GLVersion(1, 1) } },
      { 0, { GLVersion(1, 0) } },
      { GLX_CONTEXT_ES2_PROFILE_BIT_EXT, { GLVersion(3, 2) } },
      { GLX_CONTEXT_ES2_PROFILE_BIT_EXT, { GLVersion(3, 1) } },
      { GLX_CONTEXT_ES2_PROFILE_BIT_EXT, { GLVersion(3, 0) } },
      { GLX_CONTEXT_ES2_PROFILE_BIT_EXT, { GLVersion(2, 0) } },
  };
  // clang-format on

  std::string client_vendor = glXGetClientString(display, GLX_VENDOR);
  bool is_mesa = client_vendor.find("Mesa") != std::string::npos;

  const ContextCreationInfo* to_try = contexts_to_try;
  size_t to_try_length = arraysize(contexts_to_try);
  if (is_mesa) {
    to_try = mesa_contexts_to_try;
    to_try_length = arraysize(mesa_contexts_to_try);
  }

  for (size_t i = 0; i < to_try_length; ++i) {
    const ContextCreationInfo& info = to_try[i];
    if (!GLSurfaceGLX::IsCreateContextES2ProfileSupported() &&
        info.profileFlag == GLX_CONTEXT_ES2_PROFILE_BIT_EXT) {
      continue;
    }
    GLXContext context = CreateContextAttribs(display, config, share,
                                              info.version, info.profileFlag);
    if (context != nullptr) {
      return context;
    }
  }

  return nullptr;
}
}

GLContextGLX::GLContextGLX(GLShareGroup* share_group)
  : GLContextReal(share_group),
    context_(nullptr),
    display_(nullptr) {
}

XDisplay* GLContextGLX::display() {
  return display_;
}

bool GLContextGLX::Initialize(GLSurface* compatible_surface,
                              const GLContextAttribs& attribs) {
  // webgl_compatibility_context and disabling bind_generates_resource are not
  // supported.
  DCHECK(!attribs.webgl_compatibility_context &&
         attribs.bind_generates_resource);

  display_ = static_cast<XDisplay*>(compatible_surface->GetDisplay());

  GLXContext share_handle = static_cast<GLXContext>(
      share_group() ? share_group()->GetHandle() : nullptr);

  if (GLSurfaceGLX::IsCreateContextSupported()) {
    DVLOG(1) << "GLX_ARB_create_context supported.";
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kCreateDefaultGLContext)) {
      context_ = CreateContextAttribs(
          display_, static_cast<GLXFBConfig>(compatible_surface->GetConfig()),
          share_handle, GLVersion(0, 0), 0);
    } else {
      context_ = CreateHighestVersionContext(
          display_, static_cast<GLXFBConfig>(compatible_surface->GetConfig()),
          share_handle);
    }
    if (!context_) {
      LOG(ERROR) << "Failed to create GL context with "
                 << "glXCreateContextAttribsARB.";
      return false;
    }
  } else {
    DVLOG(1) << "GLX_ARB_create_context not supported.";
    context_ = glXCreateNewContext(
       display_,
       static_cast<GLXFBConfig>(compatible_surface->GetConfig()),
       GLX_RGBA_TYPE,
       share_handle,
       True);
    if (!context_) {
      LOG(ERROR) << "Failed to create GL context with glXCreateNewContext.";
      return false;
    }
  }
  DCHECK(context_);
  DVLOG(1) << "  Successfully allocated "
           << (compatible_surface->IsOffscreen() ?
               "offscreen" : "onscreen")
           << " GL context with LOSE_CONTEXT_ON_RESET_ARB";

  DVLOG(1) << (compatible_surface->IsOffscreen() ? "Offscreen" : "Onscreen")
           << " context was "
           << (glXIsDirect(display_,
                           static_cast<GLXContext>(context_))
                   ? "direct" : "indirect")
           << ".";

  return true;
}

void GLContextGLX::Destroy() {
  if (context_) {
    glXDestroyContext(display_,
                      static_cast<GLXContext>(context_));
    context_ = nullptr;
  }
}

bool GLContextGLX::MakeCurrent(GLSurface* surface) {
  DCHECK(context_);
  if (IsCurrent(surface))
    return true;

  ScopedReleaseCurrent release_current;
  TRACE_EVENT0("gpu", "GLContextGLX::MakeCurrent");
  if (!glXMakeContextCurrent(
      display_,
      reinterpret_cast<GLXDrawable>(surface->GetHandle()),
      reinterpret_cast<GLXDrawable>(surface->GetHandle()),
      static_cast<GLXContext>(context_))) {
    LOG(ERROR) << "Couldn't make context current with X drawable.";
    Destroy();
    return false;
  }

  // Set this as soon as the context is current, since we might call into GL.
  SetRealGLApi();

  SetCurrent(surface);
  InitializeDynamicBindings();

  if (!surface->OnMakeCurrent(this)) {
    LOG(ERROR) << "Could not make current.";
    Destroy();
    return false;
  }

  release_current.Cancel();
  return true;
}

void GLContextGLX::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  SetCurrent(nullptr);
  if (!glXMakeContextCurrent(display_, 0, 0, 0))
    LOG(ERROR) << "glXMakeCurrent failed in ReleaseCurrent";
}

bool GLContextGLX::IsCurrent(GLSurface* surface) {
  bool native_context_is_current =
      glXGetCurrentContext() == static_cast<GLXContext>(context_);

  // If our context is current then our notion of which GLContext is
  // current must be correct. On the other hand, third-party code
  // using OpenGL might change the current context.
  DCHECK(!native_context_is_current || (GetRealCurrent() == this));

  if (!native_context_is_current)
    return false;

  if (surface) {
    if (glXGetCurrentDrawable() !=
        reinterpret_cast<GLXDrawable>(surface->GetHandle())) {
      return false;
    }
  }

  return true;
}

void* GLContextGLX::GetHandle() {
  return context_;
}

void GLContextGLX::OnSetSwapInterval(int interval) {
  DCHECK(IsCurrent(nullptr));
  if (HasExtension("GLX_EXT_swap_control") &&
      g_driver_glx.fn.glXSwapIntervalEXTFn) {
    glXSwapIntervalEXT(
        display_,
        glXGetCurrentDrawable(),
        interval);
  } else if (HasExtension("GLX_MESA_swap_control") &&
             g_driver_glx.fn.glXSwapIntervalMESAFn) {
    glXSwapIntervalMESA(interval);
  } else {
    if(interval == 0)
      LOG(WARNING) <<
          "Could not disable vsync: driver does not "
          "support GLX_EXT_swap_control";
  }
}

std::string GLContextGLX::GetExtensions() {
  DCHECK(IsCurrent(nullptr));
  const char* extensions = GLSurfaceGLX::GetGLXExtensions();
  if (extensions) {
    return GLContext::GetExtensions() + " " + extensions;
  }

  return GLContext::GetExtensions();
}

bool GLContextGLX::WasAllocatedUsingRobustnessExtension() {
  return GLSurfaceGLX::IsCreateContextRobustnessSupported();
}

GLContextGLX::~GLContextGLX() {
  Destroy();
}

}  // namespace gl
