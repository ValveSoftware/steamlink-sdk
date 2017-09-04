// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_track_to_pepper_adapter.h"

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_registry_interface.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video_capture_types.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "url/gurl.h"

namespace content {

// PpFrameReceiver implements MediaStreamVideoSink so that it can be attached
// to video track to receive captured frames.
// It can be attached to a FrameReaderInterface to output the received frame.
class PpFrameReceiver : public MediaStreamVideoSink {
 public:
  PpFrameReceiver(blink::WebMediaStreamTrack track)
    : track_(track),
      reader_(NULL),
      weak_factory_(this) {
  }

  ~PpFrameReceiver() override {}

  void SetReader(FrameReaderInterface* reader) {
    DCHECK((reader_ && !reader) || (!reader_ && reader))
        << " |reader| = " << reader << ", |reader_| = " << reader_;
    if (reader) {
      MediaStreamVideoSink::ConnectToTrack(
          track_,
          media::BindToCurrentLoop(base::Bind(&PpFrameReceiver::OnVideoFrame,
                                              weak_factory_.GetWeakPtr())),
          false);
    } else {
      MediaStreamVideoSink::DisconnectFromTrack();
      weak_factory_.InvalidateWeakPtrs();
    }
    reader_ = reader;
  }

  void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame,
                    base::TimeTicks estimated_capture_time) {
    TRACE_EVENT0("video", "PpFrameReceiver::OnVideoFrame");
    if (reader_)
      reader_->GotFrame(frame);
  }

 private:
  const blink::WebMediaStreamTrack track_;
  FrameReaderInterface* reader_;
  base::WeakPtrFactory<PpFrameReceiver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PpFrameReceiver);
};

VideoTrackToPepperAdapter::VideoTrackToPepperAdapter(
    MediaStreamRegistryInterface* registry)
    : registry_(registry) {}

VideoTrackToPepperAdapter::~VideoTrackToPepperAdapter() {
  for (const auto& reader_and_receiver : reader_to_receiver_)
    delete reader_and_receiver.second;
}

bool VideoTrackToPepperAdapter::Open(const std::string& url,
                              FrameReaderInterface* reader) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const blink::WebMediaStreamTrack& track = GetFirstVideoTrack(url);
  if (track.isNull())
    return false;
  reader_to_receiver_[reader] = new SourceInfo(track, reader);
  return true;
}

bool VideoTrackToPepperAdapter::Close(FrameReaderInterface* reader) {
  DCHECK(thread_checker_. CalledOnValidThread());
  SourceInfoMap::iterator it = reader_to_receiver_.find(reader);
  if (it == reader_to_receiver_.end())
    return false;
  delete it->second;
  reader_to_receiver_.erase(it);
  return true;
}

blink::WebMediaStreamTrack VideoTrackToPepperAdapter::GetFirstVideoTrack(
    const std::string& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const blink::WebMediaStream stream = registry_
      ? registry_->GetMediaStream(url)
      : blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(GURL(url));

  if (stream.isNull()) {
    LOG(ERROR) << "GetFirstVideoSource - invalid url: " << url;
    return blink::WebMediaStreamTrack();
  }

  // Get the first video track from the stream.
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  stream.videoTracks(video_tracks);
  if (video_tracks.isEmpty()) {
    LOG(ERROR) << "GetFirstVideoSource - no video tracks. url: " << url;
    return blink::WebMediaStreamTrack();
  }

  return video_tracks[0];
}

void VideoTrackToPepperAdapter::DeliverFrameForTesting(
    FrameReaderInterface* reader,
    const scoped_refptr<media::VideoFrame>& frame) {
  SourceInfoMap::const_iterator it = reader_to_receiver_.find(reader);
  if (it == reader_to_receiver_.end())
    return;
  PpFrameReceiver* receiver = it->second->receiver_.get();
  receiver->OnVideoFrame(frame, base::TimeTicks());
}

VideoTrackToPepperAdapter::SourceInfo::SourceInfo(
    const blink::WebMediaStreamTrack& blink_track,
    FrameReaderInterface* reader)
    : receiver_(new PpFrameReceiver(blink_track)) {
  receiver_->SetReader(reader);
}

VideoTrackToPepperAdapter::SourceInfo::~SourceInfo() {
  receiver_->SetReader(NULL);
}

}  // namespace content
