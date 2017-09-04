// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_CONTEXT_PROVIDER_H_
#define SERVICES_UI_PUBLIC_CPP_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "cc/output/context_provider.h"
#include "mojo/public/cpp/system/core.h"

namespace gpu {
class GpuChannelHost;
}

namespace service_manager {
class Connector;
}

namespace ui {

class GLES2Context;

class ContextProvider : public cc::ContextProvider {
 public:
  explicit ContextProvider(scoped_refptr<gpu::GpuChannelHost> gpu_channel_host);

  // cc::ContextProvider implementation.
  bool BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  cc::ContextCacheController* CacheController() override;
  void InvalidateGrContext(uint32_t state) override;
  base::Lock* GetLock() override;
  gpu::Capabilities ContextCapabilities() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override {}

 protected:
  friend class base::RefCountedThreadSafe<ContextProvider>;
  ~ContextProvider() override;

 private:
  std::unique_ptr<GLES2Context> context_;
  std::unique_ptr<cc::ContextCacheController> cache_controller_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;

  DISALLOW_COPY_AND_ASSIGN(ContextProvider);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_CONTEXT_PROVIDER_H_
