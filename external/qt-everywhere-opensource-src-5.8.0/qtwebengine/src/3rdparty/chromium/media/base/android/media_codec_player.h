// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_PLAYER_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_PLAYER_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "media/base/android/demuxer_android.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_android.h"
#include "media/base/android/media_statistics.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/time_delta_interpolator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/android/scoped_java_surface.h"

// The MediaCodecPlayer class implements the media player by using Android's
// MediaCodec. It differs from MediaSourcePlayer in that it removes most
// processing away from the UI thread: it uses a dedicated Media thread to
// receive the data and to handle the commands.

// The player works as a state machine. Here are relationships between states:
//
//   [ Paused ] ------------------------                         (Any state)
//        |                             |                             |
//        |                             v                             v
//        | <------------------[ WaitingForConfig ]               [ Error ]
//        |
//        |
//        | <----------------------------------------------
//        |                                                |
//   [ Waiting for permission ]                            |
//        |                                                |
//        |                                                |
//        v                                                |
//   [ Prefetching ] -------------------                   |
//        |                             |                  |
//        |                             v                  |
//        | <-----------------[ WaitingForSurface ]        |
//        v                                                |
//   [ Playing ]                                           |
//        |                                                |
//        |                                                |
//        v                                                |
//   [ Stopping ] --------------------------------> [ WaitingForSeek ]


//  Events and actions for pause/resume workflow.
//  ---------------------------------------------
//
//                                         Start, no config:
//    ------------------------> [ Paused ] -----------------> [   Waiting   ]
//    |  StopDone:                                            [ for configs ]
//    |                            ^  |                              /
//    |                            |  |                             /
//    |                    Pause:  |  | Start w/config:            /
//    |        Permission denied:  |  |    requestPermission      /
//    |                            |  |                          /
//    |                            |  |                         /
//    |                            |  |                        /
//    |                            |  |                       / DemuxerConfigs:
//    |                            |  |                      / requestPermission
//    |                            |  |                     /
//    |                            |  |                    /
//    |                            |  v                   /
//    |                                                  /
//    |   ------------------> [ Waiting for ]  <--------/
//    |   |                   [ permission  ]
//    |   |                          |
//    |   |                          |
//    |   |                          | Permission granted:
//    |   |                          |     dec.Prefetch
//    |   |                          |
//    |   |                          |
//    |   |                          v
//    |   |
//    |   |                   [ Prefetching ]                  [   Waiting   ]
//    |   |                   [             ] -------------->  [ for surface ]
//    |   |                          |         PrefetchDone,         /
//    |   |                          |          no surface:         /
//    |   |                          |                             /
//    |   |                          |                            /
//    |   | StopDone w/              |                           /
//    |   | pending start:           | PrefetchDone:            /
//    |   |    dec.Prefetch          |   dec.Start             /
//    |   |                          |                        / SetSurface:
//    |   |                          |                       /     dec.Start
//    |   |                          |                      /
//    |   |                          v                     /
//    |   |                                               /
//    |   |                     [ Playing ]   <----------/
//    |   |
//    |   |                          |
//    |   |                          |
//    |   |                          | Pause: dec.RequestToStop
//    |   |                          |
//    |   |                          |
//    |   |                          v
//    |   |
//    ------------------------- [ Stopping ]


//   Events and actions for seek workflow.
//   -------------------------------------
//
//                   Seek:                   --       --
//                       demuxer.RequestSeek |         |
//  [   Paused    ] -----------------------> |         |
//  [             ] <----------------------- |         |--
//                      SeekDone:            |         |  |
//                                           |         |  |
//                                           |         |  |
//                                           |         |  |
//                                           |         |  | Start:
//                                           |         |  |   SetPendingStart
//                Seek: dec.Stop             |         |  |
//                      SetPendingStart      |         |  |
//                      demuxer.RequestSeek  |         |  |
//  [ Waiting for ] -----------------------> |         |  |
//  [ permission  ] <----------------------  |         |  | Pause:
//                   SeekDone                |         |  |   RemovePendingStart
//        |          w/pending start:        |         |  |
//        |            requestPermission     | Waiting |  |
//        |                                  |   for   |  | Seek:
//        |                                  |   seek  |  |   SetPendingSeek
//        |                                  |         |  |
//        |       Seek: dec.Stop             |         |  |
//        v             SetPendingStart      |         |  |
//                      demuxer.RequestSeek  |         |  |
//   [ Prefetching ] ----------------------> |         |  |
//                                           |         |  |
//        |                                  |         |  |
//        | PrefetchDone: dec.Start          |         |  |
//        |                                  |         |  | SeekDone
//        v                                  |         |  | w/pending seek:
//                                           |         |  | demuxer.RequestSeek
//  [   Playing   ]                          |         |  |
//                                           |         |  |
//        |                                  |         |<-
//        | Seek: SetPendingStart            |         |
//        |       SetPendingSeek             |         |
//        |       dec.RequestToStop          |         |
//        |                                  |         |
//        |                                  |         |
//        v                                  |         |
//                                           |         |
//  [  Stopping   ] -----------------------> |         |
//                  StopDone                 --       --
//                    w/pending seek:
//                      demuxer.RequestSeek

namespace media {

class AudioMediaCodecDecoder;
class VideoMediaCodecDecoder;

class MEDIA_EXPORT MediaCodecPlayer : public MediaPlayerAndroid,
                                      public DemuxerAndroidClient {
 public:
  // Typedefs for the notification callbacks
  typedef base::Callback<void(base::TimeDelta, const gfx::Size&)>
      MetadataChangedCallback;

  typedef base::Callback<void(base::TimeDelta, base::TimeTicks)>
      TimeUpdateCallback;

  typedef base::Callback<void(const base::TimeDelta& current_timestamp)>
      SeekDoneCallback;

  typedef base::Callback<void(int)> ErrorCallback;

  // For testing only.
  typedef base::Callback<void(DemuxerStream::Type,
                              base::TimeDelta,
                              base::TimeDelta)> DecodersTimeCallback;

  // For testing only.
  typedef base::Callback<void(DemuxerStream::Type)> CodecCreatedCallback;

  // Constructs a player with the given ID and demuxer. |manager| must outlive
  // the lifetime of this object.
  MediaCodecPlayer(
      int player_id,
      base::WeakPtr<MediaPlayerManager> manager,
      const OnDecoderResourcesReleasedCB& on_decoder_resources_released_cb,
      std::unique_ptr<DemuxerAndroid> demuxer,
      const GURL& frame_url,
      int media_session_id);
  ~MediaCodecPlayer() override;

  // A helper method that performs the media thread part of initialization.
  void Initialize();

  // MediaPlayerAndroid implementation.
  void DeleteOnCorrectThread() override;
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

  // For testing only.
  void SetDecodersTimeCallbackForTests(DecodersTimeCallback cb);
  void SetCodecCreatedCallbackForTests(CodecCreatedCallback cb);
  void SetAlwaysReconfigureForTests(DemuxerStream::Type type);
  bool IsPrerollingForTests(DemuxerStream::Type type) const;

 private:
  // The state machine states.
  enum PlayerState {
    kStatePaused,
    kStateWaitingForConfig,
    kStateWaitingForPermission,
    kStatePrefetching,
    kStatePlaying,
    kStateStopping,
    kStateWaitingForSurface,
    kStateWaitingForKey,
    kStateWaitingForMediaCrypto,
    kStateWaitingForSeek,
    kStateError,
  };

  enum StartStatus {
    kStartOk = 0,
    kStartBrowserSeekRequired,
    kStartCryptoRequired,
    kStartFailed,
  };

  // Cached values for the manager.
  struct MediaMetadata {
    base::TimeDelta duration;
    gfx::Size video_size;
  };

  // Information about current seek in progress.
  struct SeekInfo {
    const base::TimeDelta seek_time;
    const bool is_browser_seek;
    SeekInfo(base::TimeDelta time, bool browser_seek)
        : seek_time(time), is_browser_seek(browser_seek) {}
  };

  // MediaPlayerAndroid implementation.
  void UpdateEffectiveVolumeInternal(double effective_volume) override;

  // This method requests playback permission from the manager on UI
  // thread, passing total duration and whether the media has audio
  // track as arguments. The method posts the result to the media
  // thread.
  void RequestPermissionAndPostResult(base::TimeDelta duration,
                                      bool has_audio) override;

  // This method caches the data and calls manager's OnMediaMetadataChanged().
  void OnMediaMetadataChanged(base::TimeDelta duration,
                              const gfx::Size& video_size) override;

  // This method caches the current time and calls manager's OnTimeUpdate().
  void OnTimeUpdate(base::TimeDelta current_timestamp,
                    base::TimeTicks current_time_ticks) override;

  // Callback from manager
  void OnPermissionDecided(bool granted);

  // Callbacks from decoders
  void RequestDemuxerData(DemuxerStream::Type stream_type);
  void OnPrefetchDone();
  void OnPrerollDone();
  void OnDecoderDrained(DemuxerStream::Type type);
  void OnStopDone(DemuxerStream::Type type);
  void OnMissingKeyReported(DemuxerStream::Type type);
  void OnError();
  void OnStarvation(DemuxerStream::Type stream_type);
  void OnTimeIntervalUpdate(DemuxerStream::Type stream_type,
                            base::TimeDelta now_playing,
                            base::TimeDelta last_buffered,
                            bool postpone);

  // Callbacks from video decoder
  void OnVideoResolutionChanged(const gfx::Size& size);

  // Callbacks from MediaDrmBridge.
  void OnMediaCryptoReady(MediaDrmBridge::JavaObjectPtr media_crypto,
                          bool needs_protected_surface);
  void OnKeyAdded();

  // Operations called from the state machine.
  void SetState(PlayerState new_state);
  void SetPendingStart(bool need_to_start);
  bool HasPendingStart() const;
  void SetPendingSeek(base::TimeDelta timestamp);
  base::TimeDelta GetPendingSeek() const;
  void SetDemuxerConfigs(const DemuxerConfigs& configs);
  void RequestPlayPermission();
  void StartPrefetchDecoders();
  void StartPlaybackOrBrowserSeek();
  StartStatus StartPlaybackDecoders();
  StartStatus ConfigureDecoders();
  StartStatus MaybePrerollDecoders(bool* preroll_required);
  StartStatus StartDecoders();
  void StopDecoders();
  void RequestToStopDecoders();
  void RequestDemuxerSeek(base::TimeDelta seek_time,
                          bool is_browser_seek = false);
  void ReleaseDecoderResources();

  // Helper methods.
  void CreateDecoders();
  bool AudioFinished() const;
  bool VideoFinished() const;
  base::TimeDelta GetInterpolatedTime();

  static const char* AsString(PlayerState state);

  // Data.

  // Object for posting tasks on UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Major components: demuxer, audio and video decoders.
  std::unique_ptr<DemuxerAndroid> demuxer_;
  std::unique_ptr<AudioMediaCodecDecoder> audio_decoder_;
  std::unique_ptr<VideoMediaCodecDecoder> video_decoder_;

  // The state of the state machine.
  PlayerState state_;

  // Notification callbacks, they call MediaPlayerManager.
  TimeUpdateCallback time_update_cb_;
  base::Closure completion_cb_;
  base::Closure waiting_for_decryption_key_cb_;
  SeekDoneCallback seek_done_cb_;
  ErrorCallback error_cb_;

  // A callback that updates metadata cache and calls the manager.
  MetadataChangedCallback metadata_changed_cb_;

  // We call the base class' AttachListener() and DetachListener() methods on UI
  // thread with these callbacks.
  base::Closure attach_listener_cb_;
  base::Closure detach_listener_cb_;

  // Error callback is posted by decoders or by this class itself if we cannot
  // configure or start decoder.
  base::Closure internal_error_cb_;

  // Total duration reported by demuxer.
  base::TimeDelta duration_;

  // base::TickClock used by |interpolator_|.
  base::DefaultTickClock default_tick_clock_;

  // Tracks the most recent media time update and provides interpolated values
  // as playback progresses.
  TimeDeltaInterpolator interpolator_;

  // Pending data to be picked up by the upcoming state.
  gl::ScopedJavaSurface pending_surface_;
  bool pending_start_;
  base::TimeDelta pending_seek_;

  // Data associated with a seek in progress.
  std::unique_ptr<SeekInfo> seek_info_;

  // Configuration data for the manager, accessed on the UI thread.
  MediaMetadata metadata_cache_;

  // Cached current time, accessed on UI thread.
  base::TimeDelta current_time_cache_;

  // For testing only.
  DecodersTimeCallback decoders_time_cb_;

  // Holds a ref-count to the CDM to keep |media_crypto_| valid.
  scoped_refptr<MediaKeys> cdm_;

  MediaDrmBridge::JavaObjectPtr media_crypto_;

  int cdm_registration_id_;

  // The flag is set when the player receives the error from decoder that the
  // decoder needs a new decryption key. Cleared on starting the playback.
  bool key_is_required_;

  // The flag is set after the new encryption key is added to MediaDrm. Cleared
  // on starting the playback.
  bool key_is_added_;

  // Gathers and reports playback quality statistics to UMA.
  // Use pointer to enable replacement of this object for tests.
  std::unique_ptr<MediaStatistics> media_stat_;

  base::WeakPtr<MediaCodecPlayer> media_weak_this_;
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaCodecPlayer> media_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecPlayer);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_PLAYER_H_
