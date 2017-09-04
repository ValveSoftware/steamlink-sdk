// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/PlatformExport.h"
#include "wtf/ThreadSpecific.h"

#include <functional>
#include <memory>

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}
class GrContext;

namespace blink {

class WaitableEvent;
class WebGraphicsContext3DProvider;

// SharedGpuContext provides access to a thread-specific GPU context
// that is shared by many callsites throughout the thread.
// When on the main thread, provides access to the same context as
// Platform::createSharedOffscreenGraphicsContext3DProvider
class PLATFORM_EXPORT SharedGpuContext {
 public:
  // The contextId is incremented each time a new underlying context
  // is created. For example, when the context is lost, then restored.
  // User code can rely on this Id to determine whether long-lived
  // gpu resources are still alive in the current context.
  static unsigned contextId();
  static gpu::gles2::GLES2Interface*
  gl();                    // May re-create context if context was lost
  static GrContext* gr();  // May re-create context if context was lost
  static bool isValid();   // May re-create context if context was lost
  static bool isValidWithoutRestoring();
  typedef std::function<std::unique_ptr<WebGraphicsContext3DProvider>()>
      ContextProviderFactory;
  static void setContextProviderFactoryForTesting(ContextProviderFactory);

  enum {
    kNoSharedContext = 0,
  };

 private:
  static SharedGpuContext* getInstanceForCurrentThread();

  SharedGpuContext();
  void createContextProviderOnMainThread(WaitableEvent*);
  void createContextProviderIfNeeded();

  ContextProviderFactory m_contextProviderFactory = nullptr;
  std::unique_ptr<WebGraphicsContext3DProvider> m_contextProvider;
  unsigned m_contextId;
  friend class WTF::ThreadSpecific<SharedGpuContext>;
};

}  // blink
