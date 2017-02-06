// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/context_provider_command_buffer.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/managed_memory_policy.h"
#include "content/common/gpu/client/command_buffer_metrics.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_trace_implementation.h"
#include "gpu/command_buffer/client/gpu_switches.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace {

// Similar to base::AutoReset but it sets the variable to the new value
// when it is destroyed. Use Reset() to cancel setting the variable.
class AutoSet {
 public:
  AutoSet(bool* b, bool set) : b_(b), set_(set) {}
  ~AutoSet() {
    if (b_)
      *b_ = set_;
  }
  // Stops us from setting b_ on destruction.
  void Reset() { b_ = nullptr; }

 private:
  bool* b_;
  const bool set_;
};
}

namespace content {

ContextProviderCommandBuffer::SharedProviders::SharedProviders() = default;
ContextProviderCommandBuffer::SharedProviders::~SharedProviders() = default;

ContextProviderCommandBuffer::ContextProviderCommandBuffer(
    scoped_refptr<gpu::GpuChannelHost> channel,
    int32_t stream_id,
    gpu::GpuStreamPriority stream_priority,
    gpu::SurfaceHandle surface_handle,
    const GURL& active_url,
    bool automatic_flushes,
    bool support_locking,
    const gpu::SharedMemoryLimits& memory_limits,
    const gpu::gles2::ContextCreationAttribHelper& attributes,
    ContextProviderCommandBuffer* shared_context_provider,
    command_buffer_metrics::ContextType type)
    : stream_id_(stream_id),
      stream_priority_(stream_priority),
      surface_handle_(surface_handle),
      active_url_(active_url),
      automatic_flushes_(automatic_flushes),
      support_locking_(support_locking),
      memory_limits_(memory_limits),
      attributes_(attributes),
      context_type_(type),
      shared_providers_(shared_context_provider
                            ? shared_context_provider->shared_providers_
                            : new SharedProviders),
      channel_(std::move(channel)) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(channel_);
  context_thread_checker_.DetachFromThread();
}

ContextProviderCommandBuffer::~ContextProviderCommandBuffer() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());

  {
    base::AutoLock hold(shared_providers_->lock);
    auto it = std::find(shared_providers_->list.begin(),
                        shared_providers_->list.end(), this);
    if (it != shared_providers_->list.end())
      shared_providers_->list.erase(it);
  }

  if (bind_succeeded_) {
    // Clear the lock to avoid DCHECKs that the lock is being held during
    // shutdown.
    command_buffer_->SetLock(nullptr);
    // Disconnect lost callbacks during destruction.
    gles2_impl_->SetLostContextCallback(base::Closure());
  }
}

gpu::CommandBufferProxyImpl*
ContextProviderCommandBuffer::GetCommandBufferProxy() {
  return command_buffer_.get();
}

uint32_t ContextProviderCommandBuffer::GetCopyTextureInternalFormat() {
  if (attributes_.alpha_size > 0)
    return GL_RGBA;
  DCHECK_NE(attributes_.red_size, 0);
  DCHECK_NE(attributes_.green_size, 0);
  DCHECK_NE(attributes_.blue_size, 0);
  return GL_RGB;
}

bool ContextProviderCommandBuffer::BindToCurrentThread() {
  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (bind_failed_)
    return false;
  if (bind_succeeded_)
    return true;

  // Early outs should report failure.
  AutoSet set_bind_failed(&bind_failed_, true);

  // It's possible to be running BindToCurrentThread on two contexts
  // on different threads at the same time, but which will be in the same share
  // group. To ensure they end up in the same group, hold the lock on the
  // shared_providers_ (which they will share) after querying the group, until
  // this context has been added to the list.
  {
    ContextProviderCommandBuffer* shared_context_provider = nullptr;
    gpu::CommandBufferProxyImpl* shared_command_buffer = nullptr;
    scoped_refptr<gpu::gles2::ShareGroup> share_group;

    base::AutoLock hold(shared_providers_->lock);

    if (!shared_providers_->list.empty()) {
      shared_context_provider = shared_providers_->list.front();
      shared_command_buffer = shared_context_provider->command_buffer_.get();
      share_group = shared_context_provider->gles2_impl_->share_group();
      DCHECK_EQ(!!shared_command_buffer, !!share_group);
    }

    DCHECK(attributes_.buffer_preserved);

    // This command buffer is a client-side proxy to the command buffer in the
    // GPU process.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        default_task_runner_;
    if (!task_runner)
      task_runner = base::ThreadTaskRunnerHandle::Get();
    command_buffer_ = gpu::CommandBufferProxyImpl::Create(
        std::move(channel_), surface_handle_, shared_command_buffer, stream_id_,
        stream_priority_, attributes_, active_url_, std::move(task_runner));
    if (!command_buffer_) {
      DLOG(ERROR) << "GpuChannelHost failed to create command buffer.";
      command_buffer_metrics::UmaRecordContextInitFailed(context_type_);
      return false;
    }

    // The GLES2 helper writes the command buffer protocol.
    gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer_.get()));
    gles2_helper_->SetAutomaticFlushes(automatic_flushes_);
    if (!gles2_helper_->Initialize(memory_limits_.command_buffer_size)) {
      DLOG(ERROR) << "Failed to initialize GLES2CmdHelper.";
      return false;
    }

    // The transfer buffer is used to copy resources between the client
    // process and the GPU process.
    transfer_buffer_.reset(new gpu::TransferBuffer(gles2_helper_.get()));

    // The GLES2Implementation exposes the OpenGLES2 API, as well as the
    // gpu::ContextSupport interface.
    constexpr bool support_client_side_arrays = false;
    gles2_impl_.reset(new gpu::gles2::GLES2Implementation(
        gles2_helper_.get(), share_group, transfer_buffer_.get(),
        attributes_.bind_generates_resource,
        attributes_.lose_context_when_out_of_memory, support_client_side_arrays,
        command_buffer_.get()));
    if (!gles2_impl_->Initialize(memory_limits_.start_transfer_buffer_size,
                                 memory_limits_.min_transfer_buffer_size,
                                 memory_limits_.max_transfer_buffer_size,
                                 memory_limits_.mapped_memory_reclaim_limit)) {
      DLOG(ERROR) << "Failed to initialize GLES2Implementation.";
      return false;
    }

    if (command_buffer_->GetLastError() != gpu::error::kNoError) {
      DLOG(ERROR) << "Context dead on arrival. Last error: "
                  << command_buffer_->GetLastError();
      return false;
    }

    // If any context in the share group has been lost, then abort and don't
    // continue since we need to go back to the caller of the constructor to
    // find the correct share group.
    // This may happen in between the share group being chosen at the
    // constructor, and getting to run this BindToCurrentThread method which
    // can be on some other thread.
    // We intentionally call this *after* creating the command buffer via the
    // GpuChannelHost. Once that has happened, the service knows we are in the
    // share group and if a shared context is lost, our context will be informed
    // also, and the lost context callback will occur for the owner of the
    // context provider. If we check sooner, the shared context may be lost in
    // between these two states and our context here would be left in an orphan
    // share group.
    if (share_group && share_group->IsLost())
      return false;

    shared_providers_->list.push_back(this);
  }
  set_bind_failed.Reset();
  bind_succeeded_ = true;

  gles2_impl_->SetLostContextCallback(
      base::Bind(&ContextProviderCommandBuffer::OnLostContext,
                 // |this| owns the GLES2Implementation which holds the
                 // callback.
                 base::Unretained(this)));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGpuClientTracing)) {
    // This wraps the real GLES2Implementation and we should always use this
    // instead when it's present.
    trace_impl_.reset(
        new gpu::gles2::GLES2TraceImplementation(gles2_impl_.get()));
  }

  // Do this last once the context is set up.
  std::string type_name =
      command_buffer_metrics::ContextTypeToString(context_type_);
  std::string unique_context_name =
      base::StringPrintf("%s-%p", type_name.c_str(), gles2_impl_.get());
  ContextGL()->TraceBeginCHROMIUM("gpu_toplevel", unique_context_name.c_str());
  // If support_locking_ is true, the context may be used from multiple
  // threads, and any async callstacks will need to hold the same lock, so
  // give it to the command buffer.
  // We don't hold a lock here since there's no need, so set the lock very last
  // to prevent asserts that we're not holding it.
  if (support_locking_)
    command_buffer_->SetLock(&context_lock_);
  return true;
}

void ContextProviderCommandBuffer::DetachFromThread() {
  context_thread_checker_.DetachFromThread();
}

gpu::gles2::GLES2Interface* ContextProviderCommandBuffer::ContextGL() {
  DCHECK(bind_succeeded_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (trace_impl_)
    return trace_impl_.get();
  return gles2_impl_.get();
}

gpu::ContextSupport* ContextProviderCommandBuffer::ContextSupport() {
  return gles2_impl_.get();
}

class GrContext* ContextProviderCommandBuffer::GrContext() {
  DCHECK(bind_succeeded_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(new skia_bindings::GrContextForGLES2Interface(ContextGL()));

  // If GlContext is already lost, also abandon the new GrContext.
  if (gr_context_->get() &&
      ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    gr_context_->get()->abandonContext();

  return gr_context_->get();
}

void ContextProviderCommandBuffer::InvalidateGrContext(uint32_t state) {
  if (gr_context_) {
    DCHECK(bind_succeeded_);
    DCHECK(context_thread_checker_.CalledOnValidThread());
    gr_context_->ResetContext(state);
  }
}

void ContextProviderCommandBuffer::SetDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> default_task_runner) {
  DCHECK(!bind_succeeded_);
  default_task_runner_ = std::move(default_task_runner);
}

base::Lock* ContextProviderCommandBuffer::GetLock() {
  DCHECK(support_locking_);
  return &context_lock_;
}

gpu::Capabilities ContextProviderCommandBuffer::ContextCapabilities() {
  DCHECK(bind_succeeded_);
  DCHECK(context_thread_checker_.CalledOnValidThread());
  // Skips past the trace_impl_ as it doesn't have capabilities.
  return gles2_impl_->capabilities();
}

void ContextProviderCommandBuffer::DeleteCachedResources() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    gr_context_->FreeGpuResources();
}

void ContextProviderCommandBuffer::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (!lost_context_callback_.is_null())
    lost_context_callback_.Run();
  if (gr_context_)
    gr_context_->OnLostContext();

  gpu::CommandBuffer::State state = GetCommandBufferProxy()->GetLastState();
  command_buffer_metrics::UmaRecordContextLost(context_type_, state.error,
                                               state.context_lost_reason);
}

void ContextProviderCommandBuffer::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(lost_context_callback_.is_null() ||
         lost_context_callback.is_null());
  lost_context_callback_ = lost_context_callback;
}

}  // namespace content
