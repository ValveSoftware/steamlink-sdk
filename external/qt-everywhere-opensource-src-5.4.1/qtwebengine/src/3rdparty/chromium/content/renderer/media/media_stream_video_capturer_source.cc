// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_capturer_source.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"

namespace {

struct SourceVideoResolution {
  int width;
  int height;
};

// Resolutions used if the source doesn't support capability enumeration.
const SourceVideoResolution kVideoResolutions[] = {{1920, 1080},
                                                   {1280, 720},
                                                   {960, 720},
                                                   {640, 480},
                                                   {640, 360},
                                                   {320, 240},
                                                   {320, 180}};
// Frame rates for sources with no support for capability enumeration.
const int kVideoFrameRates[] = {30, 60};

}  // namespace

namespace content {

VideoCapturerDelegate::VideoCapturerDelegate(
    const StreamDeviceInfo& device_info)
    : session_id_(device_info.session_id),
      is_screen_cast_(device_info.device.type == MEDIA_TAB_VIDEO_CAPTURE ||
                      device_info.device.type == MEDIA_DESKTOP_VIDEO_CAPTURE),
      got_first_frame_(false) {
  DVLOG(3) << "VideoCapturerDelegate::ctor";

  // NULL in unit test.
  if (RenderThreadImpl::current()) {
    VideoCaptureImplManager* manager =
        RenderThreadImpl::current()->video_capture_impl_manager();
    if (manager)
      release_device_cb_ = manager->UseDevice(session_id_);
  }
}

VideoCapturerDelegate::~VideoCapturerDelegate() {
  DVLOG(3) << "VideoCapturerDelegate::dtor";
  if (!release_device_cb_.is_null())
    release_device_cb_.Run();
}

void VideoCapturerDelegate::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height,
    const VideoCaptureDeviceFormatsCB& callback) {
  DVLOG(3) << "GetCurrentSupportedFormats("
           << " { max_requested_height = " << max_requested_height << "})"
           << " { max_requested_width = " << max_requested_width << "})";

  if (is_screen_cast_) {
    media::VideoCaptureFormats formats;
    const int width = max_requested_width ?
        max_requested_width : MediaStreamVideoSource::kDefaultWidth;
    const int height = max_requested_height ?
        max_requested_height : MediaStreamVideoSource::kDefaultHeight;
    formats.push_back(
          media::VideoCaptureFormat(
              gfx::Size(width, height),
              MediaStreamVideoSource::kDefaultFrameRate,
              media::PIXEL_FORMAT_I420));
    callback.Run(formats);
    return;
  }

  // NULL in unit test.
  if (!RenderThreadImpl::current())
    return;
  VideoCaptureImplManager* manager =
      RenderThreadImpl::current()->video_capture_impl_manager();
  if (!manager)
    return;
  DCHECK(source_formats_callback_.is_null());
  source_formats_callback_ = callback;
  manager->GetDeviceFormatsInUse(
      session_id_,
      media::BindToCurrentLoop(
          base::Bind(
              &VideoCapturerDelegate::OnDeviceFormatsInUseReceived, this)));
}

void VideoCapturerDelegate::StartCapture(
    const media::VideoCaptureParams& params,
    const VideoCaptureDeliverFrameCB& new_frame_callback,
    const RunningCallback& running_callback) {
  DCHECK(params.requested_format.IsValid());
  DCHECK(thread_checker_.CalledOnValidThread());
  running_callback_ = running_callback;
  got_first_frame_ = false;

  // NULL in unit test.
  if (!RenderThreadImpl::current())
    return;
  VideoCaptureImplManager* manager =
      RenderThreadImpl::current()->video_capture_impl_manager();
  if (!manager)
    return;
  stop_capture_cb_ =
      manager->StartCapture(
          session_id_,
          params,
          media::BindToCurrentLoop(base::Bind(
              &VideoCapturerDelegate::OnStateUpdateOnRenderThread, this)),
          new_frame_callback);
}

void VideoCapturerDelegate::StopCapture() {
  // Immediately make sure we don't provide more frames.
  DVLOG(3) << "VideoCapturerDelegate::StopCapture()";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!stop_capture_cb_.is_null()) {
    base::ResetAndReturn(&stop_capture_cb_).Run();
  }
  running_callback_.Reset();
  source_formats_callback_.Reset();
}

void VideoCapturerDelegate::OnStateUpdateOnRenderThread(
    VideoCaptureState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "OnStateUpdateOnRenderThread state = " << state;
  if (state == VIDEO_CAPTURE_STATE_STARTED && !running_callback_.is_null()) {
    running_callback_.Run(true);
    return;
  }
  if (state > VIDEO_CAPTURE_STATE_STARTED && !running_callback_.is_null()) {
    base::ResetAndReturn(&running_callback_).Run(false);
  }
}

void VideoCapturerDelegate::OnDeviceFormatsInUseReceived(
    const media::VideoCaptureFormats& formats_in_use) {
  DVLOG(3) << "OnDeviceFormatsInUseReceived: " << formats_in_use.size();
  DCHECK(thread_checker_.CalledOnValidThread());
  // StopCapture() might have destroyed |source_formats_callback_| before
  // arriving here.
  if (source_formats_callback_.is_null())
    return;
  // If there are no formats in use, try to retrieve the whole list of
  // supported form.
  if (!formats_in_use.empty()) {
    source_formats_callback_.Run(formats_in_use);
    source_formats_callback_.Reset();
    return;
  }

  // NULL in unit test.
  if (!RenderThreadImpl::current())
    return;
  VideoCaptureImplManager* manager =
      RenderThreadImpl::current()->video_capture_impl_manager();
  if (!manager)
    return;
  manager->GetDeviceSupportedFormats(
      session_id_,
      media::BindToCurrentLoop(
          base::Bind(
              &VideoCapturerDelegate::OnDeviceSupportedFormatsEnumerated,
              this)));
}

void VideoCapturerDelegate::OnDeviceSupportedFormatsEnumerated(
    const media::VideoCaptureFormats& formats) {
  DVLOG(3) << "OnDeviceSupportedFormatsEnumerated: " << formats.size()
           << " received";
  DCHECK(thread_checker_.CalledOnValidThread());
  // StopCapture() might have destroyed |source_formats_callback_| before
  // arriving here.
  if (source_formats_callback_.is_null())
    return;
  if (formats.size()) {
    source_formats_callback_.Run(formats);
  } else {
    // The capture device doesn't seem to support capability enumeration,
    // compose a fallback list of capabilities.
    media::VideoCaptureFormats default_formats;
    for (size_t i = 0; i < arraysize(kVideoResolutions); ++i) {
      for (size_t j = 0; j < arraysize(kVideoFrameRates); ++j) {
        default_formats.push_back(media::VideoCaptureFormat(
            gfx::Size(kVideoResolutions[i].width, kVideoResolutions[i].height),
            kVideoFrameRates[j], media::PIXEL_FORMAT_I420));
      }
    }
    source_formats_callback_.Run(default_formats);
  }
  source_formats_callback_.Reset();
}

MediaStreamVideoCapturerSource::MediaStreamVideoCapturerSource(
    const StreamDeviceInfo& device_info,
    const SourceStoppedCallback& stop_callback,
    const scoped_refptr<VideoCapturerDelegate>& delegate)
    : delegate_(delegate) {
  SetDeviceInfo(device_info);
  SetStopCallback(stop_callback);
}

MediaStreamVideoCapturerSource::~MediaStreamVideoCapturerSource() {
}

void MediaStreamVideoCapturerSource::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height,
    const VideoCaptureDeviceFormatsCB& callback) {
  delegate_->GetCurrentSupportedFormats(
      max_requested_width,
      max_requested_height,
      callback);
}

void MediaStreamVideoCapturerSource::StartSourceImpl(
    const media::VideoCaptureParams& params,
    const VideoCaptureDeliverFrameCB& frame_callback) {
  media::VideoCaptureParams new_params(params);
  if (device_info().device.type == MEDIA_TAB_VIDEO_CAPTURE ||
      device_info().device.type == MEDIA_DESKTOP_VIDEO_CAPTURE) {
    new_params.allow_resolution_change = true;
  }
  delegate_->StartCapture(
      new_params,
      frame_callback,
      base::Bind(&MediaStreamVideoCapturerSource::OnStartDone,
                 base::Unretained(this)));
}

void MediaStreamVideoCapturerSource::StopSourceImpl() {
  delegate_->StopCapture();
}

}  // namespace content
