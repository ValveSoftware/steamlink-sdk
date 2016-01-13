// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_CONTEXT_PROVIDER_COMMAND_BUFFER
#define CONTENT_COMMON_GPU_CLIENT_CONTEXT_PROVIDER_COMMAND_BUFFER

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/output/context_provider.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "webkit/common/gpu/context_provider_web_context.h"

namespace webkit {
namespace gpu {
class GrContextForWebGraphicsContext3D;
}
}

namespace content {

// Implementation of cc::ContextProvider that provides a
// WebGraphicsContext3DCommandBufferImpl context and a GrContext.
class CONTENT_EXPORT ContextProviderCommandBuffer
    : NON_EXPORTED_BASE(public webkit::gpu::ContextProviderWebContext) {
 public:
  static scoped_refptr<ContextProviderCommandBuffer> Create(
      scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d,
      const std::string& debug_name);

  CommandBufferProxyImpl* GetCommandBufferProxy();

  // ContextProviderWebContext implementation.
  virtual WebGraphicsContext3DCommandBufferImpl* WebContext3D() OVERRIDE;

  // cc::ContextProvider implementation.
  virtual bool BindToCurrentThread() OVERRIDE;
  virtual gpu::gles2::GLES2Interface* ContextGL() OVERRIDE;
  virtual gpu::ContextSupport* ContextSupport() OVERRIDE;
  virtual class GrContext* GrContext() OVERRIDE;
  virtual Capabilities ContextCapabilities() OVERRIDE;
  virtual bool IsContextLost() OVERRIDE;
  virtual void VerifyContexts() OVERRIDE;
  virtual void DeleteCachedResources() OVERRIDE;
  virtual bool DestroyedOnMainThread() OVERRIDE;
  virtual void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) OVERRIDE;
  virtual void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& memory_policy_changed_callback)
      OVERRIDE;

 protected:
  ContextProviderCommandBuffer(
      scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d,
      const std::string& debug_name);
  virtual ~ContextProviderCommandBuffer();

  void OnLostContext();
  void OnMemoryAllocationChanged(const gpu::MemoryAllocation& allocation);

 private:
  void InitializeCapabilities();

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d_;
  scoped_ptr<webkit::gpu::GrContextForWebGraphicsContext3D> gr_context_;

  cc::ContextProvider::Capabilities capabilities_;
  std::string debug_name_;

  LostContextCallback lost_context_callback_;
  MemoryPolicyChangedCallback memory_policy_changed_callback_;

  base::Lock main_thread_lock_;
  bool destroyed_;

  class LostContextCallbackProxy;
  scoped_ptr<LostContextCallbackProxy> lost_context_callback_proxy_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_CONTEXT_PROVIDER_COMMAND_BUFFER
