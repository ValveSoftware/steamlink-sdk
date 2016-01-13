// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"

#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>
#include <vector>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "content/browser/renderer_host/compositing_iosurface_shader_programs_mac.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

namespace content {

// static
scoped_refptr<CompositingIOSurfaceContext>
CompositingIOSurfaceContext::Get(int window_number) {
  TRACE_EVENT0("browser", "CompositingIOSurfaceContext::Get");

  // Return the context for this window_number, if it exists.
  WindowMap::iterator found = window_map()->find(window_number);
  if (found != window_map()->end()) {
    DCHECK(!found->second->poisoned_);
    return found->second;
  }

  static bool is_vsync_disabled =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync);

  base::ScopedTypeRef<CGLContextObj> cgl_context_strong;
  CGLContextObj cgl_context = NULL;
  CGLError error = kCGLNoError;

  // Create the pixel format object for the context.
  std::vector<CGLPixelFormatAttribute> attribs;
  attribs.push_back(kCGLPFADepthSize);
  attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));
  if (ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus()) {
    attribs.push_back(kCGLPFAAllowOfflineRenderers);
    attribs.push_back(static_cast<CGLPixelFormatAttribute>(1));
  }
  attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));
  GLint number_virtual_screens = 0;
  base::ScopedTypeRef<CGLPixelFormatObj> pixel_format;
  error = CGLChoosePixelFormat(&attribs.front(),
                               pixel_format.InitializeInto(),
                               &number_virtual_screens);
  if (error != kCGLNoError) {
    LOG(ERROR) << "Failed to create pixel format object.";
    return NULL;
  }

  // Create all contexts in the same share group so that the textures don't
  // need to be recreated when transitioning contexts.
  CGLContextObj share_context = NULL;
  if (!window_map()->empty())
    share_context = window_map()->begin()->second->cgl_context();
  error = CGLCreateContext(
      pixel_format, share_context, cgl_context_strong.InitializeInto());
  if (error != kCGLNoError) {
    LOG(ERROR) << "Failed to create context object.";
    return NULL;
  }
  cgl_context = cgl_context_strong;

  // Note that VSync is ignored because CoreAnimation will automatically
  // rate limit draws.

  // Prepare the shader program cache. Precompile the shader programs
  // needed to draw the IO Surface for non-offscreen contexts.
  bool prepared = false;
  scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache;
  {
    gfx::ScopedCGLSetCurrentContext scoped_set_current_context(cgl_context);
    shader_program_cache.reset(new CompositingIOSurfaceShaderPrograms());
    if (window_number == kOffscreenContextWindowNumber) {
      prepared = true;
    } else {
      prepared = (
          shader_program_cache->UseBlitProgram() &&
          shader_program_cache->UseSolidWhiteProgram());
    }
    glUseProgram(0u);
  }
  if (!prepared) {
    LOG(ERROR) << "IOSurface failed to compile/link required shader programs.";
    return NULL;
  }

  return new CompositingIOSurfaceContext(
      window_number,
      cgl_context_strong,
      cgl_context,
      is_vsync_disabled,
      shader_program_cache.Pass());
}

void CompositingIOSurfaceContext::PoisonContextAndSharegroup() {
  if (poisoned_)
    return;

  for (WindowMap::iterator it = window_map()->begin();
       it != window_map()->end();
       ++it) {
    it->second->poisoned_ = true;
  }
  window_map()->clear();
}

CompositingIOSurfaceContext::CompositingIOSurfaceContext(
    int window_number,
    base::ScopedTypeRef<CGLContextObj> cgl_context_strong,
    CGLContextObj cgl_context,
    bool is_vsync_disabled,
    scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache)
    : window_number_(window_number),
      cgl_context_strong_(cgl_context_strong),
      cgl_context_(cgl_context),
      is_vsync_disabled_(is_vsync_disabled),
      shader_program_cache_(shader_program_cache.Pass()),
      poisoned_(false) {
  DCHECK(window_map()->find(window_number_) == window_map()->end());
  window_map()->insert(std::make_pair(window_number_, this));

  GpuDataManager::GetInstance()->AddObserver(this);
}

CompositingIOSurfaceContext::~CompositingIOSurfaceContext() {
  GpuDataManager::GetInstance()->RemoveObserver(this);

  {
    gfx::ScopedCGLSetCurrentContext scoped_set_current_context(cgl_context_);
    shader_program_cache_->Reset();
  }
  if (!poisoned_) {
    DCHECK(window_map()->find(window_number_) != window_map()->end());
    DCHECK(window_map()->find(window_number_)->second == this);
    window_map()->erase(window_number_);
  } else {
    WindowMap::const_iterator found = window_map()->find(window_number_);
    if (found != window_map()->end())
      DCHECK(found->second != this);
  }
}

void CompositingIOSurfaceContext::OnGpuSwitching() {
  // Recreate all browser-side GL contexts whenever the GPU switches. If this
  // is not done, performance will suffer.
  // http://crbug.com/361493
  PoisonContextAndSharegroup();
}

// static
CompositingIOSurfaceContext::WindowMap*
    CompositingIOSurfaceContext::window_map() {
  return window_map_.Pointer();
}

// static
base::LazyInstance<CompositingIOSurfaceContext::WindowMap>
    CompositingIOSurfaceContext::window_map_;

}  // namespace content
