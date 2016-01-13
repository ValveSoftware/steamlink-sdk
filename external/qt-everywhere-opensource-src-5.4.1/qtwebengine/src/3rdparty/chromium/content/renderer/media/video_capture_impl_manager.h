// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(hclam): This class should be renamed to VideoCaptureService.

// This class provides access to a video capture device in the browser
// process through IPC. The main function is to deliver video frames
// to a client.
//
// THREADING
//
// VideoCaptureImplManager lives only on the render thread. All methods
// must be called on this thread.
//
// VideoFrames are delivered on the IO thread. Callbacks provided by
// a client are also called on the IO thread.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_MANAGER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_MANAGER_H_

#include <map>

#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "media/video/capture/video_capture_types.h"

namespace content {

class VideoCaptureImpl;
class VideoCaptureMessageFilter;

class CONTENT_EXPORT VideoCaptureImplManager {
 public:
  VideoCaptureImplManager();
  virtual ~VideoCaptureImplManager();

  // Open a device associated with the session ID.
  // This method must be called before any methods with the same ID
  // is used.
  // Returns a callback that should be used to release the acquired
  // resources.
  base::Closure UseDevice(media::VideoCaptureSessionId id);

  // Start receiving video frames for the given session ID.
  //
  // |state_update_cb| will be called on the IO thread when capturing
  // state changes.
  // States will be one of the following four:
  // * VIDEO_CAPTURE_STATE_STARTED
  // * VIDEO_CAPTURE_STATE_STOPPED
  // * VIDEO_CAPTURE_STATE_PAUSED
  // * VIDEO_CAPTURE_STATE_ERROR
  //
  // |deliver_frame_cb| will be called on the IO thread when a video
  // frame is ready.
  //
  // Returns a callback that is used to stop capturing. Note that stopping
  // video capture is not synchronous. Client should handle the case where
  // callbacks are called after capturing is instructed to stop, typically
  // by binding the passed callbacks on a WeakPtr.
  base::Closure StartCapture(
      media::VideoCaptureSessionId id,
      const media::VideoCaptureParams& params,
      const VideoCaptureStateUpdateCB& state_update_cb,
      const VideoCaptureDeliverFrameCB& deliver_frame_cb);

  // Get supported formats supported by the device for the given session
  // ID. |callback| will be called on the IO thread.
  void GetDeviceSupportedFormats(
      media::VideoCaptureSessionId id,
      const VideoCaptureDeviceFormatsCB& callback);

  // Get supported formats currently in use for the given session ID.
  // |callback| will be called on the IO thread.
  void GetDeviceFormatsInUse(
      media::VideoCaptureSessionId id,
      const VideoCaptureDeviceFormatsCB& callback);

  // Make all existing VideoCaptureImpl instances stop/resume delivering
  // video frames to their clients, depends on flag |suspend|.
  void SuspendDevices(bool suspend);

  VideoCaptureMessageFilter* video_capture_message_filter() const {
    return filter_.get();
  }

 protected:
  virtual VideoCaptureImpl* CreateVideoCaptureImplForTesting(
      media::VideoCaptureSessionId id,
      VideoCaptureMessageFilter* filter) const;

 private:
  void StopCapture(int client_id, media::VideoCaptureSessionId id);
  void UnrefDevice(media::VideoCaptureSessionId id);

  // The int is used to count clients of the corresponding VideoCaptureImpl.
  // VideoCaptureImpl objects are owned by this object. But they are
  // destroyed on the IO thread. These are raw pointers because we destroy
  // them manually.
  typedef std::map<media::VideoCaptureSessionId,
                   std::pair<int, VideoCaptureImpl*> >
      VideoCaptureDeviceMap;
  VideoCaptureDeviceMap devices_;

  // This is an internal ID for identifying clients of VideoCaptureImpl.
  // The ID is global for the render process.
  int next_client_id_;

  scoped_refptr<VideoCaptureMessageFilter> filter_;

  // Bound to the render thread.
  base::ThreadChecker thread_checker_;

  // Bound to the render thread.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoCaptureImplManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImplManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_MANAGER_H_
