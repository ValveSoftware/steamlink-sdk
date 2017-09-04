// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_media_stream_adapter.h"

#include "base/logging.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/webrtc/media_stream_video_webrtc_sink.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/processed_local_audio_source.h"
#include "content/renderer/media/webrtc/webrtc_audio_sink.h"
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
  for (blink::WebMediaStreamTrack& audio_track : audio_tracks)
    AddAudioSinkToTrack(audio_track);

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream_.videoTracks(video_tracks);
  for (blink::WebMediaStreamTrack& video_track : video_tracks)
    AddVideoSinkToTrack(video_track);

  MediaStream* const native_stream = MediaStream::GetMediaStream(web_stream_);
  native_stream->AddObserver(this);
}

WebRtcMediaStreamAdapter::~WebRtcMediaStreamAdapter() {
  MediaStream* const native_stream = MediaStream::GetMediaStream(web_stream_);
  native_stream->RemoveObserver(this);

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  web_stream_.audioTracks(audio_tracks);
  for (blink::WebMediaStreamTrack& audio_track : audio_tracks)
    TrackRemoved(audio_track);
  DCHECK(audio_sinks_.empty());
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream_.videoTracks(video_tracks);
  for (blink::WebMediaStreamTrack& video_track : video_tracks)
    TrackRemoved(video_track);
  DCHECK(video_sinks_.empty());
}

void WebRtcMediaStreamAdapter::TrackAdded(
    const blink::WebMediaStreamTrack& track) {
  if (track.source().getType() == blink::WebMediaStreamSource::TypeAudio)
    AddAudioSinkToTrack(track);
  else
    AddVideoSinkToTrack(track);
}

void WebRtcMediaStreamAdapter::TrackRemoved(
    const blink::WebMediaStreamTrack& track) {
  const std::string track_id = track.id().utf8();
  if (track.source().getType() == blink::WebMediaStreamSource::TypeAudio) {
    scoped_refptr<webrtc::AudioTrackInterface> webrtc_track =
        make_scoped_refptr(
            webrtc_media_stream_->FindAudioTrack(track_id).get());
    if (!webrtc_track)
      return;
    webrtc_media_stream_->RemoveTrack(webrtc_track.get());

    for (auto it = audio_sinks_.begin(); it != audio_sinks_.end(); ++it) {
      if ((*it)->webrtc_audio_track() == webrtc_track.get()) {
        if (auto* media_stream_track = MediaStreamAudioTrack::From(track))
          media_stream_track->RemoveSink(it->get());
        audio_sinks_.erase(it);
        break;
      }
    }
  } else {
    DCHECK_EQ(track.source().getType(), blink::WebMediaStreamSource::TypeVideo);
    scoped_refptr<webrtc::VideoTrackInterface> webrtc_track =
        make_scoped_refptr(
            webrtc_media_stream_->FindVideoTrack(track_id).get());
    if (!webrtc_track)
      return;
    webrtc_media_stream_->RemoveTrack(webrtc_track.get());

    for (auto it = video_sinks_.begin(); it != video_sinks_.end(); ++it) {
      if ((*it)->webrtc_video_track() == webrtc_track.get()) {
        video_sinks_.erase(it);
        break;
      }
    }
  }
}

void WebRtcMediaStreamAdapter::AddAudioSinkToTrack(
    const blink::WebMediaStreamTrack& track) {
  MediaStreamAudioTrack* native_track = MediaStreamAudioTrack::From(track);
  if (!native_track) {
    DLOG(ERROR) << "No native track for blink audio track.";
    return;
  }

  // Non-WebRtc remote sources and local sources do not provide an instance of
  // the webrtc::AudioSourceInterface, and also do not need references to the
  // audio level calculator or audio processor passed to the sink.
  webrtc::AudioSourceInterface* source_interface = nullptr;
  WebRtcAudioSink* audio_sink = new WebRtcAudioSink(
      track.id().utf8(), source_interface,
      factory_->GetWebRtcSignalingThread());

  if (auto* media_stream_source = ProcessedLocalAudioSource::From(
          MediaStreamAudioSource::From(track.source()))) {
    audio_sink->SetLevel(media_stream_source->audio_level());
    // The sink only grabs stats from the audio processor. Stats are only
    // available if audio processing is turned on. Therefore, only provide the
    // sink a reference to the processor if audio processing is turned on.
    if (auto processor = media_stream_source->audio_processor()) {
      if (processor && processor->has_audio_processing())
        audio_sink->SetAudioProcessor(processor);
    }
  }

  audio_sinks_.push_back(std::unique_ptr<WebRtcAudioSink>(audio_sink));
  native_track->AddSink(audio_sink);
  webrtc_media_stream_->AddTrack(audio_sink->webrtc_audio_track());
}

void WebRtcMediaStreamAdapter::AddVideoSinkToTrack(
    const blink::WebMediaStreamTrack& track) {
  DCHECK_EQ(track.source().getType(), blink::WebMediaStreamSource::TypeVideo);
  MediaStreamVideoWebRtcSink* video_sink =
      new MediaStreamVideoWebRtcSink(track, factory_);
  video_sinks_.push_back(
      std::unique_ptr<MediaStreamVideoWebRtcSink>(video_sink));
  webrtc_media_stream_->AddTrack(video_sink->webrtc_video_track());
}

}  // namespace content
