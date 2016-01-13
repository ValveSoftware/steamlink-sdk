// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_track_metrics.h"

#include <inttypes.h>
#include <set>
#include <string>

#include "base/md5.h"
#include "content/common/media/media_stream_track_metrics_host_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

using webrtc::AudioTrackVector;
using webrtc::MediaStreamInterface;
using webrtc::MediaStreamTrackInterface;
using webrtc::PeerConnectionInterface;
using webrtc::VideoTrackVector;

namespace content {

class MediaStreamTrackMetricsObserver : public webrtc::ObserverInterface {
 public:
  MediaStreamTrackMetricsObserver(
      MediaStreamTrackMetrics::StreamType stream_type,
      MediaStreamInterface* stream,
      MediaStreamTrackMetrics* owner);
  virtual ~MediaStreamTrackMetricsObserver();

  // Sends begin/end messages for all tracks currently tracked.
  void SendLifetimeMessages(MediaStreamTrackMetrics::LifetimeEvent event);

  MediaStreamInterface* stream() { return stream_; }
  MediaStreamTrackMetrics::StreamType stream_type() { return stream_type_; }

 private:
  typedef std::set<std::string> IdSet;

  // webrtc::ObserverInterface implementation.
  virtual void OnChanged() OVERRIDE;

  template <class T>
  IdSet GetTrackIds(const std::vector<talk_base::scoped_refptr<T> >& tracks) {
    IdSet track_ids;
    typename std::vector<talk_base::scoped_refptr<T> >::const_iterator it =
        tracks.begin();
    for (; it != tracks.end(); ++it) {
      track_ids.insert((*it)->id());
    }
    return track_ids;
  }

  void ReportAddedAndRemovedTracks(
      const IdSet& new_ids,
      const IdSet& old_ids,
      MediaStreamTrackMetrics::TrackType track_type);

  // Sends a lifetime message for the given tracks. OK to call with an
  // empty |ids|, in which case the method has no side effects.
  void ReportTracks(const IdSet& ids,
                    MediaStreamTrackMetrics::TrackType track_type,
                    MediaStreamTrackMetrics::LifetimeEvent event);

  // False until start/end of lifetime messages have been sent.
  bool has_reported_start_;
  bool has_reported_end_;

  // IDs of audio and video tracks in the stream being observed.
  IdSet audio_track_ids_;
  IdSet video_track_ids_;

  MediaStreamTrackMetrics::StreamType stream_type_;
  talk_base::scoped_refptr<MediaStreamInterface> stream_;

  // Non-owning.
  MediaStreamTrackMetrics* owner_;
};

namespace {

// Used with std::find_if.
struct ObserverFinder {
  ObserverFinder(MediaStreamTrackMetrics::StreamType stream_type,
                 MediaStreamInterface* stream)
      : stream_type(stream_type), stream_(stream) {}
  bool operator()(MediaStreamTrackMetricsObserver* observer) {
    return stream_ == observer->stream() &&
           stream_type == observer->stream_type();
  }
  MediaStreamTrackMetrics::StreamType stream_type;
  MediaStreamInterface* stream_;
};

}  // namespace

MediaStreamTrackMetricsObserver::MediaStreamTrackMetricsObserver(
    MediaStreamTrackMetrics::StreamType stream_type,
    MediaStreamInterface* stream,
    MediaStreamTrackMetrics* owner)
    : has_reported_start_(false),
      has_reported_end_(false),
      stream_type_(stream_type),
      stream_(stream),
      owner_(owner) {
  OnChanged();  // To populate initial tracks.
  stream_->RegisterObserver(this);
}

MediaStreamTrackMetricsObserver::~MediaStreamTrackMetricsObserver() {
  stream_->UnregisterObserver(this);
  SendLifetimeMessages(MediaStreamTrackMetrics::DISCONNECTED);
}

void MediaStreamTrackMetricsObserver::SendLifetimeMessages(
    MediaStreamTrackMetrics::LifetimeEvent event) {
  if (event == MediaStreamTrackMetrics::CONNECTED) {
    // Both ICE CONNECTED and COMPLETED can trigger the first
    // start-of-life event, so we only report the first.
    if (has_reported_start_)
      return;
    DCHECK(!has_reported_start_ && !has_reported_end_);
    has_reported_start_ = true;
  } else {
    DCHECK(event == MediaStreamTrackMetrics::DISCONNECTED);

    // We only report the first end-of-life event, since there are
    // several cases where end-of-life can be reached. We also don't
    // report end unless we've reported start.
    if (has_reported_end_ || !has_reported_start_)
      return;
    has_reported_end_ = true;
  }

  ReportTracks(audio_track_ids_, MediaStreamTrackMetrics::AUDIO_TRACK, event);
  ReportTracks(video_track_ids_, MediaStreamTrackMetrics::VIDEO_TRACK, event);

  if (event == MediaStreamTrackMetrics::DISCONNECTED) {
    // After disconnection, we can get reconnected, so we need to
    // forget that we've sent lifetime events, while retaining all
    // other state.
    DCHECK(has_reported_start_ && has_reported_end_);
    has_reported_start_ = false;
    has_reported_end_ = false;
  }
}

void MediaStreamTrackMetricsObserver::OnChanged() {
  AudioTrackVector all_audio_tracks = stream_->GetAudioTracks();
  IdSet all_audio_track_ids = GetTrackIds(all_audio_tracks);

  VideoTrackVector all_video_tracks = stream_->GetVideoTracks();
  IdSet all_video_track_ids = GetTrackIds(all_video_tracks);

  // We only report changes after our initial report, and never after
  // our last report.
  if (has_reported_start_ && !has_reported_end_) {
    ReportAddedAndRemovedTracks(all_audio_track_ids,
                                audio_track_ids_,
                                MediaStreamTrackMetrics::AUDIO_TRACK);
    ReportAddedAndRemovedTracks(all_video_track_ids,
                                video_track_ids_,
                                MediaStreamTrackMetrics::VIDEO_TRACK);
  }

  // We always update our sets of tracks.
  audio_track_ids_ = all_audio_track_ids;
  video_track_ids_ = all_video_track_ids;
}

void MediaStreamTrackMetricsObserver::ReportAddedAndRemovedTracks(
    const IdSet& new_ids,
    const IdSet& old_ids,
    MediaStreamTrackMetrics::TrackType track_type) {
  DCHECK(has_reported_start_ && !has_reported_end_);

  IdSet added_tracks = base::STLSetDifference<IdSet>(new_ids, old_ids);
  IdSet removed_tracks = base::STLSetDifference<IdSet>(old_ids, new_ids);

  ReportTracks(added_tracks, track_type, MediaStreamTrackMetrics::CONNECTED);
  ReportTracks(
      removed_tracks, track_type, MediaStreamTrackMetrics::DISCONNECTED);
}

void MediaStreamTrackMetricsObserver::ReportTracks(
    const IdSet& ids,
    MediaStreamTrackMetrics::TrackType track_type,
    MediaStreamTrackMetrics::LifetimeEvent event) {
  for (IdSet::const_iterator it = ids.begin(); it != ids.end(); ++it) {
    owner_->SendLifetimeMessage(*it, track_type, event, stream_type_);
  }
}

MediaStreamTrackMetrics::MediaStreamTrackMetrics()
    : ice_state_(webrtc::PeerConnectionInterface::kIceConnectionNew) {}

MediaStreamTrackMetrics::~MediaStreamTrackMetrics() {
  for (ObserverVector::iterator it = observers_.begin(); it != observers_.end();
       ++it) {
    (*it)->SendLifetimeMessages(DISCONNECTED);
  }
}

void MediaStreamTrackMetrics::AddStream(StreamType type,
                                        MediaStreamInterface* stream) {
  DCHECK(CalledOnValidThread());
  MediaStreamTrackMetricsObserver* observer =
      new MediaStreamTrackMetricsObserver(type, stream, this);
  observers_.insert(observers_.end(), observer);
  SendLifeTimeMessageDependingOnIceState(observer);
}

void MediaStreamTrackMetrics::RemoveStream(StreamType type,
                                           MediaStreamInterface* stream) {
  DCHECK(CalledOnValidThread());
  ObserverVector::iterator it = std::find_if(
      observers_.begin(), observers_.end(), ObserverFinder(type, stream));
  if (it == observers_.end()) {
    // Since external apps could call removeStream with a stream they
    // never added, this can happen without it being an error.
    return;
  }

  observers_.erase(it);
}

void MediaStreamTrackMetrics::IceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  DCHECK(CalledOnValidThread());
  ice_state_ = new_state;
  for (ObserverVector::iterator it = observers_.begin(); it != observers_.end();
       ++it) {
    SendLifeTimeMessageDependingOnIceState(*it);
  }
}
void MediaStreamTrackMetrics::SendLifeTimeMessageDependingOnIceState(
    MediaStreamTrackMetricsObserver* observer) {
  // There is a state transition diagram for these states at
  // http://dev.w3.org/2011/webrtc/editor/webrtc.html#idl-def-RTCIceConnectionState
  switch (ice_state_) {
    case PeerConnectionInterface::kIceConnectionConnected:
    case PeerConnectionInterface::kIceConnectionCompleted:
      observer->SendLifetimeMessages(CONNECTED);
      break;

    case PeerConnectionInterface::kIceConnectionFailed:
      // We don't really need to handle FAILED (it is only supposed
      // to be preceded by CHECKING so we wouldn't yet have sent a
      // lifetime message) but we might as well use belt and
      // suspenders and handle it the same as the other "end call"
      // states. It will be ignored anyway if the call is not
      // already connected.
    case PeerConnectionInterface::kIceConnectionNew:
      // It's a bit weird to count NEW as an end-lifetime event, but
      // it's possible to transition directly from a connected state
      // (CONNECTED or COMPLETED) to NEW, which can then be followed
      // by a new connection. The observer will ignore the end
      // lifetime event if it was not preceded by a begin-lifetime
      // event.
    case PeerConnectionInterface::kIceConnectionDisconnected:
    case PeerConnectionInterface::kIceConnectionClosed:
      observer->SendLifetimeMessages(DISCONNECTED);
      break;

    default:
      // We ignore the remaining state (CHECKING) as it is never
      // involved in a transition from connected to disconnected or
      // vice versa.
      break;
  }
}

void MediaStreamTrackMetrics::SendLifetimeMessage(const std::string& track_id,
                                                  TrackType track_type,
                                                  LifetimeEvent event,
                                                  StreamType stream_type) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  // |render_thread| can be NULL in certain cases when running as part
  // |of a unit test.
  if (render_thread) {
    if (event == CONNECTED) {
      RenderThreadImpl::current()->Send(
          new MediaStreamTrackMetricsHost_AddTrack(
              MakeUniqueId(track_id, stream_type),
              track_type == AUDIO_TRACK,
              stream_type == RECEIVED_STREAM));
    } else {
      DCHECK_EQ(DISCONNECTED, event);
      RenderThreadImpl::current()->Send(
          new MediaStreamTrackMetricsHost_RemoveTrack(
              MakeUniqueId(track_id, stream_type)));
    }
  }
}

uint64 MediaStreamTrackMetrics::MakeUniqueIdImpl(uint64 pc_id,
                                                 const std::string& track_id,
                                                 StreamType stream_type) {
  // We use a hash over the |track| pointer and the PeerConnection ID,
  // plus a boolean flag indicating whether the track is remote (since
  // you might conceivably have a remote track added back as a sent
  // track) as the unique ID.
  //
  // We don't need a cryptographically secure hash (which MD5 should
  // no longer be considered), just one with virtually zero chance of
  // collisions when faced with non-malicious data.
  std::string unique_id_string =
      base::StringPrintf("%" PRIu64 " %s %d",
                         pc_id,
                         track_id.c_str(),
                         stream_type == RECEIVED_STREAM ? 1 : 0);

  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(&ctx, unique_id_string);
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);

  COMPILE_ASSERT(sizeof(digest.a) > sizeof(uint64), NeedBiggerDigest);
  return *reinterpret_cast<uint64*>(digest.a);
}

uint64 MediaStreamTrackMetrics::MakeUniqueId(const std::string& track_id,
                                             StreamType stream_type) {
  return MakeUniqueIdImpl(
      reinterpret_cast<uint64>(reinterpret_cast<void*>(this)),
      track_id,
      stream_type);
}

}  // namespace content
