// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/skcanvas_video_renderer.h"

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkCanvas.h"

// Skia internal format depends on a platform. On Android it is ABGR, on others
// it is ARGB.
#if SK_B32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_R32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define LIBYUV_I420_TO_ARGB libyuv::I420ToARGB
#define LIBYUV_I422_TO_ARGB libyuv::I422ToARGB
#elif SK_R32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_B32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define LIBYUV_I420_TO_ARGB libyuv::I420ToABGR
#define LIBYUV_I422_TO_ARGB libyuv::I422ToABGR
#else
#error Unexpected Skia ARGB_8888 layout!
#endif

namespace media {

static bool IsYUV(media::VideoFrame::Format format) {
  return format == media::VideoFrame::YV12 ||
         format == media::VideoFrame::YV16 ||
         format == media::VideoFrame::I420 ||
         format == media::VideoFrame::YV12A ||
         format == media::VideoFrame::YV12J ||
         format == media::VideoFrame::YV24;
}

static bool IsYUVOrNative(media::VideoFrame::Format format) {
  return IsYUV(format) || format == media::VideoFrame::NATIVE_TEXTURE;
}

// Converts a VideoFrame containing YUV data to a SkBitmap containing RGB data.
//
// |bitmap| will be (re)allocated to match the dimensions of |video_frame|.
static void ConvertVideoFrameToBitmap(
    const scoped_refptr<media::VideoFrame>& video_frame,
    SkBitmap* bitmap) {
  DCHECK(IsYUVOrNative(video_frame->format()))
      << video_frame->format();
  if (IsYUV(video_frame->format())) {
    DCHECK_EQ(video_frame->stride(media::VideoFrame::kUPlane),
              video_frame->stride(media::VideoFrame::kVPlane));
  }

  // Check if |bitmap| needs to be (re)allocated.
  if (bitmap->isNull() ||
      bitmap->width() != video_frame->visible_rect().width() ||
      bitmap->height() != video_frame->visible_rect().height()) {
    bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                      video_frame->visible_rect().width(),
                      video_frame->visible_rect().height());
    bitmap->allocPixels();
    bitmap->setIsVolatile(true);
  }

  bitmap->lockPixels();

  size_t y_offset = 0;
  size_t uv_offset = 0;
  if (IsYUV(video_frame->format())) {
    int y_shift = (video_frame->format() == media::VideoFrame::YV16) ? 0 : 1;
    // Use the "left" and "top" of the destination rect to locate the offset
    // in Y, U and V planes.
    y_offset = (video_frame->stride(media::VideoFrame::kYPlane) *
                video_frame->visible_rect().y()) +
                video_frame->visible_rect().x();
    // For format YV12, there is one U, V value per 2x2 block.
    // For format YV16, there is one U, V value per 2x1 block.
    uv_offset = (video_frame->stride(media::VideoFrame::kUPlane) *
                (video_frame->visible_rect().y() >> y_shift)) +
                (video_frame->visible_rect().x() >> 1);
  }

  switch (video_frame->format()) {
    case media::VideoFrame::YV12:
    case media::VideoFrame::I420:
      LIBYUV_I420_TO_ARGB(
          video_frame->data(media::VideoFrame::kYPlane) + y_offset,
          video_frame->stride(media::VideoFrame::kYPlane),
          video_frame->data(media::VideoFrame::kUPlane) + uv_offset,
          video_frame->stride(media::VideoFrame::kUPlane),
          video_frame->data(media::VideoFrame::kVPlane) + uv_offset,
          video_frame->stride(media::VideoFrame::kVPlane),
          static_cast<uint8*>(bitmap->getPixels()),
          bitmap->rowBytes(),
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height());
      break;

    case media::VideoFrame::YV12J:
      media::ConvertYUVToRGB32(
          video_frame->data(media::VideoFrame::kYPlane) + y_offset,
          video_frame->data(media::VideoFrame::kUPlane) + uv_offset,
          video_frame->data(media::VideoFrame::kVPlane) + uv_offset,
          static_cast<uint8*>(bitmap->getPixels()),
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height(),
          video_frame->stride(media::VideoFrame::kYPlane),
          video_frame->stride(media::VideoFrame::kUPlane),
          bitmap->rowBytes(),
          media::YV12J);
      break;

    case media::VideoFrame::YV16:
      LIBYUV_I422_TO_ARGB(
          video_frame->data(media::VideoFrame::kYPlane) + y_offset,
          video_frame->stride(media::VideoFrame::kYPlane),
          video_frame->data(media::VideoFrame::kUPlane) + uv_offset,
          video_frame->stride(media::VideoFrame::kUPlane),
          video_frame->data(media::VideoFrame::kVPlane) + uv_offset,
          video_frame->stride(media::VideoFrame::kVPlane),
          static_cast<uint8*>(bitmap->getPixels()),
          bitmap->rowBytes(),
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height());
      break;

    case media::VideoFrame::YV12A:
      // Since libyuv doesn't support YUVA, fallback to media, which is not ARM
      // optimized.
      // TODO(fbarchard, mtomasz): Use libyuv, then copy the alpha channel.
      media::ConvertYUVAToARGB(
          video_frame->data(media::VideoFrame::kYPlane) + y_offset,
          video_frame->data(media::VideoFrame::kUPlane) + uv_offset,
          video_frame->data(media::VideoFrame::kVPlane) + uv_offset,
          video_frame->data(media::VideoFrame::kAPlane),
          static_cast<uint8*>(bitmap->getPixels()),
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height(),
          video_frame->stride(media::VideoFrame::kYPlane),
          video_frame->stride(media::VideoFrame::kUPlane),
          video_frame->stride(media::VideoFrame::kAPlane),
          bitmap->rowBytes(),
          media::YV12);
      break;

    case media::VideoFrame::YV24:
      libyuv::I444ToARGB(
          video_frame->data(media::VideoFrame::kYPlane) + y_offset,
          video_frame->stride(media::VideoFrame::kYPlane),
          video_frame->data(media::VideoFrame::kUPlane) + uv_offset,
          video_frame->stride(media::VideoFrame::kUPlane),
          video_frame->data(media::VideoFrame::kVPlane) + uv_offset,
          video_frame->stride(media::VideoFrame::kVPlane),
          static_cast<uint8*>(bitmap->getPixels()),
          bitmap->rowBytes(),
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height());
#if SK_R32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_B32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
      libyuv::ARGBToABGR(
          static_cast<uint8*>(bitmap->getPixels()),
          bitmap->rowBytes(),
          static_cast<uint8*>(bitmap->getPixels()),
          bitmap->rowBytes(),
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height());
#endif
      break;

    case media::VideoFrame::NATIVE_TEXTURE:
      DCHECK_EQ(video_frame->format(), media::VideoFrame::NATIVE_TEXTURE);
      video_frame->ReadPixelsFromNativeTexture(*bitmap);
      break;

    default:
      NOTREACHED();
      break;
  }
  bitmap->notifyPixelsChanged();
  bitmap->unlockPixels();
}

SkCanvasVideoRenderer::SkCanvasVideoRenderer()
    : last_frame_timestamp_(media::kNoTimestamp()) {
}

SkCanvasVideoRenderer::~SkCanvasVideoRenderer() {}

void SkCanvasVideoRenderer::Paint(media::VideoFrame* video_frame,
                                  SkCanvas* canvas,
                                  const gfx::RectF& dest_rect,
                                  uint8 alpha) {
  if (alpha == 0) {
    return;
  }

  SkRect dest;
  dest.set(dest_rect.x(), dest_rect.y(), dest_rect.right(), dest_rect.bottom());

  SkPaint paint;
  paint.setAlpha(alpha);

  // Paint black rectangle if there isn't a frame available or the
  // frame has an unexpected format.
  if (!video_frame || !IsYUVOrNative(video_frame->format())) {
    canvas->drawRect(dest, paint);
    return;
  }

  // Check if we should convert and update |last_frame_|.
  if (last_frame_.isNull() ||
      video_frame->timestamp() != last_frame_timestamp_) {
    ConvertVideoFrameToBitmap(video_frame, &last_frame_);
    last_frame_timestamp_ = video_frame->timestamp();
  }

  // Paint using |last_frame_|.
  paint.setFilterLevel(SkPaint::kLow_FilterLevel);
  canvas->drawBitmapRect(last_frame_, NULL, dest, &paint);
}

}  // namespace media
