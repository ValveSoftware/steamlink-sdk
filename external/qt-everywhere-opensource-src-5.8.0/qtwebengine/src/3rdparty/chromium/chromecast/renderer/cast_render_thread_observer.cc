// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_render_thread_observer.h"

#include "build/build_config.h"
#include "chromecast/renderer/media/capabilities_message_filter.h"
#include "chromecast/renderer/media/cma_message_filter_proxy.h"
#include "content/public/renderer/render_thread.h"

namespace chromecast {
namespace shell {

CastRenderThreadObserver::CastRenderThreadObserver() {
  content::RenderThread* thread = content::RenderThread::Get();
  thread->AddObserver(this);
  CreateCustomFilters();
}

CastRenderThreadObserver::~CastRenderThreadObserver() {
  // CastRenderThreadObserver outlives content::RenderThread.
  // No need to explicitly call RemoveObserver in teardown.
}

void CastRenderThreadObserver::CreateCustomFilters() {
  content::RenderThread* thread = content::RenderThread::Get();
#if !defined(OS_ANDROID)
  cma_message_filter_proxy_ =
      new media::CmaMessageFilterProxy(thread->GetIOMessageLoopProxy());
  thread->AddFilter(cma_message_filter_proxy_.get());
#endif  // !defined(OS_ANDROID)
  capabilities_message_filter_ = new CapabilitiesMessageFilter;
  thread->AddFilter(capabilities_message_filter_.get());
}

void CastRenderThreadObserver::OnRenderProcessShutdown() {
  content::RenderThread* thread = content::RenderThread::Get();
#if !defined(OS_ANDROID)
  if (cma_message_filter_proxy_.get()) {
    thread->RemoveFilter(cma_message_filter_proxy_.get());
    cma_message_filter_proxy_ = nullptr;
  }
#endif  // !defined(OS_ANDROID)
  if (capabilities_message_filter_.get()) {
    thread->RemoveFilter(capabilities_message_filter_.get());
    capabilities_message_filter_ = nullptr;
  }
}

}  // namespace shell
}  // namespace chromecast
