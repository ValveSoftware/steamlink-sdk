// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_message_filter.h"

#include "content/common/media/video_capture_messages.h"
#include "content/common/view_messages.h"
#include "ipc/ipc_sender.h"

namespace content {

VideoCaptureMessageFilter::VideoCaptureMessageFilter()
    : last_device_id_(0),
      sender_(NULL) {
}

void VideoCaptureMessageFilter::AddDelegate(Delegate* delegate) {
  if (++last_device_id_ <= 0)
    last_device_id_ = 1;
  while (delegates_.find(last_device_id_) != delegates_.end())
    last_device_id_++;

  if (sender_) {
    delegates_[last_device_id_] = delegate;
    delegate->OnDelegateAdded(last_device_id_);
  } else {
    pending_delegates_[last_device_id_] = delegate;
  }
}

void VideoCaptureMessageFilter::RemoveDelegate(Delegate* delegate) {
  for (Delegates::iterator it = delegates_.begin();
       it != delegates_.end(); it++) {
    if (it->second == delegate) {
      delegates_.erase(it);
      break;
    }
  }
  for (Delegates::iterator it = pending_delegates_.begin();
       it != pending_delegates_.end(); it++) {
    if (it->second == delegate) {
      pending_delegates_.erase(it);
      break;
    }
  }
}

bool VideoCaptureMessageFilter::Send(IPC::Message* message) {
  if (!sender_) {
    delete message;
    return false;
  }

  return sender_->Send(message);
}

bool VideoCaptureMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(VideoCaptureMessageFilter, message)
    IPC_MESSAGE_HANDLER(VideoCaptureMsg_BufferReady, OnBufferReceived)
    IPC_MESSAGE_HANDLER(VideoCaptureMsg_StateChanged, OnDeviceStateChanged)
    IPC_MESSAGE_HANDLER(VideoCaptureMsg_NewBuffer, OnBufferCreated)
    IPC_MESSAGE_HANDLER(VideoCaptureMsg_NewBuffer2, OnBufferCreated2)
    IPC_MESSAGE_HANDLER(VideoCaptureMsg_FreeBuffer, OnBufferDestroyed)
    IPC_MESSAGE_HANDLER(VideoCaptureMsg_DeviceSupportedFormatsEnumerated,
                        OnDeviceSupportedFormatsEnumerated)
    IPC_MESSAGE_HANDLER(VideoCaptureMsg_DeviceFormatsInUseReceived,
                        OnDeviceFormatsInUseReceived)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void VideoCaptureMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  DVLOG(1) << "VideoCaptureMessageFilter::OnFilterAdded()";
  sender_ = sender;

  for (const auto& pending_delegate : pending_delegates_) {
    pending_delegate.second->OnDelegateAdded(pending_delegate.first);
    delegates_[pending_delegate.first] = pending_delegate.second;
  }
  pending_delegates_.clear();
}

void VideoCaptureMessageFilter::OnFilterRemoved() {
  sender_ = NULL;
}

void VideoCaptureMessageFilter::OnChannelClosing() {
  sender_ = NULL;
}

VideoCaptureMessageFilter::~VideoCaptureMessageFilter() {}

VideoCaptureMessageFilter::Delegate*
VideoCaptureMessageFilter::find_delegate(int device_id) const {
  Delegates::const_iterator i = delegates_.find(device_id);
  return i != delegates_.end() ? i->second : nullptr;
}

void VideoCaptureMessageFilter::OnBufferCreated(int device_id,
                                                base::SharedMemoryHandle handle,
                                                int length,
                                                int buffer_id) {
  Delegate* const delegate = find_delegate(device_id);
  if (!delegate) {
    DLOG(WARNING) << "OnBufferCreated: Got video SHM buffer for a "
                     "non-existent or removed video capture.";

    // Send the buffer back to Host in case it's waiting for all buffers
    // to be returned.
    base::SharedMemory::CloseHandle(handle);

    Send(new VideoCaptureHostMsg_BufferReady(
        device_id, buffer_id, gpu::SyncToken() /* release_sync_token */,
        -1.0 /* consumer_resource_utilization */));
    return;
  }

  delegate->OnBufferCreated(handle, length, buffer_id);
}

void VideoCaptureMessageFilter::OnBufferCreated2(
    int device_id,
    const std::vector<gfx::GpuMemoryBufferHandle>& handles,
    const gfx::Size& size,
    int buffer_id) {
  Delegate* const delegate = find_delegate(device_id);
  if (!delegate) {
    DLOG(WARNING) << "OnBufferCreated: Got video GMB buffer for a "
                     "non-existent or removed video capture.";
    Send(new VideoCaptureHostMsg_BufferReady(
        device_id, buffer_id, gpu::SyncToken() /* release_sync_token */,
        -1.0 /* consumer_resource_utilization */));
    return;
  }

  delegate->OnBufferCreated2(handles, size, buffer_id);
}

void VideoCaptureMessageFilter::OnBufferReceived(
    const VideoCaptureMsg_BufferReady_Params& params) {
  Delegate* const delegate = find_delegate(params.device_id);
  if (!delegate) {
    DLOG(WARNING) << "OnBufferReceived: Got video SHM buffer for a "
                     "non-existent or removed video capture.";

    // Send the buffer back to Host in case it's waiting for all buffers
    // to be returned.
    Send(new VideoCaptureHostMsg_BufferReady(params.device_id, params.buffer_id,
                                             gpu::SyncToken(), -1.0));
    return;
  }

  delegate->OnBufferReceived(params.buffer_id,
                             params.timestamp,
                             params.metadata,
                             params.pixel_format,
                             params.storage_type,
                             params.coded_size,
                             params.visible_rect);
}

void VideoCaptureMessageFilter::OnBufferDestroyed(int device_id,
                                                  int buffer_id) {
  Delegate* const delegate = find_delegate(device_id);
  if (!delegate) {
    DLOG(WARNING) << "OnBufferDestroyed: Instructed to free buffer for a "
        "non-existent or removed video capture.";
    return;
  }

  delegate->OnBufferDestroyed(buffer_id);
}

void VideoCaptureMessageFilter::OnDeviceStateChanged(
    int device_id,
    VideoCaptureState state) {
  Delegate* const delegate = find_delegate(device_id);
  if (!delegate) {
    DLOG(WARNING) << "OnDeviceStateChanged: Got video capture event for a "
        "non-existent or removed video capture.";
    return;
  }
  delegate->OnStateChanged(state);
}

void VideoCaptureMessageFilter::OnDeviceSupportedFormatsEnumerated(
    int device_id,
    const media::VideoCaptureFormats& supported_formats) {
  Delegate* const delegate = find_delegate(device_id);
  if (!delegate) {
    DLOG(WARNING) << "OnDeviceFormatsEnumerated: unknown device";
    return;
  }
  delegate->OnDeviceSupportedFormatsEnumerated(supported_formats);
}

void VideoCaptureMessageFilter::OnDeviceFormatsInUseReceived(
    int device_id,
    const media::VideoCaptureFormats& formats_in_use) {
  Delegate* const delegate = find_delegate(device_id);
  if (!delegate) {
    DLOG(WARNING) << "OnDeviceFormatInUse: unknown device";
    return;
  }
  delegate->OnDeviceFormatsInUseReceived(formats_in_use);
}

}  // namespace content
