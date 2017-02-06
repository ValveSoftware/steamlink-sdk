// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/resources/resource_provider.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace cc {
class ContextProvider;

class CC_EXPORT GpuRasterBufferProvider : public RasterBufferProvider {
 public:
  GpuRasterBufferProvider(ContextProvider* compositor_context_provider,
                          ContextProvider* worker_context_provider,
                          ResourceProvider* resource_provider,
                          bool use_distance_field_text,
                          int gpu_rasterization_msaa_sample_count,
                          bool async_worker_context_enabled);
  ~GpuRasterBufferProvider() override;

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override;
  void ReleaseBufferForRaster(std::unique_ptr<RasterBuffer> buffer) override;
  void OrderingBarrier() override;
  ResourceFormat GetResourceFormat(bool must_support_alpha) const override;
  bool GetResourceRequiresSwizzle(bool must_support_alpha) const override;
  void Shutdown() override;

  void PlaybackOnWorkerThread(
      ResourceProvider::ScopedWriteLockGL* resource_lock,
      const gpu::SyncToken& sync_token,
      bool resource_has_previous_content,
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      uint64_t new_content_id,
      float scale,
      const RasterSource::PlaybackSettings& playback_settings);

 private:
  class RasterBufferImpl : public RasterBuffer {
   public:
    RasterBufferImpl(GpuRasterBufferProvider* client,
                     ResourceProvider* resource_provider,
                     ResourceId resource_id,
                     bool async_worker_context_enabled,
                     bool resource_has_previous_content);
    ~RasterBufferImpl() override;

    // Overridden from RasterBuffer:
    void Playback(
        const RasterSource* raster_source,
        const gfx::Rect& raster_full_rect,
        const gfx::Rect& raster_dirty_rect,
        uint64_t new_content_id,
        float scale,
        const RasterSource::PlaybackSettings& playback_settings) override;

    void set_sync_token(const gpu::SyncToken& sync_token) {
      sync_token_ = sync_token;
    }

   private:
    GpuRasterBufferProvider* const client_;
    ResourceProvider::ScopedWriteLockGL lock_;
    const bool resource_has_previous_content_;

    gpu::SyncToken sync_token_;

    DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
  };

  ContextProvider* const compositor_context_provider_;
  ContextProvider* const worker_context_provider_;
  ResourceProvider* const resource_provider_;
  const bool use_distance_field_text_;
  const int msaa_sample_count_;
  const bool async_worker_context_enabled_;

  std::set<RasterBufferImpl*> pending_raster_buffers_;

  DISALLOW_COPY_AND_ASSIGN(GpuRasterBufferProvider);
};

}  // namespace cc

#endif  // CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_
