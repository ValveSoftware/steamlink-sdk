// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/raster_buffer_provider.h"

#include <stddef.h>

#include "base/trace_event/trace_event.h"
#include "cc/playback/raster_source.h"
#include "cc/raster/texture_compressor.h"
#include "cc/resources/platform_color.h"
#include "cc/resources/resource_format_utils.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace cc {

RasterBufferProvider::RasterBufferProvider() {}

RasterBufferProvider::~RasterBufferProvider() {}

namespace {

bool IsSupportedPlaybackToMemoryFormat(ResourceFormat format) {
  switch (format) {
    case RGBA_4444:
    case RGBA_8888:
    case BGRA_8888:
    case ETC1:
      return true;
    case ALPHA_8:
    case LUMINANCE_8:
    case RGB_565:
    case RED_8:
    case LUMINANCE_F16:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // anonymous namespace

// static
void RasterBufferProvider::PlaybackToMemory(
    void* memory,
    ResourceFormat format,
    const gfx::Size& size,
    size_t stride,
    const RasterSource* raster_source,
    const gfx::Rect& canvas_bitmap_rect,
    const gfx::Rect& canvas_playback_rect,
    float scale,
    const RasterSource::PlaybackSettings& playback_settings) {
  TRACE_EVENT0("disabled-by-default-cc.debug",
               "RasterBufferProvider::PlaybackToMemory");

  DCHECK(IsSupportedPlaybackToMemoryFormat(format)) << format;

  // Uses kPremul_SkAlphaType since the result is not known to be opaque.
  SkImageInfo info =
      SkImageInfo::MakeN32(size.width(), size.height(), kPremul_SkAlphaType);

  // Use unknown pixel geometry to disable LCD text.
  SkSurfaceProps surface_props(0, kUnknown_SkPixelGeometry);
  if (raster_source->CanUseLCDText()) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props = SkSurfaceProps(SkSurfaceProps::kLegacyFontHost_InitType);
  }

  if (!stride)
    stride = info.minRowBytes();
  DCHECK_GT(stride, 0u);

  switch (format) {
    case RGBA_8888:
    case BGRA_8888: {
      sk_sp<SkSurface> surface =
          SkSurface::MakeRasterDirect(info, memory, stride, &surface_props);
      raster_source->PlaybackToCanvas(surface->getCanvas(), canvas_bitmap_rect,
                                      canvas_playback_rect, scale,
                                      playback_settings);
      return;
    }
    case RGBA_4444:
    case ETC1: {
      sk_sp<SkSurface> surface = SkSurface::MakeRaster(info, &surface_props);
      // TODO(reveman): Improve partial raster support by reducing the size of
      // playback rect passed to PlaybackToCanvas. crbug.com/519070
      raster_source->PlaybackToCanvas(surface->getCanvas(), canvas_bitmap_rect,
                                      canvas_bitmap_rect, scale,
                                      playback_settings);

      if (format == ETC1) {
        TRACE_EVENT0("cc",
                     "RasterBufferProvider::PlaybackToMemory::CompressETC1");
        DCHECK_EQ(size.width() % 4, 0);
        DCHECK_EQ(size.height() % 4, 0);
        std::unique_ptr<TextureCompressor> texture_compressor =
            TextureCompressor::Create(TextureCompressor::kFormatETC1);
        SkPixmap pixmap;
        surface->peekPixels(&pixmap);
        texture_compressor->Compress(
            reinterpret_cast<const uint8_t*>(pixmap.addr()),
            reinterpret_cast<uint8_t*>(memory), size.width(), size.height(),
            TextureCompressor::kQualityHigh);
      } else {
        TRACE_EVENT0("cc",
                     "RasterBufferProvider::PlaybackToMemory::ConvertRGBA4444");
        SkImageInfo dst_info =
            info.makeColorType(ResourceFormatToClosestSkColorType(format));
        bool rv = surface->readPixels(dst_info, memory, stride, 0, 0);
        DCHECK(rv);
      }
      return;
    }
    case ALPHA_8:
    case LUMINANCE_8:
    case RGB_565:
    case RED_8:
    case LUMINANCE_F16:
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

bool RasterBufferProvider::ResourceFormatRequiresSwizzle(
    ResourceFormat format) {
  switch (format) {
    case RGBA_8888:
    case BGRA_8888:
      // Initialize resource using the preferred PlatformColor component
      // order and swizzle in the shader instead of in software.
      return !PlatformColor::SameComponentOrder(format);
    case RGBA_4444:
    case ETC1:
    case ALPHA_8:
    case LUMINANCE_8:
    case RGB_565:
    case RED_8:
    case LUMINANCE_F16:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace cc
