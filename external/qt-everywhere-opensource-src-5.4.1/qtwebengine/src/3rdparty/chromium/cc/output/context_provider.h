// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_CONTEXT_PROVIDER_H_
#define CC_OUTPUT_CONTEXT_PROVIDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "gpu/command_buffer/common/capabilities.h"

class GrContext;

namespace gpu {
class ContextSupport;
namespace gles2 { class GLES2Interface; }
}

namespace cc {
struct ManagedMemoryPolicy;

class ContextProvider : public base::RefCountedThreadSafe<ContextProvider> {
 public:
  // Bind the 3d context to the current thread. This should be called before
  // accessing the contexts. Calling it more than once should have no effect.
  // Once this function has been called, the class should only be accessed
  // from the same thread.
  virtual bool BindToCurrentThread() = 0;

  virtual gpu::gles2::GLES2Interface* ContextGL() = 0;
  virtual gpu::ContextSupport* ContextSupport() = 0;
  virtual class GrContext* GrContext() = 0;

  struct Capabilities {
    gpu::Capabilities gpu;
    size_t max_transfer_buffer_usage_bytes;

    CC_EXPORT Capabilities();
  };

  // Returns the capabilities of the currently bound 3d context.
  virtual Capabilities ContextCapabilities() = 0;

  // Checks if the context is currently known to be lost.
  virtual bool IsContextLost() = 0;

  // Ask the provider to check if the contexts are valid or lost. If they are,
  // this should invalidate the provider so that it can be replaced with a new
  // one.
  virtual void VerifyContexts() = 0;

  // Delete all cached gpu resources.
  virtual void DeleteCachedResources() = 0;

  // A method to be called from the main thread that should return true if
  // the context inside the provider is no longer valid.
  virtual bool DestroyedOnMainThread() = 0;

  // Sets a callback to be called when the context is lost. This should be
  // called from the same thread that the context is bound to. To avoid races,
  // it should be called before BindToCurrentThread(), or VerifyContexts()
  // should be called after setting the callback.
  typedef base::Closure LostContextCallback;
  virtual void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) = 0;

  // Sets a callback to be called when the memory policy changes. This should be
  // called from the same thread that the context is bound to.
  typedef base::Callback<void(const ManagedMemoryPolicy& policy)>
      MemoryPolicyChangedCallback;
  virtual void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& memory_policy_changed_callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ContextProvider>;
  virtual ~ContextProvider() {}
};

}  // namespace cc

#endif  // CC_OUTPUT_CONTEXT_PROVIDER_H_
