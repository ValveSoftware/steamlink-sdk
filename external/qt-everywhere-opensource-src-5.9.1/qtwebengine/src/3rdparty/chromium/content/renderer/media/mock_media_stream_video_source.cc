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
    : MockMediaStreamVideoSource(manual_get_supported_formats, false) {}

MockMediaStreamVideoSource::MockMediaStreamVideoSource(
    bool manual_get_supported_formats,
    bool respond_to_request_refresh_frame)
    : manual_get_supported_formats_(manual_get_supported_formats),
      respond_to_request_refresh_frame_(respond_to_request_refresh_frame),
      max_requested_height_(0),
      max_requested_width_(0),
      max_requested_frame_rate_(0.0),
      attempted_to_start_(false) {
  supported_formats_.push_back(media::VideoCaptureFormat(
      gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                MediaStreamVideoSource::kDefaultHeight),
      MediaStreamVideoSource::kDefaultFrameRate, media::PIXEL_FORMAT_I420));
}

MockMediaStreamVideoSource::~MockMediaStreamVideoSource() {}

void MockMediaStreamVideoSource::StartMockedSource() {
  DCHECK(attempted_to_start_);
  attempted_to_start_ = false;
  OnStartDone(MEDIA_DEVICE_OK);
}

void MockMediaStreamVideoSource::FailToStartMockedSource() {
  DCHECK(attempted_to_start_);
  attempted_to_start_ = false;
  OnStartDone(MEDIA_DEVICE_TRACK_START_FAILURE);
}

void MockMediaStreamVideoSource::CompleteGetSupportedFormats() {
  DCHECK(!formats_callback_.is_null());
  base::ResetAndReturn(&formats_callback_).Run(supported_formats_);
}

void MockMediaStreamVideoSource::RequestRefreshFrame() {
  DCHECK(!frame_callback_.is_null());
  if (respond_to_request_refresh_frame_) {
    const scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateColorFrame(format_.frame_size, 0, 0, 0,
                                            base::TimeDelta());
    io_task_runner()->PostTask(
        FROM_HERE, base::Bind(frame_callback_, frame, base::TimeTicks()));
  }
}

void MockMediaStreamVideoSource::GetCurrentSupportedFormats(
    int max_requested_height,
    int max_requested_width,
    double max_requested_frame_rate,
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(formats_callback_.is_null());
  max_requested_height_ = max_requested_height;
  max_requested_width_ = max_requested_width;
  max_requested_frame_rate_ = max_requested_frame_rate;

  if (manual_get_supported_formats_) {
    formats_callback_ = callback;
    return;
  }
  callback.Run(supported_formats_);
}

void MockMediaStreamVideoSource::StartSourceImpl(
    const media::VideoCaptureFormat& format,
    const blink::WebMediaConstraints& constraints,
    const VideoCaptureDeliverFrameCB& frame_callback) {
  DCHECK(frame_callback_.is_null());
  format_ = format;
  attempted_to_start_ = true;
  frame_callback_ = frame_callback;
}

void MockMediaStreamVideoSource::StopSourceImpl() {
}

void MockMediaStreamVideoSource::DeliverVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(!frame_callback_.is_null());
  io_task_runner()->PostTask(
      FROM_HERE, base::Bind(frame_callback_, frame, base::TimeTicks()));
}

}  // namespace content
