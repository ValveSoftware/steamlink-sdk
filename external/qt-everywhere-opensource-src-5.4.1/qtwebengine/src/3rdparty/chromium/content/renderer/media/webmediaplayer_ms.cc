// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediaplayer_ms.h"

#include <limits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "cc/layers/video_layer.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/compositor_bindings/web_layer_impl.h"
#include "content/renderer/media/media_stream_audio_renderer.h"
#include "content/renderer/media/media_stream_renderer_factory.h"
#include "content/renderer/media/video_frame_provider.h"
#include "content/renderer/media/webmediaplayer_delegate.h"
#include "content/renderer/media/webmediaplayer_util.h"
#include "content/renderer/render_frame_impl.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkBitmap.h"

using blink::WebCanvas;
using blink::WebMediaPlayer;
using blink::WebRect;
using blink::WebSize;

namespace {

// This function copies a YV12 or NATIVE_TEXTURE to a new YV12
// media::VideoFrame.
scoped_refptr<media::VideoFrame> CopyFrameToYV12(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(frame->format() == media::VideoFrame::YV12 ||
         frame->format() == media::VideoFrame::I420 ||
         frame->format() == media::VideoFrame::NATIVE_TEXTURE);
  scoped_refptr<media::VideoFrame> new_frame =
      media::VideoFrame::CreateFrame(media::VideoFrame::YV12,
                                     frame->coded_size(),
                                     frame->visible_rect(),
                                     frame->natural_size(),
                                     frame->timestamp());

  if (frame->format() == media::VideoFrame::NATIVE_TEXTURE) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(frame->visible_rect().width(),
                          frame->visible_rect().height());
    frame->ReadPixelsFromNativeTexture(bitmap);

    media::CopyRGBToVideoFrame(
        reinterpret_cast<uint8*>(bitmap.getPixels()),
        bitmap.rowBytes(),
        frame->visible_rect(),
        new_frame.get());
  } else {
    size_t number_of_planes =
        media::VideoFrame::NumPlanes(frame->format());
    for (size_t i = 0; i < number_of_planes; ++i) {
      media::CopyPlane(i, frame->data(i), frame->stride(i),
                       frame->rows(i), new_frame.get());
    }
  }
  return new_frame;
}

}  // anonymous namespace

namespace content {

WebMediaPlayerMS::WebMediaPlayerMS(
    blink::WebFrame* frame,
    blink::WebMediaPlayerClient* client,
    base::WeakPtr<WebMediaPlayerDelegate> delegate,
    media::MediaLog* media_log,
    scoped_ptr<MediaStreamRendererFactory> factory)
    : frame_(frame),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      buffered_(static_cast<size_t>(1)),
      client_(client),
      delegate_(delegate),
      paused_(true),
      current_frame_used_(false),
      pending_repaint_(false),
      video_frame_provider_client_(NULL),
      received_first_frame_(false),
      total_frame_count_(0),
      dropped_frame_count_(0),
      media_log_(media_log),
      renderer_factory_(factory.Pass()) {
  DVLOG(1) << "WebMediaPlayerMS::ctor";
  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_CREATED));
}

WebMediaPlayerMS::~WebMediaPlayerMS() {
  DVLOG(1) << "WebMediaPlayerMS::dtor";
  DCHECK(thread_checker_.CalledOnValidThread());

  SetVideoFrameProviderClient(NULL);
  GetClient()->setWebLayer(NULL);

  if (video_frame_provider_.get())
    video_frame_provider_->Stop();

  if (audio_renderer_.get())
    audio_renderer_->Stop();

  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));

  if (delegate_.get())
    delegate_->PlayerGone(this);
}

void WebMediaPlayerMS::load(LoadType load_type,
                            const blink::WebURL& url,
                            CORSMode cors_mode) {
  DVLOG(1) << "WebMediaPlayerMS::load";
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(acolwell): Change this to DCHECK_EQ(load_type,
  // LoadTypeMediaStream) once Blink-side changes land.
  DCHECK_NE(load_type, LoadTypeMediaSource);

  GURL gurl(url);

  setVolume(GetClient()->volume());
  SetNetworkState(WebMediaPlayer::NetworkStateLoading);
  SetReadyState(WebMediaPlayer::ReadyStateHaveNothing);
  media_log_->AddEvent(media_log_->CreateLoadEvent(url.spec()));

  video_frame_provider_ = renderer_factory_->GetVideoFrameProvider(
      url,
      base::Bind(&WebMediaPlayerMS::OnSourceError, AsWeakPtr()),
      base::Bind(&WebMediaPlayerMS::OnFrameAvailable, AsWeakPtr()));

  RenderFrame* frame = RenderFrame::FromWebFrame(frame_);
  audio_renderer_ = renderer_factory_->GetAudioRenderer(
    url,
    frame->GetRenderView()->GetRoutingID(),
    frame->GetRoutingID());

  if (video_frame_provider_.get() || audio_renderer_.get()) {
    if (audio_renderer_.get())
      audio_renderer_->Start();

    if (video_frame_provider_.get()) {
      video_frame_provider_->Start();
    } else {
      // This is audio-only mode.
      DCHECK(audio_renderer_.get());
      SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
      SetReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);
    }
  } else {
    SetNetworkState(WebMediaPlayer::NetworkStateNetworkError);
  }
}

void WebMediaPlayerMS::play() {
  DVLOG(1) << "WebMediaPlayerMS::play";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (paused_) {
    if (video_frame_provider_.get())
      video_frame_provider_->Play();

    if (audio_renderer_.get())
      audio_renderer_->Play();

    if (delegate_.get())
      delegate_->DidPlay(this);
  }

  paused_ = false;

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PLAY));
}

void WebMediaPlayerMS::pause() {
  DVLOG(1) << "WebMediaPlayerMS::pause";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (video_frame_provider_.get())
    video_frame_provider_->Pause();

  if (!paused_) {
    if (audio_renderer_.get())
      audio_renderer_->Pause();

    if (delegate_.get())
      delegate_->DidPause(this);
  }

  paused_ = true;

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PAUSE));

  if (!current_frame_)
    return;

  // Copy the frame so that rendering can show the last received frame.
  // The original frame must not be referenced when the player is paused since
  // there might be a finite number of available buffers. E.g, video that
  // originates from a video camera.
  scoped_refptr<media::VideoFrame> new_frame = CopyFrameToYV12(current_frame_);
  base::AutoLock auto_lock(current_frame_lock_);
  current_frame_ = new_frame;
}

bool WebMediaPlayerMS::supportsSave() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return false;
}

void WebMediaPlayerMS::seek(double seconds) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebMediaPlayerMS::setRate(double rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebMediaPlayerMS::setVolume(double volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!audio_renderer_.get())
    return;
  DVLOG(1) << "WebMediaPlayerMS::setVolume(volume=" << volume << ")";
  audio_renderer_->SetVolume(volume);
}

void WebMediaPlayerMS::setPreload(WebMediaPlayer::Preload preload) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool WebMediaPlayerMS::hasVideo() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (video_frame_provider_.get() != NULL);
}

bool WebMediaPlayerMS::hasAudio() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (audio_renderer_.get() != NULL);
}

blink::WebSize WebMediaPlayerMS::naturalSize() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  gfx::Size size;
  if (current_frame_.get())
    size = current_frame_->natural_size();
  DVLOG(3) << "WebMediaPlayerMS::naturalSize, " << size.ToString();
  return blink::WebSize(size);
}

bool WebMediaPlayerMS::paused() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return paused_;
}

bool WebMediaPlayerMS::seeking() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return false;
}

double WebMediaPlayerMS::duration() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::numeric_limits<double>::infinity();
}

double WebMediaPlayerMS::currentTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (current_time_.ToInternalValue() != 0) {
    return current_time_.InSecondsF();
  } else if (audio_renderer_.get()) {
    return audio_renderer_->GetCurrentRenderTime().InSecondsF();
  }
  return 0.0;
}

WebMediaPlayer::NetworkState WebMediaPlayerMS::networkState() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebMediaPlayerMS::networkState, state:" << network_state_;
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerMS::readyState() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebMediaPlayerMS::readyState, state:" << ready_state_;
  return ready_state_;
}

blink::WebTimeRanges WebMediaPlayerMS::buffered() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return buffered_;
}

double WebMediaPlayerMS::maxTimeSeekable() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return 0.0;
}

bool WebMediaPlayerMS::didLoadingProgress() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

void WebMediaPlayerMS::paint(WebCanvas* canvas,
                             const WebRect& rect,
                             unsigned char alpha) {
  DVLOG(3) << "WebMediaPlayerMS::paint";
  DCHECK(thread_checker_.CalledOnValidThread());

  gfx::RectF dest_rect(rect.x, rect.y, rect.width, rect.height);
  video_renderer_.Paint(current_frame_.get(), canvas, dest_rect, alpha);

  {
    base::AutoLock auto_lock(current_frame_lock_);
    if (current_frame_.get())
      current_frame_used_ = true;
  }
}

bool WebMediaPlayerMS::hasSingleSecurityOrigin() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

bool WebMediaPlayerMS::didPassCORSAccessCheck() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

double WebMediaPlayerMS::mediaTimeForTimeValue(double timeValue) const {
  return ConvertSecondsToTimestamp(timeValue).InSecondsF();
}

unsigned WebMediaPlayerMS::decodedFrameCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebMediaPlayerMS::decodedFrameCount, " << total_frame_count_;
  return total_frame_count_;
}

unsigned WebMediaPlayerMS::droppedFrameCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebMediaPlayerMS::droppedFrameCount, " << dropped_frame_count_;
  return dropped_frame_count_;
}

unsigned WebMediaPlayerMS::audioDecodedByteCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerMS::videoDecodedByteCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return 0;
}

void WebMediaPlayerMS::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  // This is called from both the main renderer thread and the compositor
  // thread (when the main thread is blocked).
  if (video_frame_provider_client_)
    video_frame_provider_client_->StopUsingProvider();
  video_frame_provider_client_ = client;
}

scoped_refptr<media::VideoFrame> WebMediaPlayerMS::GetCurrentFrame() {
  DVLOG(3) << "WebMediaPlayerMS::GetCurrentFrame";
  base::AutoLock auto_lock(current_frame_lock_);
  DCHECK(!pending_repaint_);
  if (!current_frame_.get())
    return NULL;
  pending_repaint_ = true;
  current_frame_used_ = true;
  return current_frame_;
}

void WebMediaPlayerMS::PutCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  DVLOG(3) << "WebMediaPlayerMS::PutCurrentFrame";
  DCHECK(pending_repaint_);
  pending_repaint_ = false;
}

void WebMediaPlayerMS::OnFrameAvailable(
    const scoped_refptr<media::VideoFrame>& frame) {
  DVLOG(3) << "WebMediaPlayerMS::OnFrameAvailable";
  DCHECK(thread_checker_.CalledOnValidThread());
  ++total_frame_count_;
  if (!received_first_frame_) {
    received_first_frame_ = true;
    {
      base::AutoLock auto_lock(current_frame_lock_);
      DCHECK(!current_frame_used_);
      current_frame_ = frame;
    }
    SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);
    SetReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);
    GetClient()->sizeChanged();

    if (video_frame_provider_) {
      video_weblayer_.reset(new WebLayerImpl(cc::VideoLayer::Create(this)));
      video_weblayer_->setOpaque(true);
      GetClient()->setWebLayer(video_weblayer_.get());
    }
  }

  // Do not update |current_frame_| when paused.
  if (paused_)
    return;

  bool size_changed = !current_frame_.get() ||
                      current_frame_->natural_size() != frame->natural_size();

  {
    base::AutoLock auto_lock(current_frame_lock_);
    if (!current_frame_used_ && current_frame_.get())
      ++dropped_frame_count_;
    current_frame_ = frame;
    current_time_ = frame->timestamp();
    current_frame_used_ = false;
  }

  if (size_changed)
    GetClient()->sizeChanged();

  GetClient()->repaint();
}

void WebMediaPlayerMS::RepaintInternal() {
  DVLOG(1) << "WebMediaPlayerMS::RepaintInternal";
  DCHECK(thread_checker_.CalledOnValidThread());
  GetClient()->repaint();
}

void WebMediaPlayerMS::OnSourceError() {
  DVLOG(1) << "WebMediaPlayerMS::OnSourceError";
  DCHECK(thread_checker_.CalledOnValidThread());
  SetNetworkState(WebMediaPlayer::NetworkStateFormatError);
  RepaintInternal();
}

void WebMediaPlayerMS::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->networkStateChanged();
}

void WebMediaPlayerMS::SetReadyState(WebMediaPlayer::ReadyState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ready_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->readyStateChanged();
}

blink::WebMediaPlayerClient* WebMediaPlayerMS::GetClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(client_);
  return client_;
}

}  // namespace content
