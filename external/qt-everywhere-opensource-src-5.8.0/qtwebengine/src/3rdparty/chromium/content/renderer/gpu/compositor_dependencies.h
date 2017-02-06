// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_DEPENDENCIES_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_DEPENDENCIES_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class BeginFrameSource;
class ContextProvider;
class ImageSerializationProcessor;
class SharedBitmapManager;
class TaskGraphRunner;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace scheduler {
class RendererScheduler;
}

namespace content {

class CompositorDependencies {
 public:
  virtual bool IsGpuRasterizationForced() = 0;
  virtual bool IsGpuRasterizationEnabled() = 0;
  virtual bool IsAsyncWorkerContextEnabled() = 0;
  virtual int GetGpuRasterizationMSAASampleCount() = 0;
  virtual bool IsLcdTextEnabled() = 0;
  virtual bool IsDistanceFieldTextEnabled() = 0;
  virtual bool IsZeroCopyEnabled() = 0;
  virtual bool IsPartialRasterEnabled() = 0;
  virtual bool IsGpuMemoryBufferCompositorResourcesEnabled() = 0;
  virtual bool IsElasticOverscrollEnabled() = 0;
  virtual std::vector<unsigned> GetImageTextureTargets() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorMainThreadTaskRunner() = 0;
  // Returns null if the compositor is in single-threaded mode (ie. there is no
  // compositor thread).
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorImplThreadTaskRunner() = 0;
  virtual cc::SharedBitmapManager* GetSharedBitmapManager() = 0;
  virtual gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() = 0;
  virtual scheduler::RendererScheduler* GetRendererScheduler() = 0;
  // TODO(danakj): This should be part of RenderThreadImpl (or some API from it
  // to RenderWidget). But RenderThreadImpl is null in RenderViewTest.
  virtual std::unique_ptr<cc::BeginFrameSource> CreateExternalBeginFrameSource(
      int routing_id) = 0;
  virtual cc::ImageSerializationProcessor* GetImageSerializationProcessor() = 0;
  virtual cc::TaskGraphRunner* GetTaskGraphRunner() = 0;
  virtual bool AreImageDecodeTasksEnabled() = 0;
  virtual bool IsThreadedAnimationEnabled() = 0;

  virtual ~CompositorDependencies() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_DEPENDENCIES_H_
