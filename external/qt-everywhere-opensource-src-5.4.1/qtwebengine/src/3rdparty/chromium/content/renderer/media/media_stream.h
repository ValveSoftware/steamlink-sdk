// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"

namespace webrtc {
class MediaStreamInterface;
}

namespace content {

// MediaStreamObserver can be used to get notifications of when a track is
// added or removed from a MediaStream.
class MediaStreamObserver {
 public:
  // TrackAdded is called |track| is added to the observed MediaStream.
  virtual void TrackAdded(const blink::WebMediaStreamTrack& track)  = 0;
  // TrackRemoved is called |track| is added to the observed MediaStream.
  virtual void TrackRemoved(const blink::WebMediaStreamTrack& track) = 0;

 protected:
  virtual ~MediaStreamObserver() {}
};

// MediaStream is the Chrome representation of blink::WebMediaStream.
// It is owned by blink::WebMediaStream as blink::WebMediaStream::ExtraData.
// Its lifetime is the same as the blink::WebMediaStream instance it belongs to.
class CONTENT_EXPORT MediaStream
    : NON_EXPORTED_BASE(public blink::WebMediaStream::ExtraData) {
 public:
  // Constructor for local MediaStreams.
  MediaStream(const blink::WebMediaStream& stream);

  // Constructor for remote MediaStreams.
  // TODO(xians): Remove once the audio renderer don't separate between local
  // and remotely generated streams.
  explicit MediaStream(webrtc::MediaStreamInterface* webrtc_stream);

  virtual ~MediaStream();

  // Returns an instance of MediaStream. This method will never return NULL.
  static MediaStream* GetMediaStream(
      const blink::WebMediaStream& stream);

  // Returns a libjingle representation of a remote MediaStream.
  // TODO(xians): Remove once the audio renderer don't separate between local
  // and remotely generated streams.
  static webrtc::MediaStreamInterface* GetAdapter(
      const blink::WebMediaStream& stream);

  // Adds an observer to this MediaStream. Its the callers responsibility to
  // remove the observer before the destruction of the MediaStream.
  void AddObserver(MediaStreamObserver* observer);
  void RemoveObserver(MediaStreamObserver* observer);

  // TODO(xians): Remove |is_local| once AudioTracks can be rendered the same
  // way regardless if they are local or remote.
  bool is_local() const { return is_local_; }

  // Called by MediaStreamCenter when a track has been added to a stream stream.
  bool AddTrack(const blink::WebMediaStreamTrack& track);

  // Called by MediaStreamCenter when a track has been removed from |stream|.
  bool RemoveTrack(const blink::WebMediaStreamTrack& track);

 protected:
  virtual webrtc::MediaStreamInterface* GetWebRtcAdapter(
      const blink::WebMediaStream& stream);

 private:
  base::ThreadChecker thread_checker_;
  const bool is_local_;
  const std::string label_;
  std::vector<MediaStreamObserver*> observers_;

  // TODO(xians): Remove once the audio renderer don't separate between local
  // and remotely generated streams.
  scoped_refptr<webrtc::MediaStreamInterface> webrtc_media_stream_;

  DISALLOW_COPY_AND_ASSIGN(MediaStream);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_H_
