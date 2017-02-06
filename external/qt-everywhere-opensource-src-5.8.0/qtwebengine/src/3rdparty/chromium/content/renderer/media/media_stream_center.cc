// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_center.h"

#include <stddef.h>

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/media_stream_source.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/webaudio_media_stream_source.h"
#include "content/renderer/media/webrtc_local_audio_source_provider.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrackSourcesRequest.h"
#include "third_party/WebKit/public/platform/WebSourceInfo.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebFrame.h"

using blink::WebFrame;
using blink::WebView;

namespace content {

namespace {

void CreateNativeAudioMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  blink::WebMediaStreamSource source = track.source();
  MediaStreamAudioSource* media_stream_source =
      MediaStreamAudioSource::From(source);

  // At this point, a MediaStreamAudioSource instance must exist. The one
  // exception is when a WebAudio destination node is acting as a source of
  // audio.
  //
  // TODO(miu): This needs to be moved to an appropriate location. A WebAudio
  // source should have been created before this method was called so that this
  // special case code isn't needed here.
  if (!media_stream_source && source.requiresAudioConsumer()) {
    DVLOG(1) << "Creating WebAudio media stream source.";
    media_stream_source = new WebAudioMediaStreamSource(&source);
    source.setExtraData(media_stream_source);  // Takes ownership.
  }

  if (media_stream_source)
    media_stream_source->ConnectToTrack(track);
  else
    LOG(DFATAL) << "WebMediaStreamSource missing its MediaStreamAudioSource.";
}

void CreateNativeVideoMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  DCHECK(track.getTrackData() == NULL);
  blink::WebMediaStreamSource source = track.source();
  DCHECK_EQ(source.getType(), blink::WebMediaStreamSource::TypeVideo);
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  blink::WebMediaStreamTrack writable_track(track);
  // TODO(perkj): The constraints to use here should be passed from blink when
  // a new track is created. For cloning, it should be the constraints of the
  // cloned track and not the originating source.
  // Also - source.constraints() returns an uninitialized constraint if the
  // source is coming from a remote video track. See http://crbug/287805.
  blink::WebMediaConstraints constraints = source.constraints();
  if (constraints.isNull())
    constraints.initialize();
  writable_track.setTrackData(new MediaStreamVideoTrack(
      native_source, constraints, MediaStreamVideoSource::ConstraintsCallback(),
      track.isEnabled()));
}

}  // namespace

MediaStreamCenter::MediaStreamCenter(
    blink::WebMediaStreamCenterClient* client,
    PeerConnectionDependencyFactory* factory) {}

MediaStreamCenter::~MediaStreamCenter() {}

void MediaStreamCenter::didCreateMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::didCreateMediaStreamTrack";
  DCHECK(!track.isNull() && !track.getTrackData());
  DCHECK(!track.source().isNull());

  switch (track.source().getType()) {
    case blink::WebMediaStreamSource::TypeAudio:
      CreateNativeAudioMediaStreamTrack(track);
      break;
    case blink::WebMediaStreamSource::TypeVideo:
      CreateNativeVideoMediaStreamTrack(track);
      break;
  }
}

void MediaStreamCenter::didEnableMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  MediaStreamTrack* native_track =
      MediaStreamTrack::GetTrack(track);
  if (native_track)
    native_track->SetEnabled(true);
}

void MediaStreamCenter::didDisableMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  MediaStreamTrack* native_track =
      MediaStreamTrack::GetTrack(track);
  if (native_track)
    native_track->SetEnabled(false);
}

bool MediaStreamCenter::didStopMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::didStopMediaStreamTrack";
  MediaStreamTrack* native_track = MediaStreamTrack::GetTrack(track);
  native_track->Stop();
  return true;
}

blink::WebAudioSourceProvider*
MediaStreamCenter::createWebAudioSourceFromMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::createWebAudioSourceFromMediaStreamTrack";
  MediaStreamTrack* media_stream_track =
      static_cast<MediaStreamTrack*>(track.getTrackData());
  if (!media_stream_track) {
    DLOG(ERROR) << "Native track missing for webaudio source.";
    return nullptr;
  }

  blink::WebMediaStreamSource source = track.source();
  DCHECK_EQ(source.getType(), blink::WebMediaStreamSource::TypeAudio);

  // TODO(tommi): Rename WebRtcLocalAudioSourceProvider to
  // WebAudioMediaStreamSink since it's not specific to any particular source.
  // http://crbug.com/577874
  return new WebRtcLocalAudioSourceProvider(track);
}

void MediaStreamCenter::didStopLocalMediaStream(
    const blink::WebMediaStream& stream) {
  DVLOG(1) << "MediaStreamCenter::didStopLocalMediaStream";
  MediaStream* native_stream = MediaStream::GetMediaStream(stream);
  if (!native_stream) {
    NOTREACHED();
    return;
  }

  // TODO(perkj): MediaStream::Stop is being deprecated. But for the moment we
  // need to support both MediaStream::Stop and MediaStreamTrack::Stop.
  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  stream.audioTracks(audio_tracks);
  for (size_t i = 0; i < audio_tracks.size(); ++i)
    didStopMediaStreamTrack(audio_tracks[i]);

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  stream.videoTracks(video_tracks);
  for (size_t i = 0; i < video_tracks.size(); ++i)
    didStopMediaStreamTrack(video_tracks[i]);
}

void MediaStreamCenter::didCreateMediaStream(blink::WebMediaStream& stream) {
  DVLOG(1) << "MediaStreamCenter::didCreateMediaStream";
  blink::WebMediaStream writable_stream(stream);
  MediaStream* native_stream(new MediaStream());
  writable_stream.setExtraData(native_stream);
}

bool MediaStreamCenter::didAddMediaStreamTrack(
    const blink::WebMediaStream& stream,
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::didAddMediaStreamTrack";
  MediaStream* native_stream = MediaStream::GetMediaStream(stream);
  return native_stream->AddTrack(track);
}

bool MediaStreamCenter::didRemoveMediaStreamTrack(
    const blink::WebMediaStream& stream,
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::didRemoveMediaStreamTrack";
  MediaStream* native_stream = MediaStream::GetMediaStream(stream);
  return native_stream->RemoveTrack(track);
}

}  // namespace content
