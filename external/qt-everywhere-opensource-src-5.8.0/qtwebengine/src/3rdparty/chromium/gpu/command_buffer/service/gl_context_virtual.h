// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_VIRTUAL_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_VIRTUAL_H_

#include <string>
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "gpu/gpu_export.h"
#include "ui/gl/gl_context.h"

namespace gl {
class GPUPreference;
class GPUTimingClient;
class GLShareGroup;
class GLSurface;
}

namespace gpu {
namespace gles2 {
class GLES2Decoder;
}

// Encapsulates a virtual OpenGL context.
class GPU_EXPORT GLContextVirtual : public gl::GLContext {
 public:
  GLContextVirtual(gl::GLShareGroup* share_group,
                   gl::GLContext* shared_context,
                   base::WeakPtr<gles2::GLES2Decoder> decoder);

  // Implement GLContext.
  bool Initialize(gl::GLSurface* compatible_surface,
                  gl::GpuPreference gpu_preference) override;
  bool MakeCurrent(gl::GLSurface* surface) override;
  void ReleaseCurrent(gl::GLSurface* surface) override;
  bool IsCurrent(gl::GLSurface* surface) override;
  void* GetHandle() override;
  scoped_refptr<gl::GPUTimingClient> CreateGPUTimingClient() override;
  void OnSetSwapInterval(int interval) override;
  std::string GetExtensions() override;
  void SetSafeToForceGpuSwitch() override;
  bool WasAllocatedUsingRobustnessExtension() override;
  void SetUnbindFboOnMakeCurrent() override;
  gl::YUVToRGBConverter* GetYUVToRGBConverter() override;

 protected:
  ~GLContextVirtual() override;

 private:
  void Destroy();

  scoped_refptr<gl::GLContext> shared_context_;
  base::WeakPtr<gles2::GLES2Decoder> decoder_;

  DISALLOW_COPY_AND_ASSIGN(GLContextVirtual);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_VIRTUAL_H_
