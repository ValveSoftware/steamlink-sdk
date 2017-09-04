// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_CONTEXT_PROVIDER_H_
#define CC_OUTPUT_CONTEXT_PROVIDER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "cc/base/cc_export.h"
#include "cc/output/context_cache_controller.h"
#include "gpu/command_buffer/common/capabilities.h"

class GrContext;

namespace base {
class Lock;
}

namespace gpu {
class ContextSupport;
namespace gles2 { class GLES2Interface; }
}

namespace cc {

class ContextProvider : public base::RefCountedThreadSafe<ContextProvider> {
 public:
  // Hold an instance of this lock while using a context across multiple
  // threads. This only works for ContextProviders that will return a valid
  // lock from GetLock(), so is not always supported. Most use of
  // ContextProvider should be single-thread only on the thread that
  // BindToCurrentThread is run on.
  class CC_EXPORT ScopedContextLock {
   public:
    explicit ScopedContextLock(ContextProvider* context_provider);
    ~ScopedContextLock();

    gpu::gles2::GLES2Interface* ContextGL() {
      return context_provider_->ContextGL();
    }

   private:
    ContextProvider* const context_provider_;
    base::AutoLock context_lock_;
    std::unique_ptr<ContextCacheController::ScopedBusy> busy_;
  };

  // Bind the 3d context to the current thread. This should be called before
  // accessing the contexts. Calling it more than once should have no effect.
  // Once this function has been called, the class should only be accessed
  // from the same thread unless the function has some explicitly specified
  // rules for access on a different thread. See SetupLockOnMainThread(), which
  // can be used to provide access from multiple threads.
  virtual bool BindToCurrentThread() = 0;

  virtual gpu::gles2::GLES2Interface* ContextGL() = 0;
  virtual gpu::ContextSupport* ContextSupport() = 0;
  virtual class GrContext* GrContext() = 0;
  virtual ContextCacheController* CacheController() = 0;

  // Invalidates the cached OpenGL state in GrContext.
  // See skia GrContext::resetContext for details.
  virtual void InvalidateGrContext(uint32_t state) = 0;

  // Returns the capabilities of the currently bound 3d context.
  virtual gpu::Capabilities ContextCapabilities() = 0;

  // Sets a callback to be called when the context is lost. This should be
  // called from the same thread that the context is bound to. To avoid races,
  // it should be called before BindToCurrentThread().
  // Implementation note: Implementations must avoid post-tasking the provided
  // |lost_context_callback| directly as clients expect the method to not be
  // called once they call SetLostContextCallback() again with a different
  // callback.
  typedef base::Closure LostContextCallback;
  virtual void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) = 0;

  // Below are helper methods for ScopedContextLock. Use that instead of calling
  // these directly.
  //
  // Detaches debugging thread checkers to allow use of the provider from the
  // current thread. This can be called on any thread.
  virtual void DetachFromThread() {}
  // Returns the lock that should be held if using this context from multiple
  // threads. This can be called on any thread.
  virtual base::Lock* GetLock() = 0;

 protected:
  friend class base::RefCountedThreadSafe<ContextProvider>;
  virtual ~ContextProvider() {}
};

}  // namespace cc

#endif  // CC_OUTPUT_CONTEXT_PROVIDER_H_
