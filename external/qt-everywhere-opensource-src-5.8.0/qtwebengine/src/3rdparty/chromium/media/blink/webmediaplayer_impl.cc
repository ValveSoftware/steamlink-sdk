// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_impl.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/video_layer.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/text_renderer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/blink/texttrack_impl.h"
#include "media/blink/webaudiosourceprovider_impl.h"
#include "media/blink/webcontentdecryptionmodule_impl.h"
#include "media/blink/webinbandtexttrack_impl.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_util.h"
#include "media/blink/webmediasource_impl.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "third_party/WebKit/public/platform/WebEncryptedMediaTypes.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerEncryptedMediaClient.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerSource.h"
#include "third_party/WebKit/public/platform/WebMediaSource.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

using blink::WebCanvas;
using blink::WebMediaPlayer;
using blink::WebRect;
using blink::WebSize;
using blink::WebString;
using gpu::gles2::GLES2Interface;

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

namespace media {

namespace {

// Limits the range of playback rate.
//
// TODO(kylep): Revisit these.
//
// Vista has substantially lower performance than XP or Windows7.  If you speed
// up a video too much, it can't keep up, and rendering stops updating except on
// the time bar. For really high speeds, audio becomes a bottleneck and we just
// use up the data we have, which may not achieve the speed requested, but will
// not crash the tab.
//
// A very slow speed, ie 0.00000001x, causes the machine to lock up. (It seems
// like a busy loop). It gets unresponsive, although its not completely dead.
//
// Also our timers are not very accurate (especially for ogg), which becomes
// evident at low speeds and on Vista. Since other speeds are risky and outside
// the norms, we think 1/16x to 16x is a safe and useful range for now.
const double kMinRate = 0.0625;
const double kMaxRate = 16.0;

void SetSinkIdOnMediaThread(scoped_refptr<WebAudioSourceProviderImpl> sink,
                            const std::string& device_id,
                            const url::Origin& security_origin,
                            const OutputDeviceStatusCB& callback) {
  sink->SwitchOutputDevice(device_id, security_origin, callback);
}

bool IsBackgroundedSuspendEnabled() {
#if !defined(OS_ANDROID)
  // Suspend/Resume is only enabled by default on Android.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableMediaSuspend);
#else
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableMediaSuspend);
#endif
}

bool IsNetworkStateError(blink::WebMediaPlayer::NetworkState state) {
  bool result = state == blink::WebMediaPlayer::NetworkStateFormatError ||
                state == blink::WebMediaPlayer::NetworkStateNetworkError ||
                state == blink::WebMediaPlayer::NetworkStateDecodeError;
  DCHECK_EQ(state > blink::WebMediaPlayer::NetworkStateLoaded, result);
  return result;
}

gfx::Size GetRotatedVideoSize(VideoRotation rotation, gfx::Size natural_size) {
  if (rotation == VIDEO_ROTATION_90 || rotation == VIDEO_ROTATION_270)
    return gfx::Size(natural_size.height(), natural_size.width());
  return natural_size;
}

}  // namespace

class BufferedDataSourceHostImpl;

STATIC_ASSERT_ENUM(WebMediaPlayer::CORSModeUnspecified,
                   UrlData::CORS_UNSPECIFIED);
STATIC_ASSERT_ENUM(WebMediaPlayer::CORSModeAnonymous, UrlData::CORS_ANONYMOUS);
STATIC_ASSERT_ENUM(WebMediaPlayer::CORSModeUseCredentials,
                   UrlData::CORS_USE_CREDENTIALS);

#define BIND_TO_RENDER_LOOP(function) \
  (DCHECK(main_task_runner_->BelongsToCurrentThread()), \
  BindToCurrentLoop(base::Bind(function, AsWeakPtr())))

#define BIND_TO_RENDER_LOOP1(function, arg1) \
  (DCHECK(main_task_runner_->BelongsToCurrentThread()), \
  BindToCurrentLoop(base::Bind(function, AsWeakPtr(), arg1)))

WebMediaPlayerImpl::WebMediaPlayerImpl(
    blink::WebLocalFrame* frame,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    base::WeakPtr<WebMediaPlayerDelegate> delegate,
    std::unique_ptr<RendererFactory> renderer_factory,
    linked_ptr<UrlIndex> url_index,
    const WebMediaPlayerParams& params)
    : frame_(frame),
      delegate_state_(DelegateState::GONE),
      is_idle_(false),
      must_suspend_(false),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      highest_ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      preload_(BufferedDataSource::AUTO),
      buffering_strategy_(
          BufferedDataSourceInterface::BUFFERING_STRATEGY_NORMAL),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      media_task_runner_(params.media_task_runner()),
      worker_task_runner_(params.worker_task_runner()),
      media_log_(params.media_log()),
      pipeline_(media_task_runner_, media_log_.get()),
      pipeline_controller_(
          &pipeline_,
          base::Bind(&WebMediaPlayerImpl::CreateRenderer,
                     base::Unretained(this)),
          base::Bind(&WebMediaPlayerImpl::OnPipelineSeeked, AsWeakPtr()),
          base::Bind(&WebMediaPlayerImpl::OnPipelineSuspended, AsWeakPtr()),
          base::Bind(&WebMediaPlayerImpl::OnError, AsWeakPtr())),
      load_type_(LoadTypeURL),
      opaque_(false),
      playback_rate_(0.0),
      paused_(true),
      seeking_(false),
      pending_suspend_resume_cycle_(false),
      ended_(false),
      should_notify_time_changed_(false),
      fullscreen_(false),
      decoder_requires_restart_for_fullscreen_(false),
      client_(client),
      encrypted_client_(encrypted_client),
      delegate_(delegate),
      delegate_id_(0),
      defer_load_cb_(params.defer_load_cb()),
      context_3d_cb_(params.context_3d_cb()),
      adjust_allocated_memory_cb_(params.adjust_allocated_memory_cb()),
      last_reported_memory_usage_(0),
      supports_save_(true),
      chunk_demuxer_(NULL),
      url_index_(url_index),
      // Threaded compositing isn't enabled universally yet.
      compositor_task_runner_(params.compositor_task_runner()
                                  ? params.compositor_task_runner()
                                  : base::ThreadTaskRunnerHandle::Get()),
      compositor_(new VideoFrameCompositor(compositor_task_runner_)),
      is_cdm_attached_(false),
#if defined(OS_ANDROID)  // WMPI_CAST
      cast_impl_(this, client_, params.context_3d_cb()),
#endif
      volume_(1.0),
      volume_multiplier_(1.0),
      renderer_factory_(std::move(renderer_factory)),
      surface_manager_(params.surface_manager()),
      fullscreen_surface_id_(SurfaceManager::kNoSurfaceID),
      suppress_destruction_errors_(false),
      can_suspend_state_(CanSuspendState::UNKNOWN) {
  DCHECK(!adjust_allocated_memory_cb_.is_null());
  DCHECK(renderer_factory_);
  DCHECK(client_);

  if (delegate_)
    delegate_id_ = delegate_->AddObserver(this);

  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::WEBMEDIAPLAYER_CREATED));

  if (params.initial_cdm()) {
    SetCdm(base::Bind(&IgnoreCdmAttached),
           ToWebContentDecryptionModuleImpl(params.initial_cdm())
               ->GetCdmContext());
  }

  // TODO(xhwang): When we use an external Renderer, many methods won't work,
  // e.g. GetCurrentFrameFromCompositor(). See http://crbug.com/434861

  // Use the null sink if no sink was provided.
  audio_source_provider_ = new WebAudioSourceProviderImpl(
      params.audio_renderer_sink().get()
          ? params.audio_renderer_sink()
          : new NullAudioSink(media_task_runner_));
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  suppress_destruction_errors_ = true;
  if (delegate_) {
    delegate_->PlayerGone(delegate_id_);
    delegate_->RemoveObserver(delegate_id_);
  }

  // Pipeline must be stopped before it is destroyed.
  pipeline_.Stop();

  if (last_reported_memory_usage_)
    adjust_allocated_memory_cb_.Run(-last_reported_memory_usage_);

  // Destruct compositor resources in the proper order.
  client_->setWebLayer(nullptr);
  if (video_weblayer_)
    static_cast<cc::VideoLayer*>(video_weblayer_->layer())->StopUsingProvider();
  compositor_task_runner_->DeleteSoon(FROM_HERE, compositor_);

  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));
}

void WebMediaPlayerImpl::load(LoadType load_type,
                              const blink::WebMediaPlayerSource& source,
                              CORSMode cors_mode) {
  // Only URL or MSE blob URL is supported.
  DCHECK(source.isURL());
  blink::WebURL url = source.getAsURL();
  DVLOG(1) << __FUNCTION__ << "(" << load_type << ", " << url << ", "
           << cors_mode << ")";
  if (!defer_load_cb_.is_null()) {
    defer_load_cb_.Run(base::Bind(
        &WebMediaPlayerImpl::DoLoad, AsWeakPtr(), load_type, url, cors_mode));
    return;
  }
  DoLoad(load_type, url, cors_mode);
}

bool WebMediaPlayerImpl::supportsOverlayFullscreenVideo() {
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

void WebMediaPlayerImpl::enteredFullscreen() {
  fullscreen_ = true;
  if (surface_manager_) {
    surface_created_cb_.Reset(
        base::Bind(&WebMediaPlayerImpl::OnSurfaceCreated, AsWeakPtr()));
    surface_manager_->CreateFullscreenSurface(pipeline_metadata_.natural_size,
                                              surface_created_cb_.callback());
  }
  if (decoder_requires_restart_for_fullscreen_)
    ScheduleRestart();
}

void WebMediaPlayerImpl::exitedFullscreen() {
  fullscreen_ = false;
  surface_created_cb_.Cancel();
  fullscreen_surface_id_ = SurfaceManager::kNoSurfaceID;
  if (decoder_requires_restart_for_fullscreen_)
    ScheduleRestart();
}

void WebMediaPlayerImpl::DoLoad(LoadType load_type,
                                const blink::WebURL& url,
                                CORSMode cors_mode) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  GURL gurl(url);
  ReportMetrics(load_type, gurl, frame_->getSecurityOrigin());

  // Set subresource URL for crash reporting.
  base::debug::SetCrashKeyValue("subresource_url", gurl.spec());

  load_type_ = load_type;

  SetNetworkState(WebMediaPlayer::NetworkStateLoading);
  SetReadyState(WebMediaPlayer::ReadyStateHaveNothing);
  media_log_->AddEvent(media_log_->CreateLoadEvent(url.string().utf8()));

  // Media source pipelines can start immediately.
  if (load_type == LoadTypeMediaSource) {
    supports_save_ = false;
    StartPipeline();
  } else {
    // TODO(hubbe): This experiment is temporary and should be removed once
    // we have enough data to support the primacy of the new media cache.
    // See http://crbug.com/514719 for details.
    // Otherwise it's a regular request which requires resolving the URL first.
    if (base::FeatureList::IsEnabled(kUseNewMediaCache)) {
      // Remove this when MultiBufferDataSource becomes default.
      LOG(WARNING) << "Using MultibufferDataSource";
      data_source_.reset(new MultibufferDataSource(
          url, static_cast<UrlData::CORSMode>(cors_mode), main_task_runner_,
          url_index_, frame_, media_log_.get(), &buffered_data_source_host_,
          base::Bind(&WebMediaPlayerImpl::NotifyDownloading, AsWeakPtr())));
    } else {
      data_source_.reset(new BufferedDataSource(
          url, static_cast<BufferedResourceLoader::CORSMode>(cors_mode),
          main_task_runner_, frame_, media_log_.get(),
          &buffered_data_source_host_,
          base::Bind(&WebMediaPlayerImpl::NotifyDownloading, AsWeakPtr())));
    }
    data_source_->SetPreload(preload_);
    data_source_->SetBufferingStrategy(buffering_strategy_);
    data_source_->Initialize(
        base::Bind(&WebMediaPlayerImpl::DataSourceInitialized, AsWeakPtr()));
  }

#if defined(OS_ANDROID)  // WMPI_CAST
  cast_impl_.Initialize(url, frame_, delegate_id_);
#endif
}

void WebMediaPlayerImpl::play() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

#if defined(OS_ANDROID)  // WMPI_CAST
  if (isRemote()) {
    cast_impl_.play();
    return;
  }
#endif

  paused_ = false;
  is_idle_ = false;
  pipeline_.SetPlaybackRate(playback_rate_);

  if (data_source_)
    data_source_->MediaIsPlaying();

  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::PLAY));
  UpdatePlayState();
}

void WebMediaPlayerImpl::pause() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // We update the paused state even when casting, since we expect pause() to be
  // called when casting begins, and when we exit casting we should end up in a
  // paused state.
  paused_ = true;

#if defined(OS_ANDROID)  // WMPI_CAST
  if (isRemote()) {
    cast_impl_.pause();
    return;
  }
#endif

  pipeline_.SetPlaybackRate(0.0);

  // pause() may be called after playback has ended and the HTMLMediaElement
  // requires that currentTime() == duration() after ending.  We want to ensure
  // |paused_time_| matches currentTime() in this case or a future seek() may
  // incorrectly discard what it thinks is a seek to the existing time.
  paused_time_ =
      ended_ ? pipeline_.GetMediaDuration() : pipeline_.GetMediaTime();

  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::PAUSE));
  UpdatePlayState();
}

bool WebMediaPlayerImpl::supportsSave() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return supports_save_;
}

void WebMediaPlayerImpl::seek(double seconds) {
  DVLOG(1) << __FUNCTION__ << "(" << seconds << "s)";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DoSeek(base::TimeDelta::FromSecondsD(seconds), true);
}

void WebMediaPlayerImpl::DoSeek(base::TimeDelta time, bool time_updated) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

#if defined(OS_ANDROID)  // WMPI_CAST
  if (isRemote()) {
    cast_impl_.seek(time);
    return;
  }
#endif

  ReadyState old_state = ready_state_;
  if (ready_state_ > WebMediaPlayer::ReadyStateHaveMetadata)
    SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);

  // When paused, we know exactly what the current time is and can elide seeks
  // to it. However, there are two cases that are not elided:
  //   1) When the pipeline state is not stable.
  //      In this case we just let |pipeline_controller_| decide what to do, as
  //      it has complete information.
  //   2) For MSE.
  //      Because the buffers may have changed between seeks, MSE seeks are
  //      never elided.
  if (paused_ && pipeline_controller_.IsStable() && paused_time_ == time &&
      !chunk_demuxer_) {
    // If the ready state was high enough before, we can indicate that the seek
    // completed just by restoring it. Otherwise we will just wait for the real
    // ready state change to eventually happen.
    if (old_state == ReadyStateHaveEnoughData) {
      main_task_runner_->PostTask(
          FROM_HERE, base::Bind(&WebMediaPlayerImpl::OnBufferingStateChange,
                                AsWeakPtr(), BUFFERING_HAVE_ENOUGH));
    }
    return;
  }

  // TODO(sandersd): Ideally we would not clear the idle state if
  // |pipeline_controller_| can elide the seek.
  is_idle_ = false;
  ended_ = false;

  seeking_ = true;
  seek_time_ = time;
  if (paused_)
    paused_time_ = time;
  pipeline_controller_.Seek(time, time_updated);

  // This needs to be called after Seek() so that if a resume is triggered, it
  // is to the correct time.
  UpdatePlayState();
}

void WebMediaPlayerImpl::setRate(double rate) {
  DVLOG(1) << __FUNCTION__ << "(" << rate << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // TODO(kylep): Remove when support for negatives is added. Also, modify the
  // following checks so rewind uses reasonable values also.
  if (rate < 0.0)
    return;

  // Limit rates to reasonable values by clamping.
  if (rate != 0.0) {
    if (rate < kMinRate)
      rate = kMinRate;
    else if (rate > kMaxRate)
      rate = kMaxRate;
  }

  playback_rate_ = rate;
  if (!paused_) {
    pipeline_.SetPlaybackRate(rate);
    if (data_source_)
      data_source_->MediaPlaybackRateChanged(rate);
  }
}

void WebMediaPlayerImpl::setVolume(double volume) {
  DVLOG(1) << __FUNCTION__ << "(" << volume << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  volume_ = volume;
  pipeline_.SetVolume(volume_ * volume_multiplier_);
}

void WebMediaPlayerImpl::setSinkId(
    const blink::WebString& sink_id,
    const blink::WebSecurityOrigin& security_origin,
    blink::WebSetSinkIdCallbacks* web_callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  media::OutputDeviceStatusCB callback =
      media::ConvertToOutputDeviceStatusCB(web_callback);
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SetSinkIdOnMediaThread, audio_source_provider_,
                 sink_id.utf8(), static_cast<url::Origin>(security_origin),
                 callback));
}

STATIC_ASSERT_ENUM(WebMediaPlayer::PreloadNone, BufferedDataSource::NONE);
STATIC_ASSERT_ENUM(WebMediaPlayer::PreloadMetaData,
                   BufferedDataSource::METADATA);
STATIC_ASSERT_ENUM(WebMediaPlayer::PreloadAuto, BufferedDataSource::AUTO);

void WebMediaPlayerImpl::setPreload(WebMediaPlayer::Preload preload) {
  DVLOG(1) << __FUNCTION__ << "(" << preload << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  preload_ = static_cast<BufferedDataSource::Preload>(preload);
  if (data_source_)
    data_source_->SetPreload(preload_);
}

STATIC_ASSERT_ENUM(WebMediaPlayer::BufferingStrategy::Normal,
                   BufferedDataSource::BUFFERING_STRATEGY_NORMAL);
STATIC_ASSERT_ENUM(WebMediaPlayer::BufferingStrategy::Aggressive,
                   BufferedDataSource::BUFFERING_STRATEGY_AGGRESSIVE);

void WebMediaPlayerImpl::setBufferingStrategy(
    WebMediaPlayer::BufferingStrategy buffering_strategy) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

#if defined(OS_ANDROID)
  // We disallow aggressive buffering on Android since it matches the behavior
  // of the platform media player and may have data usage penalties.
  // TODO(dalecurtis, hubbe): We should probably stop using "pause-and-buffer"
  // everywhere. See http://crbug.com/594669 for more details.
  buffering_strategy_ = BufferedDataSource::BUFFERING_STRATEGY_NORMAL;
#else
  buffering_strategy_ =
      static_cast<BufferedDataSource::BufferingStrategy>(buffering_strategy);
#endif

  if (data_source_)
    data_source_->SetBufferingStrategy(buffering_strategy_);
}

bool WebMediaPlayerImpl::hasVideo() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_metadata_.has_video;
}

bool WebMediaPlayerImpl::hasAudio() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_metadata_.has_audio;
}

blink::WebSize WebMediaPlayerImpl::naturalSize() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return blink::WebSize(pipeline_metadata_.natural_size);
}

bool WebMediaPlayerImpl::paused() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

#if defined(OS_ANDROID)  // WMPI_CAST
  if (isRemote())
    return cast_impl_.paused();
#endif

  return pipeline_.GetPlaybackRate() == 0.0f;
}

bool WebMediaPlayerImpl::seeking() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing)
    return false;

  return seeking_;
}

double WebMediaPlayerImpl::duration() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing)
    return std::numeric_limits<double>::quiet_NaN();

  return GetPipelineDuration();
}

double WebMediaPlayerImpl::timelineOffset() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (pipeline_metadata_.timeline_offset.is_null())
    return std::numeric_limits<double>::quiet_NaN();

  return pipeline_metadata_.timeline_offset.ToJsTime();
}

double WebMediaPlayerImpl::currentTime() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::ReadyStateHaveNothing);

  // TODO(scherkus): Replace with an explicit ended signal to HTMLMediaElement,
  // see http://crbug.com/409280
  if (ended_)
    return duration();

  if (seeking())
    return seek_time_.InSecondsF();

#if defined(OS_ANDROID)  // WMPI_CAST
  if (isRemote())
    return cast_impl_.currentTime();
#endif

  if (paused_)
    return paused_time_.InSecondsF();

  return pipeline_.GetMediaTime().InSecondsF();
}

WebMediaPlayer::NetworkState WebMediaPlayerImpl::getNetworkState() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerImpl::getReadyState() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return ready_state_;
}

blink::WebString WebMediaPlayerImpl::getErrorMessage() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return blink::WebString::fromUTF8(media_log_->GetLastErrorMessage());
}

blink::WebTimeRanges WebMediaPlayerImpl::buffered() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  Ranges<base::TimeDelta> buffered_time_ranges =
      pipeline_.GetBufferedTimeRanges();

  const base::TimeDelta duration = pipeline_.GetMediaDuration();
  if (duration != kInfiniteDuration()) {
    buffered_data_source_host_.AddBufferedTimeRanges(
        &buffered_time_ranges, duration);
  }
  return ConvertToWebTimeRanges(buffered_time_ranges);
}

blink::WebTimeRanges WebMediaPlayerImpl::seekable() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (ready_state_ < WebMediaPlayer::ReadyStateHaveMetadata)
    return blink::WebTimeRanges();

  const double seekable_end = duration();

  // Allow a special exception for seeks to zero for streaming sources with a
  // finite duration; this allows looping to work.
  const bool allow_seek_to_zero = data_source_ && data_source_->IsStreaming() &&
                                  std::isfinite(seekable_end);

  // TODO(dalecurtis): Technically this allows seeking on media which return an
  // infinite duration so long as DataSource::IsStreaming() is false.  While not
  // expected, disabling this breaks semi-live players, http://crbug.com/427412.
  const blink::WebTimeRange seekable_range(
      0.0, allow_seek_to_zero ? 0.0 : seekable_end);
  return blink::WebTimeRanges(&seekable_range, 1);
}

bool WebMediaPlayerImpl::didLoadingProgress() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  bool pipeline_progress = pipeline_.DidLoadingProgress();
  bool data_progress = buffered_data_source_host_.DidLoadingProgress();
  return pipeline_progress || data_progress;
}

void WebMediaPlayerImpl::paint(blink::WebCanvas* canvas,
                               const blink::WebRect& rect,
                               unsigned char alpha,
                               SkXfermode::Mode mode) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl:paint");

  if (is_cdm_attached_)
    return;

  scoped_refptr<VideoFrame> video_frame = GetCurrentFrameFromCompositor();

  gfx::Rect gfx_rect(rect);
  Context3D context_3d;
  if (video_frame.get() && video_frame->HasTextures()) {
    if (!context_3d_cb_.is_null())
      context_3d = context_3d_cb_.Run();
    if (!context_3d.gl)
      return;  // Unable to get/create a shared main thread context.
    if (!context_3d.gr_context)
      return;  // The context has been lost since and can't setup a GrContext.
  }
  skcanvas_video_renderer_.Paint(video_frame, canvas, gfx::RectF(gfx_rect),
                                 alpha, mode, pipeline_metadata_.video_rotation,
                                 context_3d);
}

bool WebMediaPlayerImpl::hasSingleSecurityOrigin() const {
  if (data_source_)
    return data_source_->HasSingleOrigin();
  return true;
}

bool WebMediaPlayerImpl::didPassCORSAccessCheck() const {
  if (data_source_)
    return data_source_->DidPassCORSAccessCheck();
  return false;
}

double WebMediaPlayerImpl::mediaTimeForTimeValue(double timeValue) const {
  return base::TimeDelta::FromSecondsD(timeValue).InSecondsF();
}

unsigned WebMediaPlayerImpl::decodedFrameCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  PipelineStatistics stats = pipeline_.GetStatistics();
  return stats.video_frames_decoded;
}

unsigned WebMediaPlayerImpl::droppedFrameCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  PipelineStatistics stats = pipeline_.GetStatistics();
  return stats.video_frames_dropped;
}

size_t WebMediaPlayerImpl::audioDecodedByteCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  PipelineStatistics stats = pipeline_.GetStatistics();
  return stats.audio_bytes_decoded;
}

size_t WebMediaPlayerImpl::videoDecodedByteCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  PipelineStatistics stats = pipeline_.GetStatistics();
  return stats.video_bytes_decoded;
}

bool WebMediaPlayerImpl::copyVideoTextureToPlatformTexture(
    gpu::gles2::GLES2Interface* gl,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int type,
    bool premultiply_alpha,
    bool flip_y) {
  TRACE_EVENT0("media", "WebMediaPlayerImpl:copyVideoTextureToPlatformTexture");

  scoped_refptr<VideoFrame> video_frame = GetCurrentFrameFromCompositor();

  if (!video_frame.get() || !video_frame->HasTextures()) {
    return false;
  }

  Context3D context_3d;
  if (!context_3d_cb_.is_null())
    context_3d = context_3d_cb_.Run();
  return skcanvas_video_renderer_.CopyVideoFrameTexturesToGLTexture(
      context_3d, gl, video_frame.get(), texture, internal_format, type,
      premultiply_alpha, flip_y);
}

void WebMediaPlayerImpl::setContentDecryptionModule(
    blink::WebContentDecryptionModule* cdm,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Once the CDM is set it can't be cleared as there may be frames being
  // decrypted on other threads. So fail this request.
  // http://crbug.com/462365#c7.
  if (!cdm) {
    result.completeWithError(
        blink::WebContentDecryptionModuleExceptionInvalidStateError, 0,
        "The existing MediaKeys object cannot be removed at this time.");
    return;
  }

  // Create a local copy of |result| to avoid problems with the callback
  // getting passed to the media thread and causing |result| to be destructed
  // on the wrong thread in some failure conditions. Blink should prevent
  // multiple simultaneous calls.
  DCHECK(!set_cdm_result_);
  set_cdm_result_.reset(new blink::WebContentDecryptionModuleResult(result));

  SetCdm(BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnCdmAttached),
         ToWebContentDecryptionModuleImpl(cdm)->GetCdmContext());
}

void WebMediaPlayerImpl::OnEncryptedMediaInitData(
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data) {
  DCHECK(init_data_type != EmeInitDataType::UNKNOWN);

  // TODO(xhwang): Update this UMA name. https://crbug.com/589251
  UMA_HISTOGRAM_COUNTS("Media.EME.NeedKey", 1);

  encrypted_client_->encrypted(
      ConvertToWebInitDataType(init_data_type), init_data.data(),
      base::saturated_cast<unsigned int>(init_data.size()));
}

void WebMediaPlayerImpl::OnFFmpegMediaTracksUpdated(
    std::unique_ptr<MediaTracks> tracks) {
  // For MSE/chunk_demuxer case the media track updates are handled by
  // WebSourceBufferImpl.
  DCHECK(demuxer_.get());
  DCHECK(!chunk_demuxer_);

  // Report the media track information to blink.
  for (const auto& track : tracks->tracks()) {
    if (track->type() == MediaTrack::Audio) {
      client_->addAudioTrack(blink::WebString::fromUTF8(track->id()),
                             blink::WebMediaPlayerClient::AudioTrackKindMain,
                             blink::WebString::fromUTF8(track->label()),
                             blink::WebString::fromUTF8(track->language()),
                             /*enabled*/ true);
    } else if (track->type() == MediaTrack::Video) {
      client_->addVideoTrack(blink::WebString::fromUTF8(track->id()),
                             blink::WebMediaPlayerClient::VideoTrackKindMain,
                             blink::WebString::fromUTF8(track->label()),
                             blink::WebString::fromUTF8(track->language()),
                             /*selected*/ true);
    } else {
      // Text tracks are not supported through this code path yet.
      NOTREACHED();
    }
  }
}

void WebMediaPlayerImpl::SetCdm(const CdmAttachedCB& cdm_attached_cb,
                                CdmContext* cdm_context) {
  if (!cdm_context) {
    cdm_attached_cb.Run(false);
    return;
  }

  // If CDM initialization succeeded, tell the pipeline about it.
  pipeline_.SetCdm(cdm_context, cdm_attached_cb);
}

void WebMediaPlayerImpl::OnCdmAttached(bool success) {
  if (success) {
    set_cdm_result_->complete();
    set_cdm_result_.reset();
    is_cdm_attached_ = true;
    return;
  }

  set_cdm_result_->completeWithError(
      blink::WebContentDecryptionModuleExceptionNotSupportedError, 0,
      "Unable to set MediaKeys object");
  set_cdm_result_.reset();
}

void WebMediaPlayerImpl::OnPipelineSeeked(bool time_updated) {
  seeking_ = false;
  seek_time_ = base::TimeDelta();
  if (paused_) {
#if defined(OS_ANDROID)  // WMPI_CAST
    if (isRemote()) {
      paused_time_ = base::TimeDelta::FromSecondsD(cast_impl_.currentTime());
    } else {
      paused_time_ = pipeline_.GetMediaTime();
    }
#else
    paused_time_ = pipeline_.GetMediaTime();
#endif
  }
  if (time_updated)
    should_notify_time_changed_ = true;
}

void WebMediaPlayerImpl::OnPipelineSuspended() {
#if defined(OS_ANDROID)
  if (isRemote()) {
    scoped_refptr<VideoFrame> frame = cast_impl_.GetCastingBanner();
    if (frame)
      compositor_->PaintSingleFrame(frame);
  }
#endif

  // If we're not in an aggressive buffering state, tell the data source we have
  // enough data so that it may release the connection.
  if (buffering_strategy_ !=
      BufferedDataSource::BUFFERING_STRATEGY_AGGRESSIVE) {
    if (data_source_)
      data_source_->OnBufferingHaveEnough(true);
  }

  ReportMemoryUsage();

  if (pending_suspend_resume_cycle_) {
    pending_suspend_resume_cycle_ = false;
    UpdatePlayState();
  }
}

void WebMediaPlayerImpl::OnDemuxerOpened() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  client_->mediaSourceOpened(
      new WebMediaSourceImpl(chunk_demuxer_, media_log_));
}

void WebMediaPlayerImpl::OnError(PipelineStatus status) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(status, PIPELINE_OK);

  if (suppress_destruction_errors_)
    return;

  ReportPipelineError(load_type_, frame_->getSecurityOrigin(), status);
  media_log_->AddEvent(media_log_->CreatePipelineErrorEvent(status));

  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    SetNetworkState(WebMediaPlayer::NetworkStateFormatError);
  } else {
    SetNetworkState(PipelineErrorToNetworkState(status));
  }

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnEnded() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Ignore state changes until we've completed all outstanding operations.
  if (!pipeline_controller_.IsStable())
    return;

  ended_ = true;
  client_->timeChanged();

  // We don't actually want this to run until |client_| calls seek() or pause(),
  // but that should have already happened in timeChanged() and so this is
  // expected to be a no-op.
  UpdatePlayState();
}

void WebMediaPlayerImpl::OnMetadata(PipelineMetadata metadata) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  pipeline_metadata_ = metadata;

  UMA_HISTOGRAM_ENUMERATION("Media.VideoRotation", metadata.video_rotation,
                            VIDEO_ROTATION_MAX + 1);
  SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);

  if (hasVideo()) {
    pipeline_metadata_.natural_size = GetRotatedVideoSize(
        pipeline_metadata_.video_rotation, pipeline_metadata_.natural_size);

    if (fullscreen_ && surface_manager_)
      surface_manager_->NaturalSizeChanged(pipeline_metadata_.natural_size);

    DCHECK(!video_weblayer_);
    video_weblayer_.reset(new cc_blink::WebLayerImpl(cc::VideoLayer::Create(
        compositor_, pipeline_metadata_.video_rotation)));
    video_weblayer_->layer()->SetContentsOpaque(opaque_);
    video_weblayer_->SetContentsOpaqueIsFixed(true);
    client_->setWebLayer(video_weblayer_.get());
  }

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnBufferingStateChange(BufferingState state) {
  DVLOG(1) << __FUNCTION__ << "(" << state << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Ignore buffering state changes until we've completed all outstanding
  // operations.
  if (!pipeline_controller_.IsStable())
    return;

  // TODO(scherkus): Handle other buffering states when Pipeline starts using
  // them and translate them ready state changes http://crbug.com/144683
  DCHECK_EQ(state, BUFFERING_HAVE_ENOUGH);
  SetReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);

  // Let the DataSource know we have enough data. It may use this information to
  // release unused network connections.
  if (data_source_)
    data_source_->OnBufferingHaveEnough(false);

  // Blink expects a timeChanged() in response to a seek().
  if (should_notify_time_changed_)
    client_->timeChanged();

  // Once we have enough, start reporting the total memory usage. We'll also
  // report once playback starts.
  ReportMemoryUsage();

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnDurationChange() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // TODO(sandersd): We should call delegate_->DidPlay() with the new duration,
  // especially if it changed from  <5s to >5s.
  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing)
    return;

  client_->durationChanged();
}

void WebMediaPlayerImpl::OnAddTextTrack(const TextTrackConfig& config,
                                        const AddTextTrackDoneCB& done_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  const WebInbandTextTrackImpl::Kind web_kind =
      static_cast<WebInbandTextTrackImpl::Kind>(config.kind());
  const blink::WebString web_label =
      blink::WebString::fromUTF8(config.label());
  const blink::WebString web_language =
      blink::WebString::fromUTF8(config.language());
  const blink::WebString web_id =
      blink::WebString::fromUTF8(config.id());

  std::unique_ptr<WebInbandTextTrackImpl> web_inband_text_track(
      new WebInbandTextTrackImpl(web_kind, web_label, web_language, web_id));

  std::unique_ptr<media::TextTrack> text_track(new TextTrackImpl(
      main_task_runner_, client_, std::move(web_inband_text_track)));

  done_cb.Run(std::move(text_track));
}

void WebMediaPlayerImpl::OnWaitingForDecryptionKey() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  encrypted_client_->didBlockPlaybackWaitingForKey();
  // TODO(jrummell): didResumePlaybackBlockedForKey() should only be called
  // when a key has been successfully added (e.g. OnSessionKeysChange() with
  // |has_additional_usable_key| = true). http://crbug.com/461903
  encrypted_client_->didResumePlaybackBlockedForKey();
}

void WebMediaPlayerImpl::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::ReadyStateHaveNothing);

  gfx::Size rotated_size =
      GetRotatedVideoSize(pipeline_metadata_.video_rotation, size);

  if (rotated_size == pipeline_metadata_.natural_size)
    return;

  TRACE_EVENT0("media", "WebMediaPlayerImpl::OnNaturalSizeChanged");
  media_log_->AddEvent(media_log_->CreateVideoSizeSetEvent(
      rotated_size.width(), rotated_size.height()));

  if (fullscreen_ && surface_manager_)
    surface_manager_->NaturalSizeChanged(rotated_size);

  pipeline_metadata_.natural_size = rotated_size;
  client_->sizeChanged();
}

void WebMediaPlayerImpl::OnVideoOpacityChange(bool opaque) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::ReadyStateHaveNothing);

  opaque_ = opaque;
  // Modify content opaqueness of cc::Layer directly so that
  // SetContentsOpaqueIsFixed is ignored.
  if (video_weblayer_)
    video_weblayer_->layer()->SetContentsOpaque(opaque_);
}

void WebMediaPlayerImpl::OnHidden() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  UpdatePlayState();

  // Schedule suspended playing media to be paused if the user doesn't come back
  // to it within some timeout period to avoid any autoplay surprises.
  ScheduleIdlePauseTimer();
}

void WebMediaPlayerImpl::OnShown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  must_suspend_ = false;
  background_pause_timer_.Stop();
  UpdatePlayState();
}

void WebMediaPlayerImpl::OnSuspendRequested(bool must_suspend) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (must_suspend) {
    must_suspend_ = true;
  } else {
    // TODO(sandersd): Remove this when idleness is separate from play state.
    if (delegate_state_ == DelegateState::PAUSED_BUT_NOT_IDLE)
      return;
    is_idle_ = true;
  }

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnPlay() {
  play();
  client_->playbackStateChanged();
}

void WebMediaPlayerImpl::OnPause() {
  pause();
  client_->playbackStateChanged();
}

void WebMediaPlayerImpl::OnVolumeMultiplierUpdate(double multiplier) {
  volume_multiplier_ = multiplier;
  setVolume(volume_);
}

void WebMediaPlayerImpl::ScheduleRestart() {
  // TODO(watk): All restart logic should be moved into PipelineController.
  if (pipeline_.IsRunning() && !pipeline_controller_.IsPipelineSuspended()) {
    pending_suspend_resume_cycle_ = true;
    UpdatePlayState();
  }
}

#if defined(OS_ANDROID)  // WMPI_CAST
bool WebMediaPlayerImpl::isRemote() const {
  return cast_impl_.isRemote();
}

void WebMediaPlayerImpl::SetMediaPlayerManager(
    RendererMediaPlayerManagerInterface* media_player_manager) {
  cast_impl_.SetMediaPlayerManager(media_player_manager);
}

void WebMediaPlayerImpl::requestRemotePlayback() {
  cast_impl_.requestRemotePlayback();
}

void WebMediaPlayerImpl::requestRemotePlaybackControl() {
  cast_impl_.requestRemotePlaybackControl();
}

void WebMediaPlayerImpl::OnRemotePlaybackEnded() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  ended_ = true;
  client_->timeChanged();
}

void WebMediaPlayerImpl::OnDisconnectedFromRemoteDevice(double t) {
  DoSeek(base::TimeDelta::FromSecondsD(t), false);

  // We already told the delegate we're paused when remoting started.
  client_->playbackStateChanged();
  client_->disconnectedFromRemoteDevice();

  UpdatePlayState();
}

void WebMediaPlayerImpl::SuspendForRemote() {
  if (pipeline_controller_.IsPipelineSuspended()) {
    scoped_refptr<VideoFrame> frame = cast_impl_.GetCastingBanner();
    if (frame)
      compositor_->PaintSingleFrame(frame);
  }

  UpdatePlayState();
}

gfx::Size WebMediaPlayerImpl::GetCanvasSize() const {
  if (!video_weblayer_)
    return pipeline_metadata_.natural_size;

  return video_weblayer_->bounds();
}

void WebMediaPlayerImpl::SetDeviceScaleFactor(float scale_factor) {
  cast_impl_.SetDeviceScaleFactor(scale_factor);
}

void WebMediaPlayerImpl::setPoster(const blink::WebURL& poster) {
  cast_impl_.setPoster(poster);
}
#endif  // defined(OS_ANDROID)  // WMPI_CAST

void WebMediaPlayerImpl::DataSourceInitialized(bool success) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

#if defined(OS_ANDROID)
  // We can't play HLS URLs with WebMediaPlayerImpl, so in cases where they are
  // encountered, instruct the HTML media element to create a new WebMediaPlayer
  // instance with the correct URL to trigger WebMediaPlayerAndroid creation.
  //
  // TODO(tguilbert): Remove this code path once we have the ability to host a
  // MediaPlayer within a Mojo media renderer.  http://crbug.com/580626
  if (data_source_) {
    const GURL url_after_redirects = data_source_->GetUrlAfterRedirects();
    if (MediaCodecUtil::IsHLSPath(url_after_redirects)) {
      client_->requestReload(url_after_redirects);
      // |this| may be destructed, do nothing after this.
      return;
    }
  }
#endif

  if (!success) {
    SetNetworkState(WebMediaPlayer::NetworkStateFormatError);

    // Not really necessary, since the pipeline was never started, but it at
    // least this makes sure that the error handling code is in sync.
    UpdatePlayState();

    return;
  }

  StartPipeline();
}

void WebMediaPlayerImpl::NotifyDownloading(bool is_downloading) {
  DVLOG(1) << __FUNCTION__;
  if (!is_downloading && network_state_ == WebMediaPlayer::NetworkStateLoading)
    SetNetworkState(WebMediaPlayer::NetworkStateIdle);
  else if (is_downloading && network_state_ == WebMediaPlayer::NetworkStateIdle)
    SetNetworkState(WebMediaPlayer::NetworkStateLoading);
  media_log_->AddEvent(
      media_log_->CreateBooleanEvent(
          MediaLogEvent::NETWORK_ACTIVITY_SET,
          "is_downloading_data", is_downloading));
}

void WebMediaPlayerImpl::OnSurfaceCreated(int surface_id) {
  fullscreen_surface_id_ = surface_id;
  if (!pending_surface_request_cb_.is_null())
    base::ResetAndReturn(&pending_surface_request_cb_).Run(surface_id);
}

// TODO(watk): Move this state management out of WMPI.
void WebMediaPlayerImpl::OnSurfaceRequested(
    const SurfaceCreatedCB& surface_created_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(surface_manager_);

  // A null callback indicates that the decoder is going away.
  if (surface_created_cb.is_null()) {
    decoder_requires_restart_for_fullscreen_ = false;
    pending_surface_request_cb_.Reset();
    return;
  }

  // If we're getting a surface request it means GVD is initializing, so until
  // we get a null surface request, GVD is the active decoder. While that's the
  // case we should restart the pipeline on fullscreen transitions so that when
  // we create a new GVD it will request a surface again and get the right kind
  // of surface for the fullscreen state.
  // TODO(watk): Don't require a pipeline restart to switch surfaces for
  // cases where it isn't necessary.
  decoder_requires_restart_for_fullscreen_ = true;
  if (fullscreen_) {
    if (fullscreen_surface_id_ != SurfaceManager::kNoSurfaceID)
      surface_created_cb.Run(fullscreen_surface_id_);
    else
      pending_surface_request_cb_ = surface_created_cb;
  } else {
    // Tell the decoder to create its own surface.
    surface_created_cb.Run(SurfaceManager::kNoSurfaceID);
  }
}

std::unique_ptr<Renderer> WebMediaPlayerImpl::CreateRenderer() {
  RequestSurfaceCB request_surface_cb;
#if defined(OS_ANDROID)
  request_surface_cb =
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnSurfaceRequested);
#endif
  return renderer_factory_->CreateRenderer(
      media_task_runner_, worker_task_runner_, audio_source_provider_.get(),
      compositor_, request_surface_cb);
}

void WebMediaPlayerImpl::StartPipeline() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb =
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnEncryptedMediaInitData);

  // Figure out which demuxer to use.
  if (load_type_ != LoadTypeMediaSource) {
    DCHECK(!chunk_demuxer_);
    DCHECK(data_source_);

#if !defined(MEDIA_DISABLE_FFMPEG)
    Demuxer::MediaTracksUpdatedCB media_tracks_updated_cb =
        BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnFFmpegMediaTracksUpdated);

    demuxer_.reset(new FFmpegDemuxer(media_task_runner_, data_source_.get(),
                                     encrypted_media_init_data_cb,
                                     media_tracks_updated_cb, media_log_));
#else
    OnError(PipelineStatus::DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
#endif
  } else {
    DCHECK(!chunk_demuxer_);
    DCHECK(!data_source_);

    chunk_demuxer_ = new ChunkDemuxer(
        BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnDemuxerOpened),
        encrypted_media_init_data_cb, media_log_, true);
    demuxer_.reset(chunk_demuxer_);
  }

  // TODO(sandersd): FileSystem objects may also be non-static, but due to our
  // caching layer such situations are broken already. http://crbug.com/593159
  bool is_static = !chunk_demuxer_;

  // ... and we're ready to go!
  seeking_ = true;

  // TODO(sandersd): On Android, defer Start() if the tab is not visible.
  bool is_streaming = (data_source_ && data_source_->IsStreaming());
  pipeline_controller_.Start(demuxer_.get(), this, is_streaming, is_static);
}

void WebMediaPlayerImpl::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DVLOG(1) << __FUNCTION__ << "(" << state << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  client_->networkStateChanged();
}

void WebMediaPlayerImpl::SetReadyState(WebMediaPlayer::ReadyState state) {
  DVLOG(1) << __FUNCTION__ << "(" << state << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (state == WebMediaPlayer::ReadyStateHaveEnoughData && data_source_ &&
      data_source_->assume_fully_buffered() &&
      network_state_ == WebMediaPlayer::NetworkStateLoading)
    SetNetworkState(WebMediaPlayer::NetworkStateLoaded);

  ready_state_ = state;
  highest_ready_state_ = std::max(highest_ready_state_, ready_state_);

  // Always notify to ensure client has the latest value.
  client_->readyStateChanged();
}

blink::WebAudioSourceProvider* WebMediaPlayerImpl::getAudioSourceProvider() {
  return audio_source_provider_.get();
}

double WebMediaPlayerImpl::GetPipelineDuration() const {
  base::TimeDelta duration = pipeline_.GetMediaDuration();

  // Return positive infinity if the resource is unbounded.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/video.html#dom-media-duration
  if (duration == kInfiniteDuration())
    return std::numeric_limits<double>::infinity();

  return duration.InSecondsF();
}

static void GetCurrentFrameAndSignal(
    VideoFrameCompositor* compositor,
    scoped_refptr<VideoFrame>* video_frame_out,
    base::WaitableEvent* event) {
  TRACE_EVENT0("media", "GetCurrentFrameAndSignal");
  *video_frame_out = compositor->GetCurrentFrameAndUpdateIfStale();
  event->Signal();
}

scoped_refptr<VideoFrame>
WebMediaPlayerImpl::GetCurrentFrameFromCompositor() {
  TRACE_EVENT0("media", "WebMediaPlayerImpl::GetCurrentFrameFromCompositor");
  if (compositor_task_runner_->BelongsToCurrentThread())
    return compositor_->GetCurrentFrameAndUpdateIfStale();

  // Use a posted task and waitable event instead of a lock otherwise
  // WebGL/Canvas can see different content than what the compositor is seeing.
  scoped_refptr<VideoFrame> video_frame;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  compositor_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(&GetCurrentFrameAndSignal,
                                               base::Unretained(compositor_),
                                               &video_frame,
                                               &event));
  event.Wait();
  return video_frame;
}

void WebMediaPlayerImpl::UpdatePlayState() {
#if defined(OS_ANDROID)  // WMPI_CAST
  bool is_remote = isRemote();
#else
  bool is_remote = false;
#endif
  bool is_suspended = pipeline_controller_.IsSuspended();
  bool is_backgrounded =
      IsBackgroundedSuspendEnabled() && delegate_ && delegate_->IsHidden();
  PlayState state = UpdatePlayState_ComputePlayState(is_remote, is_suspended,
                                                     is_backgrounded);
  SetDelegateState(state.delegate_state);
  SetMemoryReportingState(state.is_memory_reporting_enabled);
  SetSuspendState(state.is_suspended || pending_suspend_resume_cycle_);
}

void WebMediaPlayerImpl::SetDelegateState(DelegateState new_state) {
  if (!delegate_ || delegate_state_ == new_state)
    return;

  delegate_state_ = new_state;

  switch (delegate_state_) {
    case DelegateState::GONE:
      delegate_->PlayerGone(delegate_id_);
      break;
    case DelegateState::PLAYING:
      delegate_->DidPlay(delegate_id_, hasVideo(), hasAudio(), false,
                         pipeline_.GetMediaDuration());
      break;
    case DelegateState::PAUSED:
      delegate_->DidPause(delegate_id_, false);
      break;
    case DelegateState::PAUSED_BUT_NOT_IDLE:
      // It doesn't really matter what happens when we enter this state, only
      // that we reset the idle timer when leaving it.
      //
      // TODO(sandersd): Ideally the delegate would consider idleness and play
      // state as orthogonal properties so that we could avoid this.
      delegate_->DidPause(delegate_id_, false);
      break;
    case DelegateState::ENDED:
      delegate_->DidPause(delegate_id_, true);
      break;
  }
}

void WebMediaPlayerImpl::SetMemoryReportingState(
    bool is_memory_reporting_enabled) {
  if (memory_usage_reporting_timer_.IsRunning() ==
      is_memory_reporting_enabled) {
    return;
  }

  if (is_memory_reporting_enabled) {
    memory_usage_reporting_timer_.Start(FROM_HERE,
                                        base::TimeDelta::FromSeconds(2), this,
                                        &WebMediaPlayerImpl::ReportMemoryUsage);
  } else {
    memory_usage_reporting_timer_.Stop();
    ReportMemoryUsage();
  }
}

void WebMediaPlayerImpl::SetSuspendState(bool is_suspended) {
  // Do not change the state after an error has occurred.
  // TODO(sandersd): Update PipelineController to remove the need for this.
  if (IsNetworkStateError(network_state_))
    return;

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // TODO(sandersd): idle suspend is disabled if decoder owns video frame.
  // Used on OSX,Windows+Chromecast. Since GetCurrentFrameFromCompositor is
  // a synchronous cross-thread post, avoid the cost on platforms that
  // always allow suspend. Need to find a better mechanism for this. See
  // http://crbug.com/595716 and http://crbug.com/602708
  if (can_suspend_state_ == CanSuspendState::UNKNOWN) {
    scoped_refptr<VideoFrame> frame = GetCurrentFrameFromCompositor();
    if (frame) {
      can_suspend_state_ =
          frame->metadata()->IsTrue(VideoFrameMetadata::DECODER_OWNS_FRAME)
              ? CanSuspendState::NO
              : CanSuspendState::YES;
    }
  }
#else
  can_suspend_state_ = CanSuspendState::YES;
#endif

  if (can_suspend_state_ == CanSuspendState::NO)
    return;

  if (is_suspended) {
    pipeline_controller_.Suspend();
  } else {
    pipeline_controller_.Resume();
  }
}

WebMediaPlayerImpl::PlayState
WebMediaPlayerImpl::UpdatePlayState_ComputePlayState(bool is_remote,
                                                     bool is_suspended,
                                                     bool is_backgrounded) {
  PlayState result;

  // This includes both data source (before pipeline startup) and pipeline
  // errors.
  bool has_error = IsNetworkStateError(network_state_);

  // After HaveMetadata, we know which tracks are present and the duration.
  bool have_metadata = ready_state_ >= WebMediaPlayer::ReadyStateHaveMetadata;

  // After HaveFutureData, Blink will call play() if the state is not paused.
  bool have_future_data =
      highest_ready_state_ >= WebMediaPlayer::ReadyStateHaveFutureData;

  // Background suspend is not enabled for audio-only players unless paused,
  // though in the case of audio-only the session should be kept.
  bool background_suspended = is_backgrounded && have_metadata && hasVideo();

  // The |paused_| state is not reliable until we |have_future_data|.
  bool background_pause_suspended =
      is_backgrounded && have_future_data && paused_;

  // Idle suspend is enabled once there is future data. We don't want to idle
  // suspend before that because play() may never be triggered to leave the idle
  // state. There could be other theoretical problems if the page is waiting for
  // other events before actually calling play(), but at least we don't break
  // Blink.
  //
  // TODO(sandersd): Make the delegate suspend idle players immediately when
  // hidden.
  // TODO(sandersd): If Blink told us the paused state sooner, we could
  // idle suspend sooner.
  bool idle_suspended = is_idle_ && have_future_data;

  // Combined suspend state.
  result.is_suspended =
      is_remote || must_suspend_ || idle_suspended || background_suspended ||
      background_pause_suspended ||
      // If we're already suspended, see if we can wait for user interaction.
      (is_suspended && have_future_data && paused_ && !seeking_);

  // We do not treat |playback_rate_| == 0 as paused. For the media session,
  // being paused implies displaying a play button, which is incorrect in this
  // case. For memory usage reporting, we just use the same definition (but we
  // don't have to).
  //
  // Similarly, we don't consider |ended_| to be paused. Blink will immediately
  // call pause() or seek(), so |ended_| should not affect the computation.
  // Despite that, |ended_| does result in a separate paused state, to simplfy
  // the contract for SetDelegateState().
  //
  // |has_session| is used to decide when to create a media session. Idle
  // suspension does not destroy the media session, because we expect that the
  // notification controls (and audio focus) remain. We also require:
  //   - |have_metadata|, since the tracks and duration are passed to DidPlay().
  //   - |have_future_data|, since we need to know whether we are paused to
  //     correctly configure the session.
  //
  // TODO(sandersd): If Blink told us the paused state sooner, we could create
  // the media session sooner.
  bool can_play = !has_error && !is_remote && have_future_data;
  bool has_session = can_play && !must_suspend_ && !background_suspended;

  if (!has_session) {
    result.delegate_state = DelegateState::GONE;
  } else if (paused_) {
    if (seeking() || fullscreen_) {
      result.delegate_state = DelegateState::PAUSED_BUT_NOT_IDLE;
    } else if (ended_) {
      result.delegate_state = DelegateState::ENDED;
    } else {
      result.delegate_state = DelegateState::PAUSED;
    }
  } else {
    result.delegate_state = DelegateState::PLAYING;
  }

  // It's not critical if some cases where memory usage can change are missed,
  // since media memory changes are usually gradual.
  result.is_memory_reporting_enabled =
      can_play && !result.is_suspended && !paused_;

  return result;
}

void WebMediaPlayerImpl::ReportMemoryUsage() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // About base::Unretained() usage below: We destroy |demuxer_| on the main
  // thread.  Before that, however, ~WebMediaPlayerImpl() posts a task to the
  // media thread and waits for it to finish.  Hence, the GetMemoryUsage() task
  // posted here must finish earlier.

  if (demuxer_) {
    base::PostTaskAndReplyWithResult(
        media_task_runner_.get(), FROM_HERE,
        base::Bind(&Demuxer::GetMemoryUsage, base::Unretained(demuxer_.get())),
        base::Bind(&WebMediaPlayerImpl::FinishMemoryUsageReport, AsWeakPtr()));
  } else {
    FinishMemoryUsageReport(0);
  }
}

void WebMediaPlayerImpl::FinishMemoryUsageReport(int64_t demuxer_memory_usage) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  const PipelineStatistics stats = pipeline_.GetStatistics();
  const int64_t current_memory_usage =
      stats.audio_memory_usage + stats.video_memory_usage +
      (data_source_ ? data_source_->GetMemoryUsage() : 0) +
      demuxer_memory_usage;

  // Note, this isn't entirely accurate, there may be VideoFrames held by the
  // compositor or other resources that we're unaware of.

  DVLOG(2) << "Memory Usage -- Audio: " << stats.audio_memory_usage
           << ", Video: " << stats.video_memory_usage << ", DataSource: "
           << (data_source_ ? data_source_->GetMemoryUsage() : 0)
           << ", Demuxer: " << demuxer_memory_usage;

  const int64_t delta = current_memory_usage - last_reported_memory_usage_;
  last_reported_memory_usage_ = current_memory_usage;
  adjust_allocated_memory_cb_.Run(delta);
}

void WebMediaPlayerImpl::ScheduleIdlePauseTimer() {
  // Only schedule the pause timer if we're playing and are suspended. We use
  // delegate state as a proxy for suspended here since the suspension may be in
  // flight at the time of this call.
  if (paused_ || delegate_state_ != DelegateState::GONE)
    return;

#if defined(OS_ANDROID)
  // Remote players will be suspended and locally paused.
  if (isRemote())
    return;
#endif

  // Idle timeout chosen arbitrarily.
  background_pause_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(5),
                                this, &WebMediaPlayerImpl::OnPause);
}

}  // namespace media
