// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_cast_android.h"

#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/android/media_common_android.h"
#include "media/base/bind_to_current_loop.h"
#include "media/blink/webmediaplayer_impl.h"
#include "media/blink/webmediaplayer_params.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/modules/mediasession/WebMediaSession.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/SkGrPixelRef.h"

using gpu::gles2::GLES2Interface;

namespace media {

namespace {
// File-static function is to allow it to run even after WMPI is deleted.
void OnReleaseTexture(
    const base::Callback<gpu::gles2::GLES2Interface*()>& context_3d_cb,
    GLuint texture_id,
    const gpu::SyncToken& sync_token) {
  GLES2Interface* gl = context_3d_cb.Run();
  if (!gl)
    return;

  gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  gl->DeleteTextures(1, &texture_id);
  // Flush to ensure that the texture gets deleted in a timely fashion.
  gl->ShallowFlushCHROMIUM();
}

GLES2Interface* GLCBShim(
    const WebMediaPlayerParams::Context3DCB& context_3d_cb) {
  return context_3d_cb.Run().gl;
}

}  // namespace

scoped_refptr<VideoFrame> MakeTextFrameForCast(
    const std::string& remote_playback_message,
    gfx::Size canvas_size,
    gfx::Size natural_size,
    const base::Callback<gpu::gles2::GLES2Interface*()>& context_3d_cb) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(canvas_size.width(), canvas_size.height());

  // Create the canvas and draw the "Casting to <Chromecast>" text on it.
  SkCanvas canvas(bitmap);
  canvas.drawColor(SK_ColorBLACK);

  const SkScalar kTextSize(40);
  const SkScalar kMinPadding(40);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setFilterQuality(kHigh_SkFilterQuality);
  paint.setColor(SK_ColorWHITE);
  paint.setTypeface(SkTypeface::MakeFromName(
      "sans", SkFontStyle::FromOldStyle(SkTypeface::kBold)));
  paint.setTextSize(kTextSize);

  // Calculate the vertical margin from the top
  SkPaint::FontMetrics font_metrics;
  paint.getFontMetrics(&font_metrics);
  SkScalar sk_vertical_margin = kMinPadding - font_metrics.fAscent;

  // Measure the width of the entire text to display
  size_t display_text_width = paint.measureText(remote_playback_message.c_str(),
                                                remote_playback_message.size());
  std::string display_text(remote_playback_message);

  if (display_text_width + (kMinPadding * 2) > canvas_size.width()) {
    // The text is too long to fit in one line, truncate it and append ellipsis
    // to the end.

    // First, figure out how much of the canvas the '...' will take up.
    const std::string kTruncationEllipsis("\xE2\x80\xA6");
    SkScalar sk_ellipse_width = paint.measureText(kTruncationEllipsis.c_str(),
                                                  kTruncationEllipsis.size());

    // Then calculate how much of the text can be drawn with the '...' appended
    // to the end of the string.
    SkScalar sk_max_original_text_width(canvas_size.width() -
                                        (kMinPadding * 2) - sk_ellipse_width);
    size_t sk_max_original_text_length = paint.breakText(
        remote_playback_message.c_str(), remote_playback_message.size(),
        sk_max_original_text_width);

    // Remove the part of the string that doesn't fit and append '...'.
    display_text.erase(
        sk_max_original_text_length,
        remote_playback_message.size() - sk_max_original_text_length);
    display_text.append(kTruncationEllipsis);
    display_text_width =
        paint.measureText(display_text.c_str(), display_text.size());
  }

  // Center the text horizontally.
  SkScalar sk_horizontal_margin =
      (canvas_size.width() - display_text_width) / 2.0;
  canvas.drawText(display_text.c_str(), display_text.size(),
                  sk_horizontal_margin, sk_vertical_margin, paint);

  GLES2Interface* gl = context_3d_cb.Run();

  // GPU Process crashed.
  if (!gl)
    return nullptr;
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
    gl->TexImage2D(texture_target, 0 /* level */, GL_RGBA /* internalformat */,
                   bitmap.width(), bitmap.height(), 0 /* border */,
                   GL_RGBA /* format */, GL_UNSIGNED_BYTE /* type */,
                   bitmap.getPixels());
  }

  gpu::Mailbox texture_mailbox;
  gl->GenMailboxCHROMIUM(texture_mailbox.name);
  gl->ProduceTextureCHROMIUM(texture_target, texture_mailbox.name);
  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->Flush();

  gpu::SyncToken texture_mailbox_sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync,
                                     texture_mailbox_sync_token.GetData());

  gpu::MailboxHolder holders[media::VideoFrame::kMaxPlanes] = {
      gpu::MailboxHolder(texture_mailbox, texture_mailbox_sync_token,
                         texture_target)};
  return VideoFrame::WrapNativeTextures(
      media::PIXEL_FORMAT_ARGB, holders,
      media::BindToCurrentLoop(base::Bind(&OnReleaseTexture, context_3d_cb,
                                          remote_playback_texture_id)),
      canvas_size /* coded_size */, gfx::Rect(canvas_size) /* visible_rect */,
      natural_size /* natural_size */, base::TimeDelta() /* timestamp */);
}

WebMediaPlayerCast::WebMediaPlayerCast(
    WebMediaPlayerImpl* impl,
    blink::WebMediaPlayerClient* client,
    const WebMediaPlayerParams::Context3DCB& context_3d_cb)
    : webmediaplayer_(impl), client_(client), context_3d_cb_(context_3d_cb) {}

WebMediaPlayerCast::~WebMediaPlayerCast() {
  if (player_manager_) {
    if (is_player_initialized_)
      player_manager_->DestroyPlayer(player_id_);

    player_manager_->UnregisterMediaPlayer(player_id_);
  }
}

void WebMediaPlayerCast::Initialize(const GURL& url,
                                    blink::WebLocalFrame* frame,
                                    int delegate_id) {
  player_manager_->Initialize(MEDIA_PLAYER_TYPE_REMOTE_ONLY, player_id_, url,
                              frame->document().firstPartyForCookies(), 0,
                              frame->document().url(), true, delegate_id,
                              blink::WebMediaSession::DefaultID);
  is_player_initialized_ = true;
}

void WebMediaPlayerCast::SetMediaPlayerManager(
    RendererMediaPlayerManagerInterface* media_player_manager) {
  player_manager_ = media_player_manager;
  player_id_ = player_manager_->RegisterMediaPlayer(this);
}

void WebMediaPlayerCast::requestRemotePlayback() {
  player_manager_->Seek(player_id_, base::TimeDelta::FromSecondsD(
                                        webmediaplayer_->currentTime()));
  player_manager_->RequestRemotePlayback(player_id_);
}

void WebMediaPlayerCast::requestRemotePlaybackControl() {
  player_manager_->RequestRemotePlaybackControl(player_id_);
}

void WebMediaPlayerCast::OnMediaMetadataChanged(base::TimeDelta duration,
                                                int width,
                                                int height,
                                                bool success) {
  duration_ = duration;
}

void WebMediaPlayerCast::OnPlaybackComplete() {
  DVLOG(1) << __FUNCTION__;
  webmediaplayer_->OnRemotePlaybackEnded();
}

void WebMediaPlayerCast::OnBufferingUpdate(int percentage) {
  DVLOG(1) << __FUNCTION__;
}

void WebMediaPlayerCast::OnSeekRequest(const base::TimeDelta& time_to_seek) {
  DVLOG(1) << __FUNCTION__;
  client_->requestSeek(time_to_seek.InSecondsF());
}

void WebMediaPlayerCast::OnSeekComplete(const base::TimeDelta& current_time) {
  DVLOG(1) << __FUNCTION__;
  remote_time_at_ = base::TimeTicks::Now();
  remote_time_ = current_time;
  webmediaplayer_->OnPipelineSeeked(true);
}

void WebMediaPlayerCast::OnMediaError(int error_type) {
  DVLOG(1) << __FUNCTION__;
}

void WebMediaPlayerCast::OnVideoSizeChanged(int width, int height) {
  DVLOG(1) << __FUNCTION__;
}

void WebMediaPlayerCast::OnTimeUpdate(base::TimeDelta current_timestamp,
                                      base::TimeTicks current_time_ticks) {
  DVLOG(1) << __FUNCTION__ << " " << current_timestamp.InSecondsF();
  remote_time_at_ = current_time_ticks;
  remote_time_ = current_timestamp;
}

void WebMediaPlayerCast::OnPlayerReleased() {
  DVLOG(1) << __FUNCTION__;
}

void WebMediaPlayerCast::OnConnectedToRemoteDevice(
    const std::string& remote_playback_message) {
  DVLOG(1) << __FUNCTION__;
  remote_time_ = base::TimeDelta::FromSecondsD(webmediaplayer_->currentTime());
  is_remote_ = true;
  initializing_ = true;
  paused_ = false;
  client_->playbackStateChanged();

  remote_playback_message_ = remote_playback_message;
  webmediaplayer_->SuspendForRemote();
  client_->connectedToRemoteDevice();
}

double WebMediaPlayerCast::currentTime() const {
  base::TimeDelta ret = remote_time_;
  if (!paused_ && !initializing_) {
    ret += base::TimeTicks::Now() - remote_time_at_;
  }
  return ret.InSecondsF();
}

void WebMediaPlayerCast::play() {
  if (!paused_)
    return;

  player_manager_->Start(player_id_);
  remote_time_at_ = base::TimeTicks::Now();
  paused_ = false;
}

void WebMediaPlayerCast::pause() {
  player_manager_->Pause(player_id_, true);
}

void WebMediaPlayerCast::seek(base::TimeDelta t) {
  should_notify_time_changed_ = true;
  player_manager_->Seek(player_id_, t);
}

void WebMediaPlayerCast::OnDisconnectedFromRemoteDevice() {
  DVLOG(1) << __FUNCTION__;
  if (!paused_) {
    paused_ = true;
  }
  is_remote_ = false;
  double t = currentTime();
  if (t + media::kTimeUpdateInterval * 2 / 1000 > webmediaplayer_->duration()) {
    t = webmediaplayer_->duration();
  }
  webmediaplayer_->OnDisconnectedFromRemoteDevice(t);
}

void WebMediaPlayerCast::OnCancelledRemotePlaybackRequest() {
  DVLOG(1) << __FUNCTION__;
  client_->cancelledRemotePlaybackRequest();
}

void WebMediaPlayerCast::OnDidExitFullscreen() {
  DVLOG(1) << __FUNCTION__;
}

void WebMediaPlayerCast::OnMediaPlayerPlay() {
  DVLOG(1) << __FUNCTION__ << " is_remote_ = " << is_remote_;
  initializing_ = false;
  if (is_remote_ && paused_) {
    paused_ = false;
    remote_time_at_ = base::TimeTicks::Now();
    client_->playbackStateChanged();
  }
  // Blink expects a timeChanged() in response to a seek().
  if (should_notify_time_changed_)
    client_->timeChanged();
}

void WebMediaPlayerCast::OnMediaPlayerPause() {
  DVLOG(1) << __FUNCTION__ << " is_remote_ = " << is_remote_;
  if (is_remote_ && !paused_) {
    paused_ = true;
    client_->playbackStateChanged();
  }
}

void WebMediaPlayerCast::OnRemoteRouteAvailabilityChanged(
    bool routes_available) {
  DVLOG(1) << __FUNCTION__;
  client_->remoteRouteAvailabilityChanged(routes_available);
}

void WebMediaPlayerCast::SuspendAndReleaseResources() {}
void WebMediaPlayerCast::OnWaitingForDecryptionKey() {}

bool WebMediaPlayerCast::hasVideo() const {
  return true;
}

bool WebMediaPlayerCast::paused() const {
  return paused_;
}

#if defined(VIDEO_HOLE)
bool WebMediaPlayerCast::UpdateBoundaryRectangle() {
  return false;
}

const gfx::RectF WebMediaPlayerCast::GetBoundaryRectangle() {
  return gfx::RectF();
}
#endif  // defined(VIDEO_HOLE)

void WebMediaPlayerCast::SetDeviceScaleFactor(float scale_factor) {
  device_scale_factor_ = scale_factor;
}

scoped_refptr<VideoFrame> WebMediaPlayerCast::GetCastingBanner() {
  DVLOG(1) << __FUNCTION__;

  // TODO(johnme): Should redraw this frame if the layer bounds change; but
  // there seems no easy way to listen for the layer resizing (as opposed to
  // OnVideoSizeChanged, which is when the frame sizes of the video file
  // change). Perhaps have to poll (on main thread of course)?
  gfx::Size video_size_css_px = webmediaplayer_->GetCanvasSize();
  if (!video_size_css_px.width())
    return nullptr;

  // canvas_size will be the size in device pixels when pageScaleFactor == 1
  gfx::Size canvas_size(
      static_cast<int>(video_size_css_px.width() * device_scale_factor_),
      static_cast<int>(video_size_css_px.height() * device_scale_factor_));

  if (!canvas_size.width())
    return nullptr;

  return MakeTextFrameForCast(remote_playback_message_, canvas_size,
                              webmediaplayer_->naturalSize(),
                              base::Bind(&GLCBShim, context_3d_cb_));
}

void WebMediaPlayerCast::setPoster(const blink::WebURL& poster) {
  player_manager_->SetPoster(player_id_, poster);
}

}  // namespace media
