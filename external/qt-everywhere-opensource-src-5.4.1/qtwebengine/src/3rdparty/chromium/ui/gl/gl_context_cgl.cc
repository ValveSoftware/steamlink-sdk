// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_context_cgl.h"

#include <OpenGL/CGLRenderers.h>
#include <OpenGL/CGLTypes.h>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_switching_manager.h"

namespace gfx {

namespace {

bool g_support_renderer_switching;

struct CGLRendererInfoObjDeleter {
  void operator()(CGLRendererInfoObj* x) {
    if (x)
      CGLDestroyRendererInfo(*x);
  }
};

}  // namespace

static CGLPixelFormatObj GetPixelFormat() {
  static CGLPixelFormatObj format;
  if (format)
    return format;
  std::vector<CGLPixelFormatAttribute> attribs;
  // If the system supports dual gpus then allow offline renderers for every
  // context, so that they can all be in the same share group.
  if (ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus()) {
    attribs.push_back(kCGLPFAAllowOfflineRenderers);
    g_support_renderer_switching = true;
  }
  if (GetGLImplementation() == kGLImplementationAppleGL) {
    attribs.push_back(kCGLPFARendererID);
    attribs.push_back((CGLPixelFormatAttribute) kCGLRendererGenericFloatID);
    g_support_renderer_switching = false;
  }
  attribs.push_back((CGLPixelFormatAttribute) 0);

  GLint num_virtual_screens;
  if (CGLChoosePixelFormat(&attribs.front(),
                           &format,
                           &num_virtual_screens) != kCGLNoError) {
    LOG(ERROR) << "Error choosing pixel format.";
    return NULL;
  }
  if (!format) {
    LOG(ERROR) << "format == 0.";
    return NULL;
  }
  DCHECK_NE(num_virtual_screens, 0);
  return format;
}

GLContextCGL::GLContextCGL(GLShareGroup* share_group)
  : GLContextReal(share_group),
    context_(NULL),
    gpu_preference_(PreferIntegratedGpu),
    discrete_pixelformat_(NULL),
    screen_(-1),
    renderer_id_(-1),
    safe_to_force_gpu_switch_(false) {
}

bool GLContextCGL::Initialize(GLSurface* compatible_surface,
                              GpuPreference gpu_preference) {
  DCHECK(compatible_surface);

  gpu_preference = ui::GpuSwitchingManager::GetInstance()->AdjustGpuPreference(
      gpu_preference);

  GLContextCGL* share_context = share_group() ?
      static_cast<GLContextCGL*>(share_group()->GetContext()) : NULL;

  CGLPixelFormatObj format = GetPixelFormat();
  if (!format)
    return false;

  // If using the discrete gpu, create a pixel format requiring it before we
  // create the context.
  if (!ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus() ||
      gpu_preference == PreferDiscreteGpu) {
    std::vector<CGLPixelFormatAttribute> discrete_attribs;
    discrete_attribs.push_back((CGLPixelFormatAttribute) 0);
    GLint num_pixel_formats;
    if (CGLChoosePixelFormat(&discrete_attribs.front(),
                             &discrete_pixelformat_,
                             &num_pixel_formats) != kCGLNoError) {
      LOG(ERROR) << "Error choosing pixel format.";
      return false;
    }
    // The renderer might be switched after this, so ignore the saved ID.
    share_group()->SetRendererID(-1);
  }

  CGLError res = CGLCreateContext(
      format,
      share_context ?
          static_cast<CGLContextObj>(share_context->GetHandle()) : NULL,
      reinterpret_cast<CGLContextObj*>(&context_));
  if (res != kCGLNoError) {
    LOG(ERROR) << "Error creating context.";
    Destroy();
    return false;
  }

  gpu_preference_ = gpu_preference;
  return true;
}

void GLContextCGL::Destroy() {
  if (discrete_pixelformat_) {
    // Delay releasing the pixel format for 10 seconds to reduce the number of
    // unnecessary GPU switches.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CGLReleasePixelFormat, discrete_pixelformat_),
        base::TimeDelta::FromSeconds(10));
    discrete_pixelformat_ = NULL;
  }
  if (context_) {
    CGLDestroyContext(static_cast<CGLContextObj>(context_));
    context_ = NULL;
  }
}

bool GLContextCGL::MakeCurrent(GLSurface* surface) {
  DCHECK(context_);

  // The call to CGLSetVirtualScreen can hang on some AMD drivers
  // http://crbug.com/227228
  if (safe_to_force_gpu_switch_) {
    int renderer_id = share_group()->GetRendererID();
    int screen;
    CGLGetVirtualScreen(static_cast<CGLContextObj>(context_), &screen);

    if (g_support_renderer_switching &&
        !discrete_pixelformat_ && renderer_id != -1 &&
        (screen != screen_ || renderer_id != renderer_id_)) {
      // Attempt to find a virtual screen that's using the requested renderer,
      // and switch the context to use that screen. Don't attempt to switch if
      // the context requires the discrete GPU.
      CGLPixelFormatObj format = GetPixelFormat();
      int virtual_screen_count;
      if (CGLDescribePixelFormat(format, 0, kCGLPFAVirtualScreenCount,
                                 &virtual_screen_count) != kCGLNoError)
        return false;

      for (int i = 0; i < virtual_screen_count; ++i) {
        int screen_renderer_id;
        if (CGLDescribePixelFormat(format, i, kCGLPFARendererID,
                                   &screen_renderer_id) != kCGLNoError)
          return false;

        screen_renderer_id &= kCGLRendererIDMatchingMask;
        if (screen_renderer_id == renderer_id) {
          CGLSetVirtualScreen(static_cast<CGLContextObj>(context_), i);
          screen_ = i;
          break;
        }
      }
      renderer_id_ = renderer_id;
    }
  }

  if (IsCurrent(surface))
    return true;

  ScopedReleaseCurrent release_current;
  TRACE_EVENT0("gpu", "GLContextCGL::MakeCurrent");

  if (CGLSetCurrentContext(
      static_cast<CGLContextObj>(context_)) != kCGLNoError) {
    LOG(ERROR) << "Unable to make gl context current.";
    return false;
  }

  // Set this as soon as the context is current, since we might call into GL.
  SetRealGLApi();

  SetCurrent(surface);
  if (!InitializeDynamicBindings()) {
    return false;
  }

  if (!surface->OnMakeCurrent(this)) {
    LOG(ERROR) << "Unable to make gl context current.";
    return false;
  }

  release_current.Cancel();
  return true;
}

void GLContextCGL::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  SetCurrent(NULL);
  CGLSetCurrentContext(NULL);
}

bool GLContextCGL::IsCurrent(GLSurface* surface) {
  bool native_context_is_current = CGLGetCurrentContext() == context_;

  // If our context is current then our notion of which GLContext is
  // current must be correct. On the other hand, third-party code
  // using OpenGL might change the current context.
  DCHECK(!native_context_is_current || (GetRealCurrent() == this));

  if (!native_context_is_current)
    return false;

  return true;
}

void* GLContextCGL::GetHandle() {
  return context_;
}

void GLContextCGL::SetSwapInterval(int interval) {
  DCHECK(IsCurrent(NULL));
  LOG(WARNING) << "GLContex: GLContextCGL::SetSwapInterval is ignored.";
}


bool GLContextCGL::GetTotalGpuMemory(size_t* bytes) {
  DCHECK(bytes);
  *bytes = 0;

  CGLContextObj context = reinterpret_cast<CGLContextObj>(context_);
  if (!context)
    return false;

  // Retrieve the current renderer ID
  GLint current_renderer_id = 0;
  if (CGLGetParameter(context,
                      kCGLCPCurrentRendererID,
                      &current_renderer_id) != kCGLNoError)
    return false;

  // Iterate through the list of all renderers
  GLuint display_mask = static_cast<GLuint>(-1);
  CGLRendererInfoObj renderer_info = NULL;
  GLint num_renderers = 0;
  if (CGLQueryRendererInfo(display_mask,
                           &renderer_info,
                           &num_renderers) != kCGLNoError)
    return false;

  scoped_ptr<CGLRendererInfoObj,
      CGLRendererInfoObjDeleter> scoper(&renderer_info);

  for (GLint renderer_index = 0;
       renderer_index < num_renderers;
       ++renderer_index) {
    // Skip this if this renderer is not the current renderer.
    GLint renderer_id = 0;
    if (CGLDescribeRenderer(renderer_info,
                            renderer_index,
                            kCGLRPRendererID,
                            &renderer_id) != kCGLNoError)
        continue;
    if (renderer_id != current_renderer_id)
        continue;
    // Retrieve the video memory for the renderer.
    GLint video_memory = 0;
    if (CGLDescribeRenderer(renderer_info,
                            renderer_index,
                            kCGLRPVideoMemory,
                            &video_memory) != kCGLNoError)
        continue;
    *bytes = video_memory;
    return true;
  }

  return false;
}

void GLContextCGL::SetSafeToForceGpuSwitch() {
  safe_to_force_gpu_switch_ = true;
}


GLContextCGL::~GLContextCGL() {
  Destroy();
}

GpuPreference GLContextCGL::GetGpuPreference() {
  return gpu_preference_;
}

}  // namespace gfx
