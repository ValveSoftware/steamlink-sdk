// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_media_stream_adapter.h"

#include "base/logging.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

WebRtcMediaStreamAdapter::WebRtcMediaStreamAdapter(
    const blink::WebMediaStream& web_stream,
    PeerConnectionDependencyFactory* factory)
    : web_stream_(web_stream),
      factory_(factory) {
  webrtc_media_stream_ =
      factory_->CreateLocalMediaStream(web_stream.id().utf8());

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  web_stream_.audioTracks(audio_tracks);
  for (size_t i = 0; i < audio_tracks.size(); ++i)
    CreateAudioTrack(audio_tracks[i]);

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream_.videoTracks(video_tracks);
  for (size_t i = 0; i < video_tracks.size(); ++i)
    CreateVideoTrack(video_tracks[i]);

  MediaStream* native_stream = MediaStream::GetMediaStream(web_stream_);
  native_stream->AddObserver(this);
}

WebRtcMediaStreamAdapter::~WebRtcMediaStreamAdapter() {
  MediaStream* native_stream = MediaStream::GetMediaStream(web_stream_);
  native_stream->RemoveObserver(this);
}

void WebRtcMediaStreamAdapter::TrackAdded(
    const blink::WebMediaStreamTrack& track) {
  if (track.source().type() == blink::WebMediaStreamSource::TypeAudio) {
    CreateAudioTrack(track);
  } else {
    CreateVideoTrack(track);
  }
}

void WebRtcMediaStreamAdapter::TrackRemoved(
    const blink::WebMediaStreamTrack& track) {
  const std::string track_id = track.id().utf8();
  if (track.source().type() == blink::WebMediaStreamSource::TypeAudio) {
    webrtc_media_stream_->RemoveTrack(
        webrtc_media_stream_->FindAudioTrack(track_id));
  } else {
    DCHECK_EQ(track.source().type(), blink::WebMediaStreamSource::TypeVideo);
    scoped_refptr<webrtc::VideoTrackInterface> webrtc_track =
        webrtc_media_stream_->FindVideoTrack(track_id).get();
    webrtc_media_stream_->RemoveTrack(webrtc_track.get());

    for (ScopedVector<WebRtcVideoTrackAdapter>::iterator it =
             video_adapters_.begin(); it != video_adapters_.end(); ++it) {
      if ((*it)->webrtc_video_track() == webrtc_track) {
        video_adapters_.erase(it);
        break;
      }
    }
  }
}

void WebRtcMediaStreamAdapter::CreateAudioTrack(
    const blink::WebMediaStreamTrack& track) {
  DCHECK_EQ(track.source().type(), blink::WebMediaStreamSource::TypeAudio);
  // A media stream is connected to a peer connection, enable the
  // peer connection mode for the sources.
  MediaStreamTrack* native_track = MediaStreamTrack::GetTrack(track);
  if (!native_track || !native_track->is_local_track()) {
    // We don't support connecting remote audio tracks to PeerConnection yet.
    // See issue http://crbug/344303.
    // TODO(xians): Remove this after we support connecting remote audio track
    // to PeerConnection.
    DLOG(ERROR) << "webrtc audio track can not be created from a remote audio"
        << " track.";
    NOTIMPLEMENTED();
    return;
  }

  // This is a local audio track.
  const blink::WebMediaStreamSource& source = track.source();
  MediaStreamAudioSource* audio_source =
      static_cast<MediaStreamAudioSource*>(source.extraData());
  if (audio_source && audio_source->GetAudioCapturer())
    audio_source->GetAudioCapturer()->EnablePeerConnectionMode();

  webrtc_media_stream_->AddTrack(native_track->GetAudioAdapter());
}

void WebRtcMediaStreamAdapter::CreateVideoTrack(
    const blink::WebMediaStreamTrack& track) {
  DCHECK_EQ(track.source().type(), blink::WebMediaStreamSource::TypeVideo);
  WebRtcVideoTrackAdapter* adapter =
      new WebRtcVideoTrackAdapter(track, factory_);
  video_adapters_.push_back(adapter);
  webrtc_media_stream_->AddTrack(adapter->webrtc_video_track());
}

}  // namespace content
