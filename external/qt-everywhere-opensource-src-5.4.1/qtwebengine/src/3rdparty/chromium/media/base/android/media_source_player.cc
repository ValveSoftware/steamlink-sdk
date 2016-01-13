// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_source_player.h"

#include <limits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/barrier_closure.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/android/audio_decoder_job.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/android/video_decoder_job.h"


namespace media {

MediaSourcePlayer::MediaSourcePlayer(
    int player_id,
    MediaPlayerManager* manager,
    const RequestMediaResourcesCB& request_media_resources_cb,
    const ReleaseMediaResourcesCB& release_media_resources_cb,
    scoped_ptr<DemuxerAndroid> demuxer,
    const GURL& frame_url)
    : MediaPlayerAndroid(player_id,
                         manager,
                         request_media_resources_cb,
                         release_media_resources_cb,
                         frame_url),
      demuxer_(demuxer.Pass()),
      pending_event_(NO_EVENT_PENDING),
      playing_(false),
      clock_(&default_tick_clock_),
      doing_browser_seek_(false),
      pending_seek_(false),
      drm_bridge_(NULL),
      cdm_registration_id_(0),
      is_waiting_for_key_(false),
      is_waiting_for_audio_decoder_(false),
      is_waiting_for_video_decoder_(false),
      weak_factory_(this) {
  audio_decoder_job_.reset(new AudioDecoderJob(
      base::Bind(&DemuxerAndroid::RequestDemuxerData,
                 base::Unretained(demuxer_.get()),
                 DemuxerStream::AUDIO),
      base::Bind(&MediaSourcePlayer::OnDemuxerConfigsChanged,
                 weak_factory_.GetWeakPtr())));
  video_decoder_job_.reset(new VideoDecoderJob(
      base::Bind(&DemuxerAndroid::RequestDemuxerData,
                 base::Unretained(demuxer_.get()),
                 DemuxerStream::VIDEO),
      base::Bind(request_media_resources_cb_, player_id),
      base::Bind(release_media_resources_cb_, player_id),
      base::Bind(&MediaSourcePlayer::OnDemuxerConfigsChanged,
                 weak_factory_.GetWeakPtr())));
  demuxer_->Initialize(this);
  clock_.SetMaxTime(base::TimeDelta());
  weak_this_ = weak_factory_.GetWeakPtr();
}

MediaSourcePlayer::~MediaSourcePlayer() {
  Release();
  DCHECK_EQ(!drm_bridge_, !cdm_registration_id_);
  if (drm_bridge_) {
    drm_bridge_->UnregisterPlayer(cdm_registration_id_);
    cdm_registration_id_ = 0;
  }
}

void MediaSourcePlayer::SetVideoSurface(gfx::ScopedJavaSurface surface) {
  DVLOG(1) << __FUNCTION__;
  if (!video_decoder_job_->SetVideoSurface(surface.Pass()))
    return;
  // Retry video decoder creation.
  RetryDecoderCreation(false, true);
}

void MediaSourcePlayer::ScheduleSeekEventAndStopDecoding(
    base::TimeDelta seek_time) {
  DVLOG(1) << __FUNCTION__ << "(" << seek_time.InSecondsF() << ")";
  DCHECK(!IsEventPending(SEEK_EVENT_PENDING));

  pending_seek_ = false;

  clock_.SetTime(seek_time, seek_time);

  if (audio_decoder_job_->is_decoding())
    audio_decoder_job_->StopDecode();
  if (video_decoder_job_->is_decoding())
    video_decoder_job_->StopDecode();

  SetPendingEvent(SEEK_EVENT_PENDING);
  ProcessPendingEvents();
}

void MediaSourcePlayer::BrowserSeekToCurrentTime() {
  DVLOG(1) << __FUNCTION__;

  DCHECK(!IsEventPending(SEEK_EVENT_PENDING));
  doing_browser_seek_ = true;
  ScheduleSeekEventAndStopDecoding(GetCurrentTime());
}

bool MediaSourcePlayer::Seekable() {
  // If the duration TimeDelta, converted to milliseconds from microseconds,
  // is >= 2^31, then the media is assumed to be unbounded and unseekable.
  // 2^31 is the bound due to java player using 32-bit integer for time
  // values at millisecond resolution.
  return duration_ <
         base::TimeDelta::FromMilliseconds(std::numeric_limits<int32>::max());
}

void MediaSourcePlayer::Start() {
  DVLOG(1) << __FUNCTION__;

  playing_ = true;

  bool request_fullscreen = IsProtectedSurfaceRequired();
#if defined(VIDEO_HOLE)
  // Skip to request fullscreen when hole-punching is used.
  request_fullscreen = request_fullscreen &&
      !manager()->ShouldUseVideoOverlayForEmbeddedEncryptedVideo();
#endif  // defined(VIDEO_HOLE)
  if (request_fullscreen)
    manager()->RequestFullScreen(player_id());

  StartInternal();
}

void MediaSourcePlayer::Pause(bool is_media_related_action) {
  DVLOG(1) << __FUNCTION__;

  // Since decoder jobs have their own thread, decoding is not fully paused
  // until all the decoder jobs call MediaDecoderCallback(). It is possible
  // that Start() is called while the player is waiting for
  // MediaDecoderCallback(). In that case, decoding will continue when
  // MediaDecoderCallback() is called.
  playing_ = false;
  start_time_ticks_ = base::TimeTicks();
}

bool MediaSourcePlayer::IsPlaying() {
  return playing_;
}

int MediaSourcePlayer::GetVideoWidth() {
  return video_decoder_job_->width();
}

int MediaSourcePlayer::GetVideoHeight() {
  return video_decoder_job_->height();
}

void MediaSourcePlayer::SeekTo(base::TimeDelta timestamp) {
  DVLOG(1) << __FUNCTION__ << "(" << timestamp.InSecondsF() << ")";

  if (IsEventPending(SEEK_EVENT_PENDING)) {
    DCHECK(doing_browser_seek_) << "SeekTo while SeekTo in progress";
    DCHECK(!pending_seek_) << "SeekTo while SeekTo pending browser seek";

    // There is a browser seek currently in progress to obtain I-frame to feed
    // a newly constructed video decoder. Remember this real seek request so
    // it can be initiated once OnDemuxerSeekDone() occurs for the browser seek.
    pending_seek_ = true;
    pending_seek_time_ = timestamp;
    return;
  }

  doing_browser_seek_ = false;
  ScheduleSeekEventAndStopDecoding(timestamp);
}

base::TimeDelta MediaSourcePlayer::GetCurrentTime() {
  return clock_.Elapsed();
}

base::TimeDelta MediaSourcePlayer::GetDuration() {
  return duration_;
}

void MediaSourcePlayer::Release() {
  DVLOG(1) << __FUNCTION__;

  is_surface_in_use_ = false;
  audio_decoder_job_->ReleaseDecoderResources();
  video_decoder_job_->ReleaseDecoderResources();

  // Prevent player restart, including job re-creation attempts.
  playing_ = false;

  decoder_starvation_callback_.Cancel();
}

void MediaSourcePlayer::SetVolume(double volume) {
  audio_decoder_job_->SetVolume(volume);
}

bool MediaSourcePlayer::IsSurfaceInUse() const {
  return is_surface_in_use_;
}

bool MediaSourcePlayer::CanPause() {
  return Seekable();
}

bool MediaSourcePlayer::CanSeekForward() {
  return Seekable();
}

bool MediaSourcePlayer::CanSeekBackward() {
  return Seekable();
}

bool MediaSourcePlayer::IsPlayerReady() {
  return audio_decoder_job_ || video_decoder_job_;
}

void MediaSourcePlayer::StartInternal() {
  DVLOG(1) << __FUNCTION__;
  // If there are pending events, wait for them finish.
  if (pending_event_ != NO_EVENT_PENDING)
    return;

  // When we start, we'll have new demuxed data coming in. This new data could
  // be clear (not encrypted) or encrypted with different keys. So
  // |is_waiting_for_key_| condition may not be true anymore.
  is_waiting_for_key_ = false;

  SetPendingEvent(PREFETCH_REQUEST_EVENT_PENDING);
  ProcessPendingEvents();
}

void MediaSourcePlayer::OnDemuxerConfigsAvailable(
    const DemuxerConfigs& configs) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(!HasAudio() && !HasVideo());
  duration_ = configs.duration;
  clock_.SetDuration(duration_);

  audio_decoder_job_->SetDemuxerConfigs(configs);
  video_decoder_job_->SetDemuxerConfigs(configs);
  OnDemuxerConfigsChanged();
}

void MediaSourcePlayer::OnDemuxerDataAvailable(const DemuxerData& data) {
  DVLOG(1) << __FUNCTION__ << "(" << data.type << ")";
  DCHECK_LT(0u, data.access_units.size());
  CHECK_GE(1u, data.demuxer_configs.size());

  if (data.type == DemuxerStream::AUDIO)
    audio_decoder_job_->OnDataReceived(data);
  else if (data.type == DemuxerStream::VIDEO)
    video_decoder_job_->OnDataReceived(data);
}

void MediaSourcePlayer::OnDemuxerDurationChanged(base::TimeDelta duration) {
  duration_ = duration;
  clock_.SetDuration(duration_);
}

void MediaSourcePlayer::OnMediaCryptoReady() {
  DCHECK(!drm_bridge_->GetMediaCrypto().is_null());
  drm_bridge_->SetMediaCryptoReadyCB(base::Closure());

  // Retry decoder creation if the decoders are waiting for MediaCrypto.
  RetryDecoderCreation(true, true);
}

void MediaSourcePlayer::SetCdm(BrowserCdm* cdm) {
  // Currently we don't support DRM change during the middle of playback, even
  // if the player is paused.
  // TODO(qinmin): support DRM change after playback has started.
  // http://crbug.com/253792.
  if (GetCurrentTime() > base::TimeDelta()) {
    VLOG(0) << "Setting DRM bridge after playback has started. "
            << "This is not well supported!";
  }

  if (drm_bridge_) {
    NOTREACHED() << "Currently we do not support resetting CDM.";
    return;
  }

  // Only MediaDrmBridge will be set on MediaSourcePlayer.
  drm_bridge_ = static_cast<MediaDrmBridge*>(cdm);

  cdm_registration_id_ = drm_bridge_->RegisterPlayer(
      base::Bind(&MediaSourcePlayer::OnKeyAdded, weak_this_),
      base::Bind(&MediaSourcePlayer::OnCdmUnset, weak_this_));

  audio_decoder_job_->SetDrmBridge(drm_bridge_);
  video_decoder_job_->SetDrmBridge(drm_bridge_);

  if (drm_bridge_->GetMediaCrypto().is_null()) {
    drm_bridge_->SetMediaCryptoReadyCB(
        base::Bind(&MediaSourcePlayer::OnMediaCryptoReady, weak_this_));
    return;
  }

  // If the player is previously waiting for CDM, retry decoder creation.
  RetryDecoderCreation(true, true);
}

void MediaSourcePlayer::OnDemuxerSeekDone(
    base::TimeDelta actual_browser_seek_time) {
  DVLOG(1) << __FUNCTION__;

  ClearPendingEvent(SEEK_EVENT_PENDING);
  if (IsEventPending(PREFETCH_REQUEST_EVENT_PENDING))
    ClearPendingEvent(PREFETCH_REQUEST_EVENT_PENDING);

  if (pending_seek_) {
    DVLOG(1) << __FUNCTION__ << "processing pending seek";
    DCHECK(doing_browser_seek_);
    pending_seek_ = false;
    SeekTo(pending_seek_time_);
    return;
  }

  // It is possible that a browser seek to I-frame had to seek to a buffered
  // I-frame later than the requested one due to data removal or GC. Update
  // player clock to the actual seek target.
  if (doing_browser_seek_) {
    DCHECK(actual_browser_seek_time != kNoTimestamp());
    base::TimeDelta seek_time = actual_browser_seek_time;
    // A browser seek must not jump into the past. Ideally, it seeks to the
    // requested time, but it might jump into the future.
    DCHECK(seek_time >= GetCurrentTime());
    DVLOG(1) << __FUNCTION__ << " : setting clock to actual browser seek time: "
             << seek_time.InSecondsF();
    clock_.SetTime(seek_time, seek_time);
    audio_decoder_job_->SetBaseTimestamp(seek_time);
  } else {
    DCHECK(actual_browser_seek_time == kNoTimestamp());
  }

  base::TimeDelta current_time = GetCurrentTime();
  // TODO(qinmin): Simplify the logic by using |start_presentation_timestamp_|
  // to preroll media decoder jobs. Currently |start_presentation_timestamp_|
  // is calculated from decoder output, while preroll relies on the access
  // unit's timestamp. There are some differences between the two.
  preroll_timestamp_ = current_time;
  if (HasAudio())
    audio_decoder_job_->BeginPrerolling(preroll_timestamp_);
  if (HasVideo())
    video_decoder_job_->BeginPrerolling(preroll_timestamp_);

  if (!doing_browser_seek_)
    manager()->OnSeekComplete(player_id(), current_time);

  ProcessPendingEvents();
}

void MediaSourcePlayer::UpdateTimestamps(
    base::TimeDelta current_presentation_timestamp,
    base::TimeDelta max_presentation_timestamp) {
  clock_.SetTime(current_presentation_timestamp, max_presentation_timestamp);
  manager()->OnTimeUpdate(player_id(), GetCurrentTime());
}

void MediaSourcePlayer::ProcessPendingEvents() {
  DVLOG(1) << __FUNCTION__ << " : 0x" << std::hex << pending_event_;
  // Wait for all the decoding jobs to finish before processing pending tasks.
  if (video_decoder_job_->is_decoding()) {
    DVLOG(1) << __FUNCTION__ << " : A video job is still decoding.";
    return;
  }

  if (audio_decoder_job_->is_decoding()) {
    DVLOG(1) << __FUNCTION__ << " : An audio job is still decoding.";
    return;
  }

  if (IsEventPending(PREFETCH_DONE_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : PREFETCH_DONE still pending.";
    return;
  }

  if (IsEventPending(SEEK_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : Handling SEEK_EVENT";
    ClearDecodingData();
    audio_decoder_job_->SetBaseTimestamp(GetCurrentTime());
    demuxer_->RequestDemuxerSeek(GetCurrentTime(), doing_browser_seek_);
    return;
  }

  if (IsEventPending(DECODER_CREATION_EVENT_PENDING)) {
    // Don't continue if one of the decoder is not created.
    if (is_waiting_for_audio_decoder_ || is_waiting_for_video_decoder_)
      return;
    ClearPendingEvent(DECODER_CREATION_EVENT_PENDING);
  }

  if (IsEventPending(PREFETCH_REQUEST_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : Handling PREFETCH_REQUEST_EVENT.";
    int count = (AudioFinished() ? 0 : 1) + (VideoFinished() ? 0 : 1);

    // It is possible that all streams have finished decode, yet starvation
    // occurred during the last stream's EOS decode. In this case, prefetch is a
    // no-op.
    ClearPendingEvent(PREFETCH_REQUEST_EVENT_PENDING);
    if (count == 0)
      return;

    SetPendingEvent(PREFETCH_DONE_EVENT_PENDING);
    base::Closure barrier = BarrierClosure(
        count, base::Bind(&MediaSourcePlayer::OnPrefetchDone, weak_this_));

    if (!AudioFinished())
      audio_decoder_job_->Prefetch(barrier);

    if (!VideoFinished())
      video_decoder_job_->Prefetch(barrier);

    return;
  }

  DCHECK_EQ(pending_event_, NO_EVENT_PENDING);

  // Now that all pending events have been handled, resume decoding if we are
  // still playing.
  if (playing_)
    StartInternal();
}

void MediaSourcePlayer::MediaDecoderCallback(
    bool is_audio, MediaCodecStatus status,
    base::TimeDelta current_presentation_timestamp,
    base::TimeDelta max_presentation_timestamp) {
  DVLOG(1) << __FUNCTION__ << ": " << is_audio << ", " << status;

  // TODO(xhwang): Drop IntToString() when http://crbug.com/303899 is fixed.
  if (is_audio) {
    TRACE_EVENT_ASYNC_END1("media",
                           "MediaSourcePlayer::DecodeMoreAudio",
                           audio_decoder_job_.get(),
                           "MediaCodecStatus",
                           base::IntToString(status));
  } else {
    TRACE_EVENT_ASYNC_END1("media",
                           "MediaSourcePlayer::DecodeMoreVideo",
                           video_decoder_job_.get(),
                           "MediaCodecStatus",
                           base::IntToString(status));
  }

  // Let tests hook the completion of this decode cycle.
  if (!decode_callback_for_testing_.is_null())
    base::ResetAndReturn(&decode_callback_for_testing_).Run();

  bool is_clock_manager = is_audio || !HasAudio();

  if (is_clock_manager)
    decoder_starvation_callback_.Cancel();

  if (status == MEDIA_CODEC_ERROR) {
    DVLOG(1) << __FUNCTION__ << " : decode error";
    Release();
    manager()->OnError(player_id(), MEDIA_ERROR_DECODE);
    return;
  }

  DCHECK(!IsEventPending(PREFETCH_DONE_EVENT_PENDING));

  // Let |SEEK_EVENT_PENDING| (the highest priority event outside of
  // |PREFETCH_DONE_EVENT_PENDING|) preempt output EOS detection here. Process
  // any other pending events only after handling EOS detection.
  if (IsEventPending(SEEK_EVENT_PENDING)) {
    ProcessPendingEvents();
    return;
  }

  if ((status == MEDIA_CODEC_OK || status == MEDIA_CODEC_INPUT_END_OF_STREAM) &&
      is_clock_manager && current_presentation_timestamp != kNoTimestamp()) {
    UpdateTimestamps(current_presentation_timestamp,
                     max_presentation_timestamp);
  }

  if (status == MEDIA_CODEC_OUTPUT_END_OF_STREAM)
    PlaybackCompleted(is_audio);

  if (pending_event_ != NO_EVENT_PENDING) {
    ProcessPendingEvents();
    return;
  }

  if (status == MEDIA_CODEC_OUTPUT_END_OF_STREAM)
    return;

  if (!playing_) {
    if (is_clock_manager)
      clock_.Pause();
    return;
  }

  if (status == MEDIA_CODEC_NO_KEY) {
    is_waiting_for_key_ = true;
    return;
  }

  // If the status is MEDIA_CODEC_STOPPED, stop decoding new data. The player is
  // in the middle of a seek or stop event and needs to wait for the IPCs to
  // come.
  if (status == MEDIA_CODEC_STOPPED)
    return;

  if (is_clock_manager) {
    // If we have a valid timestamp, start the starvation callback. Otherwise,
    // reset the |start_time_ticks_| so that the next frame will not suffer
    // from the decoding delay caused by the current frame.
    if (current_presentation_timestamp != kNoTimestamp())
      StartStarvationCallback(current_presentation_timestamp,
                              max_presentation_timestamp);
    else
      start_time_ticks_ = base::TimeTicks::Now();
  }

  if (is_audio)
    DecodeMoreAudio();
  else
    DecodeMoreVideo();
}

void MediaSourcePlayer::DecodeMoreAudio() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(!audio_decoder_job_->is_decoding());
  DCHECK(!AudioFinished());

  if (audio_decoder_job_->Decode(
      start_time_ticks_,
      start_presentation_timestamp_,
      base::Bind(&MediaSourcePlayer::MediaDecoderCallback, weak_this_, true))) {
    TRACE_EVENT_ASYNC_BEGIN0("media", "MediaSourcePlayer::DecodeMoreAudio",
                             audio_decoder_job_.get());
    return;
  }

  is_waiting_for_audio_decoder_ = true;
  if (!IsEventPending(DECODER_CREATION_EVENT_PENDING))
    SetPendingEvent(DECODER_CREATION_EVENT_PENDING);
}

void MediaSourcePlayer::DecodeMoreVideo() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(!video_decoder_job_->is_decoding());
  DCHECK(!VideoFinished());

  if (video_decoder_job_->Decode(
      start_time_ticks_,
      start_presentation_timestamp_,
      base::Bind(&MediaSourcePlayer::MediaDecoderCallback, weak_this_,
                 false))) {
    TRACE_EVENT_ASYNC_BEGIN0("media", "MediaSourcePlayer::DecodeMoreVideo",
                             video_decoder_job_.get());
    return;
  }

  // If the decoder is waiting for iframe, trigger a browser seek.
  if (!video_decoder_job_->next_video_data_is_iframe()) {
    BrowserSeekToCurrentTime();
    return;
  }

  is_waiting_for_video_decoder_ = true;
  if (!IsEventPending(DECODER_CREATION_EVENT_PENDING))
    SetPendingEvent(DECODER_CREATION_EVENT_PENDING);
}

void MediaSourcePlayer::PlaybackCompleted(bool is_audio) {
  DVLOG(1) << __FUNCTION__ << "(" << is_audio << ")";

  if (AudioFinished() && VideoFinished()) {
    playing_ = false;
    clock_.Pause();
    start_time_ticks_ = base::TimeTicks();
    manager()->OnPlaybackComplete(player_id());
  }
}

void MediaSourcePlayer::ClearDecodingData() {
  DVLOG(1) << __FUNCTION__;
  audio_decoder_job_->Flush();
  video_decoder_job_->Flush();
  start_time_ticks_ = base::TimeTicks();
}

bool MediaSourcePlayer::HasVideo() {
  return video_decoder_job_->HasStream();
}

bool MediaSourcePlayer::HasAudio() {
  return audio_decoder_job_->HasStream();
}

bool MediaSourcePlayer::AudioFinished() {
  return audio_decoder_job_->OutputEOSReached() || !HasAudio();
}

bool MediaSourcePlayer::VideoFinished() {
  return video_decoder_job_->OutputEOSReached() || !HasVideo();
}

void MediaSourcePlayer::OnDecoderStarved() {
  DVLOG(1) << __FUNCTION__;
  SetPendingEvent(PREFETCH_REQUEST_EVENT_PENDING);
  ProcessPendingEvents();
}

void MediaSourcePlayer::StartStarvationCallback(
    base::TimeDelta current_presentation_timestamp,
    base::TimeDelta max_presentation_timestamp) {
  // 20ms was chosen because it is the typical size of a compressed audio frame.
  // Anything smaller than this would likely cause unnecessary cycling in and
  // out of the prefetch state.
  const base::TimeDelta kMinStarvationTimeout =
      base::TimeDelta::FromMilliseconds(20);

  base::TimeDelta current_timestamp = GetCurrentTime();
  base::TimeDelta timeout;
  if (HasAudio()) {
    timeout = max_presentation_timestamp - current_timestamp;
  } else {
    DCHECK(current_timestamp <= current_presentation_timestamp);

    // For video only streams, fps can be estimated from the difference
    // between the previous and current presentation timestamps. The
    // previous presentation timestamp is equal to current_timestamp.
    // TODO(qinmin): determine whether 2 is a good coefficient for estimating
    // video frame timeout.
    timeout = 2 * (current_presentation_timestamp - current_timestamp);
  }

  timeout = std::max(timeout, kMinStarvationTimeout);

  decoder_starvation_callback_.Reset(
      base::Bind(&MediaSourcePlayer::OnDecoderStarved, weak_this_));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, decoder_starvation_callback_.callback(), timeout);
}

bool MediaSourcePlayer::IsProtectedSurfaceRequired() {
  return video_decoder_job_->is_content_encrypted() &&
      drm_bridge_ && drm_bridge_->IsProtectedSurfaceRequired();
}

void MediaSourcePlayer::OnPrefetchDone() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(!audio_decoder_job_->is_decoding());
  DCHECK(!video_decoder_job_->is_decoding());

  // A previously posted OnPrefetchDone() could race against a Release(). If
  // Release() won the race, we should no longer have decoder jobs.
  // TODO(qinmin/wolenetz): Maintain channel state to not double-request data
  // or drop data received across Release()+Start(). See http://crbug.com/306314
  // and http://crbug.com/304234.
  if (!IsEventPending(PREFETCH_DONE_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : aborting";
    return;
  }

  ClearPendingEvent(PREFETCH_DONE_EVENT_PENDING);

  if (pending_event_ != NO_EVENT_PENDING) {
    ProcessPendingEvents();
    return;
  }

  if (!playing_)
    return;

  start_time_ticks_ = base::TimeTicks::Now();
  start_presentation_timestamp_ = GetCurrentTime();
  if (!clock_.IsPlaying())
    clock_.Play();

  if (!AudioFinished())
    DecodeMoreAudio();

  if (!VideoFinished())
    DecodeMoreVideo();
}

void MediaSourcePlayer::OnDemuxerConfigsChanged() {
  manager()->OnMediaMetadataChanged(
      player_id(), duration_, GetVideoWidth(), GetVideoHeight(), true);
}

const char* MediaSourcePlayer::GetEventName(PendingEventFlags event) {
  // Please keep this in sync with PendingEventFlags.
  static const char* kPendingEventNames[] = {
    "PREFETCH_DONE",
    "SEEK",
    "DECODER_CREATION",
    "PREFETCH_REQUEST",
  };

  int mask = 1;
  for (size_t i = 0; i < arraysize(kPendingEventNames); ++i, mask <<= 1) {
    if (event & mask)
      return kPendingEventNames[i];
  }

  return "UNKNOWN";
}

bool MediaSourcePlayer::IsEventPending(PendingEventFlags event) const {
  return pending_event_ & event;
}

void MediaSourcePlayer::SetPendingEvent(PendingEventFlags event) {
  DVLOG(1) << __FUNCTION__ << "(" << GetEventName(event) << ")";
  DCHECK_NE(event, NO_EVENT_PENDING);
  DCHECK(!IsEventPending(event));

  pending_event_ |= event;
}

void MediaSourcePlayer::ClearPendingEvent(PendingEventFlags event) {
  DVLOG(1) << __FUNCTION__ << "(" << GetEventName(event) << ")";
  DCHECK_NE(event, NO_EVENT_PENDING);
  DCHECK(IsEventPending(event)) << GetEventName(event);

  pending_event_ &= ~event;
}

void MediaSourcePlayer::RetryDecoderCreation(bool audio, bool video) {
  if (audio)
    is_waiting_for_audio_decoder_ = false;
  if (video)
    is_waiting_for_video_decoder_ = false;
  if (IsEventPending(DECODER_CREATION_EVENT_PENDING))
    ProcessPendingEvents();
}

void MediaSourcePlayer::OnKeyAdded() {
  DVLOG(1) << __FUNCTION__;
  if (!is_waiting_for_key_)
    return;

  is_waiting_for_key_ = false;
  if (playing_)
    StartInternal();
}

void MediaSourcePlayer::OnCdmUnset() {
  DVLOG(1) << __FUNCTION__;
  // TODO(xhwang): Support detachment of CDM. This will be needed when we start
  // to support setMediaKeys(0) (see http://crbug.com/330324), or when we
  // release MediaDrm when the video is paused, or when the device goes to
  // sleep (see http://crbug.com/272421).
  NOTREACHED() << "CDM detachment not supported.";
  DCHECK(drm_bridge_);
  audio_decoder_job_->SetDrmBridge(NULL);
  video_decoder_job_->SetDrmBridge(NULL);
  drm_bridge_ = NULL;
}

}  // namespace media
