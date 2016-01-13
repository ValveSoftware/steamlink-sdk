// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_renderer_factory.h"

#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/rtc_video_renderer.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/media/webrtc_local_audio_renderer.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/audio_hardware_config.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

namespace {

PeerConnectionDependencyFactory* GetPeerConnectionDependencyFactory() {
  return RenderThreadImpl::current()->GetPeerConnectionDependencyFactory();
}

void GetDefaultOutputDeviceParams(
    int* output_sample_rate, int* output_buffer_size) {
  // Fetch the default audio output hardware config.
  media::AudioHardwareConfig* hardware_config =
      RenderThreadImpl::current()->GetAudioHardwareConfig();
  *output_sample_rate = hardware_config->GetOutputSampleRate();
  *output_buffer_size = hardware_config->GetOutputBufferSize();
}


// Returns a valid session id if a single capture device is currently open
// (and then the matching session_id), otherwise -1.
// This is used to pass on a session id to a webrtc audio renderer (either
// local or remote), so that audio will be rendered to a matching output
// device, should one exist.
// Note that if there are more than one open capture devices the function
// will not be able to pick an appropriate device and return false.
bool GetAuthorizedDeviceInfoForAudioRenderer(
    int* session_id,
    int* output_sample_rate,
    int* output_frames_per_buffer) {
  WebRtcAudioDeviceImpl* audio_device =
      GetPeerConnectionDependencyFactory()->GetWebRtcAudioDevice();
  if (!audio_device)
    return false;

  return audio_device->GetAuthorizedDeviceInfoForAudioRenderer(
      session_id, output_sample_rate, output_frames_per_buffer);
}

scoped_refptr<WebRtcAudioRenderer> CreateRemoteAudioRenderer(
    webrtc::MediaStreamInterface* stream,
    int routing_id,
    int render_frame_id) {
  if (stream->GetAudioTracks().empty())
    return NULL;

  DVLOG(1) << "MediaStreamRendererFactory::CreateRemoteAudioRenderer label:"
           << stream->label();

  // TODO(tommi): Change the default value of session_id to be
  // StreamDeviceInfo::kNoId.  Also update AudioOutputDevice etc.
  int session_id = 0, sample_rate = 0, buffer_size = 0;
  if (!GetAuthorizedDeviceInfoForAudioRenderer(&session_id,
                                               &sample_rate,
                                               &buffer_size)) {
    GetDefaultOutputDeviceParams(&sample_rate, &buffer_size);
  }

  return new WebRtcAudioRenderer(
      stream, routing_id, render_frame_id,  session_id,
      sample_rate, buffer_size);
}


scoped_refptr<WebRtcLocalAudioRenderer> CreateLocalAudioRenderer(
    const blink::WebMediaStreamTrack& audio_track,
    int routing_id,
    int render_frame_id) {
  DVLOG(1) << "MediaStreamRendererFactory::CreateLocalAudioRenderer";

  int session_id = 0, sample_rate = 0, buffer_size = 0;
  if (!GetAuthorizedDeviceInfoForAudioRenderer(&session_id,
                                               &sample_rate,
                                               &buffer_size)) {
    GetDefaultOutputDeviceParams(&sample_rate, &buffer_size);
  }

  // Create a new WebRtcLocalAudioRenderer instance and connect it to the
  // existing WebRtcAudioCapturer so that the renderer can use it as source.
  return new WebRtcLocalAudioRenderer(
      audio_track,
      routing_id,
      render_frame_id,
      session_id,
      buffer_size);
}

}  // namespace


MediaStreamRendererFactory::MediaStreamRendererFactory() {
}

MediaStreamRendererFactory::~MediaStreamRendererFactory() {
}

scoped_refptr<VideoFrameProvider>
MediaStreamRendererFactory::GetVideoFrameProvider(
    const GURL& url,
    const base::Closure& error_cb,
    const VideoFrameProvider::RepaintCB& repaint_cb) {
  blink::WebMediaStream web_stream =
      blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(url);
  DCHECK(!web_stream.isNull());

  DVLOG(1) << "MediaStreamRendererFactory::GetVideoFrameProvider stream:"
           << base::UTF16ToUTF8(web_stream.id());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream.videoTracks(video_tracks);
  if (video_tracks.isEmpty() ||
      !MediaStreamVideoTrack::GetTrack(video_tracks[0])) {
    return NULL;
  }

  return new RTCVideoRenderer(video_tracks[0], error_cb, repaint_cb);
}

scoped_refptr<MediaStreamAudioRenderer>
MediaStreamRendererFactory::GetAudioRenderer(
    const GURL& url, int render_view_id, int render_frame_id) {
  blink::WebMediaStream web_stream =
      blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(url);

  if (web_stream.isNull() || !web_stream.extraData())
    return NULL;  // This is not a valid stream.

  DVLOG(1) << "MediaStreamRendererFactory::GetAudioRenderer stream:"
           << base::UTF16ToUTF8(web_stream.id());

  MediaStream* native_stream = MediaStream::GetMediaStream(web_stream);

  // TODO(tommi): MediaStreams do not have a 'local or not' concept.
  // Tracks _might_, but even so, we need to fix the data flow so that
  // it works the same way for all track implementations, local, remote or what
  // have you.
  // In this function, we should simply create a renderer object that receives
  // and mixes audio from all the tracks that belong to the media stream.
  // We need to remove the |is_local| property from MediaStreamExtraData since
  // this concept is peerconnection specific (is a previously recorded stream
  // local or remote?).
  if (native_stream->is_local()) {
    // Create the local audio renderer if the stream contains audio tracks.
    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
    web_stream.audioTracks(audio_tracks);
    if (audio_tracks.isEmpty())
      return NULL;

    // TODO(xians): Add support for the case where the media stream contains
    // multiple audio tracks.
    return CreateLocalAudioRenderer(audio_tracks[0], render_view_id,
                                    render_frame_id);
  }

  webrtc::MediaStreamInterface* stream =
      MediaStream::GetAdapter(web_stream);
  if (stream->GetAudioTracks().empty())
    return NULL;

  // This is a remote WebRTC media stream.
  WebRtcAudioDeviceImpl* audio_device =
      GetPeerConnectionDependencyFactory()->GetWebRtcAudioDevice();

  // Share the existing renderer if any, otherwise create a new one.
  scoped_refptr<WebRtcAudioRenderer> renderer(audio_device->renderer());
  if (!renderer.get()) {
    renderer = CreateRemoteAudioRenderer(stream, render_view_id,
                                         render_frame_id);

    if (renderer.get() && !audio_device->SetAudioRenderer(renderer.get()))
      renderer = NULL;
  }

  return renderer.get() ?
      renderer->CreateSharedAudioRendererProxy(stream) : NULL;
}

}  // namespace content
