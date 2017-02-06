// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_player.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/android/audio_media_codec_decoder.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/android/media_task_runner.h"
#include "media/base/android/video_media_codec_decoder.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/timestamp_constants.h"

#define RUN_ON_MEDIA_THREAD(METHOD, ...)                                     \
  do {                                                                       \
    if (!GetMediaTaskRunner()->BelongsToCurrentThread()) {                   \
      DCHECK(ui_task_runner_->BelongsToCurrentThread());                     \
      GetMediaTaskRunner()->PostTask(                                        \
          FROM_HERE, base::Bind(&MediaCodecPlayer::METHOD, media_weak_this_, \
                                ##__VA_ARGS__));                             \
      return;                                                                \
    }                                                                        \
  } while (0)

namespace media {

// MediaCodecPlayer implementation.

MediaCodecPlayer::MediaCodecPlayer(
    int player_id,
    base::WeakPtr<MediaPlayerManager> manager,
    const OnDecoderResourcesReleasedCB& on_decoder_resources_released_cb,
    std::unique_ptr<DemuxerAndroid> demuxer,
    const GURL& frame_url,
    int media_session_id)
    : MediaPlayerAndroid(player_id,
                         manager.get(),
                         on_decoder_resources_released_cb,
                         frame_url,
                         media_session_id),
      ui_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      demuxer_(std::move(demuxer)),
      state_(kStatePaused),
      interpolator_(&default_tick_clock_),
      pending_start_(false),
      pending_seek_(kNoTimestamp()),
      cdm_registration_id_(0),
      key_is_required_(false),
      key_is_added_(false),
      media_weak_factory_(this) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << "MediaCodecPlayer::MediaCodecPlayer: player_id:" << player_id;

  completion_cb_ =
      base::Bind(&MediaPlayerManager::OnPlaybackComplete, manager, player_id);
  waiting_for_decryption_key_cb_ = base::Bind(
      &MediaPlayerManager::OnWaitingForDecryptionKey, manager, player_id);
  seek_done_cb_ =
      base::Bind(&MediaPlayerManager::OnSeekComplete, manager, player_id);
  error_cb_ = base::Bind(&MediaPlayerManager::OnError, manager, player_id);

  attach_listener_cb_ = base::Bind(&MediaPlayerAndroid::AttachListener,
                                   WeakPtrForUIThread(), nullptr);
  detach_listener_cb_ =
      base::Bind(&MediaPlayerAndroid::DetachListener, WeakPtrForUIThread());
  metadata_changed_cb_ = base::Bind(&MediaPlayerAndroid::OnMediaMetadataChanged,
                                    WeakPtrForUIThread());
  time_update_cb_ =
      base::Bind(&MediaPlayerAndroid::OnTimeUpdate, WeakPtrForUIThread());

  media_weak_this_ = media_weak_factory_.GetWeakPtr();

  // Finish initializaton on Media thread
  GetMediaTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&MediaCodecPlayer::Initialize, media_weak_this_));
}

MediaCodecPlayer::~MediaCodecPlayer()
{
  DVLOG(1) << "MediaCodecPlayer::~MediaCodecPlayer";
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  media_stat_->StopAndReport(GetInterpolatedTime());

  // Currently the unit tests wait for the MediaCodecPlayer destruction by
  // watching the demuxer, which is destroyed as one of the member variables.
  // Release the codecs here, before any member variable is destroyed to make
  // the unit tests happy.

  if (video_decoder_)
    video_decoder_->ReleaseDecoderResources();
  if (audio_decoder_)
    audio_decoder_->ReleaseDecoderResources();

  if (cdm_) {
    // Cancel previously registered callback (if any).
    static_cast<MediaDrmBridge*>(cdm_.get())
        ->SetMediaCryptoReadyCB(MediaDrmBridge::MediaCryptoReadyCB());

    DCHECK(cdm_registration_id_);
    static_cast<MediaDrmBridge*>(cdm_.get())
        ->UnregisterPlayer(cdm_registration_id_);
  }
}

void MediaCodecPlayer::Initialize() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  interpolator_.SetUpperBound(base::TimeDelta());

  CreateDecoders();

  // This call might in turn call MediaCodecPlayer::OnDemuxerConfigsAvailable()
  // which propagates configs into decoders. Therefore CreateDecoders() should
  // be called first.
  demuxer_->Initialize(this);
}

// The implementation of MediaPlayerAndroid interface.

void MediaCodecPlayer::DeleteOnCorrectThread() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  DetachListener();

  // The base class part that deals with MediaPlayerListener
  // has to be destroyed on UI thread.
  DestroyListenerOnUIThread();

  // Post deletion onto Media thread
  GetMediaTaskRunner()->DeleteSoon(FROM_HERE, this);
}

void MediaCodecPlayer::SetVideoSurface(gl::ScopedJavaSurface surface) {
  RUN_ON_MEDIA_THREAD(SetVideoSurface, base::Passed(&surface));

  DVLOG(1) << __FUNCTION__ << (surface.IsEmpty() ? " empty" : " non-empty");

  // Save the empty-ness before we pass the surface to the decoder.
  bool surface_is_empty = surface.IsEmpty();

  // Apparently RemoveVideoSurface() can be called several times in a row,
  // ignore the second and subsequent calls.
  if (surface_is_empty && !video_decoder_->HasVideoSurface()) {
    DVLOG(1) << __FUNCTION__ << ": surface already removed, ignoring";
    return;
  }

  // Do not set unprotected surface if we know that we need a protected one.
  // Empty surface means the surface removal and we always allow for it.
  if (!surface_is_empty && video_decoder_->IsProtectedSurfaceRequired() &&
      !surface.is_protected()) {
    DVLOG(0) << __FUNCTION__ << ": surface is not protected, ignoring";
    return;
  }

  video_decoder_->SetVideoSurface(std::move(surface));

  if (surface_is_empty) {
    // Remove video surface.
    switch (state_) {
      case kStatePlaying:
        if (VideoFinished())
          break;

        DVLOG(1) << __FUNCTION__ << ": stopping and restarting";
        // Stop decoders as quickly as possible.
        StopDecoders();  // synchronous stop

        // Prefetch or wait for initial configuration.
        if (HasAudio() || HasVideo()) {
          SetState(kStatePrefetching);
          StartPrefetchDecoders();
        } else {
          SetState(kStateWaitingForConfig);
        }
        break;

      default:
        break;  // ignore
    }
  } else {
    // Replace video surface.
    switch (state_) {
      case kStateWaitingForSurface:
        SetState(kStatePlaying);
        StartPlaybackOrBrowserSeek();
        break;

      case kStatePlaying:
        if (VideoFinished())
          break;

        DVLOG(1) << __FUNCTION__ << ": requesting to stop and restart";
        SetState(kStateStopping);
        RequestToStopDecoders();
        SetPendingStart(true);
        break;

      default:
        break;  // ignore
    }
  }
}

void MediaCodecPlayer::Start() {
  RUN_ON_MEDIA_THREAD(Start);

  DVLOG(1) << __FUNCTION__;

  switch (state_) {
    case kStatePaused:
      // Request play permission or wait for initial configuration.
      if (HasAudio() || HasVideo()) {
        SetState(kStateWaitingForPermission);
        RequestPlayPermission();
      } else {
        SetState(kStateWaitingForConfig);
      }
      break;
    case kStateStopping:
    case kStateWaitingForSeek:
      SetPendingStart(true);
      break;
    case kStateWaitingForConfig:
    case kStateWaitingForPermission:
    case kStatePrefetching:
    case kStatePlaying:
    case kStateWaitingForSurface:
    case kStateWaitingForKey:
    case kStateWaitingForMediaCrypto:
    case kStateError:
      break;  // Ignore
    default:
      NOTREACHED();
      break;
  }
}

void MediaCodecPlayer::Pause(bool is_media_related_action) {
  RUN_ON_MEDIA_THREAD(Pause, is_media_related_action);

  DVLOG(1) << __FUNCTION__;

  media_stat_->StopAndReport(GetInterpolatedTime());

  SetPendingStart(false);

  switch (state_) {
    case kStateWaitingForConfig:
    case kStateWaitingForPermission:
    case kStatePrefetching:
    case kStateWaitingForSurface:
    case kStateWaitingForKey:
    case kStateWaitingForMediaCrypto:
      SetState(kStatePaused);
      StopDecoders();
      break;
    case kStatePlaying:
      SetState(kStateStopping);
      RequestToStopDecoders();
      break;
    case kStatePaused:
    case kStateStopping:
    case kStateWaitingForSeek:
    case kStateError:
      break;  // Ignore
    default:
      NOTREACHED();
      break;
  }
}

void MediaCodecPlayer::SeekTo(base::TimeDelta timestamp) {
  RUN_ON_MEDIA_THREAD(SeekTo, timestamp);

  DVLOG(1) << __FUNCTION__ << " " << timestamp;

  media_stat_->StopAndReport(GetInterpolatedTime());

  switch (state_) {
    case kStatePaused:
      SetState(kStateWaitingForSeek);
      RequestDemuxerSeek(timestamp);
      break;
    case kStateWaitingForConfig:
    case kStateWaitingForPermission:
    case kStatePrefetching:
    case kStateWaitingForSurface:
    case kStateWaitingForKey:
    case kStateWaitingForMediaCrypto:
      SetState(kStateWaitingForSeek);
      StopDecoders();
      SetPendingStart(true);
      RequestDemuxerSeek(timestamp);
      break;
    case kStatePlaying:
      SetState(kStateStopping);
      RequestToStopDecoders();
      SetPendingStart(true);
      SetPendingSeek(timestamp);
      break;
    case kStateStopping:
      SetPendingSeek(timestamp);
      break;
    case kStateWaitingForSeek:
      SetPendingSeek(timestamp);
      break;
    case kStateError:
      break;  // ignore
    default:
      NOTREACHED();
      break;
  }
}

void MediaCodecPlayer::Release() {
  // TODO(qinmin): the callback should be posted onto the UI thread when
  // Release() finishes on media thread. However, the BrowserMediaPlayerManager
  // could be gone in that case, which cause the MediaThrottler unable to
  // track the active players. We should pass
  // MediaThrottler::OnDecodeRequestFinished() to this class in the ctor, but
  // also need a way for BrowserMediaPlayerManager to track active players.
  if (ui_task_runner_->BelongsToCurrentThread())
    on_decoder_resources_released_cb_.Run(player_id());

  RUN_ON_MEDIA_THREAD(Release);

  DVLOG(1) << __FUNCTION__;

  media_stat_->StopAndReport(GetInterpolatedTime());

  // Stop decoding threads and delete MediaCodecs, but keep IPC between browser
  // and renderer processes going. Seek should work across and after Release().

  ReleaseDecoderResources();

  SetPendingStart(false);

  if (state_ != kStateWaitingForSeek)
    SetState(kStatePaused);

  // Crear encryption key related flags.
  key_is_required_ = false;
  key_is_added_ = false;

  base::TimeDelta pending_seek_time = GetPendingSeek();
  if (pending_seek_time != kNoTimestamp()) {
    SetPendingSeek(kNoTimestamp());
    SetState(kStateWaitingForSeek);
    RequestDemuxerSeek(pending_seek_time);
  }
}

void MediaCodecPlayer::UpdateEffectiveVolumeInternal(double effective_volume) {
  RUN_ON_MEDIA_THREAD(UpdateEffectiveVolumeInternal, effective_volume);

  DVLOG(1) << __FUNCTION__ << " " << effective_volume;
  audio_decoder_->SetVolume(effective_volume);
}

bool MediaCodecPlayer::HasAudio() const {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  return audio_decoder_->HasStream();
}

bool MediaCodecPlayer::HasVideo() const {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  return video_decoder_->HasStream();
}

int MediaCodecPlayer::GetVideoWidth() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  return metadata_cache_.video_size.width();
}

int MediaCodecPlayer::GetVideoHeight() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  return metadata_cache_.video_size.height();
}

base::TimeDelta MediaCodecPlayer::GetCurrentTime() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  return current_time_cache_;
}

base::TimeDelta MediaCodecPlayer::GetDuration() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  return metadata_cache_.duration;
}

bool MediaCodecPlayer::IsPlaying() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // TODO(timav): Use another variable since |state_| should only be accessed on
  // Media thread.
  return state_ == kStatePlaying || state_ == kStateStopping;
}

bool MediaCodecPlayer::CanPause() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return false;
}

bool MediaCodecPlayer::CanSeekForward() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return false;
}

bool MediaCodecPlayer::CanSeekBackward() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return false;
}

bool MediaCodecPlayer::IsPlayerReady() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  // This method is called to check whether it's safe to release the player when
  // the OS needs more resources. This class can be released at any time.
  return true;
}

void MediaCodecPlayer::SetCdm(const scoped_refptr<MediaKeys>& cdm) {
  DCHECK(cdm);
  RUN_ON_MEDIA_THREAD(SetCdm, cdm);

  DVLOG(1) << __FUNCTION__;

  // Currently we don't support DRM change during the middle of playback, even
  // if the player is paused. There is no current plan to support it, see
  // http://crbug.com/253792.
  if (state_ != kStatePaused || GetInterpolatedTime() > base::TimeDelta()) {
    VLOG(0) << "Setting DRM bridge after playback has started is not supported";
    return;
  }

  if (cdm_) {
    NOTREACHED() << "Currently we do not support resetting CDM.";
    return;
  }

  cdm_ = cdm;

  // Only MediaDrmBridge will be set on MediaCodecPlayer.
  MediaDrmBridge* drm_bridge = static_cast<MediaDrmBridge*>(cdm_.get());

  // Register CDM callbacks. The callbacks registered will be posted back to the
  // media thread via BindToCurrentLoop.

  // Since |this| holds a reference to the |cdm_|, by the time the CDM is
  // destructed, UnregisterPlayer() must have been called and |this| has been
  // destructed as well. So the |cdm_unset_cb| will never have a chance to be
  // called.
  // TODO(xhwang): Remove |cdm_unset_cb| after it's not used on all platforms.
  cdm_registration_id_ = drm_bridge->RegisterPlayer(
      BindToCurrentLoop(
          base::Bind(&MediaCodecPlayer::OnKeyAdded, media_weak_this_)),
      base::Bind(&base::DoNothing));

  drm_bridge->SetMediaCryptoReadyCB(BindToCurrentLoop(
      base::Bind(&MediaCodecPlayer::OnMediaCryptoReady, media_weak_this_)));
}

// Callbacks from Demuxer.

void MediaCodecPlayer::OnDemuxerConfigsAvailable(
    const DemuxerConfigs& configs) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DVLOG(1) << __FUNCTION__;

  duration_ = configs.duration;

  SetDemuxerConfigs(configs);

  // Update cache and notify manager on UI thread
  gfx::Size video_size = HasVideo() ? configs.video_size : gfx::Size();
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(metadata_changed_cb_, duration_, video_size));
}

void MediaCodecPlayer::OnDemuxerDataAvailable(const DemuxerData& data) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DCHECK_LT(0u, data.access_units.size());
  CHECK_GE(1u, data.demuxer_configs.size());

  DVLOG(2) << "Player::" << __FUNCTION__;

  if (data.type == DemuxerStream::AUDIO)
    audio_decoder_->OnDemuxerDataAvailable(data);

  if (data.type == DemuxerStream::VIDEO)
    video_decoder_->OnDemuxerDataAvailable(data);
}

void MediaCodecPlayer::OnDemuxerSeekDone(
    base::TimeDelta actual_browser_seek_time) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DVLOG(1) << __FUNCTION__ << " actual_time:" << actual_browser_seek_time;

  DCHECK(seek_info_.get());
  DCHECK(seek_info_->seek_time != kNoTimestamp());

  // A browser seek must not jump into the past. Ideally, it seeks to the
  // requested time, but it might jump into the future.
  DCHECK(!seek_info_->is_browser_seek ||
         seek_info_->seek_time <= actual_browser_seek_time);

  // Restrict the current time to be equal to seek_time
  // for the next StartPlaybackDecoders() call.

  base::TimeDelta seek_time = seek_info_->is_browser_seek
                                  ? actual_browser_seek_time
                                  : seek_info_->seek_time;

  interpolator_.SetBounds(seek_time, seek_time);

  audio_decoder_->SetBaseTimestamp(seek_time);

  audio_decoder_->SetPrerollTimestamp(seek_time);
  video_decoder_->SetPrerollTimestamp(seek_time);

  // The Flush() might set the state to kStateError.
  if (state_ == kStateError) {
    // Notify the Renderer.
    if (!seek_info_->is_browser_seek)
      ui_task_runner_->PostTask(FROM_HERE,
                                base::Bind(seek_done_cb_, seek_time));

    seek_info_.reset();
    return;
  }

  DCHECK_EQ(kStateWaitingForSeek, state_);

  base::TimeDelta pending_seek_time = GetPendingSeek();
  if (pending_seek_time != kNoTimestamp()) {
    // Keep kStateWaitingForSeek
    SetPendingSeek(kNoTimestamp());
    RequestDemuxerSeek(pending_seek_time);
    return;
  }

  if (HasPendingStart()) {
    SetPendingStart(false);
    // Request play permission or wait for initial configuration.
    if (HasAudio() || HasVideo()) {
      SetState(kStateWaitingForPermission);
      RequestPlayPermission();
    } else {
      SetState(kStateWaitingForConfig);
    }
  } else {
    SetState(kStatePaused);
  }

  // Notify the Renderer.
  if (!seek_info_->is_browser_seek)
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(seek_done_cb_, seek_time));

  seek_info_.reset();
}

void MediaCodecPlayer::OnDemuxerDurationChanged(
    base::TimeDelta duration) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " duration:" << duration;

  duration_ = duration;
}

void MediaCodecPlayer::SetDecodersTimeCallbackForTests(
    DecodersTimeCallback cb) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  decoders_time_cb_ = cb;
}

void MediaCodecPlayer::SetCodecCreatedCallbackForTests(
    CodecCreatedCallback cb) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(audio_decoder_ && video_decoder_);

  audio_decoder_->SetCodecCreatedCallbackForTests(
      base::Bind(cb, DemuxerStream::AUDIO));
  video_decoder_->SetCodecCreatedCallbackForTests(
      base::Bind(cb, DemuxerStream::VIDEO));
}

void MediaCodecPlayer::SetAlwaysReconfigureForTests(DemuxerStream::Type type) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(audio_decoder_ && video_decoder_);

  if (type == DemuxerStream::AUDIO)
    audio_decoder_->SetAlwaysReconfigureForTests();
  else if (type == DemuxerStream::VIDEO)
    video_decoder_->SetAlwaysReconfigureForTests();
}

bool MediaCodecPlayer::IsPrerollingForTests(DemuxerStream::Type type) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(audio_decoder_ && video_decoder_);

  if (type == DemuxerStream::AUDIO)
    return audio_decoder_->IsPrerollingForTests();
  else if (type == DemuxerStream::VIDEO)
    return video_decoder_->IsPrerollingForTests();
  else
    return false;
}

// Events from Player, called on UI thread

void MediaCodecPlayer::RequestPermissionAndPostResult(base::TimeDelta duration,
                                                      bool has_audio) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " duration:" << duration;

  bool granted = manager()->RequestPlay(player_id(), duration, has_audio);
  GetMediaTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&MediaCodecPlayer::OnPermissionDecided,
                            media_weak_this_, granted));
}

void MediaCodecPlayer::OnMediaMetadataChanged(base::TimeDelta duration,
                                              const gfx::Size& video_size) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (duration != kNoTimestamp())
    metadata_cache_.duration = duration;

  if (!video_size.IsEmpty())
    metadata_cache_.video_size = video_size;

  manager()->OnMediaMetadataChanged(player_id(), metadata_cache_.duration,
                                    metadata_cache_.video_size.width(),
                                    metadata_cache_.video_size.height(), true);
}

void MediaCodecPlayer::OnTimeUpdate(base::TimeDelta current_timestamp,
                                    base::TimeTicks current_time_ticks) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  current_time_cache_ = current_timestamp;
  manager()->OnTimeUpdate(player_id(), current_timestamp, current_time_ticks);
}

// Event from manager, called on Media thread

void MediaCodecPlayer::OnPermissionDecided(bool granted) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DVLOG(1) << __FUNCTION__ << ": " << (granted ? "granted" : "denied");

  switch (state_) {
    case kStateWaitingForPermission:
      if (granted) {
        SetState(kStatePrefetching);
        StartPrefetchDecoders();
      } else {
        SetState(kStatePaused);
        StopDecoders();
      }
      break;

    default:
      break;  // ignore
  }
}

// Events from Decoders, called on Media thread

void MediaCodecPlayer::RequestDemuxerData(DemuxerStream::Type stream_type) {
  DVLOG(2) << __FUNCTION__ << " streamType:" << stream_type;

  // Use this method instead of directly binding with
  // DemuxerAndroid::RequestDemuxerData() to avoid the race condition on
  // deletion:
  // 1. DeleteSoon is posted from UI to Media thread.
  // 2. RequestDemuxerData callback is posted from Decoder to Media thread.
  // 3. DeleteSoon arrives, we delete the player and detach from
  //    BrowserDemuxerAndroid.
  // 4. RequestDemuxerData is processed by the media thread queue. Since the
  //    weak_ptr was invalidated in (3), this is a no-op. If we used
  //    DemuxerAndroid::RequestDemuxerData() it would arrive and will try to
  //    call the client, but the client (i.e. this player) would not exist.
  demuxer_->RequestDemuxerData(stream_type);
}

void MediaCodecPlayer::OnPrefetchDone() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  if (state_ != kStatePrefetching) {
    DVLOG(1) << __FUNCTION__ << " wrong state " << AsString(state_)
             << " ignoring";
    return;  // Ignore
  }

  DVLOG(1) << __FUNCTION__;

  if (!HasAudio() && !HasVideo()) {
    // No configuration at all after prefetching.
    // This is an error, initial configuration is expected
    // before the first data chunk.
    DCHECK(!internal_error_cb_.is_null());
    GetMediaTaskRunner()->PostTask(FROM_HERE, internal_error_cb_);
    return;
  }

  if (HasVideo() && !video_decoder_->HasVideoSurface()) {
    SetState(kStateWaitingForSurface);
    return;
  }

  if (key_is_required_ && !key_is_added_) {
    SetState(kStateWaitingForKey);
    ui_task_runner_->PostTask(FROM_HERE, waiting_for_decryption_key_cb_);
    return;
  }

  SetState(kStatePlaying);
  StartPlaybackOrBrowserSeek();
}

void MediaCodecPlayer::OnPrerollDone() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  if (state_ != kStatePlaying) {
    DVLOG(1) << __FUNCTION__ << ": in state " << AsString(state_)
             << ", ignoring";
    return;
  }

  DVLOG(1) << __FUNCTION__;

  StartStatus status = StartDecoders();
  if (status != kStartOk)
    GetMediaTaskRunner()->PostTask(FROM_HERE, internal_error_cb_);
}

void MediaCodecPlayer::OnDecoderDrained(DemuxerStream::Type type) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " " << type;

  // We expect that OnStopDone() comes next.

  DCHECK(type == DemuxerStream::AUDIO || type == DemuxerStream::VIDEO);

  // DCHECK(state_ == kStatePlaying || state_ == kStateStopping)
  //    << __FUNCTION__ << " illegal state: " << AsString(state_);
  //
  // With simultaneous reconfiguration of audio and video streams the state
  // can be kStatePrefetching as well:
  // OnLastFrameRendered VIDEO (VIDEO decoder is stopped)
  // OnLastFrameRendered AUDIO (AUDIO decoder is stopped)
  // OnDecoderDrained VIDEO (kStatePlaying -> kStateStopping)
  // OnStopDone VIDEO (kStateStopping -> kStatePrefetching)
  // OnDecoderDrained AUDIO
  // OnStopDone AUDIO
  //
  // TODO(timav): combine OnDecoderDrained() and OnStopDone() ?

  switch (state_) {
    case kStatePlaying:
      SetState(kStateStopping);
      SetPendingStart(true);

      if (type == DemuxerStream::AUDIO && !VideoFinished()) {
        DVLOG(1) << __FUNCTION__ << " requesting to stop video";
        video_decoder_->RequestToStop();
      } else if (type == DemuxerStream::VIDEO && !AudioFinished()) {
        DVLOG(1) << __FUNCTION__ << " requesting to stop audio";
        audio_decoder_->RequestToStop();
      }
      break;

    default:
      break;
  }
}

void MediaCodecPlayer::OnStopDone(DemuxerStream::Type type) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " " << type
           << " interpolated time:" << GetInterpolatedTime();

  if (!(audio_decoder_->IsStopped() && video_decoder_->IsStopped())) {
    DVLOG(1) << __FUNCTION__ << " both audio and video has to be stopped"
             << ", ignoring";
    return;  // Wait until other stream is stopped
  }

  // At this point decoder threads should not be running
  if (interpolator_.interpolating())
    interpolator_.StopInterpolating();

  base::TimeDelta seek_time;
  switch (state_) {
    case kStateStopping: {
      base::TimeDelta seek_time = GetPendingSeek();
      if (seek_time != kNoTimestamp()) {
        SetState(kStateWaitingForSeek);
        SetPendingSeek(kNoTimestamp());
        RequestDemuxerSeek(seek_time);
      } else if (HasPendingStart()) {
        SetPendingStart(false);
        SetState(kStateWaitingForPermission);
        RequestPlayPermission();
      } else {
        SetState(kStatePaused);
      }
    } break;
    case kStatePlaying:
      // Unexpected stop means completion
      SetState(kStatePaused);
      break;
    default:
      // DVLOG(0) << __FUNCTION__ << " illegal state: " << AsString(state_);
      // NOTREACHED();
      // Ignore! There can be a race condition: audio posts OnStopDone,
      // then video posts, then first OnStopDone arrives at which point
      // both streams are already stopped, then second OnStopDone arrives. When
      // the second one arrives, the state us not kStateStopping any more.
      return;
  }

  // DetachListener to UI thread
  ui_task_runner_->PostTask(FROM_HERE, detach_listener_cb_);

  if (AudioFinished() && VideoFinished()) {
    media_stat_->StopAndReport(GetInterpolatedTime());
    ui_task_runner_->PostTask(FROM_HERE, completion_cb_);
  }
}

void MediaCodecPlayer::OnMissingKeyReported(DemuxerStream::Type type) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " " << type;

  // Request stop and restart to pick up the key.
  key_is_required_ = true;

  if (state_ == kStatePlaying) {
    media_stat_->StopAndReport(GetInterpolatedTime());

    SetState(kStateStopping);
    RequestToStopDecoders();
    SetPendingStart(true);
  }
}

void MediaCodecPlayer::OnError() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  media_stat_->StopAndReport(GetInterpolatedTime());

  // kStateError blocks all events
  SetState(kStateError);

  ReleaseDecoderResources();

  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(error_cb_, MEDIA_ERROR_DECODE));
}

void MediaCodecPlayer::OnStarvation(DemuxerStream::Type type) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " stream type:" << type;

  if (state_ != kStatePlaying)
    return;  // Ignore

  SetState(kStateStopping);
  RequestToStopDecoders();
  SetPendingStart(true);

  media_stat_->AddStarvation();
}

void MediaCodecPlayer::OnTimeIntervalUpdate(DemuxerStream::Type type,
                                            base::TimeDelta now_playing,
                                            base::TimeDelta last_buffered,
                                            bool postpone) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DVLOG(2) << __FUNCTION__ << ": stream type:" << type << " [" << now_playing
           << "," << last_buffered << "]" << (postpone ? " postpone" : "");

  // For testing only: report time interval as we receive it from decoders
  // as an indication of what is being rendered. Do not post this callback
  // for postponed frames: although the PTS is correct, the tests also care
  // about the wall clock time this callback arrives and deduce the rendering
  // moment from it.
  if (!decoders_time_cb_.is_null() && !postpone) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(decoders_time_cb_, type, now_playing, last_buffered));
  }

  // I assume that audio stream cannot be added after we get configs by
  // OnDemuxerConfigsAvailable(), but that audio can finish early.

  if (type == DemuxerStream::VIDEO) {
    // Ignore video PTS if there is audio stream or if it's behind current
    // time as set by audio stream.
    if (!AudioFinished() ||
        (HasAudio() && now_playing < interpolator_.GetInterpolatedTime()))
      return;
  }

  interpolator_.SetBounds(now_playing, last_buffered);

  // Post to UI thread
  if (!postpone) {
    ui_task_runner_->PostTask(FROM_HERE,
                              base::Bind(time_update_cb_, GetInterpolatedTime(),
                                         base::TimeTicks::Now()));
  }
}

void MediaCodecPlayer::OnVideoResolutionChanged(const gfx::Size& size) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DVLOG(1) << __FUNCTION__ << " " << size.width() << "x" << size.height();

  // Update cache and notify manager on UI thread
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(metadata_changed_cb_, kNoTimestamp(), size));
}

// Callbacks from MediaDrmBridge.

void MediaCodecPlayer::OnMediaCryptoReady(
    MediaDrmBridge::JavaObjectPtr media_crypto,
    bool needs_protected_surface) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " protected surface is "
           << (needs_protected_surface ? "required" : "not required");

  // We use the parameters that come with this callback every time we call
  // Configure(). This is possible only if the MediaCrypto object remains valid
  // and the surface requirement does not change until new SetCdm() is called.

  DCHECK(media_crypto);

  if (media_crypto->is_null()) {
    // TODO(timav): Fail playback nicely here if needed. Note that we could get
    // here even though the stream to play is unencrypted and therefore
    // MediaCrypto is not needed. In that case, we may ignore this error and
    // continue playback, or fail the playback.
    LOG(ERROR) << "MediaCrypto creation failed.";
    return;
  }

  media_crypto_ = std::move(media_crypto);

  if (audio_decoder_) {
    audio_decoder_->SetNeedsReconfigure();
  }

  if (video_decoder_) {
    video_decoder_->SetNeedsReconfigure();
    video_decoder_->SetProtectedSurfaceRequired(needs_protected_surface);
  }

  if (state_ == kStateWaitingForMediaCrypto) {
    // Resume start sequence (configure, etc.)
    SetState(kStatePlaying);
    StartPlaybackOrBrowserSeek();
  }

  DVLOG(1) << __FUNCTION__ << " end";
}

void MediaCodecPlayer::OnKeyAdded() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  key_is_added_ = true;

  if (state_ == kStateWaitingForKey) {
    SetState(kStatePlaying);
    StartPlaybackOrBrowserSeek();
  }
}

// State machine operations, called on Media thread

void MediaCodecPlayer::SetState(PlayerState new_state) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DVLOG(1) << "SetState:" << AsString(state_) << " -> " << AsString(new_state);
  state_ = new_state;
}

void MediaCodecPlayer::SetPendingStart(bool need_to_start) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << ": " << need_to_start;
  pending_start_ = need_to_start;
}

bool MediaCodecPlayer::HasPendingStart() const {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  return pending_start_;
}

void MediaCodecPlayer::SetPendingSeek(base::TimeDelta timestamp) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << ": " << timestamp;
  pending_seek_ = timestamp;
}

base::TimeDelta MediaCodecPlayer::GetPendingSeek() const {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  return pending_seek_;
}

void MediaCodecPlayer::SetDemuxerConfigs(const DemuxerConfigs& configs) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " " << configs;

  DCHECK(audio_decoder_);
  DCHECK(video_decoder_);

  // At least one valid codec must be present.
  DCHECK(configs.audio_codec != kUnknownAudioCodec ||
         configs.video_codec != kUnknownVideoCodec);

  if (configs.audio_codec != kUnknownAudioCodec)
    audio_decoder_->SetDemuxerConfigs(configs);

  if (configs.video_codec != kUnknownVideoCodec)
    video_decoder_->SetDemuxerConfigs(configs);

  if (state_ == kStateWaitingForConfig) {
    SetState(kStateWaitingForPermission);
    RequestPlayPermission();
  }
}

void MediaCodecPlayer::RequestPlayPermission() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  // Check that we have received demuxer config hence we know the duration.
  DCHECK(HasAudio() || HasVideo());

  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MediaPlayerAndroid::RequestPermissionAndPostResult,
                            WeakPtrForUIThread(), duration_, HasAudio()));
}

void MediaCodecPlayer::StartPrefetchDecoders() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  bool do_audio = false;
  bool do_video = false;
  int count = 0;
  if (!AudioFinished()) {
    do_audio = true;
    ++count;
  }
  if (!VideoFinished()) {
    do_video = true;
    ++count;
  }

  DCHECK_LT(0, count);  // at least one decoder should be active

  base::Closure prefetch_cb = base::BarrierClosure(
      count, base::Bind(&MediaCodecPlayer::OnPrefetchDone, media_weak_this_));

  if (do_audio)
    audio_decoder_->Prefetch(prefetch_cb);

  if (do_video)
    video_decoder_->Prefetch(prefetch_cb);
}

void MediaCodecPlayer::StartPlaybackOrBrowserSeek() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  // TODO(timav): consider replacing this method with posting a
  // browser seek task (i.e. generate an event) from StartPlaybackDecoders().

  // Clear encryption key related flags.
  key_is_required_ = false;
  key_is_added_ = false;

  StartStatus status = StartPlaybackDecoders();

  switch (status) {
    case kStartBrowserSeekRequired:
      // Browser seek
      SetState(kStateWaitingForSeek);
      SetPendingStart(true);
      StopDecoders();
      RequestDemuxerSeek(GetInterpolatedTime(), true);
      break;
    case kStartCryptoRequired:
      SetState(kStateWaitingForMediaCrypto);
      break;
    case kStartFailed:
      GetMediaTaskRunner()->PostTask(FROM_HERE, internal_error_cb_);
      break;
    case kStartOk:
      break;
  }
}

MediaCodecPlayer::StartStatus MediaCodecPlayer::StartPlaybackDecoders() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  // Configure all streams before the start since we may discover that browser
  // seek is required.
  MediaCodecPlayer::StartStatus status = ConfigureDecoders();
  if (status != kStartOk)
    return status;

  bool preroll_required = false;
  status = MaybePrerollDecoders(&preroll_required);
  if (preroll_required)
    return status;

  return StartDecoders();
}

MediaCodecPlayer::StartStatus MediaCodecPlayer::ConfigureDecoders() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  const bool do_audio = !AudioFinished();
  const bool do_video = !VideoFinished();

  // If there is nothing to play, the state machine should determine this at the
  // prefetch state and never call this method.
  DCHECK(do_audio || do_video);

  const bool need_audio_crypto =
      do_audio && audio_decoder_->IsContentEncrypted();
  const bool need_video_crypto =
      do_video && video_decoder_->IsContentEncrypted();

  // Do we need to create a local ref from the global ref?
  jobject media_crypto = media_crypto_ ? media_crypto_->obj() : nullptr;

  if (need_audio_crypto || need_video_crypto) {
    DVLOG(1) << (need_audio_crypto ? " audio" : "")
             << (need_video_crypto ? " video" : "") << " need(s) encryption";
    if (!media_crypto) {
      DVLOG(1) << __FUNCTION__ << ": MediaCrypto is not found, returning";
      return kStartCryptoRequired;
    }
  }

  // Start with video: if browser seek is required it would not make sense to
  // configure audio.

  MediaCodecDecoder::ConfigStatus status = MediaCodecDecoder::kConfigOk;
  if (do_video)
    status = video_decoder_->Configure(media_crypto);

  if (status == MediaCodecDecoder::kConfigOk && do_audio)
    status = audio_decoder_->Configure(media_crypto);

  switch (status) {
    case MediaCodecDecoder::kConfigOk:
      break;
    case MediaCodecDecoder::kConfigKeyFrameRequired:
      return kStartBrowserSeekRequired;
    case MediaCodecDecoder::kConfigFailure:
      return kStartFailed;
  }
  return kStartOk;
}

MediaCodecPlayer::StartStatus MediaCodecPlayer::MaybePrerollDecoders(
    bool* preroll_required) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  DVLOG(1) << __FUNCTION__ << " current_time:" << GetInterpolatedTime();

  // If requested, preroll is always done in the beginning of the playback,
  // after prefetch. The request might not happen at all though, in which case
  // we won't have prerolling phase. We need the prerolling when we (re)create
  // the decoder, because its configuration and initialization (getting input,
  // but not making output) can take time, and after the seek because there
  // could be some data to be skipped and there is again initialization after
  // the flush.

  *preroll_required = false;

  int count = 0;
  const bool do_audio = audio_decoder_->NotCompletedAndNeedsPreroll();
  if (do_audio)
    ++count;

  const bool do_video = video_decoder_->NotCompletedAndNeedsPreroll();
  if (do_video)
    ++count;

  if (count == 0) {
    DVLOG(1) << __FUNCTION__ << ": preroll is not required, skipping";
    return kStartOk;
  }

  *preroll_required = true;

  DCHECK(count > 0);
  DCHECK(do_audio || do_video);

  DVLOG(1) << __FUNCTION__ << ": preroll for " << count << " stream(s)";

  base::Closure preroll_cb = base::BarrierClosure(
      count, base::Bind(&MediaCodecPlayer::OnPrerollDone, media_weak_this_));

  if (do_audio && !audio_decoder_->Preroll(preroll_cb))
    return kStartFailed;

  if (do_video && !video_decoder_->Preroll(preroll_cb))
    return kStartFailed;

  return kStartOk;
}

MediaCodecPlayer::StartStatus MediaCodecPlayer::StartDecoders() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  if (!interpolator_.interpolating())
    interpolator_.StartInterpolating();

  base::TimeDelta current_time = GetInterpolatedTime();

  DVLOG(1) << __FUNCTION__ << " current_time:" << current_time;

  // At this point decoder threads are either not running at all or their
  // message pumps are in the idle state after the preroll is done.

  if (!AudioFinished()) {
    if (!audio_decoder_->Start(current_time))
      return kStartFailed;

    // Attach listener on UI thread
    ui_task_runner_->PostTask(FROM_HERE, attach_listener_cb_);
  }

  if (!VideoFinished()) {
    if (!video_decoder_->Start(current_time))
      return kStartFailed;
  }

  media_stat_->Start(current_time);

  return kStartOk;
}

void MediaCodecPlayer::StopDecoders() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  video_decoder_->SyncStop();
  audio_decoder_->SyncStop();
}

void MediaCodecPlayer::RequestToStopDecoders() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  bool do_audio = false;
  bool do_video = false;

  if (audio_decoder_->IsPrefetchingOrPlaying())
    do_audio = true;
  if (video_decoder_->IsPrefetchingOrPlaying())
    do_video = true;

  if (!do_audio && !do_video) {
    GetMediaTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&MediaCodecPlayer::OnStopDone, media_weak_this_,
                              DemuxerStream::UNKNOWN));
    return;
  }

  if (do_audio)
    audio_decoder_->RequestToStop();
  if (do_video)
    video_decoder_->RequestToStop();
}

void MediaCodecPlayer::RequestDemuxerSeek(base::TimeDelta seek_time,
                                          bool is_browser_seek) {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " " << seek_time
           << (is_browser_seek ? " BROWSER_SEEK" : "");

  // Flush decoders before requesting demuxer.
  audio_decoder_->Flush();
  video_decoder_->Flush();

  // Save active seek data. Logically it is attached to kStateWaitingForSeek.
  DCHECK_EQ(kStateWaitingForSeek, state_);
  seek_info_.reset(new SeekInfo(seek_time, is_browser_seek));

  demuxer_->RequestDemuxerSeek(seek_time, is_browser_seek);
}

void MediaCodecPlayer::ReleaseDecoderResources() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  if (audio_decoder_)
    audio_decoder_->ReleaseDecoderResources();

  if (video_decoder_)
    video_decoder_->ReleaseDecoderResources();

  // At this point decoder threads should not be running
  if (interpolator_.interpolating())
    interpolator_.StopInterpolating();
}

void MediaCodecPlayer::CreateDecoders() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  internal_error_cb_ = base::Bind(&MediaCodecPlayer::OnError, media_weak_this_);

  media_stat_.reset(new MediaStatistics());

  audio_decoder_.reset(new AudioMediaCodecDecoder(
      GetMediaTaskRunner(), &media_stat_->audio_frame_stats(),
      base::Bind(&MediaCodecPlayer::RequestDemuxerData, media_weak_this_,
                 DemuxerStream::AUDIO),
      base::Bind(&MediaCodecPlayer::OnStarvation, media_weak_this_,
                 DemuxerStream::AUDIO),
      base::Bind(&MediaCodecPlayer::OnDecoderDrained, media_weak_this_,
                 DemuxerStream::AUDIO),
      base::Bind(&MediaCodecPlayer::OnStopDone, media_weak_this_,
                 DemuxerStream::AUDIO),
      base::Bind(&MediaCodecPlayer::OnMissingKeyReported, media_weak_this_,
                 DemuxerStream::AUDIO),
      internal_error_cb_, base::Bind(&MediaCodecPlayer::OnTimeIntervalUpdate,
                                     media_weak_this_, DemuxerStream::AUDIO)));

  video_decoder_.reset(new VideoMediaCodecDecoder(
      GetMediaTaskRunner(), &media_stat_->video_frame_stats(),
      base::Bind(&MediaCodecPlayer::RequestDemuxerData, media_weak_this_,
                 DemuxerStream::VIDEO),
      base::Bind(&MediaCodecPlayer::OnStarvation, media_weak_this_,
                 DemuxerStream::VIDEO),
      base::Bind(&MediaCodecPlayer::OnDecoderDrained, media_weak_this_,
                 DemuxerStream::VIDEO),
      base::Bind(&MediaCodecPlayer::OnStopDone, media_weak_this_,
                 DemuxerStream::VIDEO),
      base::Bind(&MediaCodecPlayer::OnMissingKeyReported, media_weak_this_,
                 DemuxerStream::VIDEO),
      internal_error_cb_, base::Bind(&MediaCodecPlayer::OnTimeIntervalUpdate,
                                     media_weak_this_, DemuxerStream::VIDEO),
      base::Bind(&MediaCodecPlayer::OnVideoResolutionChanged,
                 media_weak_this_)));
}

bool MediaCodecPlayer::AudioFinished() const {
  return audio_decoder_->IsCompleted() || !audio_decoder_->HasStream();
}

bool MediaCodecPlayer::VideoFinished() const {
  return video_decoder_->IsCompleted() || !video_decoder_->HasStream();
}

base::TimeDelta MediaCodecPlayer::GetInterpolatedTime() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  base::TimeDelta interpolated_time = interpolator_.GetInterpolatedTime();
  return std::min(interpolated_time, duration_);
}

#undef RETURN_STRING
#define RETURN_STRING(x) \
  case x:                \
    return #x;

const char* MediaCodecPlayer::AsString(PlayerState state) {
  switch (state) {
    RETURN_STRING(kStatePaused);
    RETURN_STRING(kStateWaitingForConfig);
    RETURN_STRING(kStateWaitingForPermission);
    RETURN_STRING(kStatePrefetching);
    RETURN_STRING(kStatePlaying);
    RETURN_STRING(kStateStopping);
    RETURN_STRING(kStateWaitingForSurface);
    RETURN_STRING(kStateWaitingForKey);
    RETURN_STRING(kStateWaitingForMediaCrypto);
    RETURN_STRING(kStateWaitingForSeek);
    RETURN_STRING(kStateError);
  }
  return nullptr;  // crash early
}

#undef RETURN_STRING

}  // namespace media
