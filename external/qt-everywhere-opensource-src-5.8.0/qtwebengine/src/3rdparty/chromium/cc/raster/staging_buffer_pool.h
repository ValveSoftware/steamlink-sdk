// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_STAGING_BUFFER_POOL_H_
#define CC_RASTER_STAGING_BUFFER_POOL_H_

#include <stdint.h>

#include <deque>
#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/output/context_provider.h"
#include "cc/resources/resource_provider.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace cc {
class Resource;
class RasterSource;
class ResourcePool;

struct StagingBuffer {
  StagingBuffer(const gfx::Size& size, ResourceFormat format);
  ~StagingBuffer();

  void DestroyGLResources(gpu::gles2::GLES2Interface* gl);
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    ResourceFormat format,
                    bool is_free) const;

  const gfx::Size size;
  const ResourceFormat format;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;
  base::TimeTicks last_usage;
  unsigned texture_id;
  unsigned image_id;
  unsigned query_id;
  uint64_t content_id;
};

class CC_EXPORT StagingBufferPool
    : public base::trace_event::MemoryDumpProvider {
 public:
  ~StagingBufferPool() final;

  StagingBufferPool(base::SequencedTaskRunner* task_runner,
                    ContextProvider* worker_context_provider,
                    ResourceProvider* resource_provider,
                    bool use_partial_raster,
                    int max_staging_buffer_usage_in_bytes);
  void Shutdown();

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  std::unique_ptr<StagingBuffer> AcquireStagingBuffer(
      const Resource* resource,
      uint64_t previous_content_id);
  void ReleaseStagingBuffer(std::unique_ptr<StagingBuffer> staging_buffer);

 private:
  void AddStagingBuffer(const StagingBuffer* staging_buffer,
                        ResourceFormat format);
  void RemoveStagingBuffer(const StagingBuffer* staging_buffer);
  void MarkStagingBufferAsFree(const StagingBuffer* staging_buffer);
  void MarkStagingBufferAsBusy(const StagingBuffer* staging_buffer);

  base::TimeTicks GetUsageTimeForLRUBuffer();
  void ScheduleReduceMemoryUsage();
  void ReduceMemoryUsage();
  void ReleaseBuffersNotUsedSince(base::TimeTicks time);

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
      const;
  void StagingStateAsValueInto(
      base::trace_event::TracedValue* staging_state) const;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ContextProvider* const worker_context_provider_;
  ResourceProvider* const resource_provider_;
  const bool use_partial_raster_;

  mutable base::Lock lock_;
  // |lock_| must be acquired when accessing the following members.
  using StagingBufferSet = std::set<const StagingBuffer*>;
  StagingBufferSet buffers_;
  using StagingBufferDeque = std::deque<std::unique_ptr<StagingBuffer>>;
  StagingBufferDeque free_buffers_;
  StagingBufferDeque busy_buffers_;
  const int max_staging_buffer_usage_in_bytes_;
  int staging_buffer_usage_in_bytes_;
  int free_staging_buffer_usage_in_bytes_;
  const base::TimeDelta staging_buffer_expiration_delay_;
  bool reduce_memory_usage_pending_;
  base::Closure reduce_memory_usage_callback_;

  base::WeakPtrFactory<StagingBufferPool> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(StagingBufferPool);
};

}  // namespace cc

#endif  // CC_RASTER_STAGING_BUFFER_POOL_H_
