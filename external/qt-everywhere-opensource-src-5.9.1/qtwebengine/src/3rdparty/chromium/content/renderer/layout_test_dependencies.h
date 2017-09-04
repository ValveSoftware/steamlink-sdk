// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_LAYOUT_TEST_DEPENDENCIES_H_
#define CONTENT_RENDER_LAYOUT_TEST_DEPENDENCIES_H_

#include <stdint.h>
#include <memory>

#include "base/memory/ref_counted.h"

namespace cc {
class ContextProvider;
class CopyOutputRequest;
class CompositorFrameSink;
class SwapPromise;
}

namespace gpu {
class GpuChannelHost;
class GpuMemoryBufferManager;
}

namespace content {
class CompositorDependencies;

// This class allows injection of LayoutTest-specific behaviour to the
// RenderThreadImpl.
class LayoutTestDependencies {
 public:
  virtual std::unique_ptr<cc::CompositorFrameSink> CreateCompositorFrameSink(
      int32_t routing_id,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel,
      scoped_refptr<cc::ContextProvider> compositor_context_provider,
      scoped_refptr<cc::ContextProvider> worker_context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      CompositorDependencies* deps) = 0;

  // Returns a SwapPromise which should be queued for the next compositor frame.
  virtual std::unique_ptr<cc::SwapPromise> RequestCopyOfOutput(
      int32_t routing_id,
      std::unique_ptr<cc::CopyOutputRequest> request) = 0;
};

}  // namespace content

#endif  // CONTENT_RENDER_LAYOUT_TEST_DEPENDENCIES_H_
