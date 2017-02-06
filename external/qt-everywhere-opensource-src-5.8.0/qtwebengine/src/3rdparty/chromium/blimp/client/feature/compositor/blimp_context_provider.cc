// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_context_provider.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace blimp {
namespace client {

// static
scoped_refptr<BlimpContextProvider> BlimpContextProvider::Create(
    gfx::AcceleratedWidget widget,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
  return new BlimpContextProvider(widget, gpu_memory_buffer_manager);
}

BlimpContextProvider::BlimpContextProvider(
    gfx::AcceleratedWidget widget,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
  context_thread_checker_.DetachFromThread();

  gpu::gles2::ContextCreationAttribHelper attribs_for_gles2;
  attribs_for_gles2.alpha_size = 8;
  attribs_for_gles2.depth_size = 0;
  attribs_for_gles2.stencil_size = 0;
  attribs_for_gles2.samples = 0;
  attribs_for_gles2.sample_buffers = 0;
  attribs_for_gles2.fail_if_major_perf_caveat = false;
  attribs_for_gles2.bind_generates_resource = false;
  attribs_for_gles2.context_type = gpu::gles2::CONTEXT_TYPE_OPENGLES2;
  attribs_for_gles2.lose_context_when_out_of_memory = true;

  context_.reset(gpu::GLInProcessContext::Create(
      nullptr /* service */, nullptr /* surface */, false /* is_offscreen */,
      widget, nullptr /* share_context */, attribs_for_gles2,
      gpu::SharedMemoryLimits(), gpu_memory_buffer_manager,
      nullptr /* memory_limits */));
  context_->GetImplementation()->SetLostContextCallback(
      base::Bind(&BlimpContextProvider::OnLostContext, base::Unretained(this)));
}

BlimpContextProvider::~BlimpContextProvider() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());
}

bool BlimpContextProvider::BindToCurrentThread() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  return true;
}

void BlimpContextProvider::DetachFromThread() {
  context_thread_checker_.DetachFromThread();
}

gpu::Capabilities BlimpContextProvider::ContextCapabilities() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  return context_->GetImplementation()->capabilities();
}

gpu::gles2::GLES2Interface* BlimpContextProvider::ContextGL() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  return context_->GetImplementation();
}

gpu::ContextSupport* BlimpContextProvider::ContextSupport() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  return context_->GetImplementation();
}

class GrContext* BlimpContextProvider::GrContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(new skia_bindings::GrContextForGLES2Interface(ContextGL()));

  return gr_context_->get();
}

void BlimpContextProvider::InvalidateGrContext(uint32_t state) {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    gr_context_->ResetContext(state);
}

base::Lock* BlimpContextProvider::GetLock() {
  // This context provider is not used on multiple threads.
  NOTREACHED();
  return nullptr;
}

void BlimpContextProvider::DeleteCachedResources() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    gr_context_->FreeGpuResources();
}

void BlimpContextProvider::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  lost_context_callback_ = lost_context_callback;
}

void BlimpContextProvider::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  if (!lost_context_callback_.is_null())
    lost_context_callback_.Run();
  if (gr_context_)
    gr_context_->OnLostContext();
}

uint32_t BlimpContextProvider::GetCopyTextureInternalFormat() {
  // The attributes used to create the context in the constructor specify
  // an alpha channel.
  return GL_RGBA;
}

}  // namespace client
}  // namespace blimp
