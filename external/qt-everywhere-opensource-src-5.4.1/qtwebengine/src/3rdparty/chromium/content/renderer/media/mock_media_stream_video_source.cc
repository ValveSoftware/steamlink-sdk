// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_video_source.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"

namespace content {

MockMediaStreamVideoSource::MockMediaStreamVideoSource(
    bool manual_get_supported_formats)
    : manual_get_supported_formats_(manual_get_supported_formats),
      max_requested_height_(0),
      max_requested_width_(0),
      attempted_to_start_(false) {
  supported_formats_.push_back(
      media::VideoCaptureFormat(
          gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                    MediaStreamVideoSource::kDefaultHeight),
          MediaStreamVideoSource::kDefaultFrameRate,
          media::PIXEL_FORMAT_I420));
}

MockMediaStreamVideoSource::~MockMediaStreamVideoSource() {}

void MockMediaStreamVideoSource::StartMockedSource() {
  DCHECK(attempted_to_start_);
  attempted_to_start_ = false;
  OnStartDone(true);
}

void MockMediaStreamVideoSource::FailToStartMockedSource() {
  DCHECK(attempted_to_start_);
  attempted_to_start_ = false;
  OnStartDone(false);
}

void MockMediaStreamVideoSource::CompleteGetSupportedFormats() {
  DCHECK(!formats_callback_.is_null());
  base::ResetAndReturn(&formats_callback_).Run(supported_formats_);
}

void MockMediaStreamVideoSource::GetCurrentSupportedFormats(
    int max_requested_height,
    int max_requested_width,
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(formats_callback_.is_null());
  max_requested_height_ = max_requested_height;
  max_requested_width_ = max_requested_width;

  if (manual_get_supported_formats_) {
    formats_callback_ = callback;
    return;
  }
  callback.Run(supported_formats_);
}

void MockMediaStreamVideoSource::StartSourceImpl(
    const media::VideoCaptureParams& params,
    const VideoCaptureDeliverFrameCB& frame_callback) {
  DCHECK(frame_callback_.is_null());
  params_ = params;
  attempted_to_start_ = true;
  frame_callback_ = frame_callback;
}

void MockMediaStreamVideoSource::StopSourceImpl() {
}

void MockMediaStreamVideoSource::DeliverVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(!frame_callback_.is_null());
  io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MockMediaStreamVideoSource::DeliverVideoFrameOnIO,
                 base::Unretained(this), frame, params_.requested_format,
                 base::TimeTicks(), frame_callback_));
}

void MockMediaStreamVideoSource::DeliverVideoFrameOnIO(
    const scoped_refptr<media::VideoFrame>& frame,
    media::VideoCaptureFormat format,
    const base::TimeTicks& estimated_capture_time,
    const VideoCaptureDeliverFrameCB& frame_callback) {
  frame_callback.Run(frame, format, estimated_capture_time);
}

}  // namespace content
