// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/gpu/SharedGpuContext.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"

namespace blink {

SharedGpuContext* SharedGpuContext::getInstanceForCurrentThread() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<SharedGpuContext>,
                                  threadSpecificInstance,
                                  new ThreadSpecific<SharedGpuContext>);
  return threadSpecificInstance;
}

SharedGpuContext::SharedGpuContext() : m_contextId(kNoSharedContext) {
  createContextProviderIfNeeded();
}

void SharedGpuContext::createContextProviderOnMainThread(
    WaitableEvent* waitableEvent) {
  DCHECK(isMainThread());
  Platform::ContextAttributes contextAttributes;
  contextAttributes.webGLVersion = 1;  // GLES2
  Platform::GraphicsInfo graphicsInfo;
  m_contextProvider =
      wrapUnique(Platform::current()->createOffscreenGraphicsContext3DProvider(
          contextAttributes, WebURL(), nullptr, &graphicsInfo));
  if (waitableEvent)
    waitableEvent->signal();
}

void SharedGpuContext::createContextProviderIfNeeded() {
  if (m_contextProvider &&
      m_contextProvider->contextGL()->GetGraphicsResetStatusKHR() ==
          GL_NO_ERROR)
    return;

  std::unique_ptr<WebGraphicsContext3DProvider> oldContextProvider =
      std::move(m_contextProvider);
  if (m_contextProviderFactory) {
    // This path should only be used in unit tests
    m_contextProvider = m_contextProviderFactory();
  } else if (isMainThread()) {
    m_contextProvider =
        wrapUnique(blink::Platform::current()
                       ->createSharedOffscreenGraphicsContext3DProvider());
  } else {
    // This synchronous round-trip to the main thread is the reason why
    // SharedGpuContext encasulates the context provider: so we only have to do
    // this once per thread.
    WaitableEvent waitableEvent;
    WebTaskRunner* taskRunner =
        Platform::current()->mainThread()->getWebTaskRunner();
    taskRunner->postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&SharedGpuContext::createContextProviderOnMainThread,
                        crossThreadUnretained(this),
                        crossThreadUnretained(&waitableEvent)));
    waitableEvent.wait();
    if (m_contextProvider && !m_contextProvider->bindToCurrentThread())
      m_contextProvider = nullptr;
  }

  if (m_contextProvider) {
    m_contextId++;
    // In the unlikely event of an overflow...
    if (m_contextId == kNoSharedContext)
      m_contextId++;
  } else {
    m_contextProvider = std::move(oldContextProvider);
  }
}

void SharedGpuContext::setContextProviderFactoryForTesting(
    ContextProviderFactory factory) {
  SharedGpuContext* thisPtr = getInstanceForCurrentThread();
  thisPtr->m_contextProvider.reset();
  thisPtr->m_contextProviderFactory = factory;
  thisPtr->createContextProviderIfNeeded();
}

unsigned SharedGpuContext::contextId() {
  if (!isValid())
    return kNoSharedContext;
  SharedGpuContext* thisPtr = getInstanceForCurrentThread();
  return thisPtr->m_contextId;
}

gpu::gles2::GLES2Interface* SharedGpuContext::gl() {
  if (isValid()) {
    SharedGpuContext* thisPtr = getInstanceForCurrentThread();
    return thisPtr->m_contextProvider->contextGL();
  }
  return nullptr;
}

GrContext* SharedGpuContext::gr() {
  if (isValid()) {
    SharedGpuContext* thisPtr = getInstanceForCurrentThread();
    return thisPtr->m_contextProvider->grContext();
  }
  return nullptr;
}

bool SharedGpuContext::isValid() {
  SharedGpuContext* thisPtr = getInstanceForCurrentThread();
  thisPtr->createContextProviderIfNeeded();
  if (!thisPtr->m_contextProvider)
    return false;
  return thisPtr->m_contextProvider->contextGL()->GetGraphicsResetStatusKHR() ==
         GL_NO_ERROR;
}

bool SharedGpuContext::isValidWithoutRestoring() {
  SharedGpuContext* thisPtr = getInstanceForCurrentThread();
  if (!thisPtr->m_contextProvider)
    return false;
  return thisPtr->m_contextProvider->contextGL()->GetGraphicsResetStatusKHR() ==
         GL_NO_ERROR;
}

}  // blink
