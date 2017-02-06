// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_SOURCE_PLAYER_H_
#define MEDIA_BASE_ANDROID_MEDIA_SOURCE_PLAYER_H_

#include <jni.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "media/base/android/demuxer_android.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_decoder_job.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_android.h"
#include "media/base/android/media_statistics.h"
#include "media/base/media_export.h"
#include "media/base/time_delta_interpolator.h"

namespace media {

class AudioDecoderJob;
class VideoDecoderJob;

// This class handles media source extensions on Android. It uses Android
// MediaCodec to decode audio and video streams in two separate threads.
class MEDIA_EXPORT MediaSourcePlayer : public MediaPlayerAndroid,
                                       public DemuxerAndroidClient {
 public:
  // Constructs a player with the given ID and demuxer. |manager| must outlive
  // the lifetime of this object.
  MediaSourcePlayer(
      int player_id,
      MediaPlayerManager* manager,
      const OnDecoderResourcesReleasedCB& on_decoder_resources_released_cb,
      std::unique_ptr<DemuxerAndroid> demuxer,
      const GURL& frame_url,
      int media_session_id);
  ~MediaSourcePlayer() override;

  // MediaPlayerAndroid implementation.
  void SetVideoSurface(gl::ScopedJavaSurface surface) override;
  void Start() override;
  void Pause(bool is_media_related_action) override;
  void SeekTo(base::TimeDelta timestamp) override;
  void Release() override;
  bool HasVideo() const override;
  bool HasAudio() const override;
  int GetVideoWidth() override;
  int GetVideoHeight() override;
  base::TimeDelta GetCurrentTime() override;
  base::TimeDelta GetDuration() override;
  bool IsPlaying() override;
  bool CanPause() override;
  bool CanSeekForward() override;
  bool CanSeekBackward() override;
  bool IsPlayerReady() override;
  void SetCdm(const scoped_refptr<MediaKeys>& cdm) override;

  // DemuxerAndroidClient implementation.
  void OnDemuxerConfigsAvailable(const DemuxerConfigs& params) override;
  void OnDemuxerDataAvailable(const DemuxerData& params) override;
  void OnDemuxerSeekDone(base::TimeDelta actual_browser_seek_time) override;
  void OnDemuxerDurationChanged(base::TimeDelta duration) override;

 private:
  friend class MediaSourcePlayerTest;

  // MediaPlayerAndroid implementation
  void UpdateEffectiveVolumeInternal(double effective_volume) override;

  // Update the current timestamp.
  void UpdateTimestamps(base::TimeDelta current_presentation_timestamp,
                        base::TimeDelta max_presentation_timestamp);

  // Helper function for starting media playback.
  void StartInternal();

  // Playback is completed for one channel.
  void PlaybackCompleted(bool is_audio);

  // Called when the decoder finishes its task.
  void MediaDecoderCallback(bool is_audio,
                            MediaCodecStatus status,
                            bool is_late_frame,
                            base::TimeDelta current_presentation_timestamp,
                            base::TimeDelta max_presentation_timestamp);

  bool IsPrerollFinished(bool is_audio) const;

  // Gets MediaCrypto object from |drm_bridge_|.
  base::android::ScopedJavaLocalRef<jobject> GetMediaCrypto();

  // Callback to notify that MediaCrypto is ready in |drm_bridge_|.
  void OnMediaCryptoReady(MediaDrmBridge::JavaObjectPtr media_crypto,
                          bool needs_protected_surface);

  // Handle pending events if all the decoder jobs are not currently decoding.
  void ProcessPendingEvents();

  // Flush the decoders and clean up all the data needs to be decoded.
  void ClearDecodingData();

  // Called to decode more data.
  void DecodeMoreAudio();
  void DecodeMoreVideo();

  // Functions that check whether audio/video stream has reached end of output
  // or are not present in player configuration.
  bool AudioFinished();
  bool VideoFinished();

  // Determine seekability based on duration.
  bool Seekable();

  // Called when the |decoder_starvation_callback_| times out.
  void OnDecoderStarved();

  // Starts the |decoder_starvation_callback_| task with the timeout value.
  // |current_presentation_timestamp| - The presentation timestamp used for
  // starvation timeout computations. It represents the current timestamp of
  // rendered data.
  // |max_presentation_timestamp| - The presentation timestamp if all the
  // decoded data are rendered.
  void StartStarvationCallback(
      base::TimeDelta current_presentation_timestamp,
      base::TimeDelta max_presentation_timestamp);

  // Schedules a seek event in |pending_events_| and calls StopDecode() on all
  // the MediaDecoderJobs. Sets clock to |seek_time|, and resets
  // |pending_seek_|. There must not already be a seek event in
  // |pending_events_|.
  void ScheduleSeekEventAndStopDecoding(base::TimeDelta seek_time);

  // Schedules a browser seek event. We must not currently be processing any
  // seek. Note that there is possibility that browser seek of renderer demuxer
  // may unexpectedly stall due to lack of buffered data at or after the browser
  // seek time.
  // TODO(wolenetz): Instead of doing hack browser seek, replay cached data
  // since last keyframe. See http://crbug.com/304234.
  void BrowserSeekToCurrentTime();

  // Called when a MediaDecoderJob finishes prefetching data. Once all
  // MediaDecoderJobs have prefetched data, then this method updates
  // |start_time_ticks_| and |start_presentation_timestamp_| so that video can
  // resync with audio and starts decoding.
  void OnPrefetchDone();

  // Called when the demuxer config changes.
  void OnDemuxerConfigsChanged();

  // Called when new decryption key becomes available.
  void OnKeyAdded();

  // Called to resume playback after NO_KEY is received, but a new key is
  // available.
  void ResumePlaybackAfterKeyAdded();

  // Test-only method to setup hook for the completion of the next decode cycle.
  // This callback state is cleared when it is next run.
  // Prevent usage creep by only calling this from the
  // ReleaseWithOnPrefetchDoneAlreadyPosted MediaSourcePlayerTest.
  void set_decode_callback_for_testing(const base::Closure& test_decode_cb) {
    decode_callback_for_testing_ = test_decode_cb;
  }

  // Please keep this in sync with |kPendingEventNames| in GetEventName().
  enum PendingEventFlags {
    NO_EVENT_PENDING = 0,
    PREFETCH_DONE_EVENT_PENDING = 1 << 0,
    SEEK_EVENT_PENDING = 1 << 1,
    DECODER_CREATION_EVENT_PENDING = 1 << 2,
    PREFETCH_REQUEST_EVENT_PENDING = 1 << 3,
  };

  static const char* GetEventName(PendingEventFlags event);
  bool IsEventPending(PendingEventFlags event) const;
  void SetPendingEvent(PendingEventFlags event);
  void ClearPendingEvent(PendingEventFlags event);

  // If the player is previously waiting for audio or video decoder job, retry
  // creating the decoders identified by |audio| and |video|.
  void RetryDecoderCreation(bool audio, bool video);

  std::unique_ptr<DemuxerAndroid> demuxer_;

  // Pending event that the player needs to do.
  unsigned pending_event_;

  // Stats about the media.
  base::TimeDelta duration_;
  bool playing_;

  // base::TickClock used by |interpolator_|.
  base::DefaultTickClock default_tick_clock_;

  // Tracks the most recent media time update and provides interpolated values
  // as playback progresses.
  TimeDeltaInterpolator interpolator_;

  // Timestamps for providing simple A/V sync. When start decoding an audio
  // chunk, we record its presentation timestamp and the current system time.
  // Then we use this information to estimate when the next audio/video frame
  // should be rendered.
  // TODO(qinmin): Need to fix the problem if audio/video lagged too far behind
  // due to network or decoding problem.
  base::TimeTicks start_time_ticks_;
  base::TimeDelta start_presentation_timestamp_;

  // Flag that is true if doing a hack browser seek or false if doing a
  // regular seek. Only valid when |SEEK_EVENT_PENDING| is pending.
  // TODO(wolenetz): Instead of doing hack browser seek, replay cached data
  // since last keyframe. See http://crbug.com/304234.
  bool doing_browser_seek_;

  // If already doing a browser seek when a regular seek request arrives,
  // these fields remember the regular seek so OnDemuxerSeekDone() can trigger
  // it when the browser seek is done. These are only valid when
  // |SEEK_EVENT_PENDING| is pending.
  bool pending_seek_;
  base::TimeDelta pending_seek_time_;

  // Decoder jobs.
  std::unique_ptr<AudioDecoderJob, MediaDecoderJob::Deleter> audio_decoder_job_;
  std::unique_ptr<VideoDecoderJob, MediaDecoderJob::Deleter> video_decoder_job_;

  // Track the most recent preroll target. Decoder re-creation needs this to
  // resume any in-progress preroll.
  base::TimeDelta preroll_timestamp_;

  // A cancelable task that is posted when the audio decoder starts requesting
  // new data. This callback runs if no data arrives before the timeout period
  // elapses.
  base::CancelableClosure decoder_starvation_callback_;

  // Holds a ref-count to the CDM.
  scoped_refptr<MediaKeys> cdm_;

  int cdm_registration_id_;

  // No decryption key available to decrypt the encrypted buffer. In this case,
  // the player should pause. When a new key is added (OnKeyAdded()), we should
  // try to start playback again.
  bool is_waiting_for_key_;

  // Indicates the situation where new key is added during pending decode
  // (this variable can only be set when *_decoder_job_->is_decoding()). If this
  // variable is true and MEDIA_CODEC_NO_KEY is returned then we need to try
  // decoding again in case the newly added key is the correct decryption key.
  bool key_added_while_decode_pending_;

  // Indicates whether the player is waiting for audio or video decoder to be
  // created. This could happen if video surface is not available or key is
  // not added.
  bool is_waiting_for_audio_decoder_;
  bool is_waiting_for_video_decoder_;

  // Test-only callback for hooking the completion of the next decode cycle.
  base::Closure decode_callback_for_testing_;

  // Whether audio or video decoder is in the process of prerolling.
  bool prerolling_;

  // Gathers and reports playback quality statistics to UMA.
  // Use pointer to enable replacement of this object for tests.
  std::unique_ptr<MediaStatistics> media_stat_;

  // Weak pointer passed to media decoder jobs for callbacks.
  base::WeakPtr<MediaSourcePlayer> weak_this_;
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaSourcePlayer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourcePlayer);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_SOURCE_PLAYER_H_
