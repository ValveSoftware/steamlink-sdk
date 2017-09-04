// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SUPPORT_COMPOSITOR_COMPOSITOR_DEPENDENCIES_IMPL_H_
#define BLIMP_CLIENT_SUPPORT_COMPOSITOR_COMPOSITOR_DEPENDENCIES_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"

namespace cc {
class ContextProvider;
}

namespace blimp {
namespace client {

class BlimpGpuMemoryBufferManager;

class CompositorDependenciesImpl : public CompositorDependencies {
 public:
  CompositorDependenciesImpl();
  ~CompositorDependenciesImpl() override;

  // CompositorDependencies implementation.
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::SurfaceManager* GetSurfaceManager() override;
  cc::FrameSinkId AllocateFrameSinkId() override;
  void GetContextProviders(const ContextProviderCallback& callback) override;

 private:
  std::unique_ptr<BlimpGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  std::unique_ptr<cc::SurfaceManager> surface_manager_;
  uint32_t next_surface_id_;

  DISALLOW_COPY_AND_ASSIGN(CompositorDependenciesImpl);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_SUPPORT_COMPOSITOR_COMPOSITOR_DEPENDENCIES_IMPL_H_
