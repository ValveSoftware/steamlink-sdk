// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_CONTEXT_PROVIDER_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/output/context_provider.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "ui/gl/gl_surface.h"

namespace skia_bindings {
class GrContextForGLES2Interface;
}  // namespace skia_bindings

namespace blimp {
namespace client {

// Helper class to provide a graphics context for the compositor.
class BlimpContextProvider : public cc::ContextProvider {
 public:
  static scoped_refptr<BlimpContextProvider> Create(
      gfx::AcceleratedWidget widget,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);

  // cc::ContextProvider implementation.
  bool BindToCurrentThread() override;
  void DetachFromThread() override;
  gpu::Capabilities ContextCapabilities() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  void InvalidateGrContext(uint32_t state) override;
  base::Lock* GetLock() override;
  void DeleteCachedResources() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;

  // Gives the GL internal format that should be used for calling CopyTexImage2D
  // on the default framebuffer.
  uint32_t GetCopyTextureInternalFormat();

 protected:
  BlimpContextProvider(
      gfx::AcceleratedWidget widget,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);
  ~BlimpContextProvider() override;

 private:
  void OnLostContext();

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  std::unique_ptr<gpu::GLInProcessContext> context_;
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;

  LostContextCallback lost_context_callback_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContextProvider);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_CONTEXT_PROVIDER_H_
