// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"

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
  MediaStream();
  ~MediaStream() override;

  // Returns an instance of MediaStream. This method will never return NULL.
  static MediaStream* GetMediaStream(
      const blink::WebMediaStream& stream);

  // Adds an observer to this MediaStream. Its the callers responsibility to
  // remove the observer before the destruction of the MediaStream.
  void AddObserver(MediaStreamObserver* observer);
  void RemoveObserver(MediaStreamObserver* observer);

  // Called by MediaStreamCenter when a track has been added to a stream stream.
  bool AddTrack(const blink::WebMediaStreamTrack& track);

  // Called by MediaStreamCenter when a track has been removed from |stream|.
  bool RemoveTrack(const blink::WebMediaStreamTrack& track);

 private:
  base::ThreadChecker thread_checker_;
  std::vector<MediaStreamObserver*> observers_;

  DISALLOW_COPY_AND_ASSIGN(MediaStream);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_H_
