// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_RENDERER_IMPL_H_
#define MEDIA_RENDERERS_RENDERER_IMPL_H_

#include <memory>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "media/base/buffering_state.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioRenderer;
class DemuxerStreamProvider;
class TimeSource;
class VideoRenderer;
class WallClockTimeSource;

class MEDIA_EXPORT RendererImpl : public Renderer {
 public:
  // Renders audio/video streams using |audio_renderer| and |video_renderer|
  // provided. All methods except for GetMediaTime() run on the |task_runner|.
  // GetMediaTime() runs on the render main thread because it's part of JS sync
  // API.
  RendererImpl(const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
               std::unique_ptr<AudioRenderer> audio_renderer,
               std::unique_ptr<VideoRenderer> video_renderer);

  ~RendererImpl() final;

  // Renderer implementation.
  void Initialize(DemuxerStreamProvider* demuxer_stream_provider,
                  RendererClient* client,
                  const PipelineStatusCB& init_cb) final;
  void SetCdm(CdmContext* cdm_context,
              const CdmAttachedCB& cdm_attached_cb) final;
  void Flush(const base::Closure& flush_cb) final;
  void StartPlayingFrom(base::TimeDelta time) final;
  void SetPlaybackRate(double playback_rate) final;
  void SetVolume(float volume) final;
  base::TimeDelta GetMediaTime() final;
  bool HasAudio() final;
  bool HasVideo() final;

  // Helper functions for testing purposes. Must be called before Initialize().
  void DisableUnderflowForTesting();
  void EnableClocklessVideoPlaybackForTesting();
  void set_time_source_for_testing(TimeSource* time_source) {
    time_source_ = time_source;
  }
  void set_video_underflow_threshold_for_testing(base::TimeDelta threshold) {
    video_underflow_threshold_ = threshold;
  }

 private:
  class RendererClientInternal;

  enum State {
    STATE_UNINITIALIZED,
    STATE_INIT_PENDING_CDM,  // Initialization is waiting for the CDM to be set.
    STATE_INITIALIZING,      // Initializing audio/video renderers.
    STATE_FLUSHING,
    STATE_PLAYING,
    STATE_ERROR
  };

  bool GetWallClockTimes(const std::vector<base::TimeDelta>& media_timestamps,
                         std::vector<base::TimeTicks>* wall_clock_times);

  bool HasEncryptedStream();

  void FinishInitialization(PipelineStatus status);

  // Helper functions and callbacks for Initialize().
  void InitializeAudioRenderer();
  void OnAudioRendererInitializeDone(PipelineStatus status);
  void InitializeVideoRenderer();
  void OnVideoRendererInitializeDone(PipelineStatus status);

  // Helper functions and callbacks for Flush().
  void FlushAudioRenderer();
  void OnAudioRendererFlushDone();
  void FlushVideoRenderer();
  void OnVideoRendererFlushDone();

  // Callback executed by filters to update statistics.
  void OnStatisticsUpdate(const PipelineStatistics& stats);

  // Collection of callback methods and helpers for tracking changes in
  // buffering state and transition from paused/underflow states and playing
  // states.
  //
  // While in the kPlaying state:
  //   - A waiting to non-waiting transition indicates preroll has completed
  //     and StartPlayback() should be called
  //   - A non-waiting to waiting transition indicates underflow has occurred
  //     and PausePlayback() should be called
  void OnBufferingStateChange(DemuxerStream::Type type,
                              BufferingState new_buffering_state);
  bool WaitingForEnoughData() const;
  void PausePlayback();
  void StartPlayback();

  // Callbacks executed when a renderer has ended.
  void OnRendererEnded(DemuxerStream::Type type);
  bool PlaybackHasEnded() const;
  void RunEndedCallbackIfNeeded();

  // Callback executed when a runtime error happens.
  void OnError(PipelineStatus error);
  void OnWaitingForDecryptionKey();
  void OnVideoNaturalSizeChange(const gfx::Size& size);
  void OnVideoOpacityChange(bool opaque);

  State state_;

  // Task runner used to execute pipeline tasks.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DemuxerStreamProvider* demuxer_stream_provider_;
  RendererClient* client_;

  // Temporary callback used for Initialize() and Flush().
  PipelineStatusCB init_cb_;
  base::Closure flush_cb_;

  std::unique_ptr<RendererClientInternal> audio_renderer_client_;
  std::unique_ptr<RendererClientInternal> video_renderer_client_;
  std::unique_ptr<AudioRenderer> audio_renderer_;
  std::unique_ptr<VideoRenderer> video_renderer_;

  // Renderer-provided time source used to control playback.
  TimeSource* time_source_;
  std::unique_ptr<WallClockTimeSource> wall_clock_time_source_;
  bool time_ticking_;
  double playback_rate_;

  // The time to start playback from after starting/seeking has completed.
  base::TimeDelta start_time_;

  BufferingState audio_buffering_state_;
  BufferingState video_buffering_state_;

  // Whether we've received the audio/video ended events.
  bool audio_ended_;
  bool video_ended_;

  CdmContext* cdm_context_;
  CdmAttachedCB pending_cdm_attached_cb_;

  bool underflow_disabled_for_testing_;
  bool clockless_video_playback_enabled_for_testing_;

  // Used to defer underflow for video when audio is present.
  base::CancelableClosure deferred_underflow_cb_;

  // The amount of time to wait before declaring underflow if the video renderer
  // runs out of data but the audio renderer still has enough.
  base::TimeDelta video_underflow_threshold_;

  base::WeakPtr<RendererImpl> weak_this_;
  base::WeakPtrFactory<RendererImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererImpl);
};

}  // namespace media

#endif  // MEDIA_RENDERERS_RENDERER_IMPL_H_
