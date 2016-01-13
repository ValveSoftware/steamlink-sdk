// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_REMOTE_MEDIA_STREAM_IMPL_H_
#define CONTENT_RENDERER_MEDIA_REMOTE_MEDIA_STREAM_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace content {

class RemoteMediaStreamTrackAdapter;

// RemoteMediaStreamImpl serves as a container and glue between remote webrtc
// MediaStreams and WebKit MediaStreams. For each remote MediaStream received
// on a PeerConnection a RemoteMediaStreamImpl instance is created and
// owned by RtcPeerConnection.
class CONTENT_EXPORT RemoteMediaStreamImpl
    : NON_EXPORTED_BASE(public webrtc::ObserverInterface),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  explicit RemoteMediaStreamImpl(
      webrtc::MediaStreamInterface* webrtc_stream);
  virtual ~RemoteMediaStreamImpl();

  const blink::WebMediaStream& webkit_stream() { return webkit_stream_; }

 private:
  // webrtc::ObserverInterface implementation.
  virtual void OnChanged() OVERRIDE;

  scoped_refptr<webrtc::MediaStreamInterface> webrtc_stream_;
  ScopedVector<RemoteMediaStreamTrackAdapter> video_track_observers_;
  ScopedVector<RemoteMediaStreamTrackAdapter> audio_track_observers_;
  blink::WebMediaStream webkit_stream_;

  DISALLOW_COPY_AND_ASSIGN(RemoteMediaStreamImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_REMOTE_MEDIA_STREAM_IMPL_H_
