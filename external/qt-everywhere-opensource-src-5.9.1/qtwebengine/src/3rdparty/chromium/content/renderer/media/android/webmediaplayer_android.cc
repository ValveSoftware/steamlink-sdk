// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/webmediaplayer_android.h"

#include <stddef.h>

#include <limits>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/video_layer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_common_android.h"
#include "media/base/android/media_player_android.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_content_type.h"
#include "media/base/media_keys.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/blink/webcontentdecryptionmodule_impl.h"
#include "media/blink/webmediaplayer_cast_android.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_util.h"
#include "net/base/mime_util.h"
#include "skia/ext/texture_handle.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"
#include "third_party/WebKit/public/platform/WebEncryptedMediaTypes.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerEncryptedMediaClient.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerSource.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/origin.h"

static const uint32_t kGLTextureExternalOES = 0x8D65;
static const int kSDKVersionToSupportSecurityOriginCheck = 20;

using blink::WebMediaPlayer;
using blink::WebSize;
using blink::WebString;
using blink::WebURL;
using gpu::gles2::GLES2Interface;
using media::LogHelper;
using media::MediaLog;
using media::MediaPlayerAndroid;
using media::VideoFrame;

namespace {

// Values for Media.Android.IsHttpLiveStreamingMediaPredictionResult UMA.
// Never reuse values!
enum MediaTypePredictionResult {
  PREDICTION_RESULT_ALL_CORRECT,
  PREDICTION_RESULT_ALL_INCORRECT,
  PREDICTION_RESULT_PATH_BASED_WAS_BETTER,
  PREDICTION_RESULT_URL_BASED_WAS_BETTER,
  // Must always be larger than the largest logged value.
  PREDICTION_RESULT_MAX
};

// File-static function is to allow it to run even after WMPA is deleted.
void OnReleaseTexture(
    const scoped_refptr<content::StreamTextureFactory>& factories,
    uint32_t texture_id,
    const gpu::SyncToken& sync_token) {
  GLES2Interface* gl = factories->ContextGL();
  gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  gl->DeleteTextures(1, &texture_id);
  // Flush to ensure that the stream texture gets deleted in a timely fashion.
  gl->ShallowFlushCHROMIUM();
}

class SyncTokenClientImpl : public media::VideoFrame::SyncTokenClient {
 public:
  explicit SyncTokenClientImpl(gpu::gles2::GLES2Interface* gl) : gl_(gl) {}
  ~SyncTokenClientImpl() override {}
  void GenerateSyncToken(gpu::SyncToken* sync_token) override {
    const GLuint64 fence_sync = gl_->InsertFenceSyncCHROMIUM();
    gl_->ShallowFlushCHROMIUM();
    gl_->GenSyncTokenCHROMIUM(fence_sync, sync_token->GetData());
  }
  void WaitSyncToken(const gpu::SyncToken& sync_token) override {
    gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }

 private:
  gpu::gles2::GLES2Interface* gl_;
};

}  // namespace

namespace content {

WebMediaPlayerAndroid::WebMediaPlayerAndroid(
    blink::WebFrame* frame,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    base::WeakPtr<media::WebMediaPlayerDelegate> delegate,
    RendererMediaPlayerManager* player_manager,
    scoped_refptr<StreamTextureFactory> factory,
    int frame_id,
    bool enable_texture_copy,
    const media::WebMediaPlayerParams& params)
    : frame_(frame),
      client_(client),
      delegate_(delegate),
      delegate_id_(0),
      defer_load_cb_(params.defer_load_cb()),
      buffered_(static_cast<size_t>(1)),
      media_task_runner_(params.media_task_runner()),
      pending_seek_(false),
      seeking_(false),
      did_loading_progress_(false),
      player_manager_(player_manager),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      texture_id_(0),
      is_player_initialized_(false),
      is_playing_(false),
      is_play_pending_(false),
      needs_establish_peer_(true),
      has_size_info_(false),
      // Threaded compositing isn't enabled universally yet.
      compositor_task_runner_(params.compositor_task_runner()
                                  ? params.compositor_task_runner()
                                  : base::ThreadTaskRunnerHandle::Get()),
      stream_texture_factory_(factory),
      is_fullscreen_(false),
      video_frame_provider_client_(nullptr),
      player_type_(MEDIA_PLAYER_TYPE_URL),
      is_remote_(false),
      media_log_(params.media_log()),
      allow_stored_credentials_(false),
      is_local_resource_(false),
      interpolator_(&default_tick_clock_),
      frame_id_(frame_id),
      enable_texture_copy_(enable_texture_copy),
      suppress_deleting_texture_(false),
      playback_completed_(false),
      volume_(1.0),
      volume_multiplier_(1.0),
      weak_factory_(this) {
  DCHECK(player_manager_);

  DCHECK(main_thread_checker_.CalledOnValidThread());

  if (delegate_)
    delegate_id_ = delegate_->AddObserver(this);

  player_id_ = player_manager_->RegisterMediaPlayer(this);

  TryCreateStreamTextureProxyIfNeeded();
  interpolator_.SetUpperBound(base::TimeDelta());
}

WebMediaPlayerAndroid::~WebMediaPlayerAndroid() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  SetVideoFrameProviderClient(NULL);
  client_->setWebLayer(NULL);

  if (is_player_initialized_)
    player_manager_->DestroyPlayer(player_id_);

  player_manager_->UnregisterMediaPlayer(player_id_);

  if (texture_id_) {
    GLES2Interface* gl = stream_texture_factory_->ContextGL();
    gl->DeleteTextures(1, &texture_id_);
    // Flush to ensure that the stream texture gets deleted in a timely fashion.
    gl->ShallowFlushCHROMIUM();
    texture_id_ = 0;
    texture_mailbox_ = gpu::Mailbox();
  }

  {
    base::AutoLock auto_lock(current_frame_lock_);
    current_frame_ = NULL;
  }

  if (delegate_) {
    delegate_->PlayerGone(delegate_id_);
    delegate_->RemoveObserver(delegate_id_);
  }
}

void WebMediaPlayerAndroid::load(LoadType load_type,
                                 const blink::WebMediaPlayerSource& source,
                                 CORSMode cors_mode) {
  // Only URL or MSE blob URL is supported.
  DCHECK(source.isURL());
  blink::WebURL url = source.getAsURL();
  if (!defer_load_cb_.is_null()) {
    defer_load_cb_.Run(base::Bind(&WebMediaPlayerAndroid::DoLoad,
                                  weak_factory_.GetWeakPtr(), load_type, url,
                                  cors_mode));
    return;
  }
  DoLoad(load_type, url, cors_mode);
}

void WebMediaPlayerAndroid::DoLoad(LoadType load_type,
                                   const blink::WebURL& url,
                                   CORSMode cors_mode) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  media::ReportMetrics(load_type, GURL(url), frame_->getSecurityOrigin());
  DCHECK_EQ(load_type, LoadTypeURL)
      << "WebMediaPlayerAndroid doesn't support MediaStream or "
         "MediaSource on this platform";

  url_ = url;
  is_local_resource_ = IsLocalResource();
  info_loader_.reset(new MediaInfoLoader(
      url, cors_mode, base::Bind(&WebMediaPlayerAndroid::DidLoadMediaInfo,
                                 weak_factory_.GetWeakPtr())));
  info_loader_->Start(frame_);

  UpdateNetworkState(WebMediaPlayer::NetworkStateLoading);
  UpdateReadyState(WebMediaPlayer::ReadyStateHaveNothing);
}

void WebMediaPlayerAndroid::DidLoadMediaInfo(
    MediaInfoLoader::Status status,
    const GURL& redirected_url,
    const GURL& first_party_for_cookies,
    bool allow_stored_credentials) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (status == MediaInfoLoader::kFailed) {
    info_loader_.reset();
    UpdateNetworkState(WebMediaPlayer::NetworkStateNetworkError);
    return;
  }
  redirected_url_ = redirected_url;
  InitializePlayer(redirected_url, first_party_for_cookies,
                   allow_stored_credentials);

  UpdateNetworkState(WebMediaPlayer::NetworkStateIdle);
}

bool WebMediaPlayerAndroid::IsLocalResource() {
  if (url_.SchemeIsFile() || url_.SchemeIsBlob())
    return true;

  std::string host = url_.host();
  if (!host.compare("localhost") || !host.compare("127.0.0.1") ||
      !host.compare("[::1]")) {
    return true;
  }

  return false;
}

void WebMediaPlayerAndroid::play() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  if (hasVideo() && player_manager_->render_frame()->IsHidden()) {
    bool can_video_play_in_background =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableMediaSuspend) ||
        (IsBackgroundVideoCandidate() &&
            delegate_ && delegate_->IsPlayingBackgroundVideo());
    if (!can_video_play_in_background) {
      is_play_pending_ = true;
      return;
    }
  }
  is_play_pending_ = false;

  TryCreateStreamTextureProxyIfNeeded();
  // There is no need to establish the surface texture peer for fullscreen
  // video.
  if ((hasVideo() || IsHLSStream()) && needs_establish_peer_ &&
      !is_fullscreen_) {
    EstablishSurfaceTexturePeer();
  }

  // UpdatePlayingState() must be run before calling Start() to ensure that the
  // browser side MediaPlayerAndroid values for hasAudio() and hasVideo() take
  // precedent over the guesses that we make based on mime type.
  const bool is_paused = paused();
  UpdatePlayingState(true);
  if (is_paused)
    player_manager_->Start(player_id_);
  UpdateNetworkState(WebMediaPlayer::NetworkStateLoading);
}

void WebMediaPlayerAndroid::pause() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  Pause(true);
}

void WebMediaPlayerAndroid::requestRemotePlayback() {
  player_manager_->RequestRemotePlayback(player_id_);
}

void WebMediaPlayerAndroid::requestRemotePlaybackControl() {
  player_manager_->RequestRemotePlaybackControl(player_id_);
}

void WebMediaPlayerAndroid::requestRemotePlaybackStop() {
  player_manager_->RequestRemotePlaybackStop(player_id_);
}

void WebMediaPlayerAndroid::seek(double seconds) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DVLOG(1) << __FUNCTION__ << "(" << seconds << ")";

  playback_completed_ = false;
  base::TimeDelta new_seek_time = base::TimeDelta::FromSecondsD(seconds);

  if (seeking_) {
    if (new_seek_time == seek_time_) {
      pending_seek_ = false;
      return;
    }

    pending_seek_ = true;
    pending_seek_time_ = new_seek_time;

    // Later, OnSeekComplete will trigger the pending seek.
    return;
  }

  seeking_ = true;
  seek_time_ = new_seek_time;

  // Kick off the asynchronous seek!
  player_manager_->Seek(player_id_, seek_time_);
}

bool WebMediaPlayerAndroid::supportsSave() const {
  return false;
}

void WebMediaPlayerAndroid::setRate(double rate) {}

void WebMediaPlayerAndroid::setVolume(double volume) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  volume_ = volume;
  player_manager_->SetVolume(player_id_, volume_ * volume_multiplier_);
}

void WebMediaPlayerAndroid::setSinkId(
    const blink::WebString& sink_id,
    const blink::WebSecurityOrigin& security_origin,
    blink::WebSetSinkIdCallbacks* web_callback) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  std::unique_ptr<blink::WebSetSinkIdCallbacks> callback(web_callback);
  callback->onError(blink::WebSetSinkIdError::NotSupported);
}

bool WebMediaPlayerAndroid::hasVideo() const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // If we have obtained video size information before, use it.
  if (has_size_info_)
    return !natural_size_.isEmpty();

  // TODO(qinmin): need a better method to determine whether the current media
  // content contains video. Android does not provide any function to do
  // this.
  // We don't know whether the current media content has video unless
  // the player is prepared. If the player is not prepared, we fall back
  // to the mime-type. There may be no mime-type on a redirect URL.
  // In that case, we conservatively assume it contains video so that
  // enterfullscreen call will not fail.
  if (!url_.has_path())
    return false;
  std::string mime;
  if (!net::GetMimeTypeFromFile(base::FilePath(url_.path()), &mime))
    return true;
  return mime.find("audio/") == std::string::npos;
}

bool WebMediaPlayerAndroid::hasAudio() const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!url_.has_path())
    return false;
  std::string mime;
  if (!net::GetMimeTypeFromFile(base::FilePath(url_.path()), &mime))
    return true;

  if (mime.find("audio/") != std::string::npos ||
      mime.find("video/") != std::string::npos ||
      mime.find("application/ogg") != std::string::npos ||
      mime.find("application/x-mpegurl") != std::string::npos) {
    return true;
  }
  return false;
}

bool WebMediaPlayerAndroid::isRemote() const {
  return is_remote_;
}

bool WebMediaPlayerAndroid::paused() const {
  return !is_playing_;
}

bool WebMediaPlayerAndroid::seeking() const {
  return seeking_;
}

double WebMediaPlayerAndroid::duration() const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (duration_ == media::kInfiniteDuration)
    return std::numeric_limits<double>::infinity();

  return duration_.InSecondsF();
}

double WebMediaPlayerAndroid::timelineOffset() const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::Time timeline_offset;
  if (timeline_offset.is_null())
    return std::numeric_limits<double>::quiet_NaN();

  return timeline_offset.ToJsTime();
}

double WebMediaPlayerAndroid::currentTime() const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // If the player is processing a seek, return the seek time.
  // Blink may still query us if updatePlaybackState() occurs while seeking.
  if (seeking()) {
    return pending_seek_ ?
        pending_seek_time_.InSecondsF() : seek_time_.InSecondsF();
  }

  return std::min(
      (const_cast<media::TimeDeltaInterpolator*>(
          &interpolator_))->GetInterpolatedTime(), duration_).InSecondsF();
}

WebSize WebMediaPlayerAndroid::naturalSize() const {
  return natural_size_;
}

WebMediaPlayer::NetworkState WebMediaPlayerAndroid::getNetworkState() const {
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerAndroid::getReadyState() const {
  return ready_state_;
}

blink::WebString WebMediaPlayerAndroid::getErrorMessage() {
  return blink::WebString::fromUTF8(media_log_->GetLastErrorMessage());
}

blink::WebTimeRanges WebMediaPlayerAndroid::buffered() const {
  return buffered_;
}

blink::WebTimeRanges WebMediaPlayerAndroid::seekable() const {
  if (ready_state_ < WebMediaPlayer::ReadyStateHaveMetadata)
    return blink::WebTimeRanges();

  // TODO(dalecurtis): Technically this allows seeking on media which return an
  // infinite duration.  While not expected, disabling this breaks semi-live
  // players, http://crbug.com/427412.
  const blink::WebTimeRange seekable_range(0.0, duration());
  return blink::WebTimeRanges(&seekable_range, 1);
}

bool WebMediaPlayerAndroid::didLoadingProgress() {
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

void WebMediaPlayerAndroid::paint(blink::WebCanvas* canvas,
                                  const blink::WebRect& rect,
                                  SkPaint& paint) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  std::unique_ptr<blink::WebGraphicsContext3DProvider> provider(
      blink::Platform::current()
          ->createSharedOffscreenGraphicsContext3DProvider());
  if (!provider)
    return;
  gpu::gles2::GLES2Interface* gl = provider->contextGL();

  scoped_refptr<VideoFrame> video_frame;
  {
    base::AutoLock auto_lock(current_frame_lock_);
    video_frame = current_frame_;
  }

  if (!video_frame.get() || !video_frame->HasTextures())
    return;
  DCHECK_EQ(1u, media::VideoFrame::NumPlanes(video_frame->format()));
  const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(0);

  gl->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());

  uint32_t src_texture = gl->CreateAndConsumeTextureCHROMIUM(
      mailbox_holder.texture_target, mailbox_holder.mailbox.name);

  GrGLTextureInfo texture_info;
  texture_info.fID = src_texture;
  texture_info.fTarget = mailbox_holder.texture_target;

  GrBackendTextureDesc desc;
  desc.fWidth = naturalSize().width;
  desc.fHeight = naturalSize().height;
  desc.fOrigin = kTopLeft_GrSurfaceOrigin;
  desc.fConfig = kRGBA_8888_GrPixelConfig;
  desc.fSampleCnt = 0;
  desc.fTextureHandle = skia::GrGLTextureInfoToGrBackendObject(texture_info);

  sk_sp<SkImage> image(SkImage::MakeFromTexture(provider->grContext(), desc,
                                                kOpaque_SkAlphaType));
  if (!image)
    return;

  // Draw the texture based image onto the Canvas. If the canvas is
  // hardware based, this will do a GPU-GPU texture copy.
  // If the canvas is software based, the texture based bitmap will be
  // readbacked to system memory then draw onto the canvas.
  SkRect dest;
  dest.set(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
  SkPaint video_paint;
  video_paint.setAlpha(paint.getAlpha());
  video_paint.setBlendMode(paint.getBlendMode());
  // It is not necessary to pass the dest into the drawBitmap call since all
  // the context have been set up before calling paintCurrentFrameInContext.
  canvas->drawImageRect(image, dest, &video_paint);

  // Ensure the Skia draw of the GL texture is flushed to GL, delete the
  // mailboxed texture from this context, and then signal that we're done with
  // the video frame.
  canvas->flush();
  gl->DeleteTextures(1, &src_texture);
  gl->Flush();
  SyncTokenClientImpl client(gl);
  video_frame->UpdateReleaseSyncToken(&client);
}

bool WebMediaPlayerAndroid::copyVideoTextureToPlatformTexture(
    gpu::gles2::GLES2Interface* gl,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int type,
    bool premultiply_alpha,
    bool flip_y) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  scoped_refptr<VideoFrame> video_frame;
  {
    base::AutoLock auto_lock(current_frame_lock_);
    video_frame = current_frame_;
  }

  if (!video_frame.get() || !video_frame->HasTextures())
    return false;
  DCHECK_EQ(1u, media::VideoFrame::NumPlanes(video_frame->format()));
  const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(0);
  DCHECK((!is_remote_ &&
          mailbox_holder.texture_target == GL_TEXTURE_EXTERNAL_OES) ||
         (is_remote_ && mailbox_holder.texture_target == GL_TEXTURE_2D));

  gl->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());

  // Ensure the target of texture is set before copyTextureCHROMIUM, otherwise
  // an invalid texture target may be used for copy texture.
  uint32_t src_texture = gl->CreateAndConsumeTextureCHROMIUM(
      mailbox_holder.texture_target, mailbox_holder.mailbox.name);

  // Application itself needs to take care of setting the right flip_y
  // value down to get the expected result.
  // flip_y==true means to reverse the video orientation while
  // flip_y==false means to keep the intrinsic orientation.
  gl->CopyTextureCHROMIUM(src_texture, texture, internal_format, type, flip_y,
                          premultiply_alpha, false);

  gl->DeleteTextures(1, &src_texture);
  gl->Flush();

  SyncTokenClientImpl client(gl);
  video_frame->UpdateReleaseSyncToken(&client);
  return true;
}

bool WebMediaPlayerAndroid::hasSingleSecurityOrigin() const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (player_type_ != MEDIA_PLAYER_TYPE_URL)
    return true;

  if (!info_loader_ || !info_loader_->HasSingleOrigin())
    return false;

  // TODO(qinmin): After fixing crbug.com/592017, remove this command line.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kReduceSecurityForTesting))
    return true;

  // TODO(qinmin): The url might be redirected when android media player
  // requests the stream. As a result, we cannot guarantee there is only
  // a single origin. Only if the HTTP request was made without credentials,
  // we will honor the return value from  HasSingleSecurityOriginInternal()
  // in pre-L android versions.
  // Check http://crbug.com/334204.
  if (!allow_stored_credentials_)
    return true;

  return base::android::BuildInfo::GetInstance()->sdk_int() >=
      kSDKVersionToSupportSecurityOriginCheck;
}

bool WebMediaPlayerAndroid::didPassCORSAccessCheck() const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (info_loader_)
    return info_loader_->DidPassCORSAccessCheck();
  return false;
}

double WebMediaPlayerAndroid::mediaTimeForTimeValue(double timeValue) const {
  return base::TimeDelta::FromSecondsD(timeValue).InSecondsF();
}

unsigned WebMediaPlayerAndroid::decodedFrameCount() const {
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::droppedFrameCount() const {
  NOTIMPLEMENTED();
  return 0;
}

size_t WebMediaPlayerAndroid::audioDecodedByteCount() const {
  NOTIMPLEMENTED();
  return 0;
}

size_t WebMediaPlayerAndroid::videoDecodedByteCount() const {
  NOTIMPLEMENTED();
  return 0;
}

void WebMediaPlayerAndroid::OnMediaMetadataChanged(
    base::TimeDelta duration, int width, int height, bool success) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  bool need_to_signal_duration_changed = false;

  if (is_local_resource_)
    UpdateNetworkState(WebMediaPlayer::NetworkStateLoaded);

  // For HLS streams, the reported duration may be zero for infinite streams.
  // See http://crbug.com/501213.
  if (duration.is_zero() && IsHLSStream())
    duration = media::kInfiniteDuration;

  // Update duration, if necessary, prior to ready state updates that may
  // cause duration() query.
  if (duration_ != duration) {
    duration_ = duration;
    if (is_local_resource_)
      buffered_[0].end = duration_.InSecondsF();
    // Client readyState transition from HAVE_NOTHING to HAVE_METADATA
    // already triggers a durationchanged event. If this is a different
    // transition, remember to signal durationchanged.
    if (ready_state_ > WebMediaPlayer::ReadyStateHaveNothing)
      need_to_signal_duration_changed = true;
  }

  if (ready_state_ != WebMediaPlayer::ReadyStateHaveEnoughData) {
    UpdateReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
    UpdateReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);
  }

  // TODO(wolenetz): Should we just abort early and set network state to an
  // error if success == false? See http://crbug.com/248399
  if (success)
    OnVideoSizeChanged(width, height);

  if (need_to_signal_duration_changed)
    client_->durationChanged();
}

void WebMediaPlayerAndroid::OnPlaybackComplete() {
  // When playback is about to finish, android media player often stops
  // at a time which is smaller than the duration. This makes webkit never
  // know that the playback has finished. To solve this, we set the
  // current time to media duration when OnPlaybackComplete() get called.
  interpolator_.SetBounds(duration_, duration_, default_tick_clock_.NowTicks());
  client_->timeChanged();

  // If the loop attribute is set, timeChanged() will update the current time
  // to 0. It will perform a seek to 0. Issue a command to the player to start
  // playing after seek completes.
  if (is_playing_ && seeking_ && seek_time_.is_zero())
    player_manager_->Start(player_id_);
  else
    playback_completed_ = true;
}

void WebMediaPlayerAndroid::OnBufferingUpdate(int percentage) {
  // inf * 0 == nan which is not an acceptable WebTimeRange.
  const double d = duration();
  buffered_[0].end =
      d == std::numeric_limits<double>::infinity() ? d : d * percentage / 100;
  did_loading_progress_ = true;

  if (percentage == 100 && network_state_ < WebMediaPlayer::NetworkStateLoaded)
    UpdateNetworkState(WebMediaPlayer::NetworkStateLoaded);
}

void WebMediaPlayerAndroid::OnSeekRequest(const base::TimeDelta& time_to_seek) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  client_->requestSeek(time_to_seek.InSecondsF());
}

void WebMediaPlayerAndroid::OnSeekComplete(
    const base::TimeDelta& current_time) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  seeking_ = false;
  if (pending_seek_) {
    pending_seek_ = false;
    seek(pending_seek_time_.InSecondsF());
    return;
  }
  interpolator_.SetBounds(current_time, current_time,
                          default_tick_clock_.NowTicks());

  UpdateReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);

  client_->timeChanged();
}

void WebMediaPlayerAndroid::OnMediaError(int error_type) {
  switch (error_type) {
    case MediaPlayerAndroid::MEDIA_ERROR_FORMAT:
      UpdateNetworkState(WebMediaPlayer::NetworkStateFormatError);
      break;
    case MediaPlayerAndroid::MEDIA_ERROR_DECODE:
      UpdateNetworkState(WebMediaPlayer::NetworkStateDecodeError);
      break;
    case MediaPlayerAndroid::MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK:
      UpdateNetworkState(WebMediaPlayer::NetworkStateFormatError);
      break;
    case MediaPlayerAndroid::MEDIA_ERROR_INVALID_CODE:
      break;
  }
  client_->repaint();
}

void WebMediaPlayerAndroid::OnVideoSizeChanged(int width, int height) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  // For HLS streams, a bogus empty size may be reported at first, followed by
  // the actual size only once playback begins. See http://crbug.com/509972.
  if (!has_size_info_ && width == 0 && height == 0 && IsHLSStream())
    return;

  has_size_info_ = true;
  if (natural_size_.width == width && natural_size_.height == height)
    return;

  natural_size_.width = width;
  natural_size_.height = height;

  // When play() gets called, |natural_size_| may still be empty and
  // EstablishSurfaceTexturePeer() will not get called. As a result, the video
  // may play without a surface texture. When we finally get the valid video
  // size here, we should call EstablishSurfaceTexturePeer() if it has not been
  // previously called.
  if (!paused() && needs_establish_peer_)
    EstablishSurfaceTexturePeer();

  ReallocateVideoFrame();

  // For hidden video element (with style "display:none"), ensure the texture
  // size is set.
  if (!is_remote_ && cached_stream_texture_size_ != natural_size_) {
    stream_texture_proxy_->SetStreamTextureSize(
        gfx::Size(natural_size_.width, natural_size_.height));
    cached_stream_texture_size_ = natural_size_;
  }

  // Lazily allocate compositing layer.
  if (!video_weblayer_) {
    video_weblayer_.reset(new cc_blink::WebLayerImpl(
        cc::VideoLayer::Create(this, media::VIDEO_ROTATION_0)));
    client_->setWebLayer(video_weblayer_.get());

    // If we're paused after we receive metadata for the first time, tell the
    // delegate we can now be safely suspended due to inactivity if a subsequent
    // play event does not occur.
    if (paused() && delegate_)
      delegate_->DidPause(delegate_id_, false);
  }
}

void WebMediaPlayerAndroid::OnTimeUpdate(base::TimeDelta current_timestamp,
                                         base::TimeTicks current_time_ticks) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  if (seeking())
    return;

  // Compensate the current_timestamp with the IPC latency.
  base::TimeTicks now_ticks = default_tick_clock_.NowTicks();
  base::TimeDelta lower_bound =
      now_ticks - current_time_ticks + current_timestamp;

  base::TimeDelta upper_bound = lower_bound;
  // We should get another time update in about |kTimeUpdateInterval|
  // milliseconds.
  if (is_playing_) {
    upper_bound += base::TimeDelta::FromMilliseconds(
        media::kTimeUpdateInterval);
  }
  // if the lower_bound is smaller than the current time, just use the current
  // time so that the timer is always progressing.
  lower_bound =
      std::max(lower_bound, base::TimeDelta::FromSecondsD(currentTime()));
  if (lower_bound > upper_bound)
    upper_bound = lower_bound;
  interpolator_.SetBounds(lower_bound, upper_bound, now_ticks);
}

void WebMediaPlayerAndroid::OnConnectedToRemoteDevice(
    const std::string& remote_playback_message) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DrawRemotePlaybackText(remote_playback_message);
  is_remote_ = true;
  SetNeedsEstablishPeer(false);
  client_->connectedToRemoteDevice();
}

void WebMediaPlayerAndroid::OnDisconnectedFromRemoteDevice() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  SetNeedsEstablishPeer(true);
  if (!paused())
    EstablishSurfaceTexturePeer();
  is_remote_ = false;
  ReallocateVideoFrame();
  client_->disconnectedFromRemoteDevice();
}

void WebMediaPlayerAndroid::OnCancelledRemotePlaybackRequest() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  client_->cancelledRemotePlaybackRequest();
}

void WebMediaPlayerAndroid::OnRemotePlaybackStarted() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  client_->remotePlaybackStarted();
}

void WebMediaPlayerAndroid::OnDidExitFullscreen() {
  SetNeedsEstablishPeer(true);
  // We had the fullscreen surface connected to Android MediaPlayer,
  // so reconnect our surface texture for embedded playback.
  if (!paused() && needs_establish_peer_) {
    TryCreateStreamTextureProxyIfNeeded();
    EstablishSurfaceTexturePeer();
    suppress_deleting_texture_ = true;
  }

  is_fullscreen_ = false;
  ReallocateVideoFrame();
  client_->repaint();
}

void WebMediaPlayerAndroid::OnMediaPlayerPlay() {
  // The MediaPlayer might request the video to be played after it lost its
  // stream texture proxy or the peer connection, for example, if the video was
  // paused while fullscreen then fullscreen state was left.
  TryCreateStreamTextureProxyIfNeeded();
  if (needs_establish_peer_)
    EstablishSurfaceTexturePeer();

  UpdatePlayingState(true);
  client_->playbackStateChanged();
}

void WebMediaPlayerAndroid::OnMediaPlayerPause() {
  UpdatePlayingState(false);
  client_->playbackStateChanged();
}

void WebMediaPlayerAndroid::OnRemoteRouteAvailabilityChanged(
    blink::WebRemotePlaybackAvailability availability) {
  client_->remoteRouteAvailabilityChanged(availability);
}

void WebMediaPlayerAndroid::UpdateNetworkState(
    WebMediaPlayer::NetworkState state) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing &&
      (state == WebMediaPlayer::NetworkStateNetworkError ||
       state == WebMediaPlayer::NetworkStateDecodeError)) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    network_state_ = WebMediaPlayer::NetworkStateFormatError;
  } else {
    network_state_ = state;
  }
  client_->networkStateChanged();
}

void WebMediaPlayerAndroid::UpdateReadyState(
    WebMediaPlayer::ReadyState state) {
  ready_state_ = state;
  client_->readyStateChanged();
}

void WebMediaPlayerAndroid::OnPlayerReleased() {
  needs_establish_peer_ = true;

  if (is_playing_)
    OnMediaPlayerPause();

  if (delegate_)
    delegate_->PlayerGone(delegate_id_);
}

void WebMediaPlayerAndroid::SuspendAndReleaseResources() {
  switch (network_state_) {
    // Pause the media player and inform WebKit if the player is in a good
    // shape.
    case WebMediaPlayer::NetworkStateIdle:
    case WebMediaPlayer::NetworkStateLoading:
    case WebMediaPlayer::NetworkStateLoaded:
      Pause(false);
      client_->playbackStateChanged();
      if (delegate_)
        delegate_->PlayerGone(delegate_id_);
      break;
    // If a WebMediaPlayer instance has entered into one of these states,
    // the internal network state in HTMLMediaElement could be set to empty.
    // And calling playbackStateChanged() could get this object deleted.
    case WebMediaPlayer::NetworkStateEmpty:
    case WebMediaPlayer::NetworkStateFormatError:
    case WebMediaPlayer::NetworkStateNetworkError:
    case WebMediaPlayer::NetworkStateDecodeError:
      break;
  }
  player_manager_->SuspendAndReleaseResources(player_id_);
  SetNeedsEstablishPeer(true);
}

void WebMediaPlayerAndroid::InitializePlayer(
    const GURL& url,
    const GURL& first_party_for_cookies,
    bool allow_stored_credentials) {
  ReportHLSMetrics();

  allow_stored_credentials_ = allow_stored_credentials;
  player_manager_->Initialize(player_type_, player_id_, url,
                              first_party_for_cookies, frame_->document().url(),
                              allow_stored_credentials, delegate_id_);
  is_player_initialized_ = true;

  if (is_fullscreen_)
    player_manager_->EnterFullscreen(player_id_);
}

void WebMediaPlayerAndroid::Pause(bool is_media_related_action) {
  player_manager_->Pause(player_id_, is_media_related_action);
  UpdatePlayingState(false);
}

void WebMediaPlayerAndroid::DrawRemotePlaybackText(
    const std::string& remote_playback_message) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!video_weblayer_)
    return;

  // TODO(johnme): Should redraw this frame if the layer bounds change; but
  // there seems no easy way to listen for the layer resizing (as opposed to
  // OnVideoSizeChanged, which is when the frame sizes of the video file
  // change). Perhaps have to poll (on main thread of course)?
  gfx::Size video_size_css_px = video_weblayer_->bounds();
  RenderView* render_view = RenderView::FromWebView(frame_->view());
  float device_scale_factor = render_view->GetDeviceScaleFactor();
  // canvas_size will be the size in device pixels when pageScaleFactor == 1
  gfx::Size canvas_size(
      static_cast<int>(video_size_css_px.width() * device_scale_factor),
      static_cast<int>(video_size_css_px.height() * device_scale_factor));

  scoped_refptr<VideoFrame> new_frame(media::MakeTextFrameForCast(
      remote_playback_message,
      canvas_size,
      canvas_size,
      base::Bind(&StreamTextureFactory::ContextGL,
                 stream_texture_factory_)));
  if (!new_frame)
    return;
  SetCurrentFrameInternal(new_frame);
}

void WebMediaPlayerAndroid::ReallocateVideoFrame() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  if (is_fullscreen_) return;
  if (!is_remote_ && texture_id_) {
    GLES2Interface* gl = stream_texture_factory_->ContextGL();
    GLuint texture_target = kGLTextureExternalOES;
    GLuint texture_id_ref = gl->CreateAndConsumeTextureCHROMIUM(
        texture_target, texture_mailbox_.name);
    const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
    gl->Flush();

    gpu::SyncToken texture_mailbox_sync_token;
    gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync,
                                       texture_mailbox_sync_token.GetData());
    if (texture_mailbox_sync_token.namespace_id() ==
        gpu::CommandBufferNamespace::IN_PROCESS) {
      // TODO(boliu): Remove this once Android WebView switches to IPC-based
      // command buffer for video.
      GLbyte* sync_tokens[] = {texture_mailbox_sync_token.GetData()};
      gl->VerifySyncTokensCHROMIUM(sync_tokens, arraysize(sync_tokens));
    }

    gpu::MailboxHolder holders[media::VideoFrame::kMaxPlanes] = {
        gpu::MailboxHolder(texture_mailbox_, texture_mailbox_sync_token,
                           texture_target)};
    scoped_refptr<VideoFrame> new_frame = VideoFrame::WrapNativeTextures(
        media::PIXEL_FORMAT_ARGB, holders,
        media::BindToCurrentLoop(base::Bind(
            &OnReleaseTexture, stream_texture_factory_, texture_id_ref)),
        natural_size_, gfx::Rect(natural_size_), natural_size_,
        base::TimeDelta());
    if (new_frame.get()) {
      new_frame->metadata()->SetBoolean(
          media::VideoFrameMetadata::COPY_REQUIRED, enable_texture_copy_);
    }
    SetCurrentFrameInternal(new_frame);
  }
}

void WebMediaPlayerAndroid::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  // This is called from both the main renderer thread and the compositor
  // thread (when the main thread is blocked).

  // Set the callback target when a frame is produced. Need to do this before
  // StopUsingProvider to ensure we really stop using the client.
  if (stream_texture_proxy_)
    UpdateStreamTextureProxyCallback(client);

  if (video_frame_provider_client_ && video_frame_provider_client_ != client)
    video_frame_provider_client_->StopUsingProvider();
  video_frame_provider_client_ = client;
}

void WebMediaPlayerAndroid::SetCurrentFrameInternal(
    scoped_refptr<media::VideoFrame>& video_frame) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(current_frame_lock_);
  current_frame_ = video_frame;
}

bool WebMediaPlayerAndroid::UpdateCurrentFrame(base::TimeTicks deadline_min,
                                               base::TimeTicks deadline_max) {
  NOTIMPLEMENTED();
  return false;
}

bool WebMediaPlayerAndroid::HasCurrentFrame() {
  base::AutoLock auto_lock(current_frame_lock_);
  return static_cast<bool>(current_frame_);
}

scoped_refptr<media::VideoFrame> WebMediaPlayerAndroid::GetCurrentFrame() {
  scoped_refptr<VideoFrame> video_frame;
  {
    base::AutoLock auto_lock(current_frame_lock_);
    video_frame = current_frame_;
  }

  return video_frame;
}

void WebMediaPlayerAndroid::PutCurrentFrame() {
}

void WebMediaPlayerAndroid::UpdateStreamTextureProxyCallback(
    cc::VideoFrameProvider::Client* client) {
  base::Closure frame_received_cb;

  if (client) {
    // Unretained is safe here because:
    // - |client| is valid until we receive a call to
    // SetVideoFrameProviderClient(nullptr).
    // - SetVideoFrameProviderClient(nullptr) clears proxy's callback
    // guaranteeing it will no longer be run.
    frame_received_cb =
        base::Bind(&cc::VideoFrameProvider::Client::DidReceiveFrame,
                   base::Unretained(client));
  }

  stream_texture_proxy_->BindToTaskRunner(frame_received_cb,
                                          compositor_task_runner_);
}

void WebMediaPlayerAndroid::TryCreateStreamTextureProxyIfNeeded() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // Already created.
  if (stream_texture_proxy_)
    return;

  // No factory to create proxy.
  if (!stream_texture_factory_.get())
    return;

  // Not needed for hole punching.
  if (!needs_establish_peer_)
    return;

  DCHECK(!texture_id_);
  stream_texture_proxy_ = stream_texture_factory_->CreateProxy(
      kGLTextureExternalOES, &texture_id_, &texture_mailbox_);
  if (!stream_texture_proxy_)
    return;
  ReallocateVideoFrame();
  if (video_frame_provider_client_)
    UpdateStreamTextureProxyCallback(video_frame_provider_client_);
}

void WebMediaPlayerAndroid::EstablishSurfaceTexturePeer() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!stream_texture_proxy_)
    return;

  stream_texture_proxy_->EstablishPeer(player_id_, frame_id_);

  // Set the deferred size because the size was changed in remote mode.
  if (!is_remote_ && cached_stream_texture_size_ != natural_size_) {
    stream_texture_proxy_->SetStreamTextureSize(
        gfx::Size(natural_size_.width, natural_size_.height));
    cached_stream_texture_size_ = natural_size_;
  }

  needs_establish_peer_ = false;
}

void WebMediaPlayerAndroid::SetNeedsEstablishPeer(bool needs_establish_peer) {
  needs_establish_peer_ = needs_establish_peer;
}

void WebMediaPlayerAndroid::setPoster(const blink::WebURL& poster) {
  player_manager_->SetPoster(player_id_, poster);
}

void WebMediaPlayerAndroid::UpdatePlayingState(bool is_playing) {
  if (is_playing == is_playing_)
    return;

  is_playing_ = is_playing;

  if (is_playing)
    interpolator_.StartInterpolating();
  else
    interpolator_.StopInterpolating();

  if (delegate_) {
    if (is_playing) {
      // We must specify either video or audio to the delegate, but neither may
      // be known at this point -- there are no video only containers, so only
      // send audio if we know for sure its audio.  The browser side player will
      // fill in the correct value later for media sessions.
      delegate_->DidPlay(delegate_id_, hasVideo(), !hasVideo(), isRemote(),
                         media::DurationToMediaContentType(duration_));
    } else {
      // Even if OnPlaybackComplete() has not been called yet, Blink may have
      // already fired the ended event based on current time relative to
      // duration -- so we need to check both possibilities here.
      delegate_->DidPause(delegate_id_,
                          playback_completed_ || currentTime() >= duration());
    }
  }
}

void WebMediaPlayerAndroid::OnHidden() {
  // Pause audible video preserving its session.
  if (hasVideo() && IsBackgroundVideoCandidate() && !paused()) {
    Pause(false);
    is_play_pending_ = true;
    return;
  }

  OnSuspendRequested(false);
}

void WebMediaPlayerAndroid::OnShown() {
  if (is_play_pending_)
    play();
}

bool WebMediaPlayerAndroid::OnSuspendRequested(bool must_suspend) {
  if (!must_suspend &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMediaSuspend)) {
    return true;
  }

  // If we're idle or playing video, pause and release resources; audio only
  // players are allowed to continue unless indicated otherwise by the call.
  if (must_suspend || (paused() && playback_completed_) ||
      (hasVideo() && !IsBackgroundVideoCandidate())) {
    SuspendAndReleaseResources();
  }

  return true;
}

void WebMediaPlayerAndroid::OnPlay() {
  play();
  client_->playbackStateChanged();
}

void WebMediaPlayerAndroid::OnPause() {
  pause();
  client_->playbackStateChanged();
}

void WebMediaPlayerAndroid::OnVolumeMultiplierUpdate(double multiplier) {
  volume_multiplier_ = multiplier;
  setVolume(volume_);
}

bool WebMediaPlayerAndroid::supportsOverlayFullscreenVideo() {
  return true;
}

void WebMediaPlayerAndroid::enteredFullscreen() {
  if (is_player_initialized_)
    player_manager_->EnterFullscreen(player_id_);
  SetNeedsEstablishPeer(false);
  is_fullscreen_ = true;
  suppress_deleting_texture_ = false;

  // Create a transparent video frame. Blink will already have made the
  // background transparent because we returned true from
  // supportsOverlayFullscreenVideo(). By making the video frame transparent,
  // as well, everything in the LayerTreeView will be transparent except for
  // media controls. The video will be on visible on the underlaid surface.
  if (!fullscreen_frame_)
    fullscreen_frame_ = VideoFrame::CreateTransparentFrame(gfx::Size(1, 1));
  SetCurrentFrameInternal(fullscreen_frame_);
  client_->repaint();
}

bool WebMediaPlayerAndroid::IsHLSStream() const {
  const GURL& url = redirected_url_.is_empty() ? url_ : redirected_url_;
  return media::MediaCodecUtil::IsHLSURL(url);
}

void WebMediaPlayerAndroid::ReportHLSMetrics() const {
  if (player_type_ != MEDIA_PLAYER_TYPE_URL)
    return;

  bool is_hls = IsHLSStream();
  UMA_HISTOGRAM_BOOLEAN("Media.Android.IsHttpLiveStreamingMedia", is_hls);
  if (is_hls) {
    media::RecordOriginOfHLSPlayback(
        url::Origin(frame_->getSecurityOrigin()).GetURL());
  }

  // Assuming that |is_hls| is the ground truth, test predictions.
  bool is_hls_path = media::MediaCodecUtil::IsHLSPath(url_);
  bool is_hls_url = media::MediaCodecUtil::IsHLSURL(url_);
  MediaTypePredictionResult result = PREDICTION_RESULT_ALL_INCORRECT;
  if (is_hls_path == is_hls && is_hls_url == is_hls) {
    result = PREDICTION_RESULT_ALL_CORRECT;
  } else if (is_hls_path == is_hls) {
    result = PREDICTION_RESULT_PATH_BASED_WAS_BETTER;
  } else if (is_hls_url == is_hls) {
    result = PREDICTION_RESULT_URL_BASED_WAS_BETTER;
  }
  UMA_HISTOGRAM_ENUMERATION(
      "Media.Android.IsHttpLiveStreamingMediaPredictionResult",
      result, PREDICTION_RESULT_MAX);
}

bool WebMediaPlayerAndroid::IsBackgroundVideoCandidate() const {
  DCHECK(hasVideo());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMediaSuspend)) {
    return false;
  }

  return base::FeatureList::IsEnabled(media::kResumeBackgroundVideo) &&
      hasAudio() && !isRemote() && delegate_ && delegate_->IsHidden();
}

}  // namespace content
