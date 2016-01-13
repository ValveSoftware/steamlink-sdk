// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_video_capture.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/pepper/pepper_media_device_manager.h"
#include "content/renderer/pepper/pepper_video_capture_host.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "url/gurl.h"

namespace content {

PepperPlatformVideoCapture::PepperPlatformVideoCapture(
    const base::WeakPtr<RenderViewImpl>& render_view,
    const std::string& device_id,
    const GURL& document_url,
    PepperVideoCaptureHost* handler)
    : render_view_(render_view),
      device_id_(device_id),
      session_id_(0),
      handler_(handler),
      pending_open_device_(false),
      pending_open_device_id_(-1),
      weak_factory_(this) {
  // We need to open the device and obtain the label and session ID before
  // initializing.
  if (render_view_.get()) {
    pending_open_device_id_ = GetMediaDeviceManager()->OpenDevice(
        PP_DEVICETYPE_DEV_VIDEOCAPTURE,
        device_id,
        document_url,
        base::Bind(&PepperPlatformVideoCapture::OnDeviceOpened,
                   weak_factory_.GetWeakPtr()));
    pending_open_device_ = true;
  }
}

void PepperPlatformVideoCapture::StartCapture(
    const media::VideoCaptureParams& params) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!stop_capture_cb_.is_null())
    return;
  VideoCaptureImplManager* manager =
      RenderThreadImpl::current()->video_capture_impl_manager();
  stop_capture_cb_ =
      manager->StartCapture(session_id_,
                            params,
                            media::BindToCurrentLoop(base::Bind(
                                &PepperPlatformVideoCapture::OnStateUpdate,
                                weak_factory_.GetWeakPtr())),
                            media::BindToCurrentLoop(base::Bind(
                                &PepperPlatformVideoCapture::OnFrameReady,
                                weak_factory_.GetWeakPtr())));
}

void PepperPlatformVideoCapture::StopCapture() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stop_capture_cb_.is_null())
    return;
  stop_capture_cb_.Run();
  stop_capture_cb_.Reset();
}

void PepperPlatformVideoCapture::DetachEventHandler() {
  handler_ = NULL;
  StopCapture();
  if (!release_device_cb_.is_null()) {
    release_device_cb_.Run();
    release_device_cb_.Reset();
  }
  if (render_view_.get()) {
    if (!label_.empty()) {
      GetMediaDeviceManager()->CloseDevice(label_);
      label_.clear();
    }
    if (pending_open_device_) {
      GetMediaDeviceManager()->CancelOpenDevice(pending_open_device_id_);
      pending_open_device_ = false;
      pending_open_device_id_ = -1;
    }
  }
}

PepperPlatformVideoCapture::~PepperPlatformVideoCapture() {
  DCHECK(stop_capture_cb_.is_null());
  DCHECK(release_device_cb_.is_null());
  DCHECK(label_.empty());
  DCHECK(!pending_open_device_);
}

void PepperPlatformVideoCapture::OnDeviceOpened(int request_id,
                                                bool succeeded,
                                                const std::string& label) {
  pending_open_device_ = false;
  pending_open_device_id_ = -1;

  succeeded = succeeded && render_view_.get();
  if (succeeded) {
    label_ = label;
    session_id_ = GetMediaDeviceManager()->GetSessionID(
        PP_DEVICETYPE_DEV_VIDEOCAPTURE, label);
    VideoCaptureImplManager* manager =
        RenderThreadImpl::current()->video_capture_impl_manager();
    release_device_cb_ = manager->UseDevice(session_id_);
  }

  if (handler_)
    handler_->OnInitialized(succeeded);
}

void PepperPlatformVideoCapture::OnStateUpdate(VideoCaptureState state) {
  if (!handler_)
    return;
  switch (state) {
    case VIDEO_CAPTURE_STATE_STARTED:
      handler_->OnStarted();
      break;
    case VIDEO_CAPTURE_STATE_STOPPED:
      handler_->OnStopped();
      break;
    case VIDEO_CAPTURE_STATE_PAUSED:
      handler_->OnPaused();
      break;
    case VIDEO_CAPTURE_STATE_ERROR:
      handler_->OnError();
      break;
    default:
      NOTREACHED() << "Unexpected state: " << state << ".";
  }
}

void PepperPlatformVideoCapture::OnFrameReady(
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format,
    const base::TimeTicks& estimated_capture_time) {
  if (handler_ && !stop_capture_cb_.is_null())
    handler_->OnFrameReady(frame, format);
}

PepperMediaDeviceManager* PepperPlatformVideoCapture::GetMediaDeviceManager() {
  return PepperMediaDeviceManager::GetForRenderView(render_view_.get());
}

}  // namespace content
