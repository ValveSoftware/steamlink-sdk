// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/memory/aligned_memory.h"
#include "media/base/video_frame.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace content {

WebRtcVideoCapturerAdapter::WebRtcVideoCapturerAdapter(bool is_screencast)
    : is_screencast_(is_screencast),
      running_(false),
      buffer_(NULL),
      buffer_size_(0) {
  thread_checker_.DetachFromThread();
}

WebRtcVideoCapturerAdapter::~WebRtcVideoCapturerAdapter() {
  DVLOG(3) << " WebRtcVideoCapturerAdapter::dtor";
  base::AlignedFree(buffer_);
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
    std::vector<uint32>* fourccs) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!fourccs)
    return false;
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
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("video", "WebRtcVideoCapturerAdapter::OnFrameCaptured");
  if (!(media::VideoFrame::I420 == frame->format() ||
        media::VideoFrame::YV12 == frame->format())) {
    // Some types of sources support textures as output. Since connecting
    // sources and sinks do not check the format, we need to just ignore
    // formats that we can not handle.
    NOTREACHED();
    return;
  }

  if (first_frame_timestamp_ == media::kNoTimestamp())
    first_frame_timestamp_ = frame->timestamp();

  cricket::CapturedFrame captured_frame;
  captured_frame.width = frame->natural_size().width();
  captured_frame.height = frame->natural_size().height();
  // cricket::CapturedFrame time is in nanoseconds.
  captured_frame.elapsed_time =
      (frame->timestamp() - first_frame_timestamp_).InMicroseconds() *
      base::Time::kNanosecondsPerMicrosecond;
  captured_frame.time_stamp = frame->timestamp().InMicroseconds() *
                              base::Time::kNanosecondsPerMicrosecond;
  captured_frame.pixel_height = 1;
  captured_frame.pixel_width = 1;

  // TODO(perkj):
  // Libjingle expects contiguous layout of image planes as input.
  // The only format where that is true in Chrome is I420 where the
  // coded_size == natural_size().
  if (frame->format() != media::VideoFrame::I420 ||
      frame->coded_size() != frame->natural_size()) {
    // Cropping / Scaling and or switching UV planes is needed.
    UpdateI420Buffer(frame);
    captured_frame.data = buffer_;
    captured_frame.data_size = buffer_size_;
    captured_frame.fourcc = cricket::FOURCC_I420;
  } else {
    captured_frame.fourcc = media::VideoFrame::I420 == frame->format() ?
        cricket::FOURCC_I420 : cricket::FOURCC_YV12;
    captured_frame.data = frame->data(0);
    captured_frame.data_size =
        media::VideoFrame::AllocationSize(frame->format(), frame->coded_size());
  }

  // This signals to libJingle that a new VideoFrame is available.
  // libJingle have no assumptions on what thread this signal come from.
  SignalFrameCaptured(this, &captured_frame);
}

void WebRtcVideoCapturerAdapter::UpdateI420Buffer(
    const scoped_refptr<media::VideoFrame>& src) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const int dst_width = src->natural_size().width();
  const int dst_height = src->natural_size().height();
  DCHECK(src->visible_rect().width() >= dst_width &&
         src->visible_rect().height() >= dst_height);

  const gfx::Rect& visible_rect = src->visible_rect();

  const uint8* src_y = src->data(media::VideoFrame::kYPlane) +
      visible_rect.y() * src->stride(media::VideoFrame::kYPlane) +
      visible_rect.x();
  const uint8* src_u = src->data(media::VideoFrame::kUPlane) +
      visible_rect.y() / 2 * src->stride(media::VideoFrame::kUPlane) +
      visible_rect.x() / 2;
  const uint8* src_v = src->data(media::VideoFrame::kVPlane) +
      visible_rect.y() / 2 * src->stride(media::VideoFrame::kVPlane) +
      visible_rect.x() / 2;

  const size_t dst_size =
      media::VideoFrame::AllocationSize(src->format(), src->natural_size());

  if (dst_size != buffer_size_) {
    base::AlignedFree(buffer_);
    buffer_ = reinterpret_cast<uint8*>(
        base::AlignedAlloc(dst_size + media::VideoFrame::kFrameSizePadding,
                           media::VideoFrame::kFrameAddressAlignment));
    buffer_size_ = dst_size;
  }

  uint8* dst_y = buffer_;
  const int dst_stride_y = dst_width;
  uint8* dst_u = dst_y + dst_width * dst_height;
  const int dst_halfwidth = (dst_width + 1) / 2;
  const int dst_halfheight = (dst_height + 1) / 2;
  uint8* dst_v = dst_u + dst_halfwidth * dst_halfheight;

  libyuv::I420Scale(src_y,
                    src->stride(media::VideoFrame::kYPlane),
                    src_u,
                    src->stride(media::VideoFrame::kUPlane),
                    src_v,
                    src->stride(media::VideoFrame::kVPlane),
                    visible_rect.width(),
                    visible_rect.height(),
                    dst_y,
                    dst_stride_y,
                    dst_u,
                    dst_halfwidth,
                    dst_v,
                    dst_halfwidth,
                    dst_width,
                    dst_height,
                    libyuv::kFilterBilinear);
}

}  // namespace content
