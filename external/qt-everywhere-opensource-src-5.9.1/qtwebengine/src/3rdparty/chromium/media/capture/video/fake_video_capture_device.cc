// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fake_video_capture_device.h"

#include <stddef.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/string.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/codec/png_codec.h"

namespace media {

// Sweep at 600 deg/sec.
static const float kPacmanAngularVelocity = 600;
// Beep every 500 ms.
static const int kBeepInterval = 500;
// Gradient travels from bottom to top in 5 seconds.
static const float kGradientFrequency = 1.f / 5;

static const double kMinZoom = 100.0;
static const double kMaxZoom = 400.0;
static const double kZoomStep = 1.0;

// Starting from top left, -45 deg gradient.  Value at point (row, column) is
// calculated as (top_left_value + (row + column) * step) % MAX_VALUE, where
// step is MAX_VALUE / (width + height).  MAX_VALUE is 255 (for 8 bit per
// component) or 65535 for Y16.
// This is handy for pixel tests where we use the squares to verify rendering.
void DrawGradientSquares(VideoPixelFormat frame_format,
                         uint8_t* const pixels,
                         base::TimeDelta elapsed_time,
                         const gfx::Size& frame_size) {
  const int width = frame_size.width();
  const int height = frame_size.height();
  const int side = width / 16;  // square side length.
  DCHECK(side);
  const gfx::Point squares[] = {{0, 0},
                                {width - side, 0},
                                {0, height - side},
                                {width - side, height - side}};
  const float start =
      fmod(65536 * elapsed_time.InSecondsF() * kGradientFrequency, 65536);
  const float color_step = 65535 / static_cast<float>(width + height);
  for (const auto& corner : squares) {
    for (int y = corner.y(); y < corner.y() + side; ++y) {
      for (int x = corner.x(); x < corner.x() + side; ++x) {
        const unsigned int value =
            static_cast<unsigned int>(start + (x + y) * color_step) & 0xFFFF;
        size_t offset = (y * width) + x;
        switch (frame_format) {
          case PIXEL_FORMAT_Y16:
            pixels[offset * sizeof(uint16_t)] = value & 0xFF;
            pixels[offset * sizeof(uint16_t) + 1] = value >> 8;
            break;
          case PIXEL_FORMAT_ARGB:
            pixels[offset * sizeof(uint32_t) + 1] = value >> 8;
            pixels[offset * sizeof(uint32_t) + 2] = value >> 8;
            pixels[offset * sizeof(uint32_t) + 3] = value >> 8;
            break;
          default:
            pixels[offset] = value >> 8;
            break;
        }
      }
    }
  }
}

void DrawPacman(VideoPixelFormat frame_format,
                uint8_t* const data,
                base::TimeDelta elapsed_time,
                float frame_rate,
                const gfx::Size& frame_size,
                double zoom) {
  // |kN32_SkColorType| stands for the appropriate RGBA/BGRA format.
  const SkColorType colorspace = (frame_format == PIXEL_FORMAT_ARGB)
                                     ? kN32_SkColorType
                                     : kAlpha_8_SkColorType;
  // Skia doesn't support 16 bit alpha rendering, so we 8 bit alpha and then use
  // this as high byte values in 16 bit pixels.
  const SkImageInfo info = SkImageInfo::Make(
      frame_size.width(), frame_size.height(), colorspace, kOpaque_SkAlphaType);
  SkBitmap bitmap;
  bitmap.setInfo(info);
  bitmap.setPixels(data);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  SkCanvas canvas(bitmap);

  const SkScalar unscaled_zoom = zoom / 100.f;
  SkMatrix matrix;
  matrix.setScale(unscaled_zoom, unscaled_zoom, frame_size.width() / 2,
                  frame_size.height() / 2);
  canvas.setMatrix(matrix);

  // Equalize Alpha_8 that has light green background while RGBA has white.
  if (frame_format == PIXEL_FORMAT_ARGB) {
    const SkRect full_frame =
        SkRect::MakeWH(frame_size.width(), frame_size.height());
    paint.setARGB(255, 0, 127, 0);
    canvas.drawRect(full_frame, paint);
  }
  paint.setColor(SK_ColorGREEN);

  // Draw a sweeping circle to show an animation.
  const float end_angle =
      fmod(kPacmanAngularVelocity * elapsed_time.InSecondsF(), 361);
  const int radius = std::min(frame_size.width(), frame_size.height()) / 4;
  const SkRect rect = SkRect::MakeXYWH(frame_size.width() / 2 - radius,
                                       frame_size.height() / 2 - radius,
                                       2 * radius, 2 * radius);
  canvas.drawArc(rect, 0, end_angle, true, paint);

  // Draw current time.
  const int milliseconds = elapsed_time.InMilliseconds() % 1000;
  const int seconds = elapsed_time.InSeconds() % 60;
  const int minutes = elapsed_time.InMinutes() % 60;
  const int hours = elapsed_time.InHours();
  const int frame_count = elapsed_time.InMilliseconds() * frame_rate / 1000;

  const std::string time_string =
      base::StringPrintf("%d:%02d:%02d:%03d %d", hours, minutes, seconds,
                         milliseconds, frame_count);
  canvas.scale(3, 3);
  canvas.drawText(time_string.data(), time_string.length(), 30, 20, paint);

  if (frame_format == PIXEL_FORMAT_Y16) {
    // Use 8 bit bitmap rendered to first half of the buffer as high byte values
    // for the whole buffer. Low byte values are not important.
    for (int i = frame_size.GetArea() - 1; i >= 0; --i)
      data[i * 2 + 1] = data[i];
  }
  DrawGradientSquares(frame_format, data, elapsed_time, frame_size);
}

// Creates a PNG-encoded frame and sends it back to |callback|. The other
// parameters are used to replicate the PacMan rendering.
void DoTakeFakePhoto(VideoCaptureDevice::TakePhotoCallback callback,
                     const VideoCaptureFormat& capture_format,
                     base::TimeDelta elapsed_time,
                     float fake_capture_rate,
                     uint32_t zoom) {
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[VideoFrame::AllocationSize(
      PIXEL_FORMAT_ARGB, capture_format.frame_size)]);

  DrawPacman(PIXEL_FORMAT_ARGB, buffer.get(), elapsed_time, fake_capture_rate,
             capture_format.frame_size, zoom);

  mojom::BlobPtr blob = mojom::Blob::New();
  const bool result = gfx::PNGCodec::Encode(
      buffer.get(), gfx::PNGCodec::FORMAT_RGBA, capture_format.frame_size,
      capture_format.frame_size.width() * 4, true /* discard_transparency */,
      std::vector<gfx::PNGCodec::Comment>(), &blob->data);
  DCHECK(result);

  blob->mime_type = "image/png";
  callback.Run(std::move(blob));
}

FakeVideoCaptureDevice::FakeVideoCaptureDevice(BufferOwnership buffer_ownership,
                                               float fake_capture_rate,
                                               VideoPixelFormat pixel_format)
    : buffer_ownership_(buffer_ownership),
      fake_capture_rate_(fake_capture_rate),
      pixel_format_(pixel_format),
      current_zoom_(kMinZoom),
      weak_factory_(this) {}

FakeVideoCaptureDevice::~FakeVideoCaptureDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void FakeVideoCaptureDevice::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  client_ = std::move(client);

  // Incoming |params| can be none of the supported formats, so we get the
  // closest thing rounded up. TODO(mcasas): Use the |params|, if they belong to
  // the supported ones, when http://crbug.com/309554 is verified.
  capture_format_.frame_rate = fake_capture_rate_;
  if (params.requested_format.frame_size.width() > 1280)
    capture_format_.frame_size.SetSize(1920, 1080);
  else if (params.requested_format.frame_size.width() > 640)
    capture_format_.frame_size.SetSize(1280, 720);
  else if (params.requested_format.frame_size.width() > 320)
    capture_format_.frame_size.SetSize(640, 480);
  else if (params.requested_format.frame_size.width() > 96)
    capture_format_.frame_size.SetSize(320, 240);
  else
    capture_format_.frame_size.SetSize(96, 96);

  capture_format_.pixel_format = pixel_format_;
  if (buffer_ownership_ == BufferOwnership::CLIENT_BUFFERS) {
    capture_format_.pixel_storage = PIXEL_STORAGE_CPU;
    capture_format_.pixel_format = PIXEL_FORMAT_ARGB;
    DVLOG(1) << "starting with client argb buffers";
  } else if (buffer_ownership_ == BufferOwnership::OWN_BUFFERS) {
    capture_format_.pixel_storage = PIXEL_STORAGE_CPU;
    DVLOG(1) << "starting with own " << VideoPixelFormatToString(pixel_format_)
             << " buffers";
  }

  if (buffer_ownership_ == BufferOwnership::OWN_BUFFERS) {
    fake_frame_.reset(new uint8_t[VideoFrame::AllocationSize(
        pixel_format_, capture_format_.frame_size)]);
  }

  beep_time_ = base::TimeDelta();
  elapsed_time_ = base::TimeDelta();

  if (buffer_ownership_ == BufferOwnership::CLIENT_BUFFERS)
    BeepAndScheduleNextCapture(
        base::TimeTicks::Now(),
        base::Bind(&FakeVideoCaptureDevice::CaptureUsingClientBuffers,
                   weak_factory_.GetWeakPtr()));
  else if (buffer_ownership_ == BufferOwnership::OWN_BUFFERS)
    BeepAndScheduleNextCapture(
        base::TimeTicks::Now(),
        base::Bind(&FakeVideoCaptureDevice::CaptureUsingOwnBuffers,
                   weak_factory_.GetWeakPtr()));
}

void FakeVideoCaptureDevice::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  client_.reset();
}

void FakeVideoCaptureDevice::GetPhotoCapabilities(
    GetPhotoCapabilitiesCallback callback) {
  mojom::PhotoCapabilitiesPtr photo_capabilities =
      mojom::PhotoCapabilities::New();
  photo_capabilities->iso = mojom::Range::New();
  photo_capabilities->iso->current = 100.0;
  photo_capabilities->iso->max = 100.0;
  photo_capabilities->iso->min = 100.0;
  photo_capabilities->iso->step = 0.0;
  photo_capabilities->height = mojom::Range::New();
  photo_capabilities->height->current = capture_format_.frame_size.height();
  photo_capabilities->height->max = 1080.0;
  photo_capabilities->height->min = 96.0;
  photo_capabilities->height->step = 1.0;
  photo_capabilities->width = mojom::Range::New();
  photo_capabilities->width->current = capture_format_.frame_size.width();
  photo_capabilities->width->max = 1920.0;
  photo_capabilities->width->min = 96.0;
  photo_capabilities->width->step = 1;
  photo_capabilities->zoom = mojom::Range::New();
  photo_capabilities->zoom->current = current_zoom_;
  photo_capabilities->zoom->max = kMaxZoom;
  photo_capabilities->zoom->min = kMinZoom;
  photo_capabilities->zoom->step = kZoomStep;
  photo_capabilities->focus_mode = mojom::MeteringMode::NONE;
  photo_capabilities->exposure_mode = mojom::MeteringMode::NONE;
  photo_capabilities->exposure_compensation = mojom::Range::New();
  photo_capabilities->white_balance_mode = mojom::MeteringMode::NONE;
  photo_capabilities->fill_light_mode = mojom::FillLightMode::NONE;
  photo_capabilities->red_eye_reduction = false;
  photo_capabilities->color_temperature = mojom::Range::New();
  photo_capabilities->brightness = media::mojom::Range::New();
  photo_capabilities->contrast = media::mojom::Range::New();
  photo_capabilities->saturation = media::mojom::Range::New();
  photo_capabilities->sharpness = media::mojom::Range::New();
  callback.Run(std::move(photo_capabilities));
}

void FakeVideoCaptureDevice::SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                                             SetPhotoOptionsCallback callback) {
  if (settings->has_zoom)
    current_zoom_ = std::max(kMinZoom, std::min(settings->zoom, kMaxZoom));
  callback.Run(true);
}

void FakeVideoCaptureDevice::TakePhoto(TakePhotoCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&DoTakeFakePhoto, base::Passed(&callback), capture_format_,
                 elapsed_time_, fake_capture_rate_, current_zoom_));
}

void FakeVideoCaptureDevice::CaptureUsingOwnBuffers(
    base::TimeTicks expected_execution_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const size_t frame_size = capture_format_.ImageAllocationSize();

  memset(fake_frame_.get(), 0, frame_size);
  DrawPacman(capture_format_.pixel_format, fake_frame_.get(), elapsed_time_,
             fake_capture_rate_, capture_format_.frame_size, current_zoom_);
  // Give the captured frame to the client.
  base::TimeTicks now = base::TimeTicks::Now();
  if (first_ref_time_.is_null())
    first_ref_time_ = now;
  client_->OnIncomingCapturedData(fake_frame_.get(), frame_size,
                                  capture_format_, 0 /* rotation */, now,
                                  now - first_ref_time_);
  BeepAndScheduleNextCapture(
      expected_execution_time,
      base::Bind(&FakeVideoCaptureDevice::CaptureUsingOwnBuffers,
                 weak_factory_.GetWeakPtr()));
}

void FakeVideoCaptureDevice::CaptureUsingClientBuffers(
    base::TimeTicks expected_execution_time) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<VideoCaptureDevice::Client::Buffer> capture_buffer(
      client_->ReserveOutputBuffer(capture_format_.frame_size,
                                   capture_format_.pixel_format,
                                   capture_format_.pixel_storage));
  DLOG_IF(ERROR, !capture_buffer) << "Couldn't allocate Capture Buffer";
  DCHECK(capture_buffer->data()) << "Buffer has NO backing memory";

  DCHECK_EQ(PIXEL_STORAGE_CPU, capture_format_.pixel_storage);
  uint8_t* data_ptr = static_cast<uint8_t*>(capture_buffer->data());
  memset(data_ptr, 0, capture_buffer->mapped_size());
  DrawPacman(capture_format_.pixel_format, data_ptr, elapsed_time_,
             fake_capture_rate_, capture_format_.frame_size, current_zoom_);

  // Give the captured frame to the client.
  base::TimeTicks now = base::TimeTicks::Now();
  if (first_ref_time_.is_null())
    first_ref_time_ = now;
  client_->OnIncomingCapturedBuffer(std::move(capture_buffer), capture_format_,
                                    now, now - first_ref_time_);

  BeepAndScheduleNextCapture(
      expected_execution_time,
      base::Bind(&FakeVideoCaptureDevice::CaptureUsingClientBuffers,
                 weak_factory_.GetWeakPtr()));
}

void FakeVideoCaptureDevice::BeepAndScheduleNextCapture(
    base::TimeTicks expected_execution_time,
    const base::Callback<void(base::TimeTicks)>& next_capture) {
  const base::TimeDelta beep_interval =
      base::TimeDelta::FromMilliseconds(kBeepInterval);
  const base::TimeDelta frame_interval =
      base::TimeDelta::FromMicroseconds(1e6 / fake_capture_rate_);
  beep_time_ += frame_interval;
  elapsed_time_ += frame_interval;

  // Generate a synchronized beep twice per second.
  if (beep_time_ >= beep_interval) {
    FakeAudioInputStream::BeepOnce();
    beep_time_ -= beep_interval;
  }

  // Reschedule next CaptureTask.
  const base::TimeTicks current_time = base::TimeTicks::Now();
  // Don't accumulate any debt if we are lagging behind - just post the next
  // frame immediately and continue as normal.
  const base::TimeTicks next_execution_time =
      std::max(current_time, expected_execution_time + frame_interval);
  const base::TimeDelta delay = next_execution_time - current_time;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(next_capture, next_execution_time), delay);
}

}  // namespace media
