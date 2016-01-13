// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/async_pixel_transfer_manager.h"

#include "base/android/sys_utils.h"
#include "base/debug/trace_event.h"
#include "gpu/command_buffer/service/async_pixel_transfer_manager_egl.h"
#include "gpu/command_buffer/service/async_pixel_transfer_manager_idle.h"
#include "gpu/command_buffer/service/async_pixel_transfer_manager_stub.h"
#include "gpu/command_buffer/service/async_pixel_transfer_manager_sync.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"

namespace gpu {
namespace {

bool IsBroadcom() {
  const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  if (vendor)
    return std::string(vendor).find("Broadcom") != std::string::npos;
  return false;
}

bool IsImagination() {
  const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  if (vendor)
    return std::string(vendor).find("Imagination") != std::string::npos;
  return false;
}

bool IsNvidia31() {
  const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  return vendor && version &&
         std::string(vendor).find("NVIDIA") != std::string::npos &&
         std::string(version).find("OpenGL ES 3.1") != std::string::npos;
}

}

// We only used threaded uploads when we can:
// - Create EGLImages out of OpenGL textures (EGL_KHR_gl_texture_2D_image)
// - Bind EGLImages to OpenGL textures (GL_OES_EGL_image)
// - Use fences (to test for upload completion).
// - The heap size is large enough.
// TODO(kaanb|epenner): Remove the IsImagination() check pending the
// resolution of crbug.com/249147
// TODO(kaanb|epenner): Remove the IsLowEndDevice() check pending the
// resolution of crbug.com/271929
AsyncPixelTransferManager* AsyncPixelTransferManager::Create(
    gfx::GLContext* context) {
  TRACE_EVENT0("gpu", "AsyncPixelTransferManager::Create");
  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationEGLGLES2:
      DCHECK(context);
      if (context->HasExtension("EGL_KHR_fence_sync") &&
          context->HasExtension("EGL_KHR_image") &&
          context->HasExtension("EGL_KHR_image_base") &&
          context->HasExtension("EGL_KHR_gl_texture_2D_image") &&
          context->HasExtension("GL_OES_EGL_image") &&
          !IsBroadcom() &&
          !IsImagination() &&
          !IsNvidia31() &&
          !base::android::SysUtils::IsLowEndDevice()) {
        return new AsyncPixelTransferManagerEGL;
      }
      return new AsyncPixelTransferManagerIdle;
    case gfx::kGLImplementationOSMesaGL:
      return new AsyncPixelTransferManagerIdle;
    case gfx::kGLImplementationMockGL:
      return new AsyncPixelTransferManagerStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace gpu
