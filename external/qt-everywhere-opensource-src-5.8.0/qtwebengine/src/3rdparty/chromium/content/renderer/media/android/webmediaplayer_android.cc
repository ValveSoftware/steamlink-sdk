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
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/video_layer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/android/renderer_demuxer_android.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/android/webmediasession_android.h"
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
#include "media/base/cdm_context.h"
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
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/SkGrPixelRef.h"
#include "ui/gfx/image/image.h"

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

bool IsSkBitmapProperlySizedTexture(const SkBitmap* bitmap,
                                    const gfx::Size& size) {
  return bitmap->getTexture() && bitmap->width() == size.width() &&
         bitmap->height() == size.height();
}

bool AllocateSkBitmapTexture(GrContext* gr,
                             SkBitmap* bitmap,
                             const gfx::Size& size) {
  DCHECK(gr);
  GrTextureDesc desc;
  // Use kRGBA_8888_GrPixelConfig, not kSkia8888_GrPixelConfig, to avoid
  // RGBA to BGRA conversion.
  desc.fConfig = kRGBA_8888_GrPixelConfig;
  // kRenderTarget_GrTextureFlagBit avoids a copy before readback in skia.
  desc.fFlags = kRenderTarget_GrSurfaceFlag;
  desc.fSampleCnt = 0;
  desc.fOrigin = kTopLeft_GrSurfaceOrigin;
  desc.fWidth = size.width();
  desc.fHeight = size.height();
  sk_sp<GrTexture> texture(gr->textureProvider()->refScratchTexture(
        desc, GrTextureProvider::kExact_ScratchTexMatch));
  if (!texture.get())
    return false;

  SkImageInfo info = SkImageInfo::MakeN32Premul(desc.fWidth, desc.fHeight);
  SkGrPixelRef* pixel_ref = new SkGrPixelRef(info, texture.get());
  if (!pixel_ref)
    return false;
  bitmap->setInfo(info);
  bitmap->setPixelRef(pixel_ref)->unref();
  return true;
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
      encrypted_client_(encrypted_client),
      delegate_(delegate),
      delegate_id_(0),
      defer_load_cb_(params.defer_load_cb()),
      buffered_(static_cast<size_t>(1)),
      media_task_runner_(params.media_task_runner()),
      ignore_metadata_duration_change_(false),
      pending_seek_(false),
      seeking_(false),
      did_loading_progress_(false),
      player_manager_(player_manager),
      media_session_id_(params.media_session()
                            ? static_cast<const WebMediaSessionAndroid*>(
                                  params.media_session())
                                  ->media_session_id()
                            : blink::WebMediaSession::DefaultID),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      texture_id_(0),
      stream_id_(0),
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
      needs_external_surface_(false),
      is_fullscreen_(false),
      video_frame_provider_client_(nullptr),
      player_type_(MEDIA_PLAYER_TYPE_URL),
      is_remote_(false),
      media_log_(params.media_log()),
      cdm_context_(nullptr),
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

#if defined(VIDEO_HOLE)
  const RendererPreferences& prefs = RenderFrameImpl::FromRoutingID(frame_id)
                                         ->render_view()
                                         ->renderer_preferences();
  force_use_overlay_embedded_video_ = prefs.use_view_overlay_for_all_video;
  if (force_use_overlay_embedded_video_ ||
    player_manager_->ShouldUseVideoOverlayForEmbeddedEncryptedVideo()) {
    // Defer stream texture creation until we are sure it's necessary.
    needs_establish_peer_ = false;
    current_frame_ = VideoFrame::CreateBlackFrame(gfx::Size(1, 1));
  }
#endif  // defined(VIDEO_HOLE)
  TryCreateStreamTextureProxyIfNeeded();
  interpolator_.SetUpperBound(base::TimeDelta());

  if (params.initial_cdm()) {
    cdm_context_ = media::ToWebContentDecryptionModuleImpl(params.initial_cdm())
                       ->GetCdmContext();
  }
}

WebMediaPlayerAndroid::~WebMediaPlayerAndroid() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  SetVideoFrameProviderClient(NULL);
  client_->setWebLayer(NULL);

  if (is_player_initialized_)
    player_manager_->DestroyPlayer(player_id_);

  player_manager_->UnregisterMediaPlayer(player_id_);

  if (stream_id_) {
    GLES2Interface* gl = stream_texture_factory_->ContextGL();
    gl->DeleteTextures(1, &texture_id_);
    // Flush to ensure that the stream texture gets deleted in a timely fashion.
    gl->ShallowFlushCHROMIUM();
    texture_id_ = 0;
    texture_mailbox_ = gpu::Mailbox();
    stream_id_ = 0;
  }

  {
    base::AutoLock auto_lock(current_frame_lock_);
    current_frame_ = NULL;
  }

  if (delegate_) {
    delegate_->PlayerGone(delegate_id_);
    delegate_->RemoveObserver(delegate_id_);
  }

  if (media_source_delegate_) {
    // Part of |media_source_delegate_| needs to be stopped on the media thread.
    // Wait until |media_source_delegate_| is fully stopped before tearing
    // down other objects.
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    media_source_delegate_->Stop(
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&waiter)));
    waiter.Wait();
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

  switch (load_type) {
    case LoadTypeURL:
      player_type_ = MEDIA_PLAYER_TYPE_URL;
      break;

    case LoadTypeMediaSource:
      player_type_ = MEDIA_PLAYER_TYPE_MEDIA_SOURCE;
      break;

    case LoadTypeMediaStream:
      CHECK(false) << "WebMediaPlayerAndroid doesn't support MediaStream on "
                      "this platform";
      return;
  }

  url_ = url;
  is_local_resource_ = IsLocalResource();
  int demuxer_client_id = 0;
  if (player_type_ != MEDIA_PLAYER_TYPE_URL) {
    RendererDemuxerAndroid* demuxer =
        RenderThreadImpl::current()->renderer_demuxer();
    demuxer_client_id = demuxer->GetNextDemuxerClientID();

    media_source_delegate_.reset(new MediaSourceDelegate(
        demuxer, demuxer_client_id, media_task_runner_, media_log_));

    if (player_type_ == MEDIA_PLAYER_TYPE_MEDIA_SOURCE) {
      media_source_delegate_->InitializeMediaSource(
          base::Bind(&WebMediaPlayerAndroid::OnMediaSourceOpened,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&WebMediaPlayerAndroid::OnEncryptedMediaInitData,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&WebMediaPlayerAndroid::SetCdmReadyCB,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&WebMediaPlayerAndroid::UpdateNetworkState,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&WebMediaPlayerAndroid::OnDurationChanged,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&WebMediaPlayerAndroid::OnWaitingForDecryptionKey,
                     weak_factory_.GetWeakPtr()));
      InitializePlayer(url_, frame_->document().firstPartyForCookies(),
                       true, demuxer_client_id);
    }
  } else {
    info_loader_.reset(
        new MediaInfoLoader(
            url,
            cors_mode,
            base::Bind(&WebMediaPlayerAndroid::DidLoadMediaInfo,
                       weak_factory_.GetWeakPtr())));
    info_loader_->Start(frame_);
  }

  UpdateNetworkState(WebMediaPlayer::NetworkStateLoading);
  UpdateReadyState(WebMediaPlayer::ReadyStateHaveNothing);
}

void WebMediaPlayerAndroid::DidLoadMediaInfo(
    MediaInfoLoader::Status status,
    const GURL& redirected_url,
    const GURL& first_party_for_cookies,
    bool allow_stored_credentials) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(!media_source_delegate_);
  if (status == MediaInfoLoader::kFailed) {
    info_loader_.reset();
    UpdateNetworkState(WebMediaPlayer::NetworkStateNetworkError);
    return;
  }
  redirected_url_ = redirected_url;
  InitializePlayer(
      redirected_url, first_party_for_cookies, allow_stored_credentials, 0);

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

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMediaSuspend) &&
      hasVideo() && player_manager_->render_frame()->IsHidden()) {
    is_play_pending_ = true;
    return;
  }
  is_play_pending_ = false;

  // For HLS streams, some devices cannot detect the video size unless a surface
  // texture is bind to it. See http://crbug.com/400145.
#if defined(VIDEO_HOLE)
  if ((hasVideo() || IsHLSStream()) && needs_external_surface_ &&
      !is_fullscreen_) {
    DCHECK(!needs_establish_peer_);
    player_manager_->RequestExternalSurface(player_id_, last_computed_rect_);
  }
#endif  // defined(VIDEO_HOLE)

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

void WebMediaPlayerAndroid::seek(double seconds) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DVLOG(1) << __FUNCTION__ << "(" << seconds << ")";

  playback_completed_ = false;
  base::TimeDelta new_seek_time = base::TimeDelta::FromSecondsD(seconds);

  if (seeking_) {
    if (new_seek_time == seek_time_) {
      if (media_source_delegate_) {
        // Don't suppress any redundant in-progress MSE seek. There could have
        // been changes to the underlying buffers after seeking the demuxer and
        // before receiving OnSeekComplete() for the currently in-progress seek.
        MEDIA_LOG(DEBUG, media_log_)
            << "Detected MediaSource seek to same time as in-progress seek to "
            << seek_time_ << ".";
      } else {
        // Suppress all redundant seeks if unrestricted by media source
        // demuxer API.
        pending_seek_ = false;
        return;
      }
    }

    pending_seek_ = true;
    pending_seek_time_ = new_seek_time;

    if (media_source_delegate_)
      media_source_delegate_->CancelPendingSeek(pending_seek_time_);

    // Later, OnSeekComplete will trigger the pending seek.
    return;
  }

  seeking_ = true;
  seek_time_ = new_seek_time;

  if (media_source_delegate_)
    media_source_delegate_->StartWaitingForSeek(seek_time_);

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
  if (duration_ == media::kInfiniteDuration())
    return std::numeric_limits<double>::infinity();

  return duration_.InSecondsF();
}

double WebMediaPlayerAndroid::timelineOffset() const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::Time timeline_offset;
  if (media_source_delegate_)
    timeline_offset = media_source_delegate_->GetTimelineOffset();

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
  if (media_source_delegate_)
    return media_source_delegate_->Buffered();
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
                                  unsigned char alpha,
                                  SkXfermode::Mode mode) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  std::unique_ptr<blink::WebGraphicsContext3DProvider> provider(
      blink::Platform::current()
          ->createSharedOffscreenGraphicsContext3DProvider());
  if (!provider)
    return;
  gpu::gles2::GLES2Interface* gl = provider->contextGL();

  // Copy video texture into a RGBA texture based bitmap first as video texture
  // on Android is GL_TEXTURE_EXTERNAL_OES which is not supported by Skia yet.
  // The bitmap's size needs to be the same as the video and use naturalSize()
  // here. Check if we could reuse existing texture based bitmap.
  // Otherwise, release existing texture based bitmap and allocate
  // a new one based on video size.
  if (!IsSkBitmapProperlySizedTexture(&bitmap_, naturalSize())) {
    if (!AllocateSkBitmapTexture(provider->grContext(), &bitmap_,
                                 naturalSize())) {
      return;
    }
  }

  unsigned textureId = skia::GrBackendObjectToGrGLTextureInfo(
                           (bitmap_.getTexture())->getTextureHandle())
                           ->fID;
  if (!copyVideoTextureToPlatformTexture(gl, textureId, GL_RGBA,
                                         GL_UNSIGNED_BYTE, true, false)) {
    return;
  }

  // Ensure SkBitmap to make the latest change by external source visible.
  bitmap_.notifyPixelsChanged();

  // Draw the texture based bitmap onto the Canvas. If the canvas is
  // hardware based, this will do a GPU-GPU texture copy.
  // If the canvas is software based, the texture based bitmap will be
  // readbacked to system memory then draw onto the canvas.
  SkRect dest;
  dest.set(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
  SkPaint paint;
  paint.setAlpha(alpha);
  paint.setXfermodeMode(mode);
  // It is not necessary to pass the dest into the drawBitmap call since all
  // the context have been set up before calling paintCurrentFrameInContext.
  canvas->drawBitmapRect(bitmap_, dest, &paint);
  canvas->flush();
}

bool WebMediaPlayerAndroid::copyVideoTextureToPlatformTexture(
    gpu::gles2::GLES2Interface* gl,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int type,
    bool premultiply_alpha,
    bool flip_y) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // Don't allow clients to copy an encrypted video frame.
  if (needs_external_surface_)
    return false;

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
  if (media_source_delegate_)
    return media_source_delegate_->DecodedFrameCount();
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::droppedFrameCount() const {
  if (media_source_delegate_)
    return media_source_delegate_->DroppedFrameCount();
  NOTIMPLEMENTED();
  return 0;
}

size_t WebMediaPlayerAndroid::audioDecodedByteCount() const {
  if (media_source_delegate_)
    return media_source_delegate_->AudioDecodedByteCount();
  NOTIMPLEMENTED();
  return 0;
}

size_t WebMediaPlayerAndroid::videoDecodedByteCount() const {
  if (media_source_delegate_)
    return media_source_delegate_->VideoDecodedByteCount();
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
    duration = media::kInfiniteDuration();

  // Update duration, if necessary, prior to ready state updates that may
  // cause duration() query.
  if (!ignore_metadata_duration_change_ && duration_ != duration) {
    duration_ = duration;
    if (is_local_resource_)
      buffered_[0].end = duration_.InSecondsF();
    // Client readyState transition from HAVE_NOTHING to HAVE_METADATA
    // already triggers a durationchanged event. If this is a different
    // transition, remember to signal durationchanged.
    // Do not ever signal durationchanged on metadata change in MSE case
    // because OnDurationChanged() handles this.
    if (ready_state_ > WebMediaPlayer::ReadyStateHaveNothing &&
        player_type_ != MEDIA_PLAYER_TYPE_MEDIA_SOURCE) {
      need_to_signal_duration_changed = true;
    }
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
  interpolator_.SetBounds(duration_, duration_);
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
  buffered_[0].end = duration() * percentage / 100;
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
  interpolator_.SetBounds(current_time, current_time);

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

#if defined(VIDEO_HOLE)
  // Use H/W surface for encrypted video.
  // TODO(qinmin): Change this so that only EME needs the H/W surface
  if (force_use_overlay_embedded_video_ ||
      (media_source_delegate_ && media_source_delegate_->IsVideoEncrypted() &&
       player_manager_->ShouldUseVideoOverlayForEmbeddedEncryptedVideo())) {
    needs_external_surface_ = true;
    if (!paused() && !is_fullscreen_)
      player_manager_->RequestExternalSurface(player_id_, last_computed_rect_);
  } else if (!stream_texture_proxy_) {
    // Do deferred stream texture creation finally.
    SetNeedsEstablishPeer(true);
    TryCreateStreamTextureProxyIfNeeded();
  }
#endif  // defined(VIDEO_HOLE)
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
    stream_texture_factory_->SetStreamTextureSize(
        stream_id_, gfx::Size(natural_size_.width, natural_size_.height));
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
  base::TimeDelta lower_bound =
      base::TimeTicks::Now() - current_time_ticks + current_timestamp;
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
  interpolator_.SetBounds(lower_bound, upper_bound);
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

void WebMediaPlayerAndroid::OnDidExitFullscreen() {
  // |needs_external_surface_| is always false on non-TV devices.
  if (!needs_external_surface_)
    SetNeedsEstablishPeer(true);
  // We had the fullscreen surface connected to Android MediaPlayer,
  // so reconnect our surface texture for embedded playback.
  if (!paused() && needs_establish_peer_) {
    TryCreateStreamTextureProxyIfNeeded();
    EstablishSurfaceTexturePeer();
    suppress_deleting_texture_ = true;
  }

#if defined(VIDEO_HOLE)
  if (!paused() && needs_external_surface_)
    player_manager_->RequestExternalSurface(player_id_, last_computed_rect_);
#endif  // defined(VIDEO_HOLE)
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
    bool routes_available) {
  client_->remoteRouteAvailabilityChanged(routes_available);
}

void WebMediaPlayerAndroid::OnDurationChanged(const base::TimeDelta& duration) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // Only MSE |player_type_| registers this callback.
  DCHECK_EQ(player_type_, MEDIA_PLAYER_TYPE_MEDIA_SOURCE);

  // Cache the new duration value and trust it over any subsequent duration
  // values received in OnMediaMetadataChanged().
  duration_ = duration;
  ignore_metadata_duration_change_ = true;

  // Notify MediaPlayerClient that duration has changed, if > HAVE_NOTHING.
  if (ready_state_ > WebMediaPlayer::ReadyStateHaveNothing)
    client_->durationChanged();
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
  // |needs_external_surface_| is always false on non-TV devices.
  if (!needs_external_surface_)
    needs_establish_peer_ = true;

  if (is_playing_)
    OnMediaPlayerPause();

  if (delegate_)
    delegate_->PlayerGone(delegate_id_);

#if defined(VIDEO_HOLE)
  last_computed_rect_ = gfx::RectF();
#endif  // defined(VIDEO_HOLE)
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
  if (!needs_external_surface_)
    SetNeedsEstablishPeer(true);
}

void WebMediaPlayerAndroid::InitializePlayer(
    const GURL& url,
    const GURL& first_party_for_cookies,
    bool allow_stored_credentials,
    int demuxer_client_id) {
  ReportHLSMetrics();

  allow_stored_credentials_ = allow_stored_credentials;
  player_manager_->Initialize(
      player_type_, player_id_, url, first_party_for_cookies, demuxer_client_id,
      frame_->document().url(), allow_stored_credentials, delegate_id_,
      media_session_id_);
  is_player_initialized_ = true;

  if (is_fullscreen_)
    player_manager_->EnterFullscreen(player_id_);

  if (cdm_context_)
    SetCdmInternal(base::Bind(&media::IgnoreCdmAttached));
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
  if (needs_external_surface_) {
    // VideoFrame::CreateHoleFrame is only defined under VIDEO_HOLE.
#if defined(VIDEO_HOLE)
    if (!natural_size_.isEmpty()) {
      // Now we finally know that "stream texture" and "video frame" won't
      // be needed. EME uses "external surface" and "video hole" instead.
      RemoveSurfaceTextureAndProxy();
      scoped_refptr<VideoFrame> new_frame =
          VideoFrame::CreateHoleFrame(natural_size_);
      SetCurrentFrameInternal(new_frame);
      // Force the client to grab the hole frame.
      client_->repaint();
    }
#else
    NOTIMPLEMENTED() << "Hole punching not supported without VIDEO_HOLE flag";
#endif  // defined(VIDEO_HOLE)
  } else if (!is_remote_ && texture_id_) {
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
  if (stream_texture_proxy_) {
    stream_texture_proxy_->BindToLoop(stream_id_, client,
                                      compositor_task_runner_);
  }

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

void WebMediaPlayerAndroid::RemoveSurfaceTextureAndProxy() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  if (stream_id_) {
    GLES2Interface* gl = stream_texture_factory_->ContextGL();
    gl->DeleteTextures(1, &texture_id_);
    // Flush to ensure that the stream texture gets deleted in a timely fashion.
    gl->ShallowFlushCHROMIUM();
    texture_id_ = 0;
    texture_mailbox_ = gpu::Mailbox();
    stream_id_ = 0;
  }
  stream_texture_proxy_.reset();
  needs_establish_peer_ = !needs_external_surface_ && !is_remote_ &&
                          !is_fullscreen_ &&
                          (hasVideo() || IsHLSStream());
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

  stream_texture_proxy_.reset(stream_texture_factory_->CreateProxy());
  if (stream_texture_proxy_) {
    DoCreateStreamTexture();
    ReallocateVideoFrame();
    if (video_frame_provider_client_) {
      stream_texture_proxy_->BindToLoop(
          stream_id_, video_frame_provider_client_, compositor_task_runner_);
    }
  }
}

void WebMediaPlayerAndroid::EstablishSurfaceTexturePeer() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!stream_texture_proxy_)
    return;

  if (stream_texture_factory_.get() && stream_id_)
    stream_texture_factory_->EstablishPeer(stream_id_, player_id_, frame_id_);

  // Set the deferred size because the size was changed in remote mode.
  if (!is_remote_ && cached_stream_texture_size_ != natural_size_) {
    stream_texture_factory_->SetStreamTextureSize(
        stream_id_, gfx::Size(natural_size_.width, natural_size_.height));
    cached_stream_texture_size_ = natural_size_;
  }

  needs_establish_peer_ = false;
}

void WebMediaPlayerAndroid::DoCreateStreamTexture() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(!stream_id_);
  DCHECK(!texture_id_);
  stream_id_ = stream_texture_factory_->CreateStreamTexture(
      kGLTextureExternalOES, &texture_id_, &texture_mailbox_);
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
                         duration_);
    } else {
      // Even if OnPlaybackComplete() has not been called yet, Blink may have
      // already fired the ended event based on current time relative to
      // duration -- so we need to check both possibilities here.
      delegate_->DidPause(delegate_id_,
                          playback_completed_ || currentTime() >= duration());
    }
  }
}

#if defined(VIDEO_HOLE)
bool WebMediaPlayerAndroid::UpdateBoundaryRectangle() {
  if (!video_weblayer_)
    return false;

  // Compute the geometry of video frame layer.
  cc::Layer* layer = video_weblayer_->layer();
  gfx::RectF rect(gfx::SizeF(layer->bounds()));
  while (layer) {
    rect.Offset(layer->position().OffsetFromOrigin());
    layer = layer->parent();
  }

  // Return false when the geometry hasn't been changed from the last time.
  if (last_computed_rect_ == rect)
    return false;

  // Store the changed geometry information when it is actually changed.
  last_computed_rect_ = rect;
  return true;
}

const gfx::RectF WebMediaPlayerAndroid::GetBoundaryRectangle() {
  return last_computed_rect_;
}
#endif

void WebMediaPlayerAndroid::setContentDecryptionModule(
    blink::WebContentDecryptionModule* cdm,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  // Once the CDM is set it can't be cleared as there may be frames being
  // decrypted on other threads. So fail this request.
  // http://crbug.com/462365#c7.
  if (!cdm) {
    result.completeWithError(
        blink::WebContentDecryptionModuleExceptionInvalidStateError, 0,
        "The existing MediaKeys object cannot be removed at this time.");
    return;
  }

  cdm_context_ = media::ToWebContentDecryptionModuleImpl(cdm)->GetCdmContext();

  if (is_player_initialized_) {
    SetCdmInternal(
        base::Bind(&WebMediaPlayerAndroid::ContentDecryptionModuleAttached,
                   weak_factory_.GetWeakPtr(), result));
  } else {
    // No pipeline/decoder connected, so resolve the promise. When something
    // is connected, setting the CDM will happen in SetCdmReadyCB().
    ContentDecryptionModuleAttached(result, true);
  }
}

void WebMediaPlayerAndroid::ContentDecryptionModuleAttached(
    blink::WebContentDecryptionModuleResult result,
    bool success) {
  if (success) {
    result.complete();
    return;
  }

  result.completeWithError(
      blink::WebContentDecryptionModuleExceptionNotSupportedError,
      0,
      "Unable to set MediaKeys object");
}

void WebMediaPlayerAndroid::OnMediaSourceOpened(
    blink::WebMediaSource* web_media_source) {
  client_->mediaSourceOpened(web_media_source);
}

void WebMediaPlayerAndroid::OnEncryptedMediaInitData(
    media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  // TODO(xhwang): Update this UMA name. https://crbug.com/589251
  UMA_HISTOGRAM_COUNTS("Media.EME.NeedKey", 1);

  DCHECK(init_data_type != media::EmeInitDataType::UNKNOWN);

  encrypted_client_->encrypted(ConvertToWebInitDataType(init_data_type),
                               init_data.data(), init_data.size());
}

void WebMediaPlayerAndroid::OnWaitingForDecryptionKey() {
  encrypted_client_->didBlockPlaybackWaitingForKey();

  // TODO(jrummell): didResumePlaybackBlockedForKey() should only be called
  // when a key has been successfully added (e.g. OnSessionKeysChange() with
  // |has_additional_usable_key| = true). http://crbug.com/461903
  encrypted_client_->didResumePlaybackBlockedForKey();
}

void WebMediaPlayerAndroid::OnHidden() {
  OnSuspendRequested(false);
}

void WebMediaPlayerAndroid::OnShown() {
  if (is_play_pending_)
    play();
}

void WebMediaPlayerAndroid::OnSuspendRequested(bool must_suspend) {
  if (!must_suspend &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMediaSuspend)) {
    return;
  }

  // If we're idle or playing video, pause and release resources; audio only
  // players are allowed to continue unless indicated otherwise by the call.
  if (must_suspend || (paused() && playback_completed_) || hasVideo())
    SuspendAndReleaseResources();
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

void WebMediaPlayerAndroid::OnCdmContextReady(media::CdmContext* cdm_context) {
  DCHECK(!cdm_context_);

  if (!cdm_context) {
    LOG(ERROR) << "CdmContext not available (e.g. CDM creation failed).";
    return;
  }

  cdm_context_ = cdm_context;

  if (is_player_initialized_)
    SetCdmInternal(base::Bind(&media::IgnoreCdmAttached));
}

void WebMediaPlayerAndroid::SetCdmInternal(
    const media::CdmAttachedCB& cdm_attached_cb) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(cdm_context_ && is_player_initialized_);
  DCHECK(cdm_context_->GetDecryptor() ||
         cdm_context_->GetCdmId() != media::CdmContext::kInvalidCdmId)
      << "CDM should support either a Decryptor or a CDM ID.";

  if (cdm_ready_cb_.is_null()) {
    cdm_attached_cb.Run(true);
    return;
  }

  // Satisfy |cdm_ready_cb_|. Use BindToCurrentLoop() since the callback could
  // be fired on other threads.
  base::ResetAndReturn(&cdm_ready_cb_)
      .Run(cdm_context_, media::BindToCurrentLoop(base::Bind(
                             &WebMediaPlayerAndroid::OnCdmAttached,
                             weak_factory_.GetWeakPtr(), cdm_attached_cb)));
}

void WebMediaPlayerAndroid::OnCdmAttached(
    const media::CdmAttachedCB& cdm_attached_cb,
    bool success) {
  DVLOG(1) << __FUNCTION__ << ": success: " << success;
  DCHECK(main_thread_checker_.CalledOnValidThread());

  if (!success) {
    if (cdm_context_->GetCdmId() == media::CdmContext::kInvalidCdmId) {
      NOTREACHED() << "CDM cannot be attached to media player.";
      cdm_attached_cb.Run(false);
      return;
    }

    // If the CDM is not attached (e.g. the CDM does not support a Decryptor),
    // MediaSourceDelegate will fall back to use a browser side (IPC-based) CDM.
    player_manager_->SetCdm(player_id_, cdm_context_->GetCdmId());
  }

  cdm_attached_cb.Run(true);
}

void WebMediaPlayerAndroid::SetCdmReadyCB(
    const MediaSourceDelegate::CdmReadyCB& cdm_ready_cb) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(is_player_initialized_);

  // Cancels the previous CDM request.
  if (cdm_ready_cb.is_null()) {
    if (!cdm_ready_cb_.is_null()) {
      base::ResetAndReturn(&cdm_ready_cb_)
          .Run(nullptr, base::Bind(&media::IgnoreCdmAttached));
    }
    return;
  }

  // TODO(xhwang): Support multiple CDM notification request (e.g. from
  // video and audio). The current implementation is okay for the current
  // media pipeline since we initialize audio and video decoders in sequence.
  // But WebMediaPlayerAndroid should not depend on media pipeline's
  // implementation detail.
  DCHECK(cdm_ready_cb_.is_null());
  cdm_ready_cb_ = cdm_ready_cb;

  if (cdm_context_)
    SetCdmInternal(base::Bind(&media::IgnoreCdmAttached));
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
        blink::WebStringToGURL(frame_->getSecurityOrigin().toString()));
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

}  // namespace content
