// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_FRAME_SINK_H_
#define BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_FRAME_SINK_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "blimp/client/core/compositor/blimp_compositor_frame_sink_proxy.h"
#include "cc/output/compositor_frame_sink.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace cc {
class BeginFrameSource;
class ContextProvider;
}  // namespace cc

namespace blimp {
namespace client {

// This class is created on the main thread, but then becomes bound to the
// compositor thread and will be destroyed there (soon, crbug.com/640730).
class BlimpCompositorFrameSink : public cc::CompositorFrameSink,
                                 public BlimpCompositorFrameSinkProxyClient {
 public:
  BlimpCompositorFrameSink(
      scoped_refptr<cc::ContextProvider> compositor_context_provider,
      scoped_refptr<cc::ContextProvider> worker_context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      cc::SharedBitmapManager* shared_bitmap_manager,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      base::WeakPtr<BlimpCompositorFrameSinkProxy> main_thread_proxy);

  ~BlimpCompositorFrameSink() override;

  // cc::CompositorFrameSink implementation.
  bool BindToClient(cc::CompositorFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(cc::CompositorFrame frame) override;

  // BlimpCompositorFrameSinkProxyClient implementation.
  void ReclaimCompositorResources(
      const cc::ReturnedResourceArray& resources) override;
  void SubmitCompositorFrameAck() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  base::WeakPtr<BlimpCompositorFrameSinkProxy> main_thread_proxy_;

  // This CompositorFrameSink is responsible for providing the BeginFrameSource
  // to drive frame creation.  This will be built on the compositor impl thread
  // at BindToClient call time.
  std::unique_ptr<cc::BeginFrameSource> begin_frame_source_;

  base::WeakPtrFactory<BlimpCompositorFrameSink> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpCompositorFrameSink);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_FRAME_SINK_H_
