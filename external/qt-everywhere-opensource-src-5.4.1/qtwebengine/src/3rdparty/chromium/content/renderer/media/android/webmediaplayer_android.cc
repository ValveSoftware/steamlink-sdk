// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/webmediaplayer_android.h"

#include <limits>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/layers/video_layer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/compositor_bindings/web_layer_impl.h"
#include "content/renderer/media/android/renderer_demuxer_android.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "content/renderer/media/crypto/renderer_cdm_manager.h"
#include "content/renderer/media/webcontentdecryptionmodule_impl.h"
#include "content/renderer/media/webmediaplayer_delegate.h"
#include "content/renderer/media/webmediaplayer_util.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/android/media_player_android.h"
#include "media/base/bind_to_current_loop.h"
// TODO(xhwang): Remove when we remove prefixed EME implementation.
#include "media/base/media_keys.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/image/image.h"

static const uint32 kGLTextureExternalOES = 0x8D65;
static const int kSDKVersionToSupportSecurityOriginCheck = 20;

using blink::WebMediaPlayer;
using blink::WebSize;
using blink::WebString;
using blink::WebTimeRanges;
using blink::WebURL;
using gpu::gles2::GLES2Interface;
using media::MediaPlayerAndroid;
using media::VideoFrame;

namespace {
// Prefix for histograms related to Encrypted Media Extensions.
const char* kMediaEme = "Media.EME.";

// File-static function is to allow it to run even after WMPA is deleted.
void OnReleaseTexture(
    const scoped_refptr<content::StreamTextureFactory>& factories,
    uint32 texture_id,
    const std::vector<uint32>& release_sync_points) {
  GLES2Interface* gl = factories->ContextGL();
  for (size_t i = 0; i < release_sync_points.size(); i++)
    gl->WaitSyncPointCHROMIUM(release_sync_points[i]);
  gl->DeleteTextures(1, &texture_id);
}
}  // namespace

namespace content {

WebMediaPlayerAndroid::WebMediaPlayerAndroid(
    blink::WebFrame* frame,
    blink::WebMediaPlayerClient* client,
    base::WeakPtr<WebMediaPlayerDelegate> delegate,
    RendererMediaPlayerManager* player_manager,
    RendererCdmManager* cdm_manager,
    scoped_refptr<StreamTextureFactory> factory,
    const scoped_refptr<base::MessageLoopProxy>& media_loop,
    media::MediaLog* media_log)
    : RenderFrameObserver(RenderFrame::FromWebFrame(frame)),
      frame_(frame),
      client_(client),
      delegate_(delegate),
      buffered_(static_cast<size_t>(1)),
      media_loop_(media_loop),
      ignore_metadata_duration_change_(false),
      pending_seek_(false),
      seeking_(false),
      did_loading_progress_(false),
      player_manager_(player_manager),
      cdm_manager_(cdm_manager),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      texture_id_(0),
      stream_id_(0),
      is_playing_(false),
      needs_establish_peer_(true),
      stream_texture_proxy_initialized_(false),
      has_size_info_(false),
      stream_texture_factory_(factory),
      needs_external_surface_(false),
      video_frame_provider_client_(NULL),
      pending_playback_(false),
      player_type_(MEDIA_PLAYER_TYPE_URL),
      current_time_(0),
      is_remote_(false),
      media_log_(media_log),
      web_cdm_(NULL),
      allow_stored_credentials_(false),
      weak_factory_(this) {
  DCHECK(player_manager_);
  DCHECK(cdm_manager_);

  DCHECK(main_thread_checker_.CalledOnValidThread());

  player_id_ = player_manager_->RegisterMediaPlayer(this);

#if defined(VIDEO_HOLE)
  force_use_overlay_embedded_video_ = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kForceUseOverlayEmbeddedVideo);
  if (force_use_overlay_embedded_video_ ||
    player_manager_->ShouldUseVideoOverlayForEmbeddedEncryptedVideo()) {
    // Defer stream texture creation until we are sure it's necessary.
    needs_establish_peer_ = false;
    current_frame_ = VideoFrame::CreateBlackFrame(gfx::Size(1, 1));
  }
#endif  // defined(VIDEO_HOLE)
  TryCreateStreamTextureProxyIfNeeded();
}

WebMediaPlayerAndroid::~WebMediaPlayerAndroid() {
  SetVideoFrameProviderClient(NULL);
  client_->setWebLayer(NULL);

  if (player_manager_) {
    player_manager_->DestroyPlayer(player_id_);
    player_manager_->UnregisterMediaPlayer(player_id_);
  }

  if (stream_id_) {
    GLES2Interface* gl = stream_texture_factory_->ContextGL();
    gl->DeleteTextures(1, &texture_id_);
    texture_id_ = 0;
    texture_mailbox_ = gpu::Mailbox();
    stream_id_ = 0;
  }

  {
    base::AutoLock auto_lock(current_frame_lock_);
    current_frame_ = NULL;
  }

  if (player_type_ == MEDIA_PLAYER_TYPE_MEDIA_SOURCE && delegate_)
    delegate_->PlayerGone(this);
}

void WebMediaPlayerAndroid::load(LoadType load_type,
                                 const blink::WebURL& url,
                                 CORSMode cors_mode) {
  ReportMediaSchemeUma(GURL(url));

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
  int demuxer_client_id = 0;
  if (player_type_ != MEDIA_PLAYER_TYPE_URL) {
    RendererDemuxerAndroid* demuxer =
        RenderThreadImpl::current()->renderer_demuxer();
    demuxer_client_id = demuxer->GetNextDemuxerClientID();

    media_source_delegate_.reset(new MediaSourceDelegate(
        demuxer, demuxer_client_id, media_loop_, media_log_));

    if (player_type_ == MEDIA_PLAYER_TYPE_MEDIA_SOURCE) {
      media::SetDecryptorReadyCB set_decryptor_ready_cb =
          media::BindToCurrentLoop(
              base::Bind(&WebMediaPlayerAndroid::SetDecryptorReadyCB,
                         weak_factory_.GetWeakPtr()));

      media_source_delegate_->InitializeMediaSource(
          base::Bind(&WebMediaPlayerAndroid::OnMediaSourceOpened,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&WebMediaPlayerAndroid::OnNeedKey,
                     weak_factory_.GetWeakPtr()),
          set_decryptor_ready_cb,
          base::Bind(&WebMediaPlayerAndroid::UpdateNetworkState,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&WebMediaPlayerAndroid::OnDurationChanged,
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
  DCHECK(!media_source_delegate_);
  if (status == MediaInfoLoader::kFailed) {
    info_loader_.reset();
    UpdateNetworkState(WebMediaPlayer::NetworkStateNetworkError);
    return;
  }

  InitializePlayer(
      redirected_url, first_party_for_cookies, allow_stored_credentials, 0);

  UpdateNetworkState(WebMediaPlayer::NetworkStateIdle);
}

void WebMediaPlayerAndroid::play() {
#if defined(VIDEO_HOLE)
  if (hasVideo() && needs_external_surface_ &&
      !player_manager_->IsInFullscreen(frame_)) {
    DCHECK(!needs_establish_peer_);
    player_manager_->RequestExternalSurface(player_id_, last_computed_rect_);
  }
#endif  // defined(VIDEO_HOLE)

  TryCreateStreamTextureProxyIfNeeded();
  // There is no need to establish the surface texture peer for fullscreen
  // video.
  if (hasVideo() && needs_establish_peer_ &&
      !player_manager_->IsInFullscreen(frame_)) {
    EstablishSurfaceTexturePeer();
  }

  if (paused())
    player_manager_->Start(player_id_);
  UpdatePlayingState(true);
  UpdateNetworkState(WebMediaPlayer::NetworkStateLoading);
}

void WebMediaPlayerAndroid::pause() {
  Pause(true);
}

void WebMediaPlayerAndroid::seek(double seconds) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DVLOG(1) << __FUNCTION__ << "(" << seconds << ")";

  base::TimeDelta new_seek_time = ConvertSecondsToTimestamp(seconds);

  if (seeking_) {
    if (new_seek_time == seek_time_) {
      if (media_source_delegate_) {
        if (!pending_seek_) {
          // If using media source demuxer, only suppress redundant seeks if
          // there is no pending seek. This enforces that any pending seek that
          // results in a demuxer seek is preceded by matching
          // CancelPendingSeek() and StartWaitingForSeek() calls.
          return;
        }
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

void WebMediaPlayerAndroid::setRate(double rate) {
  NOTIMPLEMENTED();
}

void WebMediaPlayerAndroid::setVolume(double volume) {
  player_manager_->SetVolume(player_id_, volume);
}

bool WebMediaPlayerAndroid::hasVideo() const {
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
  if (!url_.has_path())
    return false;
  std::string mime;
  if (!net::GetMimeTypeFromFile(base::FilePath(url_.path()), &mime))
    return true;

  if (mime.find("audio/") != std::string::npos ||
      mime.find("video/") != std::string::npos ||
      mime.find("application/ogg") != std::string::npos) {
    return true;
  }
  return false;
}

bool WebMediaPlayerAndroid::paused() const {
  return !is_playing_;
}

bool WebMediaPlayerAndroid::seeking() const {
  return seeking_;
}

double WebMediaPlayerAndroid::duration() const {
  // HTML5 spec requires duration to be NaN if readyState is HAVE_NOTHING
  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing)
    return std::numeric_limits<double>::quiet_NaN();

  if (duration_ == media::kInfiniteDuration())
    return std::numeric_limits<double>::infinity();

  return duration_.InSecondsF();
}

double WebMediaPlayerAndroid::timelineOffset() const {
  base::Time timeline_offset;
  if (media_source_delegate_)
    timeline_offset = media_source_delegate_->GetTimelineOffset();

  if (timeline_offset.is_null())
    return std::numeric_limits<double>::quiet_NaN();

  return timeline_offset.ToJsTime();
}

double WebMediaPlayerAndroid::currentTime() const {
  // If the player is processing a seek, return the seek time.
  // Blink may still query us if updatePlaybackState() occurs while seeking.
  if (seeking()) {
    return pending_seek_ ?
        pending_seek_time_.InSecondsF() : seek_time_.InSecondsF();
  }

  return current_time_;
}

WebSize WebMediaPlayerAndroid::naturalSize() const {
  return natural_size_;
}

WebMediaPlayer::NetworkState WebMediaPlayerAndroid::networkState() const {
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerAndroid::readyState() const {
  return ready_state_;
}

WebTimeRanges WebMediaPlayerAndroid::buffered() const {
  if (media_source_delegate_)
    return media_source_delegate_->Buffered();
  return buffered_;
}

double WebMediaPlayerAndroid::maxTimeSeekable() const {
  // If we haven't even gotten to ReadyStateHaveMetadata yet then just
  // return 0 so that the seekable range is empty.
  if (ready_state_ < WebMediaPlayer::ReadyStateHaveMetadata)
    return 0.0;

  return duration();
}

bool WebMediaPlayerAndroid::didLoadingProgress() {
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

void WebMediaPlayerAndroid::paint(blink::WebCanvas* canvas,
                                  const blink::WebRect& rect,
                                  unsigned char alpha) {
  NOTIMPLEMENTED();
}

bool WebMediaPlayerAndroid::copyVideoTextureToPlatformTexture(
    blink::WebGraphicsContext3D* web_graphics_context,
    unsigned int texture,
    unsigned int level,
    unsigned int internal_format,
    unsigned int type,
    bool premultiply_alpha,
    bool flip_y) {
  // Don't allow clients to copy an encrypted video frame.
  if (needs_external_surface_)
    return false;

  scoped_refptr<VideoFrame> video_frame;
  {
    base::AutoLock auto_lock(current_frame_lock_);
    video_frame = current_frame_;
  }

  if (!video_frame ||
      video_frame->format() != media::VideoFrame::NATIVE_TEXTURE)
    return false;
  const gpu::MailboxHolder* mailbox_holder = video_frame->mailbox_holder();
  DCHECK((!is_remote_ &&
          mailbox_holder->texture_target == GL_TEXTURE_EXTERNAL_OES) ||
         (is_remote_ && mailbox_holder->texture_target == GL_TEXTURE_2D));

  // For hidden video element (with style "display:none"), ensure the texture
  // size is set.
  if (!is_remote_ &&
      (cached_stream_texture_size_.width != natural_size_.width ||
       cached_stream_texture_size_.height != natural_size_.height)) {
    stream_texture_factory_->SetStreamTextureSize(
        stream_id_, gfx::Size(natural_size_.width, natural_size_.height));
    cached_stream_texture_size_ = natural_size_;
  }

  uint32 source_texture = web_graphics_context->createTexture();
  web_graphics_context->waitSyncPoint(mailbox_holder->sync_point);

  // Ensure the target of texture is set before copyTextureCHROMIUM, otherwise
  // an invalid texture target may be used for copy texture.
  web_graphics_context->bindTexture(mailbox_holder->texture_target,
                                    source_texture);
  web_graphics_context->consumeTextureCHROMIUM(mailbox_holder->texture_target,
                                               mailbox_holder->mailbox.name);

  // The video is stored in an unmultiplied format, so premultiply if
  // necessary.
  web_graphics_context->pixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM,
                                    premultiply_alpha);

  // Application itself needs to take care of setting the right flip_y
  // value down to get the expected result.
  // flip_y==true means to reverse the video orientation while
  // flip_y==false means to keep the intrinsic orientation.
  web_graphics_context->pixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, flip_y);
  web_graphics_context->copyTextureCHROMIUM(GL_TEXTURE_2D, source_texture,
                                            texture, level, internal_format,
                                            type);
  web_graphics_context->pixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, false);
  web_graphics_context->pixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM,
                                    false);

  if (mailbox_holder->texture_target == GL_TEXTURE_EXTERNAL_OES)
    web_graphics_context->bindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
  else
    web_graphics_context->bindTexture(GL_TEXTURE_2D, texture);
  web_graphics_context->deleteTexture(source_texture);
  web_graphics_context->flush();
  video_frame->AppendReleaseSyncPoint(web_graphics_context->insertSyncPoint());
  return true;
}

bool WebMediaPlayerAndroid::hasSingleSecurityOrigin() const {
  if (player_type_ != MEDIA_PLAYER_TYPE_URL)
    return true;

  if (!info_loader_ || !info_loader_->HasSingleOrigin())
    return false;

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
  if (info_loader_)
    return info_loader_->DidPassCORSAccessCheck();
  return false;
}

double WebMediaPlayerAndroid::mediaTimeForTimeValue(double timeValue) const {
  return ConvertSecondsToTimestamp(timeValue).InSecondsF();
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

unsigned WebMediaPlayerAndroid::audioDecodedByteCount() const {
  if (media_source_delegate_)
    return media_source_delegate_->AudioDecodedByteCount();
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerAndroid::videoDecodedByteCount() const {
  if (media_source_delegate_)
    return media_source_delegate_->VideoDecodedByteCount();
  NOTIMPLEMENTED();
  return 0;
}

void WebMediaPlayerAndroid::OnMediaMetadataChanged(
    const base::TimeDelta& duration, int width, int height, bool success) {
  bool need_to_signal_duration_changed = false;

  if (url_.SchemeIs("file"))
    UpdateNetworkState(WebMediaPlayer::NetworkStateLoaded);

  // Update duration, if necessary, prior to ready state updates that may
  // cause duration() query.
  if (!ignore_metadata_duration_change_ && duration_ != duration) {
    duration_ = duration;

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
  OnTimeUpdate(duration_);
  client_->timeChanged();

  // if the loop attribute is set, timeChanged() will update the current time
  // to 0. It will perform a seek to 0. As the requests to the renderer
  // process are sequential, the OnSeekComplete() will only occur
  // once OnPlaybackComplete() is done. As the playback can only be executed
  // upon completion of OnSeekComplete(), the request needs to be saved.
  is_playing_ = false;
  if (seeking_ && seek_time_ == base::TimeDelta())
    pending_playback_ = true;
}

void WebMediaPlayerAndroid::OnBufferingUpdate(int percentage) {
  buffered_[0].end = duration() * percentage / 100;
  did_loading_progress_ = true;
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

  OnTimeUpdate(current_time);

  UpdateReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);

  client_->timeChanged();

  if (pending_playback_) {
    play();
    pending_playback_ = false;
  }
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
    if (!paused() && !player_manager_->IsInFullscreen(frame_))
      player_manager_->RequestExternalSurface(player_id_, last_computed_rect_);
  } else if (stream_texture_proxy_ && !stream_id_) {
    // Do deferred stream texture creation finally.
    DoCreateStreamTexture();
    SetNeedsEstablishPeer(true);
  }
#endif  // defined(VIDEO_HOLE)
  // When play() gets called, |natural_size_| may still be empty and
  // EstablishSurfaceTexturePeer() will not get called. As a result, the video
  // may play without a surface texture. When we finally get the valid video
  // size here, we should call EstablishSurfaceTexturePeer() if it has not been
  // previously called.
  if (!paused() && needs_establish_peer_)
    EstablishSurfaceTexturePeer();

  natural_size_.width = width;
  natural_size_.height = height;
  ReallocateVideoFrame();

  // Lazily allocate compositing layer.
  if (!video_weblayer_) {
    video_weblayer_.reset(new WebLayerImpl(cc::VideoLayer::Create(this)));
    client_->setWebLayer(video_weblayer_.get());
  }

  // TODO(qinmin): This is a hack. We need the media element to stop showing the
  // poster image by forcing it to call setDisplayMode(video). Should move the
  // logic into HTMLMediaElement.cpp.
  client_->timeChanged();
}

void WebMediaPlayerAndroid::OnTimeUpdate(const base::TimeDelta& current_time) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  current_time_ = current_time.InSecondsF();
}

void WebMediaPlayerAndroid::OnConnectedToRemoteDevice(
    const std::string& remote_playback_message) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(!media_source_delegate_);
  DrawRemotePlaybackText(remote_playback_message);
  is_remote_ = true;
  SetNeedsEstablishPeer(false);
}

void WebMediaPlayerAndroid::OnDisconnectedFromRemoteDevice() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(!media_source_delegate_);
  SetNeedsEstablishPeer(true);
  if (!paused())
    EstablishSurfaceTexturePeer();
  is_remote_ = false;
  ReallocateVideoFrame();
}

void WebMediaPlayerAndroid::OnDidEnterFullscreen() {
  if (!player_manager_->IsInFullscreen(frame_)) {
    frame_->view()->willEnterFullScreen();
    frame_->view()->didEnterFullScreen();
    player_manager_->DidEnterFullscreen(frame_);
  }
}

void WebMediaPlayerAndroid::OnDidExitFullscreen() {
  // |needs_external_surface_| is always false on non-TV devices.
  if (!needs_external_surface_)
    SetNeedsEstablishPeer(true);
  // We had the fullscreen surface connected to Android MediaPlayer,
  // so reconnect our surface texture for embedded playback.
  if (!paused() && needs_establish_peer_)
    EstablishSurfaceTexturePeer();

#if defined(VIDEO_HOLE)
  if (!paused() && needs_external_surface_)
    player_manager_->RequestExternalSurface(player_id_, last_computed_rect_);
#endif  // defined(VIDEO_HOLE)

  frame_->view()->willExitFullScreen();
  frame_->view()->didExitFullScreen();
  player_manager_->DidExitFullscreen();
  client_->repaint();
}

void WebMediaPlayerAndroid::OnMediaPlayerPlay() {
  UpdatePlayingState(true);
  client_->playbackStateChanged();
}

void WebMediaPlayerAndroid::OnMediaPlayerPause() {
  UpdatePlayingState(false);
  client_->playbackStateChanged();
}

void WebMediaPlayerAndroid::OnRequestFullscreen() {
  client_->requestFullscreen();
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

#if defined(VIDEO_HOLE)
  last_computed_rect_ = gfx::RectF();
#endif  // defined(VIDEO_HOLE)
}

void WebMediaPlayerAndroid::ReleaseMediaResources() {
  switch (network_state_) {
    // Pause the media player and inform WebKit if the player is in a good
    // shape.
    case WebMediaPlayer::NetworkStateIdle:
    case WebMediaPlayer::NetworkStateLoading:
    case WebMediaPlayer::NetworkStateLoaded:
      Pause(false);
      client_->playbackStateChanged();
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
  player_manager_->ReleaseResources(player_id_);
  OnPlayerReleased();
}

void WebMediaPlayerAndroid::OnDestruct() {
  NOTREACHED() << "WebMediaPlayer should be destroyed before any "
                  "RenderFrameObserver::OnDestruct() gets called when "
                  "the RenderFrame goes away.";
}

void WebMediaPlayerAndroid::InitializePlayer(
    const GURL& url,
    const GURL& first_party_for_cookies,
    bool allow_stored_credentials,
    int demuxer_client_id) {
  allow_stored_credentials_ = allow_stored_credentials;
  player_manager_->Initialize(
      player_type_, player_id_, url, first_party_for_cookies, demuxer_client_id,
      frame_->document().url(), allow_stored_credentials);
  if (player_manager_->ShouldEnterFullscreen(frame_))
    player_manager_->EnterFullscreen(player_id_, frame_);
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
  float device_scale_factor = frame_->view()->deviceScaleFactor();
  // canvas_size will be the size in device pixels when pageScaleFactor == 1
  gfx::Size canvas_size(
      static_cast<int>(video_size_css_px.width() * device_scale_factor),
      static_cast<int>(video_size_css_px.height() * device_scale_factor));

  SkBitmap bitmap;
  bitmap.setConfig(
      SkBitmap::kARGB_8888_Config, canvas_size.width(), canvas_size.height());
  bitmap.allocPixels();

  // Create the canvas and draw the "Casting to <Chromecast>" text on it.
  SkCanvas canvas(bitmap);
  canvas.drawColor(SK_ColorBLACK);

  const SkScalar kTextSize(40);
  const SkScalar kMinPadding(40);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setFilterLevel(SkPaint::kHigh_FilterLevel);
  paint.setColor(SK_ColorWHITE);
  paint.setTypeface(SkTypeface::CreateFromName("sans", SkTypeface::kBold));
  paint.setTextSize(kTextSize);

  // Calculate the vertical margin from the top
  SkPaint::FontMetrics font_metrics;
  paint.getFontMetrics(&font_metrics);
  SkScalar sk_vertical_margin = kMinPadding - font_metrics.fAscent;

  // Measure the width of the entire text to display
  size_t display_text_width = paint.measureText(
      remote_playback_message.c_str(), remote_playback_message.size());
  std::string display_text(remote_playback_message);

  if (display_text_width + (kMinPadding * 2) > canvas_size.width()) {
    // The text is too long to fit in one line, truncate it and append ellipsis
    // to the end.

    // First, figure out how much of the canvas the '...' will take up.
    const std::string kTruncationEllipsis("\xE2\x80\xA6");
    SkScalar sk_ellipse_width = paint.measureText(
        kTruncationEllipsis.c_str(), kTruncationEllipsis.size());

    // Then calculate how much of the text can be drawn with the '...' appended
    // to the end of the string.
    SkScalar sk_max_original_text_width(
        canvas_size.width() - (kMinPadding * 2) - sk_ellipse_width);
    size_t sk_max_original_text_length = paint.breakText(
        remote_playback_message.c_str(),
        remote_playback_message.size(),
        sk_max_original_text_width);

    // Remove the part of the string that doesn't fit and append '...'.
    display_text.erase(sk_max_original_text_length,
        remote_playback_message.size() - sk_max_original_text_length);
    display_text.append(kTruncationEllipsis);
    display_text_width = paint.measureText(
        display_text.c_str(), display_text.size());
  }

  // Center the text horizontally.
  SkScalar sk_horizontal_margin =
      (canvas_size.width() - display_text_width) / 2.0;
  canvas.drawText(display_text.c_str(),
      display_text.size(),
      sk_horizontal_margin,
      sk_vertical_margin,
      paint);

  GLES2Interface* gl = stream_texture_factory_->ContextGL();
  GLuint remote_playback_texture_id = 0;
  gl->GenTextures(1, &remote_playback_texture_id);
  GLuint texture_target = GL_TEXTURE_2D;
  gl->BindTexture(texture_target, remote_playback_texture_id);
  gl->TexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->TexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  {
    SkAutoLockPixels lock(bitmap);
    gl->TexImage2D(texture_target,
                   0 /* level */,
                   GL_RGBA /* internalformat */,
                   bitmap.width(),
                   bitmap.height(),
                   0 /* border */,
                   GL_RGBA /* format */,
                   GL_UNSIGNED_BYTE /* type */,
                   bitmap.getPixels());
  }

  gpu::Mailbox texture_mailbox;
  gl->GenMailboxCHROMIUM(texture_mailbox.name);
  gl->ProduceTextureCHROMIUM(texture_target, texture_mailbox.name);
  gl->Flush();
  GLuint texture_mailbox_sync_point = gl->InsertSyncPointCHROMIUM();

  scoped_refptr<VideoFrame> new_frame = VideoFrame::WrapNativeTexture(
      make_scoped_ptr(new gpu::MailboxHolder(
          texture_mailbox, texture_target, texture_mailbox_sync_point)),
      media::BindToCurrentLoop(base::Bind(&OnReleaseTexture,
                                          stream_texture_factory_,
                                          remote_playback_texture_id)),
      canvas_size /* coded_size */,
      gfx::Rect(canvas_size) /* visible_rect */,
      canvas_size /* natural_size */,
      base::TimeDelta() /* timestamp */,
      VideoFrame::ReadPixelsCB());
  SetCurrentFrameInternal(new_frame);
}

void WebMediaPlayerAndroid::ReallocateVideoFrame() {
  if (needs_external_surface_) {
    // VideoFrame::CreateHoleFrame is only defined under VIDEO_HOLE.
#if defined(VIDEO_HOLE)
    if (!natural_size_.isEmpty()) {
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
    GLuint texture_id_ref = 0;
    gl->GenTextures(1, &texture_id_ref);
    GLuint texture_target = kGLTextureExternalOES;
    gl->BindTexture(texture_target, texture_id_ref);
    gl->ConsumeTextureCHROMIUM(texture_target, texture_mailbox_.name);
    gl->Flush();
    GLuint texture_mailbox_sync_point = gl->InsertSyncPointCHROMIUM();

    scoped_refptr<VideoFrame> new_frame = VideoFrame::WrapNativeTexture(
        make_scoped_ptr(new gpu::MailboxHolder(
            texture_mailbox_, texture_target, texture_mailbox_sync_point)),
        media::BindToCurrentLoop(base::Bind(
            &OnReleaseTexture, stream_texture_factory_, texture_id_ref)),
        natural_size_,
        gfx::Rect(natural_size_),
        natural_size_,
        base::TimeDelta(),
        VideoFrame::ReadPixelsCB());
    SetCurrentFrameInternal(new_frame);
  }
}

void WebMediaPlayerAndroid::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  // This is called from both the main renderer thread and the compositor
  // thread (when the main thread is blocked).
  if (video_frame_provider_client_)
    video_frame_provider_client_->StopUsingProvider();
  video_frame_provider_client_ = client;

  // Set the callback target when a frame is produced.
  if (stream_texture_proxy_)
    stream_texture_proxy_->SetClient(client);
}

void WebMediaPlayerAndroid::SetCurrentFrameInternal(
    scoped_refptr<media::VideoFrame>& video_frame) {
  base::AutoLock auto_lock(current_frame_lock_);
  current_frame_ = video_frame;
}

scoped_refptr<media::VideoFrame> WebMediaPlayerAndroid::GetCurrentFrame() {
  scoped_refptr<VideoFrame> video_frame;
  {
    base::AutoLock auto_lock(current_frame_lock_);
    video_frame = current_frame_;
  }

  if (!stream_texture_proxy_initialized_ && stream_texture_proxy_ &&
      stream_id_ && !needs_external_surface_ && !is_remote_) {
    gfx::Size natural_size = video_frame->natural_size();
    // TODO(sievers): These variables are accessed on the wrong thread here.
    stream_texture_proxy_->BindToCurrentThread(stream_id_);
    stream_texture_factory_->SetStreamTextureSize(stream_id_, natural_size);
    stream_texture_proxy_initialized_ = true;
    cached_stream_texture_size_ = natural_size;
  }

  return video_frame;
}

void WebMediaPlayerAndroid::PutCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
}

void WebMediaPlayerAndroid::TryCreateStreamTextureProxyIfNeeded() {
  // Already created.
  if (stream_texture_proxy_)
    return;

  // No factory to create proxy.
  if (!stream_texture_factory_)
    return;

  stream_texture_proxy_.reset(stream_texture_factory_->CreateProxy());
  if (needs_establish_peer_ && stream_texture_proxy_) {
    DoCreateStreamTexture();
    ReallocateVideoFrame();
  }

  if (stream_texture_proxy_ && video_frame_provider_client_)
    stream_texture_proxy_->SetClient(video_frame_provider_client_);
}

void WebMediaPlayerAndroid::EstablishSurfaceTexturePeer() {
  if (!stream_texture_proxy_)
    return;

  if (stream_texture_factory_.get() && stream_id_)
    stream_texture_factory_->EstablishPeer(stream_id_, player_id_);
  needs_establish_peer_ = false;
}

void WebMediaPlayerAndroid::DoCreateStreamTexture() {
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
  is_playing_ = is_playing;
  if (!delegate_)
    return;
  if (is_playing)
    delegate_->DidPlay(this);
  else
    delegate_->DidPause(this);
}

#if defined(VIDEO_HOLE)
bool WebMediaPlayerAndroid::UpdateBoundaryRectangle() {
  if (!video_weblayer_)
    return false;

  // Compute the geometry of video frame layer.
  cc::Layer* layer = video_weblayer_->layer();
  gfx::RectF rect(layer->bounds());
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

// The following EME related code is copied from WebMediaPlayerImpl.
// TODO(xhwang): Remove duplicate code between WebMediaPlayerAndroid and
// WebMediaPlayerImpl.

// Convert a WebString to ASCII, falling back on an empty string in the case
// of a non-ASCII string.
static std::string ToASCIIOrEmpty(const blink::WebString& string) {
  return base::IsStringASCII(string) ? base::UTF16ToASCII(string)
                                     : std::string();
}

// Helper functions to report media EME related stats to UMA. They follow the
// convention of more commonly used macros UMA_HISTOGRAM_ENUMERATION and
// UMA_HISTOGRAM_COUNTS. The reason that we cannot use those macros directly is
// that UMA_* macros require the names to be constant throughout the process'
// lifetime.

static void EmeUMAHistogramEnumeration(const std::string& key_system,
                                       const std::string& method,
                                       int sample,
                                       int boundary_value) {
  base::LinearHistogram::FactoryGet(
      kMediaEme + KeySystemNameForUMA(key_system) + "." + method,
      1, boundary_value, boundary_value + 1,
      base::Histogram::kUmaTargetedHistogramFlag)->Add(sample);
}

static void EmeUMAHistogramCounts(const std::string& key_system,
                                  const std::string& method,
                                  int sample) {
  // Use the same parameters as UMA_HISTOGRAM_COUNTS.
  base::Histogram::FactoryGet(
      kMediaEme + KeySystemNameForUMA(key_system) + "." + method,
      1, 1000000, 50, base::Histogram::kUmaTargetedHistogramFlag)->Add(sample);
}

// Helper enum for reporting generateKeyRequest/addKey histograms.
enum MediaKeyException {
  kUnknownResultId,
  kSuccess,
  kKeySystemNotSupported,
  kInvalidPlayerState,
  kMaxMediaKeyException
};

static MediaKeyException MediaKeyExceptionForUMA(
    WebMediaPlayer::MediaKeyException e) {
  switch (e) {
    case WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported:
      return kKeySystemNotSupported;
    case WebMediaPlayer::MediaKeyExceptionInvalidPlayerState:
      return kInvalidPlayerState;
    case WebMediaPlayer::MediaKeyExceptionNoError:
      return kSuccess;
    default:
      return kUnknownResultId;
  }
}

// Helper for converting |key_system| name and exception |e| to a pair of enum
// values from above, for reporting to UMA.
static void ReportMediaKeyExceptionToUMA(const std::string& method,
                                         const std::string& key_system,
                                         WebMediaPlayer::MediaKeyException e) {
  MediaKeyException result_id = MediaKeyExceptionForUMA(e);
  DCHECK_NE(result_id, kUnknownResultId) << e;
  EmeUMAHistogramEnumeration(
      key_system, method, result_id, kMaxMediaKeyException);
}

bool WebMediaPlayerAndroid::IsKeySystemSupported(
    const std::string& key_system) {
  // On Android, EME only works with MSE.
  return player_type_ == MEDIA_PLAYER_TYPE_MEDIA_SOURCE &&
         IsConcreteSupportedKeySystem(key_system);
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::generateKeyRequest(
    const WebString& key_system,
    const unsigned char* init_data,
    unsigned init_data_length) {
  DVLOG(1) << "generateKeyRequest: " << base::string16(key_system) << ": "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length));

  std::string ascii_key_system =
      GetUnprefixedKeySystemName(ToASCIIOrEmpty(key_system));

  WebMediaPlayer::MediaKeyException e =
      GenerateKeyRequestInternal(ascii_key_system, init_data, init_data_length);
  ReportMediaKeyExceptionToUMA("generateKeyRequest", ascii_key_system, e);
  return e;
}

// Guess the type of |init_data|. This is only used to handle some corner cases
// so we keep it as simple as possible without breaking major use cases.
static std::string GuessInitDataType(const unsigned char* init_data,
                                     unsigned init_data_length) {
  // Most WebM files use KeyId of 16 bytes. MP4 init data are always >16 bytes.
  if (init_data_length == 16)
    return "video/webm";

  return "video/mp4";
}

// TODO(xhwang): Report an error when there is encrypted stream but EME is
// not enabled. Currently the player just doesn't start and waits for
// ever.
WebMediaPlayer::MediaKeyException
WebMediaPlayerAndroid::GenerateKeyRequestInternal(
    const std::string& key_system,
    const unsigned char* init_data,
    unsigned init_data_length) {
  if (!IsKeySystemSupported(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  // We do not support run-time switching between key systems for now.
  if (current_key_system_.empty()) {
    if (!proxy_decryptor_) {
      proxy_decryptor_.reset(new ProxyDecryptor(
          cdm_manager_,
          base::Bind(&WebMediaPlayerAndroid::OnKeyAdded,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&WebMediaPlayerAndroid::OnKeyError,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&WebMediaPlayerAndroid::OnKeyMessage,
                     weak_factory_.GetWeakPtr())));
    }

    GURL security_origin(frame_->document().securityOrigin().toString());
    if (!proxy_decryptor_->InitializeCDM(key_system, security_origin))
      return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

    if (!decryptor_ready_cb_.is_null()) {
      base::ResetAndReturn(&decryptor_ready_cb_)
          .Run(proxy_decryptor_->GetDecryptor());
    }

    // Only browser CDMs have CDM ID. Render side CDMs (e.g. ClearKey CDM) do
    // not have a CDM ID and there is no need to call player_manager_->SetCdm().
    if (proxy_decryptor_->GetCdmId() != RendererCdmManager::kInvalidCdmId)
      player_manager_->SetCdm(player_id_, proxy_decryptor_->GetCdmId());

    current_key_system_ = key_system;
  } else if (key_system != current_key_system_) {
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;
  }

  std::string init_data_type = init_data_type_;
  if (init_data_type.empty())
    init_data_type = GuessInitDataType(init_data, init_data_length);

  // TODO(xhwang): We assume all streams are from the same container (thus have
  // the same "type") for now. In the future, the "type" should be passed down
  // from the application.
  if (!proxy_decryptor_->GenerateKeyRequest(
           init_data_type, init_data, init_data_length)) {
    current_key_system_.clear();
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;
  }

  return WebMediaPlayer::MediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::addKey(
    const WebString& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const WebString& session_id) {
  DVLOG(1) << "addKey: " << base::string16(key_system) << ": "
           << std::string(reinterpret_cast<const char*>(key),
                          static_cast<size_t>(key_length)) << ", "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length)) << " ["
           << base::string16(session_id) << "]";

  std::string ascii_key_system =
      GetUnprefixedKeySystemName(ToASCIIOrEmpty(key_system));
  std::string ascii_session_id = ToASCIIOrEmpty(session_id);

  WebMediaPlayer::MediaKeyException e = AddKeyInternal(ascii_key_system,
                                                       key,
                                                       key_length,
                                                       init_data,
                                                       init_data_length,
                                                       ascii_session_id);
  ReportMediaKeyExceptionToUMA("addKey", ascii_key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::AddKeyInternal(
    const std::string& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const std::string& session_id) {
  DCHECK(key);
  DCHECK_GT(key_length, 0u);

  if (!IsKeySystemSupported(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.empty() || key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  proxy_decryptor_->AddKey(
      key, key_length, init_data, init_data_length, session_id);
  return WebMediaPlayer::MediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerAndroid::cancelKeyRequest(
    const WebString& key_system,
    const WebString& session_id) {
  DVLOG(1) << "cancelKeyRequest: " << base::string16(key_system) << ": "
           << " [" << base::string16(session_id) << "]";

  std::string ascii_key_system =
      GetUnprefixedKeySystemName(ToASCIIOrEmpty(key_system));
  std::string ascii_session_id = ToASCIIOrEmpty(session_id);

  WebMediaPlayer::MediaKeyException e =
      CancelKeyRequestInternal(ascii_key_system, ascii_session_id);
  ReportMediaKeyExceptionToUMA("cancelKeyRequest", ascii_key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException
WebMediaPlayerAndroid::CancelKeyRequestInternal(const std::string& key_system,
                                                const std::string& session_id) {
  if (!IsKeySystemSupported(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.empty() || key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  proxy_decryptor_->CancelKeyRequest(session_id);
  return WebMediaPlayer::MediaKeyExceptionNoError;
}

void WebMediaPlayerAndroid::setContentDecryptionModule(
    blink::WebContentDecryptionModule* cdm) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  // TODO(xhwang): Support setMediaKeys(0) if necessary: http://crbug.com/330324
  if (!cdm)
    return;

  web_cdm_ = ToWebContentDecryptionModuleImpl(cdm);
  if (!web_cdm_)
    return;

  if (!decryptor_ready_cb_.is_null())
    base::ResetAndReturn(&decryptor_ready_cb_).Run(web_cdm_->GetDecryptor());

  if (web_cdm_->GetCdmId() != RendererCdmManager::kInvalidCdmId)
    player_manager_->SetCdm(player_id_, web_cdm_->GetCdmId());
}

void WebMediaPlayerAndroid::OnKeyAdded(const std::string& session_id) {
  EmeUMAHistogramCounts(current_key_system_, "KeyAdded", 1);

  client_->keyAdded(
      WebString::fromUTF8(GetPrefixedKeySystemName(current_key_system_)),
      WebString::fromUTF8(session_id));
}

void WebMediaPlayerAndroid::OnKeyError(const std::string& session_id,
                                       media::MediaKeys::KeyError error_code,
                                       uint32 system_code) {
  EmeUMAHistogramEnumeration(current_key_system_, "KeyError",
                             error_code, media::MediaKeys::kMaxKeyError);

  unsigned short short_system_code = 0;
  if (system_code > std::numeric_limits<unsigned short>::max()) {
    LOG(WARNING) << "system_code exceeds unsigned short limit.";
    short_system_code = std::numeric_limits<unsigned short>::max();
  } else {
    short_system_code = static_cast<unsigned short>(system_code);
  }

  client_->keyError(
      WebString::fromUTF8(GetPrefixedKeySystemName(current_key_system_)),
      WebString::fromUTF8(session_id),
      static_cast<blink::WebMediaPlayerClient::MediaKeyErrorCode>(error_code),
      short_system_code);
}

void WebMediaPlayerAndroid::OnKeyMessage(const std::string& session_id,
                                         const std::vector<uint8>& message,
                                         const GURL& destination_url) {
  DCHECK(destination_url.is_empty() || destination_url.is_valid());

  client_->keyMessage(
      WebString::fromUTF8(GetPrefixedKeySystemName(current_key_system_)),
      WebString::fromUTF8(session_id),
      message.empty() ? NULL : &message[0],
      message.size(),
      destination_url);
}

void WebMediaPlayerAndroid::OnMediaSourceOpened(
    blink::WebMediaSource* web_media_source) {
  client_->mediaSourceOpened(web_media_source);
}

void WebMediaPlayerAndroid::OnNeedKey(const std::string& type,
                                      const std::vector<uint8>& init_data) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  // Do not fire NeedKey event if encrypted media is not enabled.
  if (!blink::WebRuntimeFeatures::isPrefixedEncryptedMediaEnabled() &&
      !blink::WebRuntimeFeatures::isEncryptedMediaEnabled()) {
    return;
  }

  UMA_HISTOGRAM_COUNTS(kMediaEme + std::string("NeedKey"), 1);

  DCHECK(init_data_type_.empty() || type.empty() || type == init_data_type_);
  if (init_data_type_.empty())
    init_data_type_ = type;

  const uint8* init_data_ptr = init_data.empty() ? NULL : &init_data[0];
  client_->keyNeeded(
      WebString::fromUTF8(type), init_data_ptr, init_data.size());
}

void WebMediaPlayerAndroid::SetDecryptorReadyCB(
    const media::DecryptorReadyCB& decryptor_ready_cb) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  // Cancels the previous decryptor request.
  if (decryptor_ready_cb.is_null()) {
    if (!decryptor_ready_cb_.is_null())
      base::ResetAndReturn(&decryptor_ready_cb_).Run(NULL);
    return;
  }

  // TODO(xhwang): Support multiple decryptor notification request (e.g. from
  // video and audio). The current implementation is okay for the current
  // media pipeline since we initialize audio and video decoders in sequence.
  // But WebMediaPlayerImpl should not depend on media pipeline's implementation
  // detail.
  DCHECK(decryptor_ready_cb_.is_null());

  // Mixed use of prefixed and unprefixed EME APIs is disallowed by Blink.
  DCHECK(!proxy_decryptor_ || !web_cdm_);

  if (proxy_decryptor_) {
    decryptor_ready_cb.Run(proxy_decryptor_->GetDecryptor());
    return;
  }

  if (web_cdm_) {
    decryptor_ready_cb.Run(web_cdm_->GetDecryptor());
    return;
  }

  decryptor_ready_cb_ = decryptor_ready_cb;
}

void WebMediaPlayerAndroid::enterFullscreen() {
  if (player_manager_->CanEnterFullscreen(frame_)) {
    player_manager_->EnterFullscreen(player_id_, frame_);
    SetNeedsEstablishPeer(false);
  }
}

void WebMediaPlayerAndroid::exitFullscreen() {
  player_manager_->ExitFullscreen(player_id_);
}

bool WebMediaPlayerAndroid::canEnterFullscreen() const {
  return player_manager_->CanEnterFullscreen(frame_);
}

}  // namespace content
