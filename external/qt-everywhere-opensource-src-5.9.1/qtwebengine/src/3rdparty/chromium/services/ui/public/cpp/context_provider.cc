// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/context_provider.h"

#include <stdint.h>

#include "base/logging.h"
#include "cc/output/context_cache_controller.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/ui/public/cpp/gles2_context.h"

namespace ui {

ContextProvider::ContextProvider(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host)
    : gpu_channel_host_(std::move(gpu_channel_host)) {}

bool ContextProvider::BindToCurrentThread() {
  context_ = GLES2Context::CreateOffscreenContext(
      gpu_channel_host_, base::ThreadTaskRunnerHandle::Get());
  if (context_) {
    cache_controller_.reset(new cc::ContextCacheController(
        context_->context_support(), base::ThreadTaskRunnerHandle::Get()));
  }
  return !!context_;
}

gpu::gles2::GLES2Interface* ContextProvider::ContextGL() {
  return context_->interface();
}

gpu::ContextSupport* ContextProvider::ContextSupport() {
  if (!context_)
    return NULL;
  return context_->context_support();
}

class GrContext* ContextProvider::GrContext() {
  return NULL;
}

cc::ContextCacheController* ContextProvider::CacheController() {
  return cache_controller_.get();
}

void ContextProvider::InvalidateGrContext(uint32_t state) {}

gpu::Capabilities ContextProvider::ContextCapabilities() {
  return gpu::Capabilities();
}

base::Lock* ContextProvider::GetLock() {
  // This context provider is not used on multiple threads.
  NOTREACHED();
  return nullptr;
}

ContextProvider::~ContextProvider() {
}

}  // namespace ui
