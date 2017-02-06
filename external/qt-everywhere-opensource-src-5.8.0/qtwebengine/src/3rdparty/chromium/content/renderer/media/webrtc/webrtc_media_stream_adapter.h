// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

class PeerConnectionDependencyFactory;
class MediaStreamVideoWebRtcSink;
class WebRtcAudioSink;

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
  ~WebRtcMediaStreamAdapter() override;

  bool IsEqual(const blink::WebMediaStream& web_stream) const {
    return web_stream_.getExtraData() == web_stream.getExtraData();
  }

  webrtc::MediaStreamInterface* webrtc_media_stream() const {
    return webrtc_media_stream_.get();
  }

 protected:
  // MediaStreamObserver implementation.
  void TrackAdded(const blink::WebMediaStreamTrack& track) override;
  void TrackRemoved(const blink::WebMediaStreamTrack& track) override;

 private:
  void AddAudioSinkToTrack(const blink::WebMediaStreamTrack& track);
  void AddVideoSinkToTrack(const blink::WebMediaStreamTrack& track);

  const blink::WebMediaStream web_stream_;

  // Pointer to a PeerConnectionDependencyFactory, owned by the RenderThread.
  // It's valid for the lifetime of RenderThread.
  PeerConnectionDependencyFactory* const factory_;

  scoped_refptr<webrtc::MediaStreamInterface> webrtc_media_stream_;
  std::vector<std::unique_ptr<WebRtcAudioSink>> audio_sinks_;
  std::vector<std::unique_ptr<MediaStreamVideoWebRtcSink>> video_sinks_;

  DISALLOW_COPY_AND_ASSIGN (WebRtcMediaStreamAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_H_
