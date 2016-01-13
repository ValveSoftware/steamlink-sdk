// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/fake_video_capture_device.h"

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace media {

static const int kFakeCaptureBeepCycle = 10;  // Visual beep every 0.5s.
static const int kFakeCaptureCapabilityChangePeriod = 30;

FakeVideoCaptureDevice::FakeVideoCaptureDevice()
    : capture_thread_("CaptureThread"),
      frame_count_(0),
      format_roster_index_(0) {}

FakeVideoCaptureDevice::~FakeVideoCaptureDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!capture_thread_.IsRunning());
}

void FakeVideoCaptureDevice::AllocateAndStart(
    const VideoCaptureParams& params,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!capture_thread_.IsRunning());

  capture_thread_.Start();
  capture_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&FakeVideoCaptureDevice::OnAllocateAndStart,
                 base::Unretained(this),
                 params,
                 base::Passed(&client)));
}

void FakeVideoCaptureDevice::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(capture_thread_.IsRunning());
  capture_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&FakeVideoCaptureDevice::OnStopAndDeAllocate,
                 base::Unretained(this)));
  capture_thread_.Stop();
}

void FakeVideoCaptureDevice::PopulateVariableFormatsRoster(
    const VideoCaptureFormats& formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!capture_thread_.IsRunning());
  format_roster_ = formats;
  format_roster_index_ = 0;
}

void FakeVideoCaptureDevice::OnAllocateAndStart(
    const VideoCaptureParams& params,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  DCHECK_EQ(capture_thread_.message_loop(), base::MessageLoop::current());
  client_ = client.Pass();

  // Incoming |params| can be none of the supported formats, so we get the
  // closest thing rounded up. TODO(mcasas): Use the |params|, if they belong to
  // the supported ones, when http://crbug.com/309554 is verified.
  DCHECK_EQ(params.requested_format.pixel_format, PIXEL_FORMAT_I420);
  capture_format_.pixel_format = params.requested_format.pixel_format;
  capture_format_.frame_rate = 30;
  if (params.requested_format.frame_size.width() > 640)
      capture_format_.frame_size.SetSize(1280, 720);
  else if (params.requested_format.frame_size.width() > 320)
    capture_format_.frame_size.SetSize(640, 480);
  else
    capture_format_.frame_size.SetSize(320, 240);
  const size_t fake_frame_size =
      VideoFrame::AllocationSize(VideoFrame::I420, capture_format_.frame_size);
  fake_frame_.reset(new uint8[fake_frame_size]);

  capture_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&FakeVideoCaptureDevice::OnCaptureTask,
                 base::Unretained(this)));
}

void FakeVideoCaptureDevice::OnStopAndDeAllocate() {
  DCHECK_EQ(capture_thread_.message_loop(), base::MessageLoop::current());
  client_.reset();
}

void FakeVideoCaptureDevice::OnCaptureTask() {
  if (!client_)
    return;

  const size_t frame_size =
      VideoFrame::AllocationSize(VideoFrame::I420, capture_format_.frame_size);
  memset(fake_frame_.get(), 0, frame_size);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kA8_Config,
                   capture_format_.frame_size.width(),
                   capture_format_.frame_size.height(),
                   capture_format_.frame_size.width()),
      bitmap.setPixels(fake_frame_.get());
  SkCanvas canvas(bitmap);

  // Draw a sweeping circle to show an animation.
  int radius = std::min(capture_format_.frame_size.width(),
                        capture_format_.frame_size.height()) / 4;
  SkRect rect =
      SkRect::MakeXYWH(capture_format_.frame_size.width() / 2 - radius,
                       capture_format_.frame_size.height() / 2 - radius,
                       2 * radius,
                       2 * radius);

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);

  // Only Y plane is being drawn and this gives 50% grey on the Y
  // plane. The result is a light green color in RGB space.
  paint.setAlpha(128);

  int end_angle = (frame_count_ % kFakeCaptureBeepCycle * 360) /
      kFakeCaptureBeepCycle;
  if (!end_angle)
    end_angle = 360;
  canvas.drawArc(rect, 0, end_angle, true, paint);

  // Draw current time.
  int elapsed_ms = kFakeCaptureTimeoutMs * frame_count_;
  int milliseconds = elapsed_ms % 1000;
  int seconds = (elapsed_ms / 1000) % 60;
  int minutes = (elapsed_ms / 1000 / 60) % 60;
  int hours = (elapsed_ms / 1000 / 60 / 60) % 60;

  std::string time_string =
      base::StringPrintf("%d:%02d:%02d:%03d %d", hours, minutes,
                         seconds, milliseconds, frame_count_);
  canvas.scale(3, 3);
  canvas.drawText(time_string.data(), time_string.length(), 30, 20,
                  paint);

  if (frame_count_ % kFakeCaptureBeepCycle == 0) {
    // Generate a synchronized beep sound if there is one audio input
    // stream created.
    FakeAudioInputStream::BeepOnce();
  }

  frame_count_++;

  // Give the captured frame to the client.
  client_->OnIncomingCapturedData(fake_frame_.get(),
                                  frame_size,
                                  capture_format_,
                                  0,
                                  base::TimeTicks::Now());
  if (!(frame_count_ % kFakeCaptureCapabilityChangePeriod) &&
      format_roster_.size() > 0U) {
    Reallocate();
  }
  // Reschedule next CaptureTask.
  capture_thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeVideoCaptureDevice::OnCaptureTask,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kFakeCaptureTimeoutMs));
}

void FakeVideoCaptureDevice::Reallocate() {
  DCHECK_EQ(capture_thread_.message_loop(), base::MessageLoop::current());
  capture_format_ =
      format_roster_.at(++format_roster_index_ % format_roster_.size());
  DCHECK_EQ(capture_format_.pixel_format, PIXEL_FORMAT_I420);
  DVLOG(3) << "Reallocating FakeVideoCaptureDevice, new capture resolution "
           << capture_format_.frame_size.ToString();

  const size_t fake_frame_size =
      VideoFrame::AllocationSize(VideoFrame::I420, capture_format_.frame_size);
  fake_frame_.reset(new uint8[fake_frame_size]);
}

}  // namespace media
