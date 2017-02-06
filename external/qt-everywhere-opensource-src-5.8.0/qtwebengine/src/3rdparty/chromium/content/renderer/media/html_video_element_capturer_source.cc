// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/html_video_element_capturer_source.h"

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "media/base/limits.h"
#include "media/blink/webmediaplayer_impl.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkXfermode.h"

namespace {
const float kMinFramesPerSecond = 1.0;
}  // anonymous namespace

namespace content {

//static
std::unique_ptr<HtmlVideoElementCapturerSource>
HtmlVideoElementCapturerSource::CreateFromWebMediaPlayerImpl(
    blink::WebMediaPlayer* player,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner) {
  // Save histogram data so we can see how much HTML Video capture is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(WEBKIT_VIDEO_CAPTURE_STREAM);

  return base::WrapUnique(new HtmlVideoElementCapturerSource(
      static_cast<media::WebMediaPlayerImpl*>(player)->AsWeakPtr(),
      io_task_runner));
}

HtmlVideoElementCapturerSource::HtmlVideoElementCapturerSource(
    const base::WeakPtr<blink::WebMediaPlayer>& player,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : web_media_player_(player),
      io_task_runner_(io_task_runner),
      capture_frame_rate_(0.0),
      weak_factory_(this) {
  DCHECK(web_media_player_);
}

HtmlVideoElementCapturerSource::~HtmlVideoElementCapturerSource() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void HtmlVideoElementCapturerSource::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height,
    double max_requested_frame_rate,
    const VideoCaptureDeviceFormatsCB& callback) {
  DVLOG(3) << __FUNCTION__ << "{ max_requested_height = "
           << max_requested_height << "}) { max_requested_width = "
           << max_requested_width << "}) { max_requested_frame_rate = "
           << max_requested_frame_rate << "})";
  DCHECK(thread_checker_.CalledOnValidThread());

  // WebMediaPlayer has a setRate() but can't be read back.
  // TODO(mcasas): Add getRate() to WMPlayer and/or fix the spec to allow users
  // to specify it.
  const media::VideoCaptureFormat format(
      web_media_player_->naturalSize(),
      MediaStreamVideoSource::kDefaultFrameRate,
      media::PIXEL_FORMAT_I420);
  media::VideoCaptureFormats formats;
  formats.push_back(format);
  callback.Run(formats);
}

void HtmlVideoElementCapturerSource::StartCapture(
    const media::VideoCaptureParams& params,
    const VideoCaptureDeliverFrameCB& new_frame_callback,
    const RunningCallback& running_callback) {
  DVLOG(3) << __FUNCTION__ << " requested "
           << media::VideoCaptureFormat::ToString(params.requested_format);
  DCHECK(params.requested_format.IsValid());
  DCHECK(thread_checker_.CalledOnValidThread());

  running_callback_ = running_callback;
  if (!web_media_player_ || !web_media_player_->hasVideo()) {
    running_callback_.Run(false);
    return;
  }
  const blink::WebSize resolution = web_media_player_->naturalSize();
  surface_ =
      SkSurface::MakeRasterN32Premul(resolution.width, resolution.height);
  if (!surface_) {
    running_callback_.Run(false);
    return;
  }

  new_frame_callback_ = new_frame_callback;
  // Force |capture_frame_rate_| to be in between k{Min,Max}FramesPerSecond.
  capture_frame_rate_ =
      std::max(kMinFramesPerSecond,
               std::min(static_cast<float>(media::limits::kMaxFramesPerSecond),
                        params.requested_format.frame_rate));

  running_callback_.Run(true);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&HtmlVideoElementCapturerSource::sendNewFrame,
                            weak_factory_.GetWeakPtr()));
}

void HtmlVideoElementCapturerSource::StopCapture() {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  running_callback_.Reset();
  new_frame_callback_.Reset();
  next_capture_time_ = base::TimeTicks();
}

void HtmlVideoElementCapturerSource::sendNewFrame() {
  DVLOG(3) << __FUNCTION__;
  TRACE_EVENT0("video", "HtmlVideoElementCapturerSource::sendNewFrame");
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!web_media_player_ || new_frame_callback_.is_null())
    return;

  const base::TimeTicks current_time = base::TimeTicks::Now();
  const blink::WebSize resolution = web_media_player_->naturalSize();

  SkCanvas* canvas = surface_->getCanvas();
  web_media_player_->paint(
      canvas, blink::WebRect(0, 0, resolution.width, resolution.height),
      0xFF /* alpha */, SkXfermode::kSrc_Mode);
  DCHECK_NE(kUnknown_SkColorType, canvas->imageInfo().colorType());
  DCHECK_EQ(canvas->imageInfo().width(), resolution.width);
  DCHECK_EQ(canvas->imageInfo().height(), resolution.height);

  const SkBitmap bitmap = skia::ReadPixels(canvas);
  DCHECK_NE(kUnknown_SkColorType, bitmap.colorType());
  DCHECK(!bitmap.drawsNothing());
  DCHECK(bitmap.getPixels());
  if (bitmap.colorType() != kN32_SkColorType) {
    DLOG(ERROR) << "Only supported capture format is ARGB";
    return;
  }

  scoped_refptr<media::VideoFrame> frame = frame_pool_.CreateFrame(
      media::PIXEL_FORMAT_I420, resolution, gfx::Rect(resolution), resolution,
      base::TimeTicks::Now() - base::TimeTicks());
  DCHECK(frame);

  if (libyuv::ConvertToI420(static_cast<uint8*>(bitmap.getPixels()),
                            bitmap.getSize(),
                            frame->data(media::VideoFrame::kYPlane),
                            frame->stride(media::VideoFrame::kYPlane),
                            frame->data(media::VideoFrame::kUPlane),
                            frame->stride(media::VideoFrame::kUPlane),
                            frame->data(media::VideoFrame::kVPlane),
                            frame->stride(media::VideoFrame::kVPlane),
                            0 /* crop_x */,
                            0 /* crop_y */,
                            bitmap.info().width(),
                            bitmap.info().height(),
                            frame->coded_size().width(),
                            frame->coded_size().height(),
                            libyuv::kRotate0,
                            libyuv::FOURCC_ARGB) == 0) {
    // Success!
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(new_frame_callback_, frame, current_time));
  }

  // Calculate the time in the future where the next frame should be created.
  const base::TimeDelta frame_interval =
      base::TimeDelta::FromMicroseconds(1E6 / capture_frame_rate_);
  if (next_capture_time_.is_null()) {
    next_capture_time_ = current_time + frame_interval;
  } else {
    next_capture_time_ += frame_interval;
    // Don't accumulate any debt if we are lagging behind - just post next frame
    // immediately and continue as normal.
    if (next_capture_time_ < current_time)
      next_capture_time_ = current_time;
  }
  // Schedule next capture.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&HtmlVideoElementCapturerSource::sendNewFrame,
                            weak_factory_.GetWeakPtr()),
      next_capture_time_ - current_time);
}

}  // namespace content
