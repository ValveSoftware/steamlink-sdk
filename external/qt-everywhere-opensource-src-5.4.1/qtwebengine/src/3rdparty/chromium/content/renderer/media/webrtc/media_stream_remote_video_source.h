// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_VIDEO_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_VIDEO_SOURCE_H_

#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

// MediaStreamRemoteVideoSource implements the MediaStreamVideoSource interface
// for video tracks received on a PeerConnection. The purpose of the class is
// to make sure there is no difference between a video track where the source is
// a local source and a video track where the source is a remote video track.
class CONTENT_EXPORT MediaStreamRemoteVideoSource
     : public MediaStreamVideoSource,
       NON_EXPORTED_BASE(public webrtc::ObserverInterface) {
 public:
  explicit MediaStreamRemoteVideoSource(
      webrtc::VideoTrackInterface* remote_track);
  virtual ~MediaStreamRemoteVideoSource();

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

  // Used by tests to test that a frame can be received and that the
  // MediaStreamRemoteVideoSource behaves as expected.
  webrtc::VideoRendererInterface* RenderInterfaceForTest();

 private:
  // webrtc::ObserverInterface implementation.
  virtual void OnChanged() OVERRIDE;

  scoped_refptr<webrtc::VideoTrackInterface> remote_track_;
  webrtc::MediaStreamTrackInterface::TrackState last_state_;

  // Internal class used for receiving frames from the webrtc track on a
  // libjingle thread and forward it to the IO-thread.
  class RemoteVideoSourceDelegate;
  scoped_refptr<RemoteVideoSourceDelegate> delegate_;

  // Bound to the render thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamRemoteVideoSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_VIDEO_SOURCE_H_
