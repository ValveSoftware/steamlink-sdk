// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/zero_copy_raster_buffer_provider.h"

#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/platform_color.h"
#include "cc/resources/resource.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace cc {
namespace {

class RasterBufferImpl : public RasterBuffer {
 public:
  RasterBufferImpl(ResourceProvider* resource_provider,
                   const Resource* resource)
      : lock_(resource_provider, resource->id()), resource_(resource) {}

  // Overridden from RasterBuffer:
  void Playback(
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      uint64_t new_content_id,
      float scale,
      const RasterSource::PlaybackSettings& playback_settings) override {
    TRACE_EVENT0("cc", "ZeroCopyRasterBuffer::Playback");
    gfx::GpuMemoryBuffer* buffer = lock_.GetGpuMemoryBuffer();
    if (!buffer)
      return;

    DCHECK_EQ(1u, gfx::NumberOfPlanesForBufferFormat(buffer->GetFormat()));
    bool rv = buffer->Map();
    DCHECK(rv);
    DCHECK(buffer->memory(0));
    // RasterBufferProvider::PlaybackToMemory only supports unsigned strides.
    DCHECK_GE(buffer->stride(0), 0);

    // TODO(danakj): Implement partial raster with raster_dirty_rect.
    RasterBufferProvider::PlaybackToMemory(
        buffer->memory(0), resource_->format(), resource_->size(),
        buffer->stride(0), raster_source, raster_full_rect, raster_full_rect,
        scale, playback_settings);
    buffer->Unmap();
  }

 private:
  ResourceProvider::ScopedWriteLockGpuMemoryBuffer lock_;
  const Resource* resource_;

  DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
};

}  // namespace

// static
std::unique_ptr<RasterBufferProvider> ZeroCopyRasterBufferProvider::Create(
    ResourceProvider* resource_provider,
    ResourceFormat preferred_tile_format) {
  return base::WrapUnique<RasterBufferProvider>(
      new ZeroCopyRasterBufferProvider(resource_provider,
                                       preferred_tile_format));
}

ZeroCopyRasterBufferProvider::ZeroCopyRasterBufferProvider(
    ResourceProvider* resource_provider,
    ResourceFormat preferred_tile_format)
    : resource_provider_(resource_provider),
      preferred_tile_format_(preferred_tile_format) {}

ZeroCopyRasterBufferProvider::~ZeroCopyRasterBufferProvider() {}

std::unique_ptr<RasterBuffer>
ZeroCopyRasterBufferProvider::AcquireBufferForRaster(
    const Resource* resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  return base::WrapUnique<RasterBuffer>(
      new RasterBufferImpl(resource_provider_, resource));
}

void ZeroCopyRasterBufferProvider::ReleaseBufferForRaster(
    std::unique_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

void ZeroCopyRasterBufferProvider::OrderingBarrier() {
  // No need to sync resources as this provider does not use GL context.
}

ResourceFormat ZeroCopyRasterBufferProvider::GetResourceFormat(
    bool must_support_alpha) const {
  if (resource_provider_->IsResourceFormatSupported(preferred_tile_format_) &&
      (DoesResourceFormatSupportAlpha(preferred_tile_format_) ||
       !must_support_alpha)) {
    return preferred_tile_format_;
  }

  return resource_provider_->best_texture_format();
}

bool ZeroCopyRasterBufferProvider::GetResourceRequiresSwizzle(
    bool must_support_alpha) const {
  return ResourceFormatRequiresSwizzle(GetResourceFormat(must_support_alpha));
}

void ZeroCopyRasterBufferProvider::Shutdown() {}

}  // namespace cc
