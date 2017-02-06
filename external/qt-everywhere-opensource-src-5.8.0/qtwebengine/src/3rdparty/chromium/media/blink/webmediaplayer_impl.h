// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBMEDIAPLAYER_IMPL_H_
#define MEDIA_BLINK_WEBMEDIAPLAYER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "media/base/media_tracks.h"
#include "media/base/pipeline_impl.h"
#include "media/base/renderer_factory.h"
#include "media/base/surface_manager.h"
#include "media/base/text_track.h"
#include "media/blink/buffered_data_source.h"
#include "media/blink/buffered_data_source_host_impl.h"
#include "media/blink/media_blink_export.h"
#include "media/blink/multibuffer_data_source.h"
#include "media/blink/video_frame_compositor.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/blink/webmediaplayer_util.h"
#include "media/filters/pipeline_controller.h"
#include "media/renderers/skcanvas_video_renderer.h"
#include "third_party/WebKit/public/platform/WebAudioSourceProvider.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)  // WMPI_CAST
// Delete this file when WMPI_CAST is no longer needed.
#include "media/blink/webmediaplayer_cast_android.h"
#endif

namespace blink {
class WebLocalFrame;
class WebMediaPlayerClient;
class WebMediaPlayerEncryptedMediaClient;
}

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace cc_blink {
class WebLayerImpl;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace media {
class AudioHardwareConfig;
class ChunkDemuxer;
class GpuVideoAcceleratorFactories;
class MediaLog;
class UrlIndex;
class VideoFrameCompositor;
class WebAudioSourceProviderImpl;
class WebMediaPlayerDelegate;
class WebTextTrackImpl;

// The canonical implementation of blink::WebMediaPlayer that's backed by
// Pipeline. Handles normal resource loading, Media Source, and
// Encrypted Media.
class MEDIA_BLINK_EXPORT WebMediaPlayerImpl
    : public NON_EXPORTED_BASE(blink::WebMediaPlayer),
      public NON_EXPORTED_BASE(WebMediaPlayerDelegate::Observer),
      public NON_EXPORTED_BASE(Pipeline::Client),
      public base::SupportsWeakPtr<WebMediaPlayerImpl> {
 public:
  // Constructs a WebMediaPlayer implementation using Chromium's media stack.
  // |delegate| may be null. |renderer_factory| must not be null.
  WebMediaPlayerImpl(
      blink::WebLocalFrame* frame,
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      base::WeakPtr<WebMediaPlayerDelegate> delegate,
      std::unique_ptr<RendererFactory> renderer_factory,
      linked_ptr<UrlIndex> url_index,
      const WebMediaPlayerParams& params);
  ~WebMediaPlayerImpl() override;

  void load(LoadType load_type,
            const blink::WebMediaPlayerSource& source,
            CORSMode cors_mode) override;

  // Playback controls.
  void play() override;
  void pause() override;
  bool supportsSave() const override;
  void seek(double seconds) override;
  void setRate(double rate) override;
  void setVolume(double volume) override;
  void setSinkId(const blink::WebString& sink_id,
                 const blink::WebSecurityOrigin& security_origin,
                 blink::WebSetSinkIdCallbacks* web_callback) override;
  void setPreload(blink::WebMediaPlayer::Preload preload) override;
  void setBufferingStrategy(
      blink::WebMediaPlayer::BufferingStrategy buffering_strategy) override;
  blink::WebTimeRanges buffered() const override;
  blink::WebTimeRanges seekable() const override;

  // paint() the current video frame into |canvas|. This is used to support
  // various APIs and functionalities, including but not limited to: <canvas>,
  // WebGL texImage2D, ImageBitmap, printing and capturing capabilities.
  void paint(blink::WebCanvas* canvas,
             const blink::WebRect& rect,
             unsigned char alpha,
             SkXfermode::Mode mode) override;

  // True if the loaded media has a playable video/audio track.
  bool hasVideo() const override;
  bool hasAudio() const override;

  // Dimensions of the video.
  blink::WebSize naturalSize() const override;

  // Getters of playback state.
  bool paused() const override;
  bool seeking() const override;
  double duration() const override;
  virtual double timelineOffset() const;
  double currentTime() const override;

  // Internal states of loading and network.
  // TODO(hclam): Ask the pipeline about the state rather than having reading
  // them from members which would cause race conditions.
  blink::WebMediaPlayer::NetworkState getNetworkState() const override;
  blink::WebMediaPlayer::ReadyState getReadyState() const override;

  blink::WebString getErrorMessage() override;
  bool didLoadingProgress() override;

  bool hasSingleSecurityOrigin() const override;
  bool didPassCORSAccessCheck() const override;

  double mediaTimeForTimeValue(double timeValue) const override;

  unsigned decodedFrameCount() const override;
  unsigned droppedFrameCount() const override;
  size_t audioDecodedByteCount() const override;
  size_t videoDecodedByteCount() const override;

  bool copyVideoTextureToPlatformTexture(gpu::gles2::GLES2Interface* gl,
                                         unsigned int texture,
                                         unsigned int internal_format,
                                         unsigned int type,
                                         bool premultiply_alpha,
                                         bool flip_y) override;

  blink::WebAudioSourceProvider* getAudioSourceProvider() override;

  void setContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm,
      blink::WebContentDecryptionModuleResult result) override;

  bool supportsOverlayFullscreenVideo() override;
  void enteredFullscreen() override;
  void exitedFullscreen() override;

  // WebMediaPlayerDelegate::Observer implementation.
  void OnHidden() override;
  void OnShown() override;
  void OnSuspendRequested(bool must_suspend) override;
  void OnPlay() override;
  void OnPause() override;
  void OnVolumeMultiplierUpdate(double multiplier) override;

#if defined(OS_ANDROID)  // WMPI_CAST
  bool isRemote() const override;
  void requestRemotePlayback() override;
  void requestRemotePlaybackControl() override;

  void SetMediaPlayerManager(
      RendererMediaPlayerManagerInterface* media_player_manager);
  void OnRemotePlaybackEnded();
  void OnDisconnectedFromRemoteDevice(double t);
  void SuspendForRemote();
  void DisplayCastFrameAfterSuspend(const scoped_refptr<VideoFrame>& new_frame,
                                    PipelineStatus status);
  gfx::Size GetCanvasSize() const;
  void SetDeviceScaleFactor(float scale_factor);
  void setPoster(const blink::WebURL& poster) override;
#endif

  // Called from WebMediaPlayerCast.
  // TODO(hubbe): WMPI_CAST make private.
  void OnPipelineSeeked(bool time_updated);

  // Distinct states that |delegate_| can be in.
  // TODO(sandersd): This should move into WebMediaPlayerDelegate.
  // (Public for testing.)
  enum class DelegateState {
    GONE,
    PLAYING,
    PAUSED,
    PAUSED_BUT_NOT_IDLE,
    ENDED,
  };

  // Playback state variables computed together in UpdatePlayState().
  // (Public for testing.)
  struct PlayState {
    DelegateState delegate_state;
    bool is_memory_reporting_enabled;
    bool is_suspended;
  };

 private:
  friend class WebMediaPlayerImplTest;

  void OnPipelineSuspended();
  void OnDemuxerOpened();

  // Pipeline::Client overrides.
  void OnError(PipelineStatus status) override;
  void OnEnded() override;
  void OnMetadata(PipelineMetadata metadata) override;
  void OnBufferingStateChange(BufferingState state) override;
  void OnDurationChange() override;
  void OnAddTextTrack(const TextTrackConfig& config,
                      const AddTextTrackDoneCB& done_cb) override;
  void OnWaitingForDecryptionKey() override;
  void OnVideoNaturalSizeChange(const gfx::Size& size) override;
  void OnVideoOpacityChange(bool opaque) override;

  // Actually seek. Avoids causing |should_notify_time_changed_| to be set when
  // |time_updated| is false.
  void DoSeek(base::TimeDelta time, bool time_updated);

  // Ask for the renderer to be restarted (destructed and recreated).
  void ScheduleRestart();

  // Called after |defer_load_cb_| has decided to allow the load. If
  // |defer_load_cb_| is null this is called immediately.
  void DoLoad(LoadType load_type,
              const blink::WebURL& url,
              CORSMode cors_mode);

  // Called after asynchronous initialization of a data source completed.
  void DataSourceInitialized(bool success);

  // Called when the data source is downloading or paused.
  void NotifyDownloading(bool is_downloading);

  // Called by SurfaceManager when a surface is created.
  void OnSurfaceCreated(int surface_id);

  // Called by GpuVideoDecoder on Android to request a surface to render to (if
  // necessary).
  void OnSurfaceRequested(const SurfaceCreatedCB& surface_created_cb);

  // Creates a Renderer via the |renderer_factory_|.
  std::unique_ptr<Renderer> CreateRenderer();

  // Finishes starting the pipeline due to a call to load().
  void StartPipeline();

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(blink::WebMediaPlayer::NetworkState state);
  void SetReadyState(blink::WebMediaPlayer::ReadyState state);

  // Gets the duration value reported by the pipeline.
  double GetPipelineDuration() const;

  // Called by VideoRendererImpl on its internal thread with the new frame to be
  // painted.
  void FrameReady(const scoped_refptr<VideoFrame>& frame);

  // Returns the current video frame from |compositor_|. Blocks until the
  // compositor can return the frame.
  scoped_refptr<VideoFrame> GetCurrentFrameFromCompositor();

  // Called when the demuxer encounters encrypted streams.
  void OnEncryptedMediaInitData(EmeInitDataType init_data_type,
                                const std::vector<uint8_t>& init_data);

  // Called when the FFmpegDemuxer encounters new media tracks. This is only
  // invoked when using FFmpegDemuxer, since MSE/ChunkDemuxer handle media
  // tracks separately in WebSourceBufferImpl.
  void OnFFmpegMediaTracksUpdated(std::unique_ptr<MediaTracks> tracks);

  // Sets |cdm_context| on the pipeline and fires |cdm_attached_cb| when done.
  // Parameter order is reversed for easy binding.
  void SetCdm(const CdmAttachedCB& cdm_attached_cb, CdmContext* cdm_context);

  // Called when a CDM has been attached to the |pipeline_|.
  void OnCdmAttached(bool success);

  // Inspects the current playback state and:
  //   - notifies |delegate_|,
  //   - toggles the memory usage reporting timer, and
  //   - toggles suspend/resume as necessary.
  //
  // This method should be called any time its dependent values change. These
  // are:
  //   - isRemote(),
  //   - hasVideo(),
  //   - delegate_->IsHidden(),
  //   - network_state_, ready_state_,
  //   - is_idle_, must_suspend_,
  //   - paused_, ended_,
  //   - pending_suspend_resume_cycle_.
  void UpdatePlayState();

  // Methods internal to UpdatePlayState().
  PlayState UpdatePlayState_ComputePlayState(bool is_remote,
                                             bool is_suspended,
                                             bool is_backgrounded);
  void SetDelegateState(DelegateState new_state);
  void SetMemoryReportingState(bool is_memory_reporting_enabled);
  void SetSuspendState(bool is_suspended);

  // Called at low frequency to tell external observers how much memory we're
  // using for video playback.  Called by |memory_usage_reporting_timer_|.
  // Memory usage reporting is done in two steps, because |demuxer_| must be
  // accessed on the media thread.
  void ReportMemoryUsage();
  void FinishMemoryUsageReport(int64_t demuxer_memory_usage);

  // Called during OnHidden() when we want a suspended player to enter the
  // paused state after some idle timeout.
  void ScheduleIdlePauseTimer();

  blink::WebLocalFrame* frame_;

  // The playback state last reported to |delegate_|, to avoid setting duplicate
  // states. (Which can have undesired effects like resetting the idle timer.)
  DelegateState delegate_state_;

  // Set when OnSuspendRequested() is called with |must_suspend| unset.
  bool is_idle_;

  // Set when OnSuspendRequested() is called with |must_suspend| set.
  bool must_suspend_;

  blink::WebMediaPlayer::NetworkState network_state_;
  blink::WebMediaPlayer::ReadyState ready_state_;
  blink::WebMediaPlayer::ReadyState highest_ready_state_;

  // Preload state for when |data_source_| is created after setPreload().
  BufferedDataSource::Preload preload_;

  // Buffering strategy for when |data_source_| is created after
  // setBufferingStrategy().
  BufferedDataSource::BufferingStrategy buffering_strategy_;

  // Task runner for posting tasks on Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<base::TaskRunner> worker_task_runner_;
  scoped_refptr<MediaLog> media_log_;

  // |pipeline_controller_| references |pipeline_| and therefore must be
  // constructed after and destructed before |pipeline_|.
  PipelineImpl pipeline_;
  PipelineController pipeline_controller_;

  // The LoadType passed in the |load_type| parameter of the load() call.
  LoadType load_type_;

  // Cache of metadata for answering hasAudio(), hasVideo(), and naturalSize().
  PipelineMetadata pipeline_metadata_;

  // Whether the video is known to be opaque or not.
  bool opaque_;

  // Playback state.
  //
  // TODO(scherkus): we have these because Pipeline favours the simplicity of a
  // single "playback rate" over worrying about paused/stopped etc...  It forces
  // all clients to manage the pause+playback rate externally, but is that
  // really a bad thing?
  //
  // TODO(scherkus): since SetPlaybackRate(0) is asynchronous and we don't want
  // to hang the render thread during pause(), we record the time at the same
  // time we pause and then return that value in currentTime().  Otherwise our
  // clock can creep forward a little bit while the asynchronous
  // SetPlaybackRate(0) is being executed.
  double playback_rate_;

  // Set while paused. |paused_time_| is only valid when |paused_| is true.
  bool paused_;
  base::TimeDelta paused_time_;

  // Set when starting, seeking, and resuming (all of which require a Pipeline
  // seek). |seek_time_| is only valid when |seeking_| is true.
  bool seeking_;
  base::TimeDelta seek_time_;

  // Set when doing a restart (a suspend and resume in sequence) of the pipeline
  // in order to destruct and reinitialize the decoders. This is separate from
  // |pending_resume_| and |pending_suspend_| because they can be elided in
  // certain cases, whereas for a restart they must happen.
  // TODO(sandersd,watk): Create a simpler interface for a pipeline restart.
  bool pending_suspend_resume_cycle_;

  // TODO(scherkus): Replace with an explicit ended signal to HTMLMediaElement,
  // see http://crbug.com/409280
  bool ended_;

  // Tracks whether to issue time changed notifications during buffering state
  // changes.
  bool should_notify_time_changed_;

  bool fullscreen_;

  // Whether the current decoder requires a restart on fullscreen transitions.
  bool decoder_requires_restart_for_fullscreen_;

  blink::WebMediaPlayerClient* client_;
  blink::WebMediaPlayerEncryptedMediaClient* encrypted_client_;

  // WebMediaPlayer notifies the |delegate_| of playback state changes using
  // |delegate_id_|; an id provided after registering with the delegate.  The
  // WebMediaPlayer may also receive directives (play, pause) from the delegate
  // via the WebMediaPlayerDelegate::Observer interface after registration.
  base::WeakPtr<WebMediaPlayerDelegate> delegate_;
  int delegate_id_;

  WebMediaPlayerParams::DeferLoadCB defer_load_cb_;
  WebMediaPlayerParams::Context3DCB context_3d_cb_;

  // Members for notifying upstream clients about internal memory usage.  The
  // |adjust_allocated_memory_cb_| must only be called on |main_task_runner_|.
  base::RepeatingTimer memory_usage_reporting_timer_;
  WebMediaPlayerParams::AdjustAllocatedMemoryCB adjust_allocated_memory_cb_;
  int64_t last_reported_memory_usage_;

  // Routes audio playback to either AudioRendererSink or WebAudio.
  scoped_refptr<WebAudioSourceProviderImpl> audio_source_provider_;

  bool supports_save_;

  // These two are mutually exclusive:
  //   |data_source_| is used for regular resource loads.
  //   |chunk_demuxer_| is used for Media Source resource loads.
  //
  // |demuxer_| will contain the appropriate demuxer based on which resource
  // load strategy we're using.
  std::unique_ptr<BufferedDataSourceInterface> data_source_;
  std::unique_ptr<Demuxer> demuxer_;
  ChunkDemuxer* chunk_demuxer_;

  BufferedDataSourceHostImpl buffered_data_source_host_;
  linked_ptr<UrlIndex> url_index_;

  // Video rendering members.
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  VideoFrameCompositor* compositor_;  // Deleted on |compositor_task_runner_|.
  SkCanvasVideoRenderer skcanvas_video_renderer_;

  // The compositor layer for displaying the video content when using composited
  // playback.
  std::unique_ptr<cc_blink::WebLayerImpl> video_weblayer_;

  std::unique_ptr<blink::WebContentDecryptionModuleResult> set_cdm_result_;

  // Whether a CDM has been successfully attached.
  bool is_cdm_attached_;

#if defined(OS_ANDROID)  // WMPI_CAST
  WebMediaPlayerCast cast_impl_;
#endif

  // The last volume received by setVolume() and the last volume multiplier from
  // OnVolumeMultiplierUpdate().  The multiplier is typical 1.0, but may be less
  // if the WebMediaPlayerDelegate has requested a volume reduction (ducking)
  // for a transient sound.  Playout volume is derived by volume * multiplier.
  double volume_;
  double volume_multiplier_;

  std::unique_ptr<RendererFactory> renderer_factory_;

  // For requesting surfaces on behalf of the Android H/W decoder in fullscreen.
  // This will be null everywhere but Android.
  SurfaceManager* surface_manager_;

  // For canceling ongoing surface creation requests when exiting fullscreen.
  base::CancelableCallback<void(int)> surface_created_cb_;

  // The current fullscreen surface id. Populated while in fullscreen once the
  // surface is created.
  int fullscreen_surface_id_;

  // If a surface is requested before it's finished being created, the request
  // is saved and satisfied once the surface is available.
  SurfaceCreatedCB pending_surface_request_cb_;

  // Suppresses calls to OnPipelineError() after destruction / shutdown has been
  // started; prevents us from spuriously logging errors that are transient or
  // unimportant.
  bool suppress_destruction_errors_;

  // State indicating if it's okay to suspend or not. Updated on the first time
  // OnSuspendRequested() is called. If the state is UNKNOWN, the current frame
  // from the compositor will be queried to see if suspend is supported; the
  // state will be set to YES or NO respectively if a frame is available.
  enum class CanSuspendState { UNKNOWN, YES, NO };
  CanSuspendState can_suspend_state_;

  // Called some-time after OnHidden() if the media was suspended in a playing
  // state as part of the call to OnHidden().
  base::OneShotTimer background_pause_timer_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBMEDIAPLAYER_IMPL_H_
