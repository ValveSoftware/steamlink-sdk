// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MessageFilter that handles video capture messages and delegates them to
// video captures. VideoCaptureMessageFilter is operated on IO thread of
// render process. It intercepts video capture messages and process them on
// IO thread since these messages are time critical.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_H_

#include <map>

#include "base/memory/shared_memory.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "ipc/message_filter.h"
#include "media/video/capture/video_capture_types.h"

namespace gpu {
struct MailboxHolder;
}  // namespace gpu

namespace content {

class CONTENT_EXPORT VideoCaptureMessageFilter : public IPC::MessageFilter {
 public:
  class CONTENT_EXPORT Delegate {
   public:
    // Called when a video frame buffer is created in the browser process.
    virtual void OnBufferCreated(base::SharedMemoryHandle handle,
                                 int length,
                                 int buffer_id) = 0;

    virtual void OnBufferDestroyed(int buffer_id) = 0;

    // Called when a video frame buffer is received from the browser process.
    virtual void OnBufferReceived(int buffer_id,
                                  const media::VideoCaptureFormat& format,
                                  base::TimeTicks timestamp) = 0;

    // Called when a video mailbox buffer is received from the browser process.
    virtual void OnMailboxBufferReceived(
        int buffer_id,
        const gpu::MailboxHolder& mailbox_holder,
        const media::VideoCaptureFormat& format,
        base::TimeTicks timestamp) = 0;

    // Called when state of a video capture device has changed in the browser
    // process.
    virtual void OnStateChanged(VideoCaptureState state) = 0;

    // Called upon reception of device's supported formats back from browser.
    virtual void OnDeviceSupportedFormatsEnumerated(
        const media::VideoCaptureFormats& supported_formats) = 0;

    // Called upon reception of format(s) in use by a device back from browser.
    virtual void OnDeviceFormatsInUseReceived(
        const media::VideoCaptureFormats& formats_in_use) = 0;

    // Called when the delegate has been added to filter's delegate list.
    // |device_id| is the device id for the delegate.
    virtual void OnDelegateAdded(int32 device_id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  VideoCaptureMessageFilter();

  // Add a delegate to the map.
  void AddDelegate(Delegate* delegate);

  // Remove a delegate from the map.
  void RemoveDelegate(Delegate* delegate);

  // Send a message asynchronously.
  virtual bool Send(IPC::Message* message);

  // IPC::MessageFilter override. Called on IO thread.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

 protected:
  virtual ~VideoCaptureMessageFilter();

 private:
  typedef std::map<int32, Delegate*> Delegates;

  // Receive a newly created buffer from browser process.
  void OnBufferCreated(int device_id,
                       base::SharedMemoryHandle handle,
                       int length,
                       int buffer_id);

  // Release a buffer received by OnBufferCreated.
  void OnBufferDestroyed(int device_id,
                         int buffer_id);

  // Receive a filled buffer from browser process.
  void OnBufferReceived(int device_id,
                        int buffer_id,
                        const media::VideoCaptureFormat& format,
                        base::TimeTicks timestamp);

  // Receive a filled texture mailbox buffer from browser process.
  void OnMailboxBufferReceived(int device_id,
                               int buffer_id,
                               const gpu::MailboxHolder& mailbox_holder,
                               const media::VideoCaptureFormat& format,
                               base::TimeTicks timestamp);

  // State of browser process' video capture device has changed.
  void OnDeviceStateChanged(int device_id, VideoCaptureState state);

  // Receive a device's supported formats back from browser process.
  void OnDeviceSupportedFormatsEnumerated(
      int device_id,
      const media::VideoCaptureFormats& supported_formats);

  // Receive the formats in-use by a device back from browser process.
  void OnDeviceFormatsInUseReceived(
      int device_id,
      const media::VideoCaptureFormats& formats_in_use);

  // Finds the delegate associated with |device_id|, NULL if not found.
  Delegate* find_delegate(int device_id) const;

  // A map of device ids to delegates.
  Delegates delegates_;
  Delegates pending_delegates_;
  int32 last_device_id_;

  IPC::Sender* sender_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_H_
