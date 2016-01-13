// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/gpu/context_provider_in_process.h"

#include <set>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "cc/output/managed_memory_policy.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"

namespace webkit {
namespace gpu {

class ContextProviderInProcess::LostContextCallbackProxy
    : public blink::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  explicit LostContextCallbackProxy(ContextProviderInProcess* provider)
      : provider_(provider) {
    provider_->context3d_->setContextLostCallback(this);
  }

  virtual ~LostContextCallbackProxy() {
    provider_->context3d_->setContextLostCallback(NULL);
  }

  virtual void onContextLost() {
    provider_->OnLostContext();
  }

 private:
  ContextProviderInProcess* provider_;
};

// static
scoped_refptr<ContextProviderInProcess> ContextProviderInProcess::Create(
    scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d,
    const std::string& debug_name) {
  if (!context3d)
    return NULL;
  return new ContextProviderInProcess(context3d.Pass(), debug_name);
}

// static
scoped_refptr<ContextProviderInProcess>
ContextProviderInProcess::CreateOffscreen(
    bool lose_context_when_out_of_memory) {
  blink::WebGraphicsContext3D::Attributes attributes;
  attributes.depth = false;
  attributes.stencil = true;
  attributes.antialias = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;

  return Create(
      WebGraphicsContext3DInProcessCommandBufferImpl::CreateOffscreenContext(
          attributes, lose_context_when_out_of_memory),
      "Offscreen");
}

ContextProviderInProcess::ContextProviderInProcess(
    scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d,
    const std::string& debug_name)
    : context3d_(context3d.Pass()),
      destroyed_(false),
      debug_name_(debug_name) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(context3d_);
  context_thread_checker_.DetachFromThread();
}

ContextProviderInProcess::~ContextProviderInProcess() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());
}

blink::WebGraphicsContext3D* ContextProviderInProcess::WebContext3D() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_.get();
}

bool ContextProviderInProcess::BindToCurrentThread() {
  DCHECK(context3d_);

  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (lost_context_callback_proxy_)
    return true;

  if (!context3d_->makeContextCurrent())
    return false;

  InitializeCapabilities();

  std::string unique_context_name =
      base::StringPrintf("%s-%p", debug_name_.c_str(), context3d_.get());
  context3d_->pushGroupMarkerEXT(unique_context_name.c_str());

  lost_context_callback_proxy_.reset(new LostContextCallbackProxy(this));
  return true;
}

void ContextProviderInProcess::InitializeCapabilities() {
  capabilities_.gpu = context3d_->GetImplementation()->capabilities();
}

cc::ContextProvider::Capabilities
ContextProviderInProcess::ContextCapabilities() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());
  return capabilities_;
}

::gpu::gles2::GLES2Interface* ContextProviderInProcess::ContextGL() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_->GetGLInterface();
}

::gpu::ContextSupport* ContextProviderInProcess::ContextSupport() {
  DCHECK(context3d_);
  if (!lost_context_callback_proxy_)
    return NULL;  // Not bound to anything.

  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_->GetContextSupport();
}

class GrContext* ContextProviderInProcess::GrContext() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(
      new webkit::gpu::GrContextForWebGraphicsContext3D(context3d_.get()));
  return gr_context_->get();
}

bool ContextProviderInProcess::IsContextLost() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_->isContextLost();
}

void ContextProviderInProcess::VerifyContexts() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (context3d_->isContextLost())
    OnLostContext();
}

void ContextProviderInProcess::DeleteCachedResources() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    gr_context_->FreeGpuResources();
}

void ContextProviderInProcess::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  {
    base::AutoLock lock(destroyed_lock_);
    if (destroyed_)
      return;
    destroyed_ = true;
  }
  if (!lost_context_callback_.is_null())
    base::ResetAndReturn(&lost_context_callback_).Run();
  if (gr_context_)
    gr_context_->OnLostContext();
}

bool ContextProviderInProcess::DestroyedOnMainThread() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  base::AutoLock lock(destroyed_lock_);
  return destroyed_;
}

void ContextProviderInProcess::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(lost_context_callback_.is_null() ||
         lost_context_callback.is_null());
  lost_context_callback_ = lost_context_callback;
}

void ContextProviderInProcess::SetMemoryPolicyChangedCallback(
    const MemoryPolicyChangedCallback& memory_policy_changed_callback) {
  // There's no memory manager for the in-process implementation.
}

}  // namespace gpu
}  // namespace webkit
