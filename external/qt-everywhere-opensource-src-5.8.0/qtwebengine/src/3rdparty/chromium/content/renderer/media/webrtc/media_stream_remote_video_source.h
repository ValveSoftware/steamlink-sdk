// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_VIDEO_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_VIDEO_SOURCE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

class TrackObserver;

// MediaStreamRemoteVideoSource implements the MediaStreamVideoSource interface
// for video tracks received on a PeerConnection. The purpose of the class is
// to make sure there is no difference between a video track where the source is
// a local source and a video track where the source is a remote video track.
class CONTENT_EXPORT MediaStreamRemoteVideoSource
     : public MediaStreamVideoSource {
 public:
  MediaStreamRemoteVideoSource(std::unique_ptr<TrackObserver> observer);
  ~MediaStreamRemoteVideoSource() override;

  // Should be called when the remote video track this source originates from is
  // no longer received on a PeerConnection. This cleans up the references to
  // the webrtc::MediaStreamTrackInterface instance held by |observer_|.
  void OnSourceTerminated();

 protected:
  // Implements MediaStreamVideoSource.
  void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      double max_requested_frame_rate,
      const VideoCaptureDeviceFormatsCB& callback) override;

  void StartSourceImpl(
      const media::VideoCaptureFormat& format,
      const blink::WebMediaConstraints& constraints,
      const VideoCaptureDeliverFrameCB& frame_callback) override;

  void StopSourceImpl() override;

  // Used by tests to test that a frame can be received and that the
  // MediaStreamRemoteVideoSource behaves as expected.
  rtc::VideoSinkInterface<cricket::VideoFrame>* SinkInterfaceForTest();

 private:
  void OnChanged(webrtc::MediaStreamTrackInterface::TrackState state);

  // Internal class used for receiving frames from the webrtc track on a
  // libjingle thread and forward it to the IO-thread.
  class RemoteVideoSourceDelegate;
  scoped_refptr<RemoteVideoSourceDelegate> delegate_;
  std::unique_ptr<TrackObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamRemoteVideoSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_VIDEO_SOURCE_H_
