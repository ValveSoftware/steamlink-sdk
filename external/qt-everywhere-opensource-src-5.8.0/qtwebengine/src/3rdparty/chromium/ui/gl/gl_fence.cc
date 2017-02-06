// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence.h"

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_arb.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_fence_nv.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"

#if defined(OS_MACOSX)
#include "ui/gl/gl_fence_apple.h"
#endif

namespace gl {

GLFence::GLFence() {
}

GLFence::~GLFence() {
}

bool GLFence::IsSupported() {
  DCHECK(GetGLVersionInfo());
  return g_driver_gl.ext.b_GL_ARB_sync || GetGLVersionInfo()->is_es3 ||
         GetGLImplementation() == kGLImplementationDesktopGLCoreProfile ||
#if defined(OS_MACOSX)
         g_driver_gl.ext.b_GL_APPLE_fence ||
#else
         g_driver_egl.ext.b_EGL_KHR_fence_sync ||
#endif
         g_driver_gl.ext.b_GL_NV_fence;
}

GLFence* GLFence::Create() {
  DCHECK(GLContext::GetCurrent())
      << "Trying to create fence with no context";

  std::unique_ptr<GLFence> fence;
#if !defined(OS_MACOSX)
  if (g_driver_egl.ext.b_EGL_KHR_fence_sync &&
      g_driver_egl.ext.b_EGL_KHR_wait_sync) {
    // Prefer GLFenceEGL which doesn't require GL context switching.
    fence.reset(new GLFenceEGL);
  } else
#endif
      if (g_driver_gl.ext.b_GL_ARB_sync || GetGLVersionInfo()->is_es3 ||
          GetGLImplementation() == kGLImplementationDesktopGLCoreProfile) {
    // Prefer ARB_sync which supports server-side wait.
    fence.reset(new GLFenceARB);
#if defined(OS_MACOSX)
  } else if (g_driver_gl.ext.b_GL_APPLE_fence) {
    fence.reset(new GLFenceAPPLE);
#else
  } else if (g_driver_egl.ext.b_EGL_KHR_fence_sync) {
    fence.reset(new GLFenceEGL);
#endif
  } else if (g_driver_gl.ext.b_GL_NV_fence) {
    fence.reset(new GLFenceNV);
  }

  DCHECK_EQ(!!fence.get(), GLFence::IsSupported());
  return fence.release();
}

bool GLFence::ResetSupported() {
  // Resetting a fence to its original state isn't supported by default.
  return false;
}

void GLFence::ResetState() {
  NOTIMPLEMENTED();
}

void GLFence::Invalidate() {
  NOTIMPLEMENTED();
}

}  // namespace gl
