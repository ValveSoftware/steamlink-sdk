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
#include "content/common/media/video_capture_messages.h"

namespace content {

VideoCaptureHost::VideoCaptureHost(MediaStreamManager* media_stream_manager)
    : BrowserMessageFilter(VideoCaptureMsgStart),
      media_stream_manager_(media_stream_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

VideoCaptureHost::~VideoCaptureHost() {}

void VideoCaptureHost::OnChannelClosing() {
  // Since the IPC sender is gone, close all requested VideoCaptureDevices.
  for (EntryMap::iterator it = entries_.begin(); it != entries_.end(); ) {
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
      entries_.erase(it++);
    }
  }
}

void VideoCaptureHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

///////////////////////////////////////////////////////////////////////////////

// Implements VideoCaptureControllerEventHandler.
void VideoCaptureHost::OnError(VideoCaptureControllerID controller_id) {
  DVLOG(1) << "VideoCaptureHost::OnError";
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoError, this, controller_id));
}

void VideoCaptureHost::OnBufferCreated(VideoCaptureControllerID controller_id,
                                       base::SharedMemoryHandle handle,
                                       int length,
                                       int buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_NewBuffer(controller_id, handle, length, buffer_id));
}

void VideoCaptureHost::OnBufferCreated2(
    VideoCaptureControllerID controller_id,
    const std::vector<gfx::GpuMemoryBufferHandle>& handles,
    const gfx::Size& size,
    int buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_NewBuffer2(controller_id, handles, size, buffer_id));
}

void VideoCaptureHost::OnBufferDestroyed(VideoCaptureControllerID controller_id,
                                         int buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_FreeBuffer(controller_id, buffer_id));
}

void VideoCaptureHost::OnBufferReady(
    VideoCaptureControllerID controller_id,
    int buffer_id,
    const scoped_refptr<media::VideoFrame>& video_frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (entries_.find(controller_id) == entries_.end())
    return;

  VideoCaptureMsg_BufferReady_Params params;
  params.device_id = controller_id;
  params.buffer_id = buffer_id;
  params.timestamp = video_frame->timestamp();
  video_frame->metadata()->MergeInternalValuesInto(&params.metadata);
  params.pixel_format = video_frame->format();
  params.storage_type = video_frame->storage_type();
  params.coded_size = video_frame->coded_size();
  params.visible_rect = video_frame->visible_rect();

  Send(new VideoCaptureMsg_BufferReady(params));
}

void VideoCaptureHost::OnEnded(VideoCaptureControllerID controller_id) {
  DVLOG(1) << "VideoCaptureHost::OnEnded";
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoEnded, this, controller_id));
}

void VideoCaptureHost::DoError(VideoCaptureControllerID controller_id) {
  DVLOG(1) << "VideoCaptureHost::DoError";
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_StateChanged(controller_id,
                                        VIDEO_CAPTURE_STATE_ERROR));
  DeleteVideoCaptureController(controller_id, true);
}

void VideoCaptureHost::DoEnded(VideoCaptureControllerID controller_id) {
  DVLOG(1) << "VideoCaptureHost::DoEnded";
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_StateChanged(controller_id,
                                        VIDEO_CAPTURE_STATE_ENDED));
  DeleteVideoCaptureController(controller_id, false);
}

///////////////////////////////////////////////////////////////////////////////
// IPC Messages handler.
bool VideoCaptureHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(VideoCaptureHost, message)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Start, OnStartCapture)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Pause, OnPauseCapture)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Resume, OnResumeCapture)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_RequestRefreshFrame,
                        OnRequestRefreshFrame)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Stop, OnStopCapture)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_BufferReady,
                        OnRendererFinishedWithBuffer)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_GetDeviceSupportedFormats,
                        OnGetDeviceSupportedFormats)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_GetDeviceFormatsInUse,
                        OnGetDeviceFormatsInUse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void VideoCaptureHost::OnStartCapture(int device_id,
                                      media::VideoCaptureSessionId session_id,
                                      const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureHost::OnStartCapture:"
           << " session_id=" << session_id << ", device_id=" << device_id
           << ", format="
           << media::VideoCaptureFormat::ToString(params.requested_format)
           << "@" << params.requested_format.frame_rate << " ("
           << (params.resolution_change_policy ==
                       media::RESOLUTION_POLICY_FIXED_RESOLUTION
                   ? "fixed resolution"
                   : (params.resolution_change_policy ==
                              media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO
                          ? "fixed aspect ratio"
                          : "variable resolution")) << ")";
  VideoCaptureControllerID controller_id(device_id);
  if (entries_.find(controller_id) != entries_.end()) {
    Send(new VideoCaptureMsg_StateChanged(device_id,
                                          VIDEO_CAPTURE_STATE_ERROR));
    return;
  }

  entries_[controller_id] = base::WeakPtr<VideoCaptureController>();
  media_stream_manager_->video_capture_manager()->StartCaptureForClient(
      session_id,
      params,
      PeerHandle(),
      controller_id,
      this,
      base::Bind(&VideoCaptureHost::OnControllerAdded, this, device_id));
}

void VideoCaptureHost::OnControllerAdded(
    int device_id,
    const base::WeakPtr<VideoCaptureController>& controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);
  if (it == entries_.end()) {
    if (controller) {
      media_stream_manager_->video_capture_manager()->StopCaptureForClient(
          controller.get(), controller_id, this, false);
    }
    return;
  }

  if (!controller) {
    Send(new VideoCaptureMsg_StateChanged(device_id,
                                          VIDEO_CAPTURE_STATE_ERROR));
    entries_.erase(controller_id);
    return;
  }

  DCHECK(!it->second);
  it->second = controller;
}

void VideoCaptureHost::OnStopCapture(int device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureHost::OnStopCapture, device_id " << device_id;

  VideoCaptureControllerID controller_id(device_id);

  Send(new VideoCaptureMsg_StateChanged(device_id,
                                        VIDEO_CAPTURE_STATE_STOPPED));
  DeleteVideoCaptureController(controller_id, false);
}

void VideoCaptureHost::OnPauseCapture(int device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureHost::OnPauseCapture, device_id " << device_id;

  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);
  if (it == entries_.end())
    return;

  if (it->second) {
    media_stream_manager_->video_capture_manager()->PauseCaptureForClient(
        it->second.get(), controller_id, this);
  }
}

void VideoCaptureHost::OnResumeCapture(
    int device_id,
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureHost::OnResumeCapture, device_id " << device_id;

  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);
  if (it == entries_.end())
    return;

  if (it->second) {
    media_stream_manager_->video_capture_manager()->ResumeCaptureForClient(
        session_id, params, it->second.get(), controller_id, this);
  }
}

void VideoCaptureHost::OnRequestRefreshFrame(int device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureHost::OnRequestRefreshFrame, device_id "
           << device_id;

  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);
  if (it == entries_.end())
    return;

  if (VideoCaptureController* controller = it->second.get()) {
    media_stream_manager_->video_capture_manager()
        ->RequestRefreshFrameForClient(controller);
  }
}

void VideoCaptureHost::OnRendererFinishedWithBuffer(
    int device_id,
    int buffer_id,
    const gpu::SyncToken& sync_token,
    double consumer_resource_utilization) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);
  if (it != entries_.end()) {
    const base::WeakPtr<VideoCaptureController>& controller = it->second;
    if (controller) {
      controller->ReturnBuffer(controller_id, this, buffer_id, sync_token,
                               consumer_resource_utilization);
    }
  }
}

void VideoCaptureHost::OnGetDeviceSupportedFormats(
    int device_id,
    media::VideoCaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureHost::OnGetDeviceFormats, capture_session_id "
           << capture_session_id;
  media::VideoCaptureFormats device_supported_formats;
  if (!media_stream_manager_->video_capture_manager()
           ->GetDeviceSupportedFormats(capture_session_id,
                                       &device_supported_formats)) {
    DLOG(WARNING)
        << "Could not retrieve device supported formats for device_id="
        << device_id << " capture_session_id=" << capture_session_id;
  }
  Send(new VideoCaptureMsg_DeviceSupportedFormatsEnumerated(
      device_id, device_supported_formats));
}

void VideoCaptureHost::OnGetDeviceFormatsInUse(
    int device_id,
    media::VideoCaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureHost::OnGetDeviceFormatsInUse, capture_session_id "
           << capture_session_id;
  media::VideoCaptureFormats formats_in_use;
  if (!media_stream_manager_->video_capture_manager()->GetDeviceFormatsInUse(
           capture_session_id, &formats_in_use)) {
    DVLOG(1) << "Could not retrieve device format(s) in use for device_id="
             << device_id << " capture_session_id=" << capture_session_id;
  }
  Send(new VideoCaptureMsg_DeviceFormatsInUseReceived(device_id,
                                                      formats_in_use));
}

void VideoCaptureHost::DeleteVideoCaptureController(
    VideoCaptureControllerID controller_id, bool on_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  EntryMap::iterator it = entries_.find(controller_id);
  if (it == entries_.end())
    return;

  if (it->second) {
    media_stream_manager_->video_capture_manager()->StopCaptureForClient(
        it->second.get(), controller_id, this, on_error);
  }
  entries_.erase(it);
}

}  // namespace content
