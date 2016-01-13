// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_EVENT_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_EVENT_HANDLER_H_

#include "base/memory/shared_memory.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace gpu {
struct MailboxHolder;
}  // namespace gpu

namespace media {
class VideoCaptureFormat;
}  // namespace media

namespace content {

// ID used for identifying an object of VideoCaptureController.
struct CONTENT_EXPORT VideoCaptureControllerID {
  explicit VideoCaptureControllerID(int device_id);

  bool operator<(const VideoCaptureControllerID& vc) const;
  bool operator==(const VideoCaptureControllerID& vc) const;

  int device_id;
};

// VideoCaptureControllerEventHandler is the interface for
// VideoCaptureController to notify clients about the events such as
// BufferReady, FrameInfo, Error, etc.
class CONTENT_EXPORT VideoCaptureControllerEventHandler {
 public:
  // An Error has occurred in the VideoCaptureDevice.
  virtual void OnError(const VideoCaptureControllerID& id) = 0;

  // A buffer has been newly created.
  virtual void OnBufferCreated(const VideoCaptureControllerID& id,
                               base::SharedMemoryHandle handle,
                               int length,
                               int buffer_id) = 0;

  // A previously created buffer has been freed and will no longer be used.
  virtual void OnBufferDestroyed(const VideoCaptureControllerID& id,
                                 int buffer_id) = 0;

  // A buffer has been filled with I420 video.
  virtual void OnBufferReady(const VideoCaptureControllerID& id,
                             int buffer_id,
                             const media::VideoCaptureFormat& format,
                             base::TimeTicks timestamp) = 0;

  // A texture mailbox buffer has been filled with data.
  virtual void OnMailboxBufferReady(const VideoCaptureControllerID& id,
                                    int buffer_id,
                                    const gpu::MailboxHolder& mailbox_holder,
                                    const media::VideoCaptureFormat& format,
                                    base::TimeTicks timestamp) = 0;

  // The capture session has ended and no more frames will be sent.
  virtual void OnEnded(const VideoCaptureControllerID& id) = 0;

 protected:
  virtual ~VideoCaptureControllerEventHandler() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_EVENT_HANDLER_H_
