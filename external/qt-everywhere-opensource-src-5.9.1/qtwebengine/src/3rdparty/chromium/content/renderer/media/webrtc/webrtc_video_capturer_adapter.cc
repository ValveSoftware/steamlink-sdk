// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"

#include "base/bind.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/renderer/media/webrtc/webrtc_video_frame_adapter.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_util.h"
#include "media/renderers/skcanvas_video_renderer.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/webrtc/common_video/rotation.h"

namespace content {

namespace {

// Empty method used for keeping a reference to the original media::VideoFrame.
// The reference to |frame| is kept in the closure that calls this method.
void ReleaseOriginalFrame(const scoped_refptr<media::VideoFrame>& frame) {
}

// Helper class that signals a WaitableEvent when it goes out of scope.
class ScopedWaitableEvent {
 public:
  explicit ScopedWaitableEvent(base::WaitableEvent* event) : event_(event) {}
  ~ScopedWaitableEvent() {
    if (event_)
      event_->Signal();
  }

 private:
  base::WaitableEvent* const event_;
};

}  // anonymous namespace

// Initializes the GL context environment and provides a method for copying
// texture backed frames into CPU mappable memory.
// The class is created and destroyed on the main render thread.
class WebRtcVideoCapturerAdapter::TextureFrameCopier
    : public base::RefCounted<WebRtcVideoCapturerAdapter::TextureFrameCopier> {
 public:
  TextureFrameCopier()
      : main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        canvas_video_renderer_(new media::SkCanvasVideoRenderer) {
    RenderThreadImpl* const main_thread = RenderThreadImpl::current();
    if (main_thread)
      provider_ = main_thread->SharedMainThreadContextProvider();
  }

  // Synchronous call to copy a texture backed |frame| into a CPU mappable
  // |new_frame|. If it is not called on the main render thread, this call posts
  // a task on main thread by calling CopyTextureFrameOnMainThread() and blocks
  // until it is completed.
  void CopyTextureFrame(const scoped_refptr<media::VideoFrame>& frame,
                        scoped_refptr<media::VideoFrame>* new_frame) {
    if (main_thread_task_runner_->BelongsToCurrentThread()) {
      CopyTextureFrameOnMainThread(frame, new_frame, nullptr);
      return;
    }

    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&TextureFrameCopier::CopyTextureFrameOnMainThread,
                              this, frame, new_frame, &waiter));
    waiter.Wait();
  }

 private:
  friend class base::RefCounted<TextureFrameCopier>;
  ~TextureFrameCopier() {
    // |canvas_video_renderer_| should be deleted on the thread it was created.
    if (!main_thread_task_runner_->BelongsToCurrentThread()) {
      main_thread_task_runner_->DeleteSoon(FROM_HERE,
                                           canvas_video_renderer_.release());
    }
  }

  void CopyTextureFrameOnMainThread(
      const scoped_refptr<media::VideoFrame>& frame,
      scoped_refptr<media::VideoFrame>* new_frame,
      base::WaitableEvent* waiter) {
    DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
    DCHECK(frame->format() == media::PIXEL_FORMAT_ARGB ||
           frame->format() == media::PIXEL_FORMAT_XRGB ||
           frame->format() == media::PIXEL_FORMAT_I420 ||
           frame->format() == media::PIXEL_FORMAT_UYVY ||
           frame->format() == media::PIXEL_FORMAT_NV12);
    ScopedWaitableEvent event(waiter);
    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(
        frame->visible_rect().width(), frame->visible_rect().height());

    if (!surface || !provider_) {
      // Return a black frame (yuv = {0, 0x80, 0x80}).
      *new_frame = media::VideoFrame::CreateColorFrame(
          frame->visible_rect().size(), 0u, 0x80, 0x80, frame->timestamp());
      return;
    }

    *new_frame = media::VideoFrame::CreateFrame(
        media::PIXEL_FORMAT_I420, frame->coded_size(), frame->visible_rect(),
        frame->natural_size(), frame->timestamp());
    DCHECK(provider_->ContextGL());
    canvas_video_renderer_->Copy(
        frame.get(), surface->getCanvas(),
        media::Context3D(provider_->ContextGL(), provider_->GrContext()));

    SkPixmap pixmap;
    const bool result = surface->getCanvas()->peekPixels(&pixmap);
    DCHECK(result) << "Error trying to access SkSurface's pixels";
    const uint32 source_pixel_format =
        (kN32_SkColorType == kRGBA_8888_SkColorType) ? cricket::FOURCC_ABGR
                                                     : cricket::FOURCC_ARGB;
    libyuv::ConvertToI420(
        static_cast<const uint8*>(pixmap.addr(0, 0)), pixmap.getSafeSize64(),
        (*new_frame)->visible_data(media::VideoFrame::kYPlane),
        (*new_frame)->stride(media::VideoFrame::kYPlane),
        (*new_frame)->visible_data(media::VideoFrame::kUPlane),
        (*new_frame)->stride(media::VideoFrame::kUPlane),
        (*new_frame)->visible_data(media::VideoFrame::kVPlane),
        (*new_frame)->stride(media::VideoFrame::kVPlane), 0 /* crop_x */,
        0 /* crop_y */, pixmap.width(), pixmap.height(),
        (*new_frame)->visible_rect().width(),
        (*new_frame)->visible_rect().height(), libyuv::kRotate0,
        source_pixel_format);
  }

  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<ContextProviderCommandBuffer> provider_;
  std::unique_ptr<media::SkCanvasVideoRenderer> canvas_video_renderer_;
};

WebRtcVideoCapturerAdapter::WebRtcVideoCapturerAdapter(bool is_screencast)
    : texture_copier_(new WebRtcVideoCapturerAdapter::TextureFrameCopier()),
      is_screencast_(is_screencast),
      running_(false) {
  thread_checker_.DetachFromThread();
}

WebRtcVideoCapturerAdapter::~WebRtcVideoCapturerAdapter() {
  DVLOG(3) << __func__;
}

void WebRtcVideoCapturerAdapter::OnFrameCaptured(
    const scoped_refptr<media::VideoFrame>& input_frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("video", "WebRtcVideoCapturerAdapter::OnFrameCaptured");
  if (!(input_frame->IsMappable() &&
        (input_frame->format() == media::PIXEL_FORMAT_I420 ||
         input_frame->format() == media::PIXEL_FORMAT_YV12 ||
         input_frame->format() == media::PIXEL_FORMAT_YV12A)) &&
      !input_frame->HasTextures()) {
    // Since connecting sources and sinks do not check the format, we need to
    // just ignore formats that we can not handle.
    LOG(ERROR) << "We cannot send frame with storage type: "
               << input_frame->AsHumanReadableString();
    NOTREACHED();
    return;
  }
  scoped_refptr<media::VideoFrame> frame = input_frame;
  // Drop alpha channel since we do not support it yet.
  if (frame->format() == media::PIXEL_FORMAT_YV12A)
    frame = media::WrapAsI420VideoFrame(input_frame);

  const int orig_width = frame->natural_size().width();
  const int orig_height = frame->natural_size().height();
  int adapted_width;
  int adapted_height;
  // The VideoAdapter is only used for cpu-adaptation downscaling, no
  // aspect changes. So we ignore these crop-related outputs.
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;
  int64_t translated_camera_time_us;

  if (!AdaptFrame(orig_width, orig_height,
                  frame->timestamp().InMicroseconds(),
                  rtc::TimeMicros(),
                  &adapted_width, &adapted_height,
                  &crop_width, &crop_height, &crop_x, &crop_y,
                  &translated_camera_time_us)) {
    return;
  }

  // Return |frame| directly if it is texture backed, because there is no
  // cropping support for texture yet. See http://crbug/503653.
  if (frame->HasTextures()) {
    OnFrame(webrtc::VideoFrame(
                new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
                    frame, base::Bind(&TextureFrameCopier::CopyTextureFrame,
                                      texture_copier_)),
                webrtc::kVideoRotation_0, translated_camera_time_us),
            orig_width, orig_height);
    return;
  }

  // Translate crop rectangle from natural size to visible size.
  gfx::Rect cropped_visible_rect(
      frame->visible_rect().x() +
          crop_x * frame->visible_rect().width() / orig_width,
      frame->visible_rect().y() +
          crop_y * frame->visible_rect().height() / orig_height,
      crop_width * frame->visible_rect().width() / orig_width,
      crop_height * frame->visible_rect().height() / orig_height);

  const gfx::Size adapted_size(adapted_width, adapted_height);
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::WrapVideoFrame(frame, frame->format(),
                                        cropped_visible_rect, adapted_size);
  if (!video_frame)
    return;

  video_frame->AddDestructionObserver(base::Bind(&ReleaseOriginalFrame, frame));

  // If no scaling is needed, return a wrapped version of |frame| directly.
  if (video_frame->natural_size() == video_frame->visible_rect().size()) {
    OnFrame(webrtc::VideoFrame(
                new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
                    video_frame,
                    WebRtcVideoFrameAdapter::CopyTextureFrameCallback()),
                webrtc::kVideoRotation_0, translated_camera_time_us),
            orig_width, orig_height);
    return;
  }

  // We need to scale the frame before we hand it over to webrtc.
  scoped_refptr<media::VideoFrame> scaled_frame =
      scaled_frame_pool_.CreateFrame(media::PIXEL_FORMAT_I420, adapted_size,
                                     gfx::Rect(adapted_size), adapted_size,
                                     frame->timestamp());
  libyuv::I420Scale(video_frame->visible_data(media::VideoFrame::kYPlane),
                    video_frame->stride(media::VideoFrame::kYPlane),
                    video_frame->visible_data(media::VideoFrame::kUPlane),
                    video_frame->stride(media::VideoFrame::kUPlane),
                    video_frame->visible_data(media::VideoFrame::kVPlane),
                    video_frame->stride(media::VideoFrame::kVPlane),
                    video_frame->visible_rect().width(),
                    video_frame->visible_rect().height(),
                    scaled_frame->data(media::VideoFrame::kYPlane),
                    scaled_frame->stride(media::VideoFrame::kYPlane),
                    scaled_frame->data(media::VideoFrame::kUPlane),
                    scaled_frame->stride(media::VideoFrame::kUPlane),
                    scaled_frame->data(media::VideoFrame::kVPlane),
                    scaled_frame->stride(media::VideoFrame::kVPlane),
                    adapted_width, adapted_height, libyuv::kFilterBilinear);

  OnFrame(webrtc::VideoFrame(
              new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
                  scaled_frame,
                  WebRtcVideoFrameAdapter::CopyTextureFrameCallback()),
              webrtc::kVideoRotation_0, translated_camera_time_us),
          orig_width, orig_height);
}

cricket::CaptureState WebRtcVideoCapturerAdapter::Start(
    const cricket::VideoFormat& capture_format) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!running_);
  DVLOG(3) << __func__ << " capture format: " << capture_format.ToString();

  running_ = true;
  return cricket::CS_RUNNING;
}

void WebRtcVideoCapturerAdapter::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << __func__;
  DCHECK(running_);
  running_ = false;
  SetCaptureFormat(NULL);
  SignalStateChange(this, cricket::CS_STOPPED);
}

bool WebRtcVideoCapturerAdapter::IsRunning() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return running_;
}

bool WebRtcVideoCapturerAdapter::GetPreferredFourccs(
    std::vector<uint32_t>* fourccs) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!fourccs)
    return false;
  DCHECK(fourccs->empty());
  fourccs->push_back(cricket::FOURCC_I420);
  return true;
}

bool WebRtcVideoCapturerAdapter::IsScreencast() const {
  return is_screencast_;
}

bool WebRtcVideoCapturerAdapter::GetBestCaptureFormat(
    const cricket::VideoFormat& desired,
    cricket::VideoFormat* best_format) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << __func__ << " desired: " << desired.ToString();

  // Capability enumeration is done in MediaStreamVideoSource. The adapter can
  // just use what is provided.
  // Use the desired format as the best format.
  best_format->width = desired.width;
  best_format->height = desired.height;
  best_format->fourcc = cricket::FOURCC_I420;
  best_format->interval = desired.interval;
  return true;
}

}  // namespace content
