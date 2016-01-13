// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread_checker.h"
#include "content/common/media/video_capture.h"
#include "content/renderer/media/media_stream_video_source.h"

namespace content {

// VideoCapturerDelegate is a delegate used by MediaStreamVideoCapturerSource
// for local video capturer. It uses VideoCaptureImplManager to start / stop
// and receive I420 frames from Chrome's video capture implementation.
//
// This is a render thread only object.
class CONTENT_EXPORT VideoCapturerDelegate
    : public base::RefCountedThreadSafe<VideoCapturerDelegate> {
 public:
  typedef base::Callback<void(bool running)> RunningCallback;

  explicit VideoCapturerDelegate(
      const StreamDeviceInfo& device_info);

  // Collects the formats that can currently be used.
  // |max_requested_height| and |max_requested_width| is used by Tab and Screen
  // capture to decide what resolution to generate.
  // |callback| is triggered when the formats have been collected.
  virtual void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      const VideoCaptureDeviceFormatsCB& callback);

  // Starts capturing frames using the resolution in |params|.
  // |new_frame_callback| is triggered when a new video frame is available.
  // If capturing is started successfully then |running_callback| will be
  // called with a parameter of true.
  // If capturing fails to start or stopped due to an external event then
  // |running_callback| will be called with a parameter of false.
  virtual void StartCapture(
      const media::VideoCaptureParams& params,
      const VideoCaptureDeliverFrameCB& new_frame_callback,
      const RunningCallback& running_callback);

  // Stops capturing frames and clears all callbacks including the
  // SupportedFormatsCallback callback.
  virtual void StopCapture();

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaStreamVideoCapturerSourceTest, Ended);
  friend class base::RefCountedThreadSafe<VideoCapturerDelegate>;
  friend class MockVideoCapturerDelegate;

  virtual ~VideoCapturerDelegate();

  void OnStateUpdateOnRenderThread(VideoCaptureState state);
  void OnDeviceFormatsInUseReceived(const media::VideoCaptureFormats& formats);
  void OnDeviceSupportedFormatsEnumerated(
      const media::VideoCaptureFormats& formats);

  // The id identifies which video capture device is used for this video
  // capture session.
  media::VideoCaptureSessionId session_id_;
  base::Closure release_device_cb_;
  base::Closure stop_capture_cb_;

  bool is_screen_cast_;
  bool got_first_frame_;

  // |running_callback| is provided to this class in StartCapture and must be
  // valid until StopCapture is called.
  RunningCallback running_callback_;

  VideoCaptureDeviceFormatsCB source_formats_callback_;

  // Bound to the render thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(VideoCapturerDelegate);
};

// Owned by WebMediaStreamSource in Blink as a representation of a video
// stream coming from a camera.
// This is a render thread only object. All methods must be called on the
// render thread.
class CONTENT_EXPORT MediaStreamVideoCapturerSource
    : public MediaStreamVideoSource {
 public:
  MediaStreamVideoCapturerSource(
      const StreamDeviceInfo& device_info,
      const SourceStoppedCallback& stop_callback,
      const scoped_refptr<VideoCapturerDelegate>& delegate);

  virtual ~MediaStreamVideoCapturerSource();

 protected:
  // Implements MediaStreamVideoSource.
  virtual void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      const VideoCaptureDeviceFormatsCB& callback) OVERRIDE;

  virtual void StartSourceImpl(
      const media::VideoCaptureParams& params,
      const VideoCaptureDeliverFrameCB& frame_callback) OVERRIDE;

  virtual void StopSourceImpl() OVERRIDE;

 private:
  // The delegate that provides video frames.
  scoped_refptr<VideoCapturerDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoCapturerSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_
