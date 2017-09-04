// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_host.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

VideoCaptureHost::VideoCaptureHost(MediaStreamManager* media_stream_manager)
    : media_stream_manager_(media_stream_manager),
      weak_factory_(this) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

// static
void VideoCaptureHost::Create(MediaStreamManager* media_stream_manager,
                              mojom::VideoCaptureHostRequest request) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mojo::MakeStrongBinding(
      base::MakeUnique<VideoCaptureHost>(media_stream_manager),
      std::move(request));
}

VideoCaptureHost::~VideoCaptureHost() {
  for (auto it = controllers_.begin(); it != controllers_.end(); ) {
    const base::WeakPtr<VideoCaptureController>& controller = it->second;
    if (controller) {
      const VideoCaptureControllerID controller_id(it->first);
      media_stream_manager_->video_capture_manager()->StopCaptureForClient(
          controller.get(), controller_id, this, false);
      ++it;
    } else {
      // Remove the entry for this controller_id so that when the controller
      // is added, the controller will be notified to stop for this client
      // in DoControllerAdded.
      controllers_.erase(it++);
    }
  }
}

void VideoCaptureHost::OnError(VideoCaptureControllerID controller_id) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoError, weak_factory_.GetWeakPtr(),
                 controller_id));
}

void VideoCaptureHost::OnBufferCreated(VideoCaptureControllerID controller_id,
                                       mojo::ScopedSharedBufferHandle handle,
                                       int length,
                                       int buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end())
    return;

  if (base::ContainsKey(device_id_to_observer_map_, controller_id)) {
    device_id_to_observer_map_[controller_id]->OnBufferCreated(
        buffer_id, std::move(handle));
  }
}

void VideoCaptureHost::OnBufferDestroyed(VideoCaptureControllerID controller_id,
                                         int buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end())
    return;

  if (base::ContainsKey(device_id_to_observer_map_, controller_id))
    device_id_to_observer_map_[controller_id]->OnBufferDestroyed(buffer_id);
}

void VideoCaptureHost::OnBufferReady(
    VideoCaptureControllerID controller_id,
    int buffer_id,
    const scoped_refptr<media::VideoFrame>& video_frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end())
    return;

  if (!base::ContainsKey(device_id_to_observer_map_, controller_id))
    return;

  mojom::VideoFrameInfoPtr info = mojom::VideoFrameInfo::New();
  info->timestamp = video_frame->timestamp();
  video_frame->metadata()->MergeInternalValuesInto(&info->metadata);

  DCHECK(media::PIXEL_FORMAT_I420 == video_frame->format() ||
         media::PIXEL_FORMAT_Y16 == video_frame->format());
  info->pixel_format = video_frame->format();
  info->storage_type = media::PIXEL_STORAGE_CPU;
  info->coded_size = video_frame->coded_size();
  info->visible_rect = video_frame->visible_rect();

  device_id_to_observer_map_[controller_id]->OnBufferReady(buffer_id,
                                                           std::move(info));
}

void VideoCaptureHost::OnEnded(VideoCaptureControllerID controller_id) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoEnded, weak_factory_.GetWeakPtr(),
                 controller_id));
}

void VideoCaptureHost::Start(int32_t device_id,
                             int32_t session_id,
                             const media::VideoCaptureParams& params,
                             mojom::VideoCaptureObserverPtr observer) {
  DVLOG(1) << __func__ << " session_id=" << session_id
           << ", device_id=" << device_id << ", format="
           << media::VideoCaptureFormat::ToString(params.requested_format);
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!base::ContainsKey(device_id_to_observer_map_, device_id));
  device_id_to_observer_map_[device_id] = std::move(observer);

  const VideoCaptureControllerID controller_id(device_id);
  if (controllers_.find(controller_id) != controllers_.end()) {
    device_id_to_observer_map_[device_id]->OnStateChanged(
        mojom::VideoCaptureState::STARTED);
    return;
  }

  controllers_[controller_id] = base::WeakPtr<VideoCaptureController>();
  media_stream_manager_->video_capture_manager()->StartCaptureForClient(
      session_id, params, controller_id, this,
      base::Bind(&VideoCaptureHost::OnControllerAdded,
                 weak_factory_.GetWeakPtr(), device_id));
}

void VideoCaptureHost::Stop(int32_t device_id) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);

  if (base::ContainsKey(device_id_to_observer_map_, device_id)) {
    device_id_to_observer_map_[device_id]->OnStateChanged(
        mojom::VideoCaptureState::STOPPED);
  }
  device_id_to_observer_map_.erase(controller_id);

  DeleteVideoCaptureController(controller_id, false);
}

void VideoCaptureHost::Pause(int32_t device_id) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end() || !it->second)
    return;

  media_stream_manager_->video_capture_manager()->PauseCaptureForClient(
      it->second.get(), controller_id, this);
  if (base::ContainsKey(device_id_to_observer_map_, device_id)) {
    device_id_to_observer_map_[device_id]->OnStateChanged(
        mojom::VideoCaptureState::PAUSED);
  }
}

void VideoCaptureHost::Resume(int32_t device_id,
                              int32_t session_id,
                              const media::VideoCaptureParams& params) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end() || !it->second)
    return;

  media_stream_manager_->video_capture_manager()->ResumeCaptureForClient(
      session_id, params, it->second.get(), controller_id, this);
  if (base::ContainsKey(device_id_to_observer_map_, device_id)) {
    device_id_to_observer_map_[device_id]->OnStateChanged(
        mojom::VideoCaptureState::RESUMED);
  }
}

void VideoCaptureHost::RequestRefreshFrame(int32_t device_id) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end())
    return;

  if (VideoCaptureController* controller = it->second.get()) {
    media_stream_manager_->video_capture_manager()
        ->RequestRefreshFrameForClient(controller);
  }
}

void VideoCaptureHost::ReleaseBuffer(int32_t device_id,
                                     int32_t buffer_id,
                                     const gpu::SyncToken& sync_token,
                                     double consumer_resource_utilization) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end())
    return;

  const base::WeakPtr<VideoCaptureController>& controller = it->second;
  if (controller) {
    controller->ReturnBuffer(controller_id, this, buffer_id, sync_token,
                             consumer_resource_utilization);
  }
}

void VideoCaptureHost::GetDeviceSupportedFormats(
    int32_t device_id,
    int32_t session_id,
    const GetDeviceSupportedFormatsCallback& callback) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media::VideoCaptureFormats supported_formats;
  if (!media_stream_manager_->video_capture_manager()
           ->GetDeviceSupportedFormats(session_id, &supported_formats)) {
    DLOG(WARNING) << "Could not retrieve device supported formats";
  }
  callback.Run(supported_formats);
}

void VideoCaptureHost::GetDeviceFormatsInUse(
    int32_t device_id,
    int32_t session_id,
    const GetDeviceFormatsInUseCallback& callback) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media::VideoCaptureFormats formats_in_use;
  if (!media_stream_manager_->video_capture_manager()->GetDeviceFormatsInUse(
           session_id, &formats_in_use)) {
    DLOG(WARNING) << "Could not retrieve device format(s) in use";
  }
  callback.Run(formats_in_use);
}

void VideoCaptureHost::DoError(VideoCaptureControllerID controller_id) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end())
    return;

  if (base::ContainsKey(device_id_to_observer_map_, controller_id)) {
    device_id_to_observer_map_[controller_id]->OnStateChanged(
        mojom::VideoCaptureState::FAILED);
  }

  DeleteVideoCaptureController(controller_id, true);
}

void VideoCaptureHost::DoEnded(VideoCaptureControllerID controller_id) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end())
    return;

  if (base::ContainsKey(device_id_to_observer_map_, controller_id)) {
    device_id_to_observer_map_[controller_id]->OnStateChanged(
        mojom::VideoCaptureState::ENDED);
  }

  DeleteVideoCaptureController(controller_id, false);
}

void VideoCaptureHost::OnControllerAdded(
    int device_id,
    const base::WeakPtr<VideoCaptureController>& controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end()) {
    if (controller) {
      media_stream_manager_->video_capture_manager()->StopCaptureForClient(
          controller.get(), controller_id, this, false);
    }
    return;
  }

  if (!controller) {
    if (base::ContainsKey(device_id_to_observer_map_, controller_id)) {
      device_id_to_observer_map_[device_id]->OnStateChanged(
          mojom::VideoCaptureState::FAILED);
    }
    controllers_.erase(controller_id);
    return;
  }

  if (base::ContainsKey(device_id_to_observer_map_, controller_id)) {
    device_id_to_observer_map_[device_id]->OnStateChanged(
        mojom::VideoCaptureState::STARTED);
  }

  DCHECK(!it->second);
  it->second = controller;
}

void VideoCaptureHost::DeleteVideoCaptureController(
    VideoCaptureControllerID controller_id, bool on_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = controllers_.find(controller_id);
  if (it == controllers_.end())
    return;

  const base::WeakPtr<VideoCaptureController> controller = it->second;
  controllers_.erase(it);
  if (!controller)
    return;

  media_stream_manager_->video_capture_manager()->StopCaptureForClient(
      controller.get(), controller_id, this, on_error);
}

}  // namespace content
