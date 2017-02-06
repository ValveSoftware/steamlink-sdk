// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/surface_utils.h"

#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/single_release_callback.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "components/display_compositor/gl_helper.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_ANDROID)
#include "content/browser/renderer_host/compositor_impl_android.h"
#else
#include "content/browser/compositor/image_transport_factory.h"
#include "ui/compositor/compositor.h"  // nogncheck
#endif

namespace {

#if !defined(OS_ANDROID) || defined(USE_AURA)
void CopyFromCompositingSurfaceFinished(
    const content::ReadbackRequestCallback& callback,
    std::unique_ptr<cc::SingleReleaseCallback> release_callback,
    std::unique_ptr<SkBitmap> bitmap,
    std::unique_ptr<SkAutoLockPixels> bitmap_pixels_lock,
    bool result) {
  bitmap_pixels_lock.reset();

  gpu::SyncToken sync_token;
  if (result) {
    display_compositor::GLHelper* gl_helper =
        content::ImageTransportFactory::GetInstance()->GetGLHelper();
    if (gl_helper)
      gl_helper->GenerateSyncToken(&sync_token);
  }
  const bool lost_resource = !sync_token.HasData();
  release_callback->Run(sync_token, lost_resource);

  callback.Run(*bitmap,
               result ? content::READBACK_SUCCESS : content::READBACK_FAILED);
}
#endif

// TODO(wjmaclean): There is significant overlap between
// PrepareTextureCopyOutputResult and CopyFromCompositingSurfaceFinished in
// this file, and the versions in RenderWidgetHostViewAndroid. They should
// be merged. See https://crbug.com/582955
void PrepareTextureCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const SkColorType color_type,
    const content::ReadbackRequestCallback& callback,
    std::unique_ptr<cc::CopyOutputResult> result) {
#if defined(OS_ANDROID) && !defined(USE_AURA)
  // TODO(wjmaclean): See if there's an equivalent pathway for Android and
  // implement it here.
  callback.Run(SkBitmap(), content::READBACK_FAILED);
#else
  DCHECK(result->HasTexture());
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, SkBitmap(), content::READBACK_FAILED));

  // TODO(siva.gunturi): We should be able to validate the format here using
  // GLHelper::IsReadbackConfigSupported before we processs the result.
  // See crbug.com/415682 and crbug.com/415131.
  std::unique_ptr<SkBitmap> bitmap(new SkBitmap);
  if (!bitmap->tryAllocPixels(SkImageInfo::Make(
          dst_size_in_pixel.width(), dst_size_in_pixel.height(), color_type,
          kOpaque_SkAlphaType))) {
    scoped_callback_runner.Reset(base::Bind(
        callback, SkBitmap(), content::READBACK_BITMAP_ALLOCATION_FAILURE));
    return;
  }

  content::ImageTransportFactory* factory =
      content::ImageTransportFactory::GetInstance();
  display_compositor::GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;

  std::unique_ptr<SkAutoLockPixels> bitmap_pixels_lock(
      new SkAutoLockPixels(*bitmap));
  uint8_t* pixels = static_cast<uint8_t*>(bitmap->getPixels());

  cc::TextureMailbox texture_mailbox;
  std::unique_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());

  ignore_result(scoped_callback_runner.Release());

  gl_helper->CropScaleReadbackAndCleanMailbox(
      texture_mailbox.mailbox(), texture_mailbox.sync_token(), result->size(),
      gfx::Rect(result->size()), dst_size_in_pixel, pixels, color_type,
      base::Bind(&CopyFromCompositingSurfaceFinished, callback,
                 base::Passed(&release_callback), base::Passed(&bitmap),
                 base::Passed(&bitmap_pixels_lock)),
      display_compositor::GLHelper::SCALER_QUALITY_GOOD);
#endif
}

void PrepareBitmapCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const SkColorType preferred_color_type,
    const content::ReadbackRequestCallback& callback,
    std::unique_ptr<cc::CopyOutputResult> result) {
  SkColorType color_type = preferred_color_type;
  if (color_type != kN32_SkColorType && color_type != kAlpha_8_SkColorType) {
    // Switch back to default colortype if format not supported.
    color_type = kN32_SkColorType;
  }
  DCHECK(result->HasBitmap());
  std::unique_ptr<SkBitmap> source = result->TakeBitmap();
  DCHECK(source);
  SkBitmap scaled_bitmap;
  if (source->width() != dst_size_in_pixel.width() ||
      source->height() != dst_size_in_pixel.height()) {
    scaled_bitmap = skia::ImageOperations::Resize(
        *source, skia::ImageOperations::RESIZE_BEST, dst_size_in_pixel.width(),
        dst_size_in_pixel.height());
  } else {
    scaled_bitmap = *source;
  }
  if (color_type == kN32_SkColorType) {
    DCHECK_EQ(scaled_bitmap.colorType(), kN32_SkColorType);
    callback.Run(scaled_bitmap, content::READBACK_SUCCESS);
    return;
  }
  DCHECK_EQ(color_type, kAlpha_8_SkColorType);
  // The software path currently always returns N32 bitmap regardless of the
  // |color_type| we ask for.
  DCHECK_EQ(scaled_bitmap.colorType(), kN32_SkColorType);
  // Paint |scaledBitmap| to alpha-only |grayscale_bitmap|.
  SkBitmap grayscale_bitmap;
  bool success = grayscale_bitmap.tryAllocPixels(
      SkImageInfo::MakeA8(scaled_bitmap.width(), scaled_bitmap.height()));
  if (!success) {
    callback.Run(SkBitmap(), content::READBACK_BITMAP_ALLOCATION_FAILURE);
    return;
  }
  SkCanvas canvas(grayscale_bitmap);
  SkPaint paint;
  paint.setColorFilter(SkLumaColorFilter::Make());
  canvas.drawBitmap(scaled_bitmap, SkIntToScalar(0), SkIntToScalar(0), &paint);
  callback.Run(grayscale_bitmap, content::READBACK_SUCCESS);
}

}  // namespace

namespace content {

std::unique_ptr<cc::SurfaceIdAllocator> CreateSurfaceIdAllocator() {
#if defined(OS_ANDROID)
  return CompositorImpl::CreateSurfaceIdAllocator();
#else
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  return factory->GetContextFactory()->CreateSurfaceIdAllocator();
#endif
}

cc::SurfaceManager* GetSurfaceManager() {
#if defined(OS_ANDROID)
  return CompositorImpl::GetSurfaceManager();
#else
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  if (factory == NULL)
    return nullptr;
  return factory->GetSurfaceManager();
#endif
}

void CopyFromCompositingSurfaceHasResult(
    const gfx::Size& dst_size_in_pixel,
    const SkColorType color_type,
    const ReadbackRequestCallback& callback,
    std::unique_ptr<cc::CopyOutputResult> result) {
  if (result->IsEmpty() || result->size().IsEmpty()) {
    callback.Run(SkBitmap(), READBACK_FAILED);
    return;
  }

  gfx::Size output_size_in_pixel;
  if (dst_size_in_pixel.IsEmpty())
    output_size_in_pixel = result->size();
  else
    output_size_in_pixel = dst_size_in_pixel;

  if (result->HasTexture()) {
    // GPU-accelerated path
    PrepareTextureCopyOutputResult(output_size_in_pixel, color_type, callback,
                                   std::move(result));
    return;
  }

  DCHECK(result->HasBitmap());
  // Software path
  PrepareBitmapCopyOutputResult(output_size_in_pixel, color_type, callback,
                                std::move(result));
}

}  // namespace content
