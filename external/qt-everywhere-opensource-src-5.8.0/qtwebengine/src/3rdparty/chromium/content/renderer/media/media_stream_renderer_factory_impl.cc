// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_renderer_factory_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/media_stream_video_renderer_sink.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/track_audio_renderer.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/peer_connection_remote_audio_source.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/audio_hardware_config.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

namespace {

PeerConnectionDependencyFactory* GetPeerConnectionDependencyFactory() {
  return RenderThreadImpl::current()->GetPeerConnectionDependencyFactory();
}

// Returns a valid session id if a single WebRTC capture device is currently
// open (and then the matching session_id), otherwise 0.
// This is used to pass on a session id to an audio renderer, so that audio will
// be rendered to a matching output device, should one exist.
// Note that if there are more than one open capture devices the function
// will not be able to pick an appropriate device and return 0.
int GetSessionIdForWebRtcAudioRenderer() {
  WebRtcAudioDeviceImpl* audio_device =
      GetPeerConnectionDependencyFactory()->GetWebRtcAudioDevice();
  if (!audio_device)
    return 0;

  int session_id = 0;
  int sample_rate;        // ignored, read from output device
  int frames_per_buffer;  // ignored, read from output device
  if (!audio_device->GetAuthorizedDeviceInfoForAudioRenderer(
          &session_id, &sample_rate, &frames_per_buffer)) {
    session_id = 0;
  }
  return session_id;
}

}  // namespace


MediaStreamRendererFactoryImpl::MediaStreamRendererFactoryImpl() {
}

MediaStreamRendererFactoryImpl::~MediaStreamRendererFactoryImpl() {
}

scoped_refptr<MediaStreamVideoRenderer>
MediaStreamRendererFactoryImpl::GetVideoRenderer(
    const blink::WebMediaStream& web_stream,
    const base::Closure& error_cb,
    const MediaStreamVideoRenderer::RepaintCB& repaint_cb,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  DCHECK(!web_stream.isNull());

  DVLOG(1) << "MediaStreamRendererFactoryImpl::GetVideoRenderer stream:"
           << base::UTF16ToUTF8(base::StringPiece16(web_stream.id()));

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream.videoTracks(video_tracks);
  if (video_tracks.isEmpty() ||
      !MediaStreamVideoTrack::GetTrack(video_tracks[0])) {
    return NULL;
  }

  return new MediaStreamVideoRendererSink(video_tracks[0], error_cb, repaint_cb,
                                          media_task_runner, worker_task_runner,
                                          gpu_factories);
}

scoped_refptr<MediaStreamAudioRenderer>
MediaStreamRendererFactoryImpl::GetAudioRenderer(
    const blink::WebMediaStream& web_stream,
    int render_frame_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  DCHECK(!web_stream.isNull());
  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  web_stream.audioTracks(audio_tracks);
  if (audio_tracks.isEmpty())
    return NULL;

  DVLOG(1) << "MediaStreamRendererFactoryImpl::GetAudioRenderer stream:"
           << base::UTF16ToUTF8(base::StringPiece16(web_stream.id()));

  // TODO(tommi): We need to fix the data flow so that
  // it works the same way for all track implementations, local, remote or what
  // have you.
  // In this function, we should simply create a renderer object that receives
  // and mixes audio from all the tracks that belong to the media stream.
  // For now, we have separate renderers depending on if the first audio track
  // in the stream is local or remote.
  MediaStreamAudioTrack* audio_track =
      MediaStreamAudioTrack::From(audio_tracks[0]);
  if (!audio_track) {
    // This can happen if the track was cloned.
    // TODO(tommi, perkj): Fix cloning of tracks to handle extra data too.
    LOG(ERROR) << "No native track for WebMediaStreamTrack.";
    return nullptr;
  }

  // If the track has a local source, or is a remote track that does not use the
  // WebRTC audio pipeline, return a new TrackAudioRenderer instance.
  if (!PeerConnectionRemoteAudioTrack::From(audio_track)) {
    // TODO(xians): Add support for the case where the media stream contains
    // multiple audio tracks.
    DVLOG(1) << "Creating TrackAudioRenderer for "
             << (audio_track->is_local_track() ? "local" : "remote")
             << " track.";
    return new TrackAudioRenderer(audio_tracks[0], render_frame_id,
                                  0 /* no session_id */, device_id,
                                  security_origin);
  }

  // This is a remote WebRTC media stream.
  WebRtcAudioDeviceImpl* audio_device =
      GetPeerConnectionDependencyFactory()->GetWebRtcAudioDevice();
  DCHECK(audio_device);

  // Share the existing renderer if any, otherwise create a new one.
  scoped_refptr<WebRtcAudioRenderer> renderer(audio_device->renderer());
  if (renderer) {
    DVLOG(1) << "Using existing WebRtcAudioRenderer for remote WebRTC track.";
  } else {
    DVLOG(1) << "Creating WebRtcAudioRenderer for remote WebRTC track.";
    renderer = new WebRtcAudioRenderer(
        GetPeerConnectionDependencyFactory()->GetWebRtcSignalingThread(),
        web_stream, render_frame_id, GetSessionIdForWebRtcAudioRenderer(),
        device_id, security_origin);

    if (!audio_device->SetAudioRenderer(renderer.get()))
      return nullptr;
  }

  return renderer->CreateSharedAudioRendererProxy(web_stream);
}

}  // namespace content
