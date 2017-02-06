// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_EVENT_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_EVENT_HANDLER_H_

#include <memory>

#include "base/memory/shared_memory.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {
class VideoFrame;
}  // namespace media

namespace content {

typedef int VideoCaptureControllerID;

// VideoCaptureControllerEventHandler is the interface for
// VideoCaptureController to notify clients about the events such as
// BufferReady, FrameInfo, Error, etc.
class CONTENT_EXPORT VideoCaptureControllerEventHandler {
 public:
  // An Error has occurred in the VideoCaptureDevice.
  virtual void OnError(VideoCaptureControllerID id) = 0;

  // A buffer has been newly created.
  virtual void OnBufferCreated(VideoCaptureControllerID id,
                               base::SharedMemoryHandle handle,
                               int length,
                               int buffer_id) = 0;

  // A GpuMemoryBuffer backed buffer has been newly created.
  virtual void OnBufferCreated2(
      VideoCaptureControllerID id,
      const std::vector<gfx::GpuMemoryBufferHandle>& handles,
      const gfx::Size& size,
      int buffer_id) = 0;

  // A previously created buffer has been freed and will no longer be used.
  virtual void OnBufferDestroyed(VideoCaptureControllerID id,
                                 int buffer_id) = 0;

  // A buffer has been filled with a captured VideoFrame.
  virtual void OnBufferReady(VideoCaptureControllerID id,
                             int buffer_id,
                             const scoped_refptr<media::VideoFrame>& frame) = 0;

  // The capture session has ended and no more frames will be sent.
  virtual void OnEnded(VideoCaptureControllerID id) = 0;

 protected:
  virtual ~VideoCaptureControllerEventHandler() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_EVENT_HANDLER_H_
