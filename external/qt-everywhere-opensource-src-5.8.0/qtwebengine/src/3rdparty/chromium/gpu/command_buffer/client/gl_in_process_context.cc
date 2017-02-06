// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gl_in_process_context.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include <GLES2/gl2.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_image.h"

#if defined(OS_ANDROID)
#include "ui/gl/android/surface_texture.h"
#endif

namespace gpu {

namespace {

class GLInProcessContextImpl
    : public GLInProcessContext,
      public base::SupportsWeakPtr<GLInProcessContextImpl> {
 public:
  GLInProcessContextImpl();
  ~GLInProcessContextImpl() override;

  bool Initialize(scoped_refptr<gl::GLSurface> surface,
                  bool is_offscreen,
                  GLInProcessContext* share_context,
                  gfx::AcceleratedWidget window,
                  const gpu::gles2::ContextCreationAttribHelper& attribs,
                  const scoped_refptr<InProcessCommandBuffer::Service>& service,
                  const SharedMemoryLimits& mem_limits,
                  GpuMemoryBufferManager* gpu_memory_buffer_manager,
                  ImageFactory* image_factory);

  // GLInProcessContext implementation:
  gles2::GLES2Implementation* GetImplementation() override;
  void SetLock(base::Lock* lock) override;

 private:
  void Destroy();
  void OnSignalSyncPoint(const base::Closure& callback);

  std::unique_ptr<gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<TransferBuffer> transfer_buffer_;
  std::unique_ptr<gles2::GLES2Implementation> gles2_implementation_;
  std::unique_ptr<InProcessCommandBuffer> command_buffer_;

  DISALLOW_COPY_AND_ASSIGN(GLInProcessContextImpl);
};

GLInProcessContextImpl::GLInProcessContextImpl() = default;

GLInProcessContextImpl::~GLInProcessContextImpl() {
  Destroy();
}

gles2::GLES2Implementation* GLInProcessContextImpl::GetImplementation() {
  return gles2_implementation_.get();
}

void GLInProcessContextImpl::SetLock(base::Lock* lock) {
  NOTREACHED();
}

bool GLInProcessContextImpl::Initialize(
    scoped_refptr<gl::GLSurface> surface,
    bool is_offscreen,
    GLInProcessContext* share_context,
    gfx::AcceleratedWidget window,
    const gles2::ContextCreationAttribHelper& attribs,
    const scoped_refptr<InProcessCommandBuffer::Service>& service,
    const SharedMemoryLimits& mem_limits,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    ImageFactory* image_factory) {
  DCHECK_GE(attribs.offscreen_framebuffer_size.width(), 0);
  DCHECK_GE(attribs.offscreen_framebuffer_size.height(), 0);

  command_buffer_.reset(new InProcessCommandBuffer(service));

  scoped_refptr<gles2::ShareGroup> share_group;
  InProcessCommandBuffer* share_command_buffer = nullptr;
  if (share_context) {
    GLInProcessContextImpl* impl =
        static_cast<GLInProcessContextImpl*>(share_context);
    share_group = impl->gles2_implementation_->share_group();
    share_command_buffer = impl->command_buffer_.get();
    DCHECK(share_group);
    DCHECK(share_command_buffer);
  }

  if (!command_buffer_->Initialize(surface,
                                   is_offscreen,
                                   window,
                                   attribs,
                                   share_command_buffer,
                                   gpu_memory_buffer_manager,
                                   image_factory)) {
    DLOG(ERROR) << "Failed to initialize InProcessCommmandBuffer";
    return false;
  }

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_.reset(new gles2::GLES2CmdHelper(command_buffer_.get()));
  if (!gles2_helper_->Initialize(mem_limits.command_buffer_size)) {
    LOG(ERROR) << "Failed to initialize GLES2CmdHelper";
    Destroy();
    return false;
  }

  // Create a transfer buffer.
  transfer_buffer_.reset(new TransferBuffer(gles2_helper_.get()));

  // Check for consistency.
  DCHECK(!attribs.bind_generates_resource);
  const bool bind_generates_resource = false;
  const bool support_client_side_arrays = false;

  // Create the object exposing the OpenGL API.
  gles2_implementation_.reset(
      new gles2::GLES2Implementation(gles2_helper_.get(),
                                     share_group.get(),
                                     transfer_buffer_.get(),
                                     bind_generates_resource,
                                     attribs.lose_context_when_out_of_memory,
                                     support_client_side_arrays,
                                     command_buffer_.get()));

  if (!gles2_implementation_->Initialize(
          mem_limits.start_transfer_buffer_size,
          mem_limits.min_transfer_buffer_size,
          mem_limits.max_transfer_buffer_size,
          mem_limits.mapped_memory_reclaim_limit)) {
    return false;
  }

  return true;
}

void GLInProcessContextImpl::Destroy() {
  if (gles2_implementation_) {
    // First flush the context to ensure that any pending frees of resources
    // are completed. Otherwise, if this context is part of a share group,
    // those resources might leak. Also, any remaining side effects of commands
    // issued on this context might not be visible to other contexts in the
    // share group.
    gles2_implementation_->Flush();

    gles2_implementation_.reset();
  }

  transfer_buffer_.reset();
  gles2_helper_.reset();
  command_buffer_.reset();
}

}  // anonymous namespace

// static
GLInProcessContext* GLInProcessContext::Create(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
    scoped_refptr<gl::GLSurface> surface,
    bool is_offscreen,
    gfx::AcceleratedWidget window,
    GLInProcessContext* share_context,
    const ::gpu::gles2::ContextCreationAttribHelper& attribs,
    const SharedMemoryLimits& memory_limits,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    ImageFactory* image_factory) {
  if (surface.get()) {
    DCHECK_EQ(surface->IsOffscreen(), is_offscreen);
    DCHECK_EQ(gfx::kNullAcceleratedWidget, window);
  }

  std::unique_ptr<GLInProcessContextImpl> context(new GLInProcessContextImpl);
  if (!context->Initialize(surface, is_offscreen, share_context, window,
                           attribs, service, memory_limits,
                           gpu_memory_buffer_manager, image_factory))
    return NULL;

  return context.release();
}

}  // namespace gpu
