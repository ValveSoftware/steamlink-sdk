// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/remote_media_stream_impl.h"

#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/webrtc/media_stream_remote_video_source.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/peer_connection_remote_audio_source.h"
#include "content/renderer/media/webrtc/track_observer.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {
namespace {

template <typename WebRtcTrackVector, typename AdapterType>
void CreateAdaptersForTracks(
    const WebRtcTrackVector& tracks,
    std::vector<scoped_refptr<AdapterType>>* observers,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread) {
  for (auto& track : tracks)
    observers->push_back(new AdapterType(main_thread, track));
}

template<typename VectorType>
bool IsTrackInVector(const VectorType& v, const std::string& id) {
  for (const auto& t : v) {
    if (t->id() == id)
      return true;
  }
  return false;
}
}  // namespace

// Base class used for mapping between webrtc and blink MediaStream tracks.
// An instance of a RemoteMediaStreamTrackAdapter is stored in
// RemoteMediaStreamImpl per remote audio and video track.
template<typename WebRtcMediaStreamTrackType>
class RemoteMediaStreamTrackAdapter
    : public base::RefCountedThreadSafe<
          RemoteMediaStreamTrackAdapter<WebRtcMediaStreamTrackType>> {
 public:
  RemoteMediaStreamTrackAdapter(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      WebRtcMediaStreamTrackType* webrtc_track)
      : main_thread_(main_thread), webrtc_track_(webrtc_track),
        id_(webrtc_track->id()) {
  }

  const scoped_refptr<WebRtcMediaStreamTrackType>& observed_track() {
    return webrtc_track_;
  }

  blink::WebMediaStreamTrack* webkit_track() {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(!webkit_track_.isNull());
    return &webkit_track_;
  }

  const std::string& id() const { return id_; }

  bool initialized() const {
    DCHECK(main_thread_->BelongsToCurrentThread());
    return !webkit_track_.isNull();
  }

  void Initialize() {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(!initialized());
    webkit_initialize_.Run();
    webkit_initialize_.Reset();
    DCHECK(initialized());
  }

 protected:
  friend class base::RefCountedThreadSafe<
      RemoteMediaStreamTrackAdapter<WebRtcMediaStreamTrackType>>;

  virtual ~RemoteMediaStreamTrackAdapter() {
    DCHECK(main_thread_->BelongsToCurrentThread());
  }

  void InitializeWebkitTrack(blink::WebMediaStreamSource::Type type) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(webkit_track_.isNull());

    blink::WebString webkit_track_id(base::UTF8ToUTF16(id_));
    blink::WebMediaStreamSource webkit_source;
    webkit_source.initialize(webkit_track_id, type, webkit_track_id,
                             true /* remote */);
    webkit_track_.initialize(webkit_track_id, webkit_source);
    DCHECK(!webkit_track_.isNull());
  }

  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  // This callback will be run when Initialize() is called and then freed.
  // The callback is used by derived classes to bind objects that need to be
  // instantiated and initialized on the signaling thread but then moved to
  // and used on the main thread when initializing the webkit object(s).
  base::Callback<void()> webkit_initialize_;

 private:
  const scoped_refptr<WebRtcMediaStreamTrackType> webrtc_track_;
  blink::WebMediaStreamTrack webkit_track_;
  // const copy of the webrtc track id that allows us to check it from both the
  // main and signaling threads without incurring a synchronous thread hop.
  const std::string id_;

  DISALLOW_COPY_AND_ASSIGN(RemoteMediaStreamTrackAdapter);
};

class RemoteVideoTrackAdapter
    : public RemoteMediaStreamTrackAdapter<webrtc::VideoTrackInterface> {
 public:
  // Called on the signaling thread
  RemoteVideoTrackAdapter(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      webrtc::VideoTrackInterface* webrtc_track)
      : RemoteMediaStreamTrackAdapter(main_thread, webrtc_track) {
    std::unique_ptr<TrackObserver> observer(
        new TrackObserver(main_thread, observed_track().get()));
    // Here, we use base::Unretained() to avoid a circular reference.
    webkit_initialize_ = base::Bind(
        &RemoteVideoTrackAdapter::InitializeWebkitVideoTrack,
        base::Unretained(this), base::Passed(&observer),
        observed_track()->enabled());
  }

 protected:
  ~RemoteVideoTrackAdapter() override {
    DCHECK(main_thread_->BelongsToCurrentThread());
    if (initialized()) {
      static_cast<MediaStreamRemoteVideoSource*>(
          webkit_track()->source().getExtraData())
          ->OnSourceTerminated();
    }
  }

 private:
  void InitializeWebkitVideoTrack(std::unique_ptr<TrackObserver> observer,
                                  bool enabled) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    std::unique_ptr<MediaStreamRemoteVideoSource> video_source(
        new MediaStreamRemoteVideoSource(std::move(observer)));
    InitializeWebkitTrack(blink::WebMediaStreamSource::TypeVideo);
    webkit_track()->source().setExtraData(video_source.get());
    // Initial constraints must be provided to a MediaStreamVideoTrack. But
    // no constraints are available initially on a remote video track.
    blink::WebMediaConstraints constraints;
    constraints.initialize();
    MediaStreamVideoTrack* media_stream_track =
        new MediaStreamVideoTrack(video_source.release(), constraints,
            MediaStreamVideoSource::ConstraintsCallback(), enabled);
    webkit_track()->setTrackData(media_stream_track);
  }
};

// RemoteAudioTrackAdapter is responsible for listening on state
// change notifications on a remote webrtc audio MediaStreamTracks and notify
// WebKit.
class RemoteAudioTrackAdapter
    : public RemoteMediaStreamTrackAdapter<webrtc::AudioTrackInterface>,
      public webrtc::ObserverInterface {
 public:
  RemoteAudioTrackAdapter(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      webrtc::AudioTrackInterface* webrtc_track);

  void Unregister();

 protected:
  ~RemoteAudioTrackAdapter() override;

 private:
  void InitializeWebkitAudioTrack();

  // webrtc::ObserverInterface implementation.
  void OnChanged() override;

  void OnChangedOnMainThread(
      webrtc::MediaStreamTrackInterface::TrackState state);

#if DCHECK_IS_ON()
  bool unregistered_;
#endif

  webrtc::MediaStreamTrackInterface::TrackState state_;

  DISALLOW_COPY_AND_ASSIGN(RemoteAudioTrackAdapter);
};

// Called on the signaling thread.
RemoteAudioTrackAdapter::RemoteAudioTrackAdapter(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::AudioTrackInterface* webrtc_track)
    : RemoteMediaStreamTrackAdapter(main_thread, webrtc_track),
#if DCHECK_IS_ON()
      unregistered_(false),
#endif
      state_(observed_track()->state()) {
  // TODO(tommi): Use TrackObserver instead.
  observed_track()->RegisterObserver(this);
  // Here, we use base::Unretained() to avoid a circular reference.
  webkit_initialize_ =
      base::Bind(&RemoteAudioTrackAdapter::InitializeWebkitAudioTrack,
                 base::Unretained(this));
}

RemoteAudioTrackAdapter::~RemoteAudioTrackAdapter() {
#if DCHECK_IS_ON()
  DCHECK(unregistered_);
#endif
}

void RemoteAudioTrackAdapter::Unregister() {
#if DCHECK_IS_ON()
  DCHECK(!unregistered_);
  unregistered_ = true;
#endif
  observed_track()->UnregisterObserver(this);
}

void RemoteAudioTrackAdapter::InitializeWebkitAudioTrack() {
  InitializeWebkitTrack(blink::WebMediaStreamSource::TypeAudio);

  MediaStreamAudioSource* const source =
      new PeerConnectionRemoteAudioSource(observed_track().get());
  webkit_track()->source().setExtraData(source);  // Takes ownership.
  source->ConnectToTrack(*(webkit_track()));
}

void RemoteAudioTrackAdapter::OnChanged() {
  main_thread_->PostTask(FROM_HERE,
      base::Bind(&RemoteAudioTrackAdapter::OnChangedOnMainThread,
          this, observed_track()->state()));
}

void RemoteAudioTrackAdapter::OnChangedOnMainThread(
    webrtc::MediaStreamTrackInterface::TrackState state) {
  DCHECK(main_thread_->BelongsToCurrentThread());

  if (state == state_ || !initialized())
    return;

  state_ = state;

  switch (state) {
    case webrtc::MediaStreamTrackInterface::kLive:
      webkit_track()->source().setReadyState(
          blink::WebMediaStreamSource::ReadyStateLive);
      break;
    case webrtc::MediaStreamTrackInterface::kEnded:
      webkit_track()->source().setReadyState(
          blink::WebMediaStreamSource::ReadyStateEnded);
      break;
    default:
      NOTREACHED();
      break;
  }
}

RemoteMediaStreamImpl::Observer::Observer(
    const base::WeakPtr<RemoteMediaStreamImpl>& media_stream,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::MediaStreamInterface* webrtc_stream)
    : media_stream_(media_stream),
      main_thread_(main_thread),
      webrtc_stream_(webrtc_stream) {
  webrtc_stream_->RegisterObserver(this);
}

RemoteMediaStreamImpl::Observer::~Observer() {
  DCHECK(!webrtc_stream_.get()) << "Unregister hasn't been called";
}

void RemoteMediaStreamImpl::Observer::InitializeOnMainThread(
    const std::string& label) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (media_stream_)
    media_stream_->InitializeOnMainThread(label);
}

void RemoteMediaStreamImpl::Observer::Unregister() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  webrtc_stream_->UnregisterObserver(this);
  // Since we're guaranteed to not get further notifications, it's safe to
  // release the webrtc_stream_ here.
  webrtc_stream_ = nullptr;
}

void RemoteMediaStreamImpl::Observer::OnChanged() {
  std::unique_ptr<RemoteAudioTrackAdapters> audio(
      new RemoteAudioTrackAdapters());
  std::unique_ptr<RemoteVideoTrackAdapters> video(
      new RemoteVideoTrackAdapters());

  CreateAdaptersForTracks(
      webrtc_stream_->GetAudioTracks(), audio.get(), main_thread_);
  CreateAdaptersForTracks(
      webrtc_stream_->GetVideoTracks(), video.get(), main_thread_);

  main_thread_->PostTask(FROM_HERE,
      base::Bind(&RemoteMediaStreamImpl::Observer::OnChangedOnMainThread,
      this, base::Passed(&audio), base::Passed(&video)));
}

void RemoteMediaStreamImpl::Observer::OnChangedOnMainThread(
    std::unique_ptr<RemoteAudioTrackAdapters> audio_tracks,
    std::unique_ptr<RemoteVideoTrackAdapters> video_tracks) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (media_stream_)
    media_stream_->OnChanged(std::move(audio_tracks), std::move(video_tracks));
}

// Called on the signaling thread.
RemoteMediaStreamImpl::RemoteMediaStreamImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::MediaStreamInterface* webrtc_stream)
    : signaling_thread_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  observer_ = new RemoteMediaStreamImpl::Observer(
      weak_factory_.GetWeakPtr(), main_thread, webrtc_stream);
  CreateAdaptersForTracks(webrtc_stream->GetAudioTracks(),
      &audio_track_observers_, main_thread);
  CreateAdaptersForTracks(webrtc_stream->GetVideoTracks(),
      &video_track_observers_, main_thread);

  main_thread->PostTask(FROM_HERE,
      base::Bind(&RemoteMediaStreamImpl::Observer::InitializeOnMainThread,
          observer_, webrtc_stream->label()));
}

RemoteMediaStreamImpl::~RemoteMediaStreamImpl() {
  DCHECK(observer_->main_thread()->BelongsToCurrentThread());
  for (auto& track : audio_track_observers_)
    track->Unregister();
  observer_->Unregister();
}

void RemoteMediaStreamImpl::InitializeOnMainThread(const std::string& label) {
  DCHECK(observer_->main_thread()->BelongsToCurrentThread());
  blink::WebVector<blink::WebMediaStreamTrack> webkit_audio_tracks(
        audio_track_observers_.size());
  for (size_t i = 0; i < audio_track_observers_.size(); ++i) {
    audio_track_observers_[i]->Initialize();
    webkit_audio_tracks[i] = *audio_track_observers_[i]->webkit_track();
  }

  blink::WebVector<blink::WebMediaStreamTrack> webkit_video_tracks(
        video_track_observers_.size());
  for (size_t i = 0; i < video_track_observers_.size(); ++i) {
    video_track_observers_[i]->Initialize();
    webkit_video_tracks[i] = *video_track_observers_[i]->webkit_track();
  }

  webkit_stream_.initialize(base::UTF8ToUTF16(label),
                            webkit_audio_tracks, webkit_video_tracks);
  webkit_stream_.setExtraData(new MediaStream());
}

void RemoteMediaStreamImpl::OnChanged(
    std::unique_ptr<RemoteAudioTrackAdapters> audio_tracks,
    std::unique_ptr<RemoteVideoTrackAdapters> video_tracks) {
  // Find removed tracks.
  auto audio_it = audio_track_observers_.begin();
  while (audio_it != audio_track_observers_.end()) {
    if (!IsTrackInVector(*audio_tracks.get(), (*audio_it)->id())) {
      (*audio_it)->Unregister();
       webkit_stream_.removeTrack(*(*audio_it)->webkit_track());
       audio_it = audio_track_observers_.erase(audio_it);
    } else {
      ++audio_it;
    }
  }

  auto video_it = video_track_observers_.begin();
  while (video_it != video_track_observers_.end()) {
    if (!IsTrackInVector(*video_tracks.get(), (*video_it)->id())) {
      webkit_stream_.removeTrack(*(*video_it)->webkit_track());
      video_it = video_track_observers_.erase(video_it);
    } else {
      ++video_it;
    }
  }

  // Find added tracks.
  for (auto& track : *audio_tracks.get()) {
    if (!IsTrackInVector(audio_track_observers_, track->id())) {
      track->Initialize();
      audio_track_observers_.push_back(track);
      webkit_stream_.addTrack(*track->webkit_track());
      // Set the track to null to avoid unregistering it below now that it's
      // been associated with a media stream.
      track = nullptr;
    }
  }

  // Find added video tracks.
  for (const auto& track : *video_tracks.get()) {
    if (!IsTrackInVector(video_track_observers_, track->id())) {
      track->Initialize();
      video_track_observers_.push_back(track);
      webkit_stream_.addTrack(*track->webkit_track());
    }
  }

  // Unregister all the audio track observers that were not used.
  // We need to do this before destruction since the observers can't unregister
  // from within the dtor due to a race.
  for (auto& track : *audio_tracks.get()) {
    if (track.get())
      track->Unregister();
  }
}

}  // namespace content
