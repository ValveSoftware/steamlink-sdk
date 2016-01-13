// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/webrtc/webrtc_video_track_adapter.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

class PeerConnectionDependencyFactory;

// WebRtcMediaStreamAdapter is an adapter between a blink::WebMediaStream
// object and a webrtc MediaStreams that is currently sent on a PeerConnection.
// The responsibility of the class is to create and own a representation of a
// webrtc MediaStream that can be added and removed from a RTCPeerConnection.
// An instance of WebRtcMediaStreamAdapter is created when a MediaStream is
// added to an RTCPeerConnection object
// Instances of this class is owned by the RTCPeerConnectionHandler object that
// created it.
class CONTENT_EXPORT WebRtcMediaStreamAdapter
    : NON_EXPORTED_BASE(public MediaStreamObserver) {
 public:
  WebRtcMediaStreamAdapter(const blink::WebMediaStream& web_stream,
                           PeerConnectionDependencyFactory* factory);
  virtual ~WebRtcMediaStreamAdapter();

  bool IsEqual(const blink::WebMediaStream& web_stream) {
    return web_stream_.extraData() == web_stream.extraData();
  }

  webrtc::MediaStreamInterface* webrtc_media_stream() {
    return webrtc_media_stream_.get();
  }

 protected:
  // MediaStreamObserver implementation.
  virtual void TrackAdded(const blink::WebMediaStreamTrack& track) OVERRIDE;
  virtual void TrackRemoved(const blink::WebMediaStreamTrack& track) OVERRIDE;

 private:
  void CreateAudioTrack(const blink::WebMediaStreamTrack& track);
  void CreateVideoTrack(const blink::WebMediaStreamTrack& track);

  blink::WebMediaStream web_stream_;

  // Pointer to a PeerConnectionDependencyFactory, owned by the RenderThread.
  // It's valid for the lifetime of RenderThread.
  PeerConnectionDependencyFactory* factory_;

  scoped_refptr<webrtc::MediaStreamInterface> webrtc_media_stream_;
  ScopedVector<WebRtcVideoTrackAdapter> video_adapters_;

  DISALLOW_COPY_AND_ASSIGN (WebRtcMediaStreamAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_H_
