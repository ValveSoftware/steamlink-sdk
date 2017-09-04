// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_interstitial_ui.h"

#include "media/base/media_resources.h"
#include "media/base/video_frame.h"
#include "media/base/video_renderer_sink.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/vector_icons_public.h"

namespace media {

namespace {

gfx::Size GetRotatedVideoSize(VideoRotation rotation, gfx::Size natural_size) {
  if (rotation == VIDEO_ROTATION_90 || rotation == VIDEO_ROTATION_270)
    return gfx::Size(natural_size.height(), natural_size.width());
  return natural_size;
}

}  // namespace

RemotingInterstitialUI::RemotingInterstitialUI(
    VideoRendererSink* video_renderer_sink,
    const PipelineMetadata& pipeline_metadata)
    : video_renderer_sink_(video_renderer_sink),
      pipeline_metadata_(pipeline_metadata) {
  DCHECK(pipeline_metadata_.has_video);
  pipeline_metadata_.natural_size = GetRotatedVideoSize(
      pipeline_metadata_.video_rotation, pipeline_metadata_.natural_size);
}

RemotingInterstitialUI::~RemotingInterstitialUI() {}

scoped_refptr<VideoFrame> RemotingInterstitialUI::GetInterstitial(
    const SkBitmap& background_image,
    bool is_remoting_successful) {
  const gfx::Size canvas_size =
      gfx::Size(background_image.width(), background_image.height());
  DCHECK(canvas_size == pipeline_metadata_.natural_size);

  color_utils::HSL shift = {-1, 0, 0.2};  // Make monochromatic.
  SkBitmap modified_bitmap =
      SkBitmapOperations::CreateHSLShiftedBitmap(background_image, shift);

  SkCanvas canvas(modified_bitmap);

  // Blur the background image.
  SkScalar sigma = SkDoubleToScalar(10);
  SkPaint paint_blur;
  paint_blur.setImageFilter(SkBlurImageFilter::Make(sigma, sigma, nullptr));
  canvas.saveLayer(0, &paint_blur);
  canvas.restore();

  // Create SkPaint for text and icon bitmap.
  // After |paint| draws, the new canvas should look like this:
  //       _________________________________
  //       |                                |
  //       |                                |
  //       |             [   ]              |
  //       |        Casting Video...        |
  //       |                                |
  //       |________________________________|
  //
  // Both text and icon are centered horizontally. Together, they are
  // centered vertically.
  SkPaint paint;
  int text_size = SkIntToScalar(30);
  paint.setAntiAlias(true);
  paint.setFilterQuality(kHigh_SkFilterQuality);
  paint.setColor(SK_ColorLTGRAY);
  paint.setTypeface(SkTypeface::MakeFromName(
      "sans", SkFontStyle::FromOldStyle(SkTypeface::kNormal)));
  paint.setTextSize(text_size);

  // Draw the appropriate text.
  const std::string remote_playback_message =
      is_remoting_successful
          ? GetLocalizedStringUTF8(MEDIA_REMOTING_CASTING_VIDEO_TEXT)
          : GetLocalizedStringUTF8(MEDIA_REMOTING_CAST_ERROR_TEXT);
  size_t display_text_width = paint.measureText(remote_playback_message.data(),
                                                remote_playback_message.size());
  SkScalar sk_text_offset_x = (canvas_size.width() - display_text_width) / 2.0;
  SkScalar sk_text_offset_y = (canvas_size.height() / 2.0) + text_size;
  canvas.drawText(remote_playback_message.data(),
                  remote_playback_message.size(), sk_text_offset_x,
                  sk_text_offset_y, paint);

  // Draw the appropriate Cast icon.
  gfx::VectorIconId current_icon =
      is_remoting_successful ? gfx::VectorIconId::MEDIA_ROUTER_ACTIVE
                             : gfx::VectorIconId::MEDIA_ROUTER_WARNING;
  gfx::ImageSkia icon_image =
      gfx::CreateVectorIcon(current_icon, 65, SK_ColorLTGRAY);
  const SkBitmap* icon_bitmap = icon_image.bitmap();
  SkScalar sk_image_offset_x = (canvas_size.width() - icon_image.width()) / 2.0;
  SkScalar sk_image_offset_y =
      (canvas_size.height() / 2.0) - icon_image.height();
  canvas.drawBitmap(*icon_bitmap, sk_image_offset_x, sk_image_offset_y, &paint);

  // Create a new VideoFrame, copy the bitmap, then return it.
  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, canvas_size, gfx::Rect(canvas_size),
      canvas_size, base::TimeDelta());
  modified_bitmap.lockPixels();
  media::CopyRGBToVideoFrame(
      reinterpret_cast<uint8_t*>(modified_bitmap.getPixels()),
      modified_bitmap.rowBytes(),
      gfx::Rect(canvas_size.width(), canvas_size.height()), video_frame.get());
  modified_bitmap.unlockPixels();
  return video_frame;
}

void RemotingInterstitialUI::ShowInterstitial(bool is_remoting_successful) {
  if (!video_renderer_sink_)
    return;

  // TODO(xjz): Provide poster image, if available, rather than a blank, black
  // image.
  SkBitmap background_image;
  const gfx::Size size = pipeline_metadata_.natural_size;
  background_image.allocN32Pixels(size.width(), size.height());
  background_image.eraseColor(SK_ColorBLACK);

  const scoped_refptr<VideoFrame> interstitial =
      GetInterstitial(background_image, is_remoting_successful);
  if (!interstitial)
    return;

  video_renderer_sink_->PaintSingleFrame(interstitial);
}

}  // namespace media
