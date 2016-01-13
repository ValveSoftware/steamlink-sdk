// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

// static
MediaStream* MediaStream::GetMediaStream(
    const blink::WebMediaStream& stream) {
  return static_cast<MediaStream*>(stream.extraData());
}

// static
webrtc::MediaStreamInterface* MediaStream::GetAdapter(
     const blink::WebMediaStream& stream) {
  MediaStream* native_stream = GetMediaStream(stream);
  DCHECK(native_stream);
  return native_stream->GetWebRtcAdapter(stream);
}

MediaStream::MediaStream(const blink::WebMediaStream& stream)
    : is_local_(true),
      webrtc_media_stream_(NULL) {
}

MediaStream::MediaStream(webrtc::MediaStreamInterface* webrtc_stream)
    : is_local_(false),
      webrtc_media_stream_(webrtc_stream) {
}

MediaStream::~MediaStream() {
  DCHECK(observers_.empty());
}

webrtc::MediaStreamInterface* MediaStream::GetWebRtcAdapter(
    const blink::WebMediaStream& stream) {
  DCHECK(webrtc_media_stream_);
  DCHECK(thread_checker_.CalledOnValidThread());
  return webrtc_media_stream_.get();
}

void MediaStream::AddObserver(MediaStreamObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(std::find(observers_.begin(), observers_.end(), observer) ==
      observers_.end());
  observers_.push_back(observer);
}

void MediaStream::RemoveObserver(MediaStreamObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<MediaStreamObserver*>::iterator it =
      std::find(observers_.begin(), observers_.end(), observer);
  DCHECK(it != observers_.end());
  observers_.erase(it);
}

bool MediaStream::AddTrack(const blink::WebMediaStreamTrack& track) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (std::vector<MediaStreamObserver*>::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    (*it)->TrackAdded(track);
  }
  return true;
}

bool MediaStream::RemoveTrack(const blink::WebMediaStreamTrack& track) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (std::vector<MediaStreamObserver*>::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    (*it)->TrackRemoved(track);
  }
  return true;
}

}  // namespace content
