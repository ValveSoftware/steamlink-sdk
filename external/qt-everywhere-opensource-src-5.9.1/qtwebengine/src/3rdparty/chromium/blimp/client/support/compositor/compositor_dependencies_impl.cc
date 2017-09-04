// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/support/compositor/compositor_dependencies_impl.h"

#include "blimp/client/support/compositor/blimp_context_provider.h"
#include "blimp/client/support/compositor/blimp_gpu_memory_buffer_manager.h"
#include "blimp/client/support/compositor/blimp_layer_tree_settings.h"
#include "cc/output/context_provider.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/trees/layer_tree_settings.h"

namespace blimp {
namespace client {

CompositorDependenciesImpl::CompositorDependenciesImpl()
    : gpu_memory_buffer_manager_(
          base::MakeUnique<BlimpGpuMemoryBufferManager>()),
      surface_manager_(base::MakeUnique<cc::SurfaceManager>()),
      next_surface_id_(0) {}

CompositorDependenciesImpl::~CompositorDependenciesImpl() = default;

gpu::GpuMemoryBufferManager*
CompositorDependenciesImpl::GetGpuMemoryBufferManager() {
  return gpu_memory_buffer_manager_.get();
}

cc::SurfaceManager* CompositorDependenciesImpl::GetSurfaceManager() {
  return surface_manager_.get();
}

cc::FrameSinkId CompositorDependenciesImpl::AllocateFrameSinkId() {
  return cc::FrameSinkId(++next_surface_id_, 0 /* sink_id */);
}

void CompositorDependenciesImpl::GetContextProviders(
    const CompositorDependencies::ContextProviderCallback& callback) {
  scoped_refptr<cc::ContextProvider> compositor_context =
      BlimpContextProvider::Create(gfx::kNullAcceleratedWidget,
                                   gpu_memory_buffer_manager_.get());

  // TODO(khushalsagar): Make a worker context and bind to the current thread.
  callback.Run(compositor_context, nullptr);
}

}  // namespace client
}  // namespace blimp
