// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_host.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/video_capture_messages.h"

namespace content {

VideoCaptureHost::VideoCaptureHost(MediaStreamManager* media_stream_manager)
    : BrowserMessageFilter(VideoCaptureMsgStart),
      media_stream_manager_(media_stream_manager) {
}

VideoCaptureHost::~VideoCaptureHost() {}

void VideoCaptureHost::OnChannelClosing() {
  // Since the IPC sender is gone, close all requested VideoCaptureDevices.
  for (EntryMap::iterator it = entries_.begin(); it != entries_.end(); ) {
    const base::WeakPtr<VideoCaptureController>& controller = it->second;
    if (controller) {
      VideoCaptureControllerID controller_id(it->first);
      media_stream_manager_->video_capture_manager()->StopCaptureForClient(
          controller.get(), controller_id, this, false);
      ++it;
    } else {
      // Remove the entry for this controller_id so that when the controller
      // is added, the controller will be notified to stop for this client
      // in DoControllerAddedOnIOThread.
      entries_.erase(it++);
    }
  }
}

void VideoCaptureHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

///////////////////////////////////////////////////////////////////////////////

// Implements VideoCaptureControllerEventHandler.
void VideoCaptureHost::OnError(const VideoCaptureControllerID& controller_id) {
  DVLOG(1) << "VideoCaptureHost::OnError";
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoHandleErrorOnIOThread,
                 this, controller_id));
}

void VideoCaptureHost::OnBufferCreated(
    const VideoCaptureControllerID& controller_id,
    base::SharedMemoryHandle handle,
    int length,
    int buffer_id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoSendNewBufferOnIOThread,
                 this, controller_id, handle, length, buffer_id));
}

void VideoCaptureHost::OnBufferDestroyed(
    const VideoCaptureControllerID& controller_id,
    int buffer_id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoSendFreeBufferOnIOThread,
                 this, controller_id, buffer_id));
}

void VideoCaptureHost::OnBufferReady(
    const VideoCaptureControllerID& controller_id,
    int buffer_id,
    const media::VideoCaptureFormat& frame_format,
    base::TimeTicks timestamp) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureHost::DoSendFilledBufferOnIOThread,
                 this,
                 controller_id,
                 buffer_id,
                 frame_format,
                 timestamp));
}

void VideoCaptureHost::OnMailboxBufferReady(
    const VideoCaptureControllerID& controller_id,
    int buffer_id,
    const gpu::MailboxHolder& mailbox_holder,
    const media::VideoCaptureFormat& frame_format,
    base::TimeTicks timestamp) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureHost::DoSendFilledMailboxBufferOnIOThread,
                 this,
                 controller_id,
                 buffer_id,
                 mailbox_holder,
                 frame_format,
                 timestamp));
}

void VideoCaptureHost::OnEnded(const VideoCaptureControllerID& controller_id) {
  DVLOG(1) << "VideoCaptureHost::OnEnded";
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoEndedOnIOThread, this, controller_id));
}

void VideoCaptureHost::DoSendNewBufferOnIOThread(
    const VideoCaptureControllerID& controller_id,
    base::SharedMemoryHandle handle,
    int length,
    int buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_NewBuffer(controller_id.device_id, handle,
                                     length, buffer_id));
}

void VideoCaptureHost::DoSendFreeBufferOnIOThread(
    const VideoCaptureControllerID& controller_id,
    int buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_FreeBuffer(controller_id.device_id, buffer_id));
}

void VideoCaptureHost::DoSendFilledBufferOnIOThread(
    const VideoCaptureControllerID& controller_id,
    int buffer_id,
    const media::VideoCaptureFormat& format,
    base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_BufferReady(
      controller_id.device_id, buffer_id, format, timestamp));
}

void VideoCaptureHost::DoSendFilledMailboxBufferOnIOThread(
    const VideoCaptureControllerID& controller_id,
    int buffer_id,
    const gpu::MailboxHolder& mailbox_holder,
    const media::VideoCaptureFormat& format,
    base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_MailboxBufferReady(
      controller_id.device_id, buffer_id, mailbox_holder, format, timestamp));
}

void VideoCaptureHost::DoHandleErrorOnIOThread(
    const VideoCaptureControllerID& controller_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_StateChanged(controller_id.device_id,
                                        VIDEO_CAPTURE_STATE_ERROR));
  DeleteVideoCaptureControllerOnIOThread(controller_id, true);
}

void VideoCaptureHost::DoEndedOnIOThread(
    const VideoCaptureControllerID& controller_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureHost::DoEndedOnIOThread";
  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_StateChanged(controller_id.device_id,
                                        VIDEO_CAPTURE_STATE_ENDED));
  DeleteVideoCaptureControllerOnIOThread(controller_id, false);
}

///////////////////////////////////////////////////////////////////////////////
// IPC Messages handler.
bool VideoCaptureHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(VideoCaptureHost, message)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Start, OnStartCapture)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Pause, OnPauseCapture)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Stop, OnStopCapture)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_BufferReady, OnReceiveEmptyBuffer)
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
           << " session_id=" << session_id
           << ", device_id=" << device_id
           << ", format=" << params.requested_format.frame_size.ToString()
           << "@" << params.requested_format.frame_rate
           << " (" << (params.allow_resolution_change ? "variable" : "constant")
           << ")";
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
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureHost::DoControllerAddedOnIOThread,
                 this,
                 device_id,
                 controller));
}

void VideoCaptureHost::DoControllerAddedOnIOThread(
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
  DeleteVideoCaptureControllerOnIOThread(controller_id, false);
}

void VideoCaptureHost::OnPauseCapture(int device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureHost::OnPauseCapture, device_id " << device_id;
  // Not used.
  Send(new VideoCaptureMsg_StateChanged(device_id, VIDEO_CAPTURE_STATE_ERROR));
}

void VideoCaptureHost::OnReceiveEmptyBuffer(
    int device_id,
    int buffer_id,
    const std::vector<uint32>& sync_points) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);
  if (it != entries_.end()) {
    const base::WeakPtr<VideoCaptureController>& controller = it->second;
    if (controller)
      controller->ReturnBuffer(controller_id, this, buffer_id, sync_points);
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

void VideoCaptureHost::DeleteVideoCaptureControllerOnIOThread(
    const VideoCaptureControllerID& controller_id, bool on_error) {
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
