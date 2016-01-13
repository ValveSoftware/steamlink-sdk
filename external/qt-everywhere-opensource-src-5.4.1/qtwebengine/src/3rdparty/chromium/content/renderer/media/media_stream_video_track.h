// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_TRACK_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/media_stream_video_source.h"

namespace content {

// MediaStreamVideoTrack is a video specific representation of a
// blink::WebMediaStreamTrack in content. It is owned by the blink object
// and can be retrieved from a blink object using
// WebMediaStreamTrack::extraData() or MediaStreamVideoTrack::GetVideoTrack.
class CONTENT_EXPORT MediaStreamVideoTrack : public MediaStreamTrack {
 public:
  // Help method to create a blink::WebMediaStreamTrack and a
  // MediaStreamVideoTrack instance. The MediaStreamVideoTrack object is owned
  // by the blink object in its WebMediaStreamTrack::ExtraData member.
  // |callback| is triggered if the track is added to the source
  // successfully and will receive video frames that match |constraints|
  // or if the source fail to provide video frames.
  // If |enabled| is true, sinks added to the track will
  // receive video frames when the source deliver frames to the track.
  static blink::WebMediaStreamTrack CreateVideoTrack(
      MediaStreamVideoSource* source,
      const blink::WebMediaConstraints& constraints,
      const MediaStreamVideoSource::ConstraintsCallback& callback,
      bool enabled);

  static MediaStreamVideoTrack* GetVideoTrack(
      const blink::WebMediaStreamTrack& track);

  // Constructor for video tracks.
  MediaStreamVideoTrack(
      MediaStreamVideoSource* source,
      const blink::WebMediaConstraints& constraints,
      const MediaStreamVideoSource::ConstraintsCallback& callback,
      bool enabled);
  virtual ~MediaStreamVideoTrack();

  virtual void SetEnabled(bool enabled) OVERRIDE;
  virtual void Stop() OVERRIDE;

  void OnReadyStateChanged(blink::WebMediaStreamSource::ReadyState state);

  const blink::WebMediaConstraints& constraints() const {
    return constraints_;
  }

 protected:
  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

 private:
  // MediaStreamVideoSink is a friend to allow it to call AddSink() and
  // RemoveSink().
  friend class MediaStreamVideoSink;
  FRIEND_TEST_ALL_PREFIXES(MediaStreamRemoteVideoSourceTest, StartTrack);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamRemoteVideoSourceTest, RemoteTrackStop);
  FRIEND_TEST_ALL_PREFIXES(VideoDestinationHandlerTest, PutFrame);

  // Add |sink| to receive state changes on the main render thread and video
  // frames in the |callback| method on the IO-thread.
  // |callback| will be reset on the render thread.
  // These two methods are private such that no subclass can intercept and
  // store the callback. This is important to ensure that we can release
  // the callback on render thread without reference to it on the IO-thread.
  void AddSink(MediaStreamVideoSink* sink,
               const VideoCaptureDeliverFrameCB& callback);
  void RemoveSink(MediaStreamVideoSink* sink);

  std::vector<MediaStreamVideoSink*> sinks_;

  // |FrameDeliverer| is an internal helper object used for delivering video
  // frames on the IO-thread using callbacks to all registered tracks.
  class FrameDeliverer;
  scoped_refptr<FrameDeliverer> frame_deliverer_;

  blink::WebMediaConstraints constraints_;

  // Weak ref to the source this tracks is connected to.  |source_| is owned
  // by the blink::WebMediaStreamSource and is guaranteed to outlive the
  // track.
  MediaStreamVideoSource* source_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_TRACK_H_
