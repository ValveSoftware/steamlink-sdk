// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_TRACK_EXTRA_DATA_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_TRACK_EXTRA_DATA_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace webrtc {
class AudioTrackInterface;
class MediaStreamTrackInterface;
}  // namespace webrtc

namespace content {

// MediaStreamTrack is a Chrome representation of blink::WebMediaStreamTrack.
// It is owned by blink::WebMediaStreamTrack as
// blink::WebMediaStreamTrack::ExtraData.
class CONTENT_EXPORT MediaStreamTrack
    : NON_EXPORTED_BASE(public blink::WebMediaStreamTrack::ExtraData) {
 public:
  MediaStreamTrack(webrtc::MediaStreamTrackInterface* track,
                   bool is_local_track);
  virtual ~MediaStreamTrack();

  static MediaStreamTrack* GetTrack(
      const blink::WebMediaStreamTrack& track);

  // If a subclass overrides this method it has to call the base class.
  virtual void SetEnabled(bool enabled);

  // TODO(xians): Make this pure virtual when Stop[Track] has been
  // implemented for remote audio tracks.
  virtual void Stop();

  virtual webrtc::AudioTrackInterface* GetAudioAdapter();

  bool is_local_track () const { return is_local_track_; }

 protected:
  scoped_refptr<webrtc::MediaStreamTrackInterface> track_;

 private:
  const bool is_local_track_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_TRACK_EXTRA_DATA_H_
