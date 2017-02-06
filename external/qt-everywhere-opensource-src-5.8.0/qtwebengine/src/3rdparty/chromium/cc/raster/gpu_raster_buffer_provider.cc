// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/gpu_raster_buffer_provider.h"

#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/playback/image_hijack_canvas.h"
#include "cc/playback/raster_source.h"
#include "cc/raster/scoped_gpu_raster.h"
#include "cc/resources/resource.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/core/SkMultiPictureDraw.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {
namespace {

static sk_sp<SkPicture> PlaybackToPicture(
    const RasterSource* raster_source,
    bool resource_has_previous_content,
    const gfx::Size& resource_size,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    float scale,
    const RasterSource::PlaybackSettings& playback_settings) {
  // GPU raster doesn't do low res tiles, so should always include images.
  DCHECK(!playback_settings.skip_images);

  gfx::Rect playback_rect = raster_full_rect;
  if (resource_has_previous_content) {
    playback_rect.Intersect(raster_dirty_rect);
  }
  DCHECK(!playback_rect.IsEmpty())
      << "Why are we rastering a tile that's not dirty?";

  // Log a histogram of the percentage of pixels that were saved due to
  // partial raster.
  float full_rect_size = raster_full_rect.size().GetArea();
  if (full_rect_size > 0) {
    float fraction_partial_rastered =
        static_cast<float>(playback_rect.size().GetArea()) / full_rect_size;
    float fraction_saved = 1.0f - fraction_partial_rastered;

    UMA_HISTOGRAM_PERCENTAGE("Renderer4.PartialRasterPercentageSaved.Gpu",
                             100.0f * fraction_saved);
  }

  // Play back raster_source into temp SkPicture.
  SkPictureRecorder recorder;
  const int flags = SkPictureRecorder::kComputeSaveLayerInfo_RecordFlag;
  sk_sp<SkCanvas> canvas = sk_ref_sp(recorder.beginRecording(
      resource_size.width(), resource_size.height(), NULL, flags));
  canvas->save();

  // The GPU image decode controller assumes that Skia is done with an image
  // when playback is complete. However, in this case, where we play back to a
  // picture, we don't actually finish with the images until the picture is
  // rasterized later. This can cause lifetime issues in the GPU image decode
  // controller. To avoid this, we disable the image hijack canvas (and image
  // decode controller) for this playback step, instead enabling it for the
  // later picture rasterization.
  RasterSource::PlaybackSettings settings = playback_settings;
  settings.use_image_hijack_canvas = false;
  raster_source->PlaybackToCanvas(canvas.get(), raster_full_rect, playback_rect,
                                  scale, settings);
  canvas->restore();
  return recorder.finishRecordingAsPicture();
}

static void RasterizePicture(SkPicture* picture,
                             ContextProvider* context_provider,
                             ResourceProvider::ScopedWriteLockGL* resource_lock,
                             bool async_worker_context_enabled,
                             bool use_distance_field_text,
                             bool can_use_lcd_text,
                             int msaa_sample_count,
                             ImageDecodeController* image_decode_controller,
                             bool use_image_hijack_canvas) {
  ScopedGpuRaster gpu_raster(context_provider);

  ResourceProvider::ScopedSkSurfaceProvider scoped_surface(
      context_provider, resource_lock, async_worker_context_enabled,
      use_distance_field_text, can_use_lcd_text, msaa_sample_count);
  SkSurface* sk_surface = scoped_surface.sk_surface();
  // Allocating an SkSurface will fail after a lost context.  Pretend we
  // rasterized, as the contents of the resource don't matter anymore.
  if (!sk_surface)
    return;

  // As we did not use the image hijack canvas during the initial playback to
  // |picture| (see PlaybackToPicture), we must enable it here if requested.
  SkCanvas* canvas = sk_surface->getCanvas();
  std::unique_ptr<ImageHijackCanvas> hijack_canvas;
  if (use_image_hijack_canvas) {
    DCHECK(image_decode_controller);
    const SkImageInfo& info = canvas->imageInfo();
    hijack_canvas.reset(new ImageHijackCanvas(info.width(), info.height(),
                                              image_decode_controller));
    SkIRect raster_bounds;
    canvas->getClipDeviceBounds(&raster_bounds);
    hijack_canvas->clipRect(SkRect::MakeFromIRect(raster_bounds));
    hijack_canvas->setMatrix(canvas->getTotalMatrix());
    hijack_canvas->addCanvas(canvas);

    // Replace canvas with our ImageHijackCanvas which is wrapping it.
    canvas = hijack_canvas.get();
  }

  SkMultiPictureDraw multi_picture_draw;
  multi_picture_draw.add(canvas, picture);
  multi_picture_draw.draw(false);
}

}  // namespace

GpuRasterBufferProvider::RasterBufferImpl::RasterBufferImpl(
    GpuRasterBufferProvider* client,
    ResourceProvider* resource_provider,
    ResourceId resource_id,
    bool async_worker_context_enabled,
    bool resource_has_previous_content)
    : client_(client),
      lock_(resource_provider, resource_id, async_worker_context_enabled),
      resource_has_previous_content_(resource_has_previous_content) {
  client_->pending_raster_buffers_.insert(this);
}

GpuRasterBufferProvider::RasterBufferImpl::~RasterBufferImpl() {
  client_->pending_raster_buffers_.erase(this);
}

void GpuRasterBufferProvider::RasterBufferImpl::Playback(
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    float scale,
    const RasterSource::PlaybackSettings& playback_settings) {
  TRACE_EVENT0("cc", "GpuRasterBuffer::Playback");
  client_->PlaybackOnWorkerThread(&lock_, sync_token_,
                                  resource_has_previous_content_, raster_source,
                                  raster_full_rect, raster_dirty_rect,
                                  new_content_id, scale, playback_settings);
}

GpuRasterBufferProvider::GpuRasterBufferProvider(
    ContextProvider* compositor_context_provider,
    ContextProvider* worker_context_provider,
    ResourceProvider* resource_provider,
    bool use_distance_field_text,
    int gpu_rasterization_msaa_sample_count,
    bool async_worker_context_enabled)
    : compositor_context_provider_(compositor_context_provider),
      worker_context_provider_(worker_context_provider),
      resource_provider_(resource_provider),
      use_distance_field_text_(use_distance_field_text),
      msaa_sample_count_(gpu_rasterization_msaa_sample_count),
      async_worker_context_enabled_(async_worker_context_enabled) {
  DCHECK(compositor_context_provider);
  DCHECK(worker_context_provider);
}

GpuRasterBufferProvider::~GpuRasterBufferProvider() {
  DCHECK(pending_raster_buffers_.empty());
}

std::unique_ptr<RasterBuffer> GpuRasterBufferProvider::AcquireBufferForRaster(
    const Resource* resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  bool resource_has_previous_content =
      resource_content_id && resource_content_id == previous_content_id;
  return base::WrapUnique(new RasterBufferImpl(
      this, resource_provider_, resource->id(), async_worker_context_enabled_,
      resource_has_previous_content));
}

void GpuRasterBufferProvider::ReleaseBufferForRaster(
    std::unique_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

void GpuRasterBufferProvider::OrderingBarrier() {
  TRACE_EVENT0("cc", "GpuRasterBufferProvider::OrderingBarrier");

  gpu::gles2::GLES2Interface* gl = compositor_context_provider_->ContextGL();
  if (async_worker_context_enabled_) {
    GLuint64 fence = gl->InsertFenceSyncCHROMIUM();
    gl->OrderingBarrierCHROMIUM();

    gpu::SyncToken sync_token;
    gl->GenUnverifiedSyncTokenCHROMIUM(fence, sync_token.GetData());

    DCHECK(sync_token.HasData() ||
           gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR);

    for (RasterBufferImpl* buffer : pending_raster_buffers_)
      buffer->set_sync_token(sync_token);
  } else {
    gl->OrderingBarrierCHROMIUM();
  }
  pending_raster_buffers_.clear();
}

ResourceFormat GpuRasterBufferProvider::GetResourceFormat(
    bool must_support_alpha) const {
  return resource_provider_->best_render_buffer_format();
}

bool GpuRasterBufferProvider::GetResourceRequiresSwizzle(
    bool must_support_alpha) const {
  // This doesn't require a swizzle because we rasterize to the correct format.
  return false;
}

void GpuRasterBufferProvider::Shutdown() {
  pending_raster_buffers_.clear();
}

void GpuRasterBufferProvider::PlaybackOnWorkerThread(
    ResourceProvider::ScopedWriteLockGL* resource_lock,
    const gpu::SyncToken& sync_token,
    bool resource_has_previous_content,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    float scale,
    const RasterSource::PlaybackSettings& playback_settings) {
  ContextProvider::ScopedContextLock scoped_context(worker_context_provider_);
  gpu::gles2::GLES2Interface* gl = scoped_context.ContextGL();
  DCHECK(gl);

  if (async_worker_context_enabled_) {
    // Early out if sync token is invalid. This happens if the compositor
    // context was lost before ScheduleTasks was called.
    if (!sync_token.HasData())
      return;
    // Synchronize with compositor.
    gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }

  sk_sp<SkPicture> picture = PlaybackToPicture(
      raster_source, resource_has_previous_content, resource_lock->size(),
      raster_full_rect, raster_dirty_rect, scale, playback_settings);

  // Turn on distance fields for layers that have ever animated.
  bool use_distance_field_text =
      use_distance_field_text_ ||
      raster_source->ShouldAttemptToUseDistanceFieldText();

  RasterizePicture(picture.get(), worker_context_provider_, resource_lock,
                   async_worker_context_enabled_, use_distance_field_text,
                   raster_source->CanUseLCDText(), msaa_sample_count_,
                   raster_source->image_decode_controller(),
                   playback_settings.use_image_hijack_canvas);

  const uint64_t fence_sync = gl->InsertFenceSyncCHROMIUM();

  // Barrier to sync worker context output to cc context.
  gl->OrderingBarrierCHROMIUM();

  // Generate sync token after the barrier for cross context synchronization.
  gpu::SyncToken resource_sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, resource_sync_token.GetData());
  resource_lock->set_sync_token(resource_sync_token);
  resource_lock->set_synchronized(!async_worker_context_enabled_);
}

}  // namespace cc
