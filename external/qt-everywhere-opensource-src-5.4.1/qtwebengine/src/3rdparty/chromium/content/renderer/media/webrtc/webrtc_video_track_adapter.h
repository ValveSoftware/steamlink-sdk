// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_TRACK_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_TRACK_ADAPTER_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/libjingle/source/talk/app/webrtc/videosourceinterface.h"

namespace content {

class MediaStreamVideoTrack;

// WebRtcVideoTrackAdapter is an adapter between a
// content::MediaStreamVideoTrack object and a webrtc VideoTrack that is
// currently sent on a PeerConnection.
// The responsibility of the class is to create and own a representation of a
// webrtc VideoTrack that can be added and removed from a RTCPeerConnection.
// An instance of WebRtcVideoTrackAdapter is created when a VideoTrack is
// added to an RTCPeerConnection object.
// Instances of this class is owned by the WebRtcMediaStreamAdapter object that
// created it.
class WebRtcVideoTrackAdapter : public MediaStreamVideoSink {
 public:
  WebRtcVideoTrackAdapter(const blink::WebMediaStreamTrack& track,
                          PeerConnectionDependencyFactory* factory);
  virtual ~WebRtcVideoTrackAdapter();

  webrtc::VideoTrackInterface* webrtc_video_track() {
    return video_track_.get();
  }

 protected:
  // Implementation of MediaStreamSink.
  virtual void OnEnabledChanged(bool enabled) OVERRIDE;

 private:
  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  scoped_refptr<webrtc::VideoTrackInterface> video_track_;
  blink::WebMediaStreamTrack web_track_;

  class WebRtcVideoSourceAdapter;
  scoped_refptr<WebRtcVideoSourceAdapter> source_adapter_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcVideoTrackAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_TRACK_ADAPTER_H_
