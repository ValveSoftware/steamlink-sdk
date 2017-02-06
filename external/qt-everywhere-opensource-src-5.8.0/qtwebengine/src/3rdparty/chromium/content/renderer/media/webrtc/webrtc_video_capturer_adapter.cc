// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"

#include "base/bind.h"
#include "base/memory/aligned_memory.h"
#include "base/trace_event/trace_event.h"
#include "content/renderer/media/webrtc/webrtc_video_frame_adapter.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "media/base/video_util.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/webrtc/common_video/include/video_frame_buffer.h"
#include "third_party/webrtc/common_video/rotation.h"
#include "third_party/webrtc/media/base/videoframefactory.h"
#include "third_party/webrtc/media/engine/webrtcvideoframe.h"

namespace content {
namespace {

// Empty method used for keeping a reference to the original media::VideoFrame.
// The reference to |frame| is kept in the closure that calls this method.
void ReleaseOriginalFrame(const scoped_refptr<media::VideoFrame>& frame) {
}

}  // anonymous namespace

// A cricket::VideoFrameFactory for media::VideoFrame. The purpose of this
// class is to avoid a premature frame copy. A media::VideoFrame is injected
// with SetFrame, and converted into a cricket::VideoFrame with
// CreateAliasedFrame. SetFrame should be called before CreateAliasedFrame
// for every frame.
class WebRtcVideoCapturerAdapter::MediaVideoFrameFactory
    : public cricket::VideoFrameFactory {
 public:
  void SetFrame(const scoped_refptr<media::VideoFrame>& frame) {
    DCHECK(frame.get());
    // Create a CapturedFrame that only contains header information, not the
    // actual pixel data.
    captured_frame_.width = frame->natural_size().width();
    captured_frame_.height = frame->natural_size().height();
    captured_frame_.time_stamp = frame->timestamp().InMicroseconds() *
                                 base::Time::kNanosecondsPerMicrosecond;
    captured_frame_.pixel_height = 1;
    captured_frame_.pixel_width = 1;
    captured_frame_.rotation = webrtc::kVideoRotation_0;
    captured_frame_.data = NULL;
    captured_frame_.data_size = cricket::CapturedFrame::kUnknownDataSize;
    captured_frame_.fourcc = static_cast<uint32_t>(cricket::FOURCC_ANY);

    frame_ = frame;
  }

  void ReleaseFrame() { frame_ = NULL; }

  const cricket::CapturedFrame* GetCapturedFrame() const {
    return &captured_frame_;
  }

  cricket::VideoFrame* CreateAliasedFrame(
      const cricket::CapturedFrame* input_frame,
      int cropped_input_width,
      int cropped_input_height,
      int output_width,
      int output_height) const override {
    // Check that captured_frame is actually our frame.
    DCHECK(input_frame == &captured_frame_);
    DCHECK(frame_.get());

    const int64_t timestamp_ns = frame_->timestamp().InMicroseconds() *
                                 base::Time::kNanosecondsPerMicrosecond;

    // Return |frame_| directly if it is texture backed, because there is no
    // cropping support for texture yet. See http://crbug/503653.
    // Return |frame_| directly if it is GpuMemoryBuffer backed, as we want to
    // keep the frame on native buffers.
    if (frame_->HasTextures() ||
        frame_->storage_type() ==
            media::VideoFrame::STORAGE_GPU_MEMORY_BUFFERS) {
      return new cricket::WebRtcVideoFrame(
          new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(frame_),
          timestamp_ns, webrtc::kVideoRotation_0);
    }

    // Create a centered cropped visible rect that preservers aspect ratio for
    // cropped natural size.
    gfx::Rect visible_rect = frame_->visible_rect();
    visible_rect.ClampToCenteredSize(gfx::Size(
        visible_rect.width() * cropped_input_width / input_frame->width,
        visible_rect.height() * cropped_input_height / input_frame->height));

    const gfx::Size output_size(output_width, output_height);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::WrapVideoFrame(frame_, frame_->format(),
                                          visible_rect, output_size);
    if (!video_frame)
      return nullptr;
    video_frame->AddDestructionObserver(
        base::Bind(&ReleaseOriginalFrame, frame_));

    // If no scaling is needed, return a wrapped version of |frame_| directly.
    if (video_frame->natural_size() == video_frame->visible_rect().size()) {
      return new cricket::WebRtcVideoFrame(
          new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(video_frame),
          timestamp_ns, webrtc::kVideoRotation_0);
    }

    // We need to scale the frame before we hand it over to cricket.
    scoped_refptr<media::VideoFrame> scaled_frame =
        scaled_frame_pool_.CreateFrame(media::PIXEL_FORMAT_I420, output_size,
                                       gfx::Rect(output_size), output_size,
                                       frame_->timestamp());
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
                      output_width, output_height, libyuv::kFilterBilinear);
    return new cricket::WebRtcVideoFrame(
        new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(scaled_frame),
        timestamp_ns, webrtc::kVideoRotation_0);
  }

  cricket::VideoFrame* CreateAliasedFrame(
      const cricket::CapturedFrame* input_frame,
      int output_width,
      int output_height) const override {
    return CreateAliasedFrame(input_frame, input_frame->width,
                              input_frame->height, output_width, output_height);
  }

 private:
  scoped_refptr<media::VideoFrame> frame_;
  cricket::CapturedFrame captured_frame_;
  // This is used only if scaling is needed.
  mutable media::VideoFramePool scaled_frame_pool_;
};

WebRtcVideoCapturerAdapter::WebRtcVideoCapturerAdapter(bool is_screencast)
    : is_screencast_(is_screencast),
      running_(false) {
  thread_checker_.DetachFromThread();
  // The base class takes ownership of the frame factory.
  set_frame_factory(new MediaVideoFrameFactory);
}

WebRtcVideoCapturerAdapter::~WebRtcVideoCapturerAdapter() {
  DVLOG(3) << " WebRtcVideoCapturerAdapter::dtor";
}

cricket::CaptureState WebRtcVideoCapturerAdapter::Start(
    const cricket::VideoFormat& capture_format) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!running_);
  DVLOG(3) << " WebRtcVideoCapturerAdapter::Start w = " << capture_format.width
           << " h = " << capture_format.height;

  running_ = true;
  return cricket::CS_RUNNING;
}

void WebRtcVideoCapturerAdapter::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << " WebRtcVideoCapturerAdapter::Stop ";
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
  DCHECK(!fourccs || fourccs->empty());
  if (fourccs)
    fourccs->push_back(cricket::FOURCC_I420);
  return fourccs != NULL;
}

bool WebRtcVideoCapturerAdapter::IsScreencast() const {
  return is_screencast_;
}

bool WebRtcVideoCapturerAdapter::GetBestCaptureFormat(
    const cricket::VideoFormat& desired,
    cricket::VideoFormat* best_format) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << " GetBestCaptureFormat:: "
           << " w = " << desired.width
           << " h = " << desired.height;

  // Capability enumeration is done in MediaStreamVideoSource. The adapter can
  // just use what is provided.
  // Use the desired format as the best format.
  best_format->width = desired.width;
  best_format->height = desired.height;
  best_format->fourcc = cricket::FOURCC_I420;
  best_format->interval = desired.interval;
  return true;
}

void WebRtcVideoCapturerAdapter::OnFrameCaptured(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("video", "WebRtcVideoCapturerAdapter::OnFrameCaptured");
  if (!(video_frame->IsMappable() &&
        (video_frame->format() == media::PIXEL_FORMAT_I420 ||
         video_frame->format() == media::PIXEL_FORMAT_YV12 ||
         video_frame->format() == media::PIXEL_FORMAT_YV12A))) {
    // Since connecting sources and sinks do not check the format, we need to
    // just ignore formats that we can not handle.
    NOTREACHED();
    return;
  }
  scoped_refptr<media::VideoFrame> frame = video_frame;
  // Drop alpha channel since we do not support it yet.
  if (frame->format() == media::PIXEL_FORMAT_YV12A)
    frame = media::WrapAsI420VideoFrame(video_frame);

  // Inject the frame via the VideoFrameFactory of base class.
  MediaVideoFrameFactory* media_video_frame_factory =
      reinterpret_cast<MediaVideoFrameFactory*>(frame_factory());
  media_video_frame_factory->SetFrame(frame);

  // This signals to libJingle that a new VideoFrame is available.
  SignalFrameCaptured(this, media_video_frame_factory->GetCapturedFrame());

  media_video_frame_factory->ReleaseFrame();  // Release the frame ASAP.
}

}  // namespace content
