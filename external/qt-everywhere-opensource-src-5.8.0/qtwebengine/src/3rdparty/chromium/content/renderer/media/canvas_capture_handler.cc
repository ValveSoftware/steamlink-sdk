// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/canvas_capture_handler.h"

#include <utility>

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkImage.h"

namespace {

using media::VideoFrame;

static void CopyAlphaChannelIntoVideoFrame(
    const uint8_t* const source_data,
    const gfx::Size& source_size,
    const scoped_refptr<VideoFrame>& dest_frame) {
  const int dest_stride = dest_frame->stride(VideoFrame::kAPlane);
  if (dest_stride == source_size.width()) {
    for (int p = 0; p < source_size.GetArea(); ++p) {
      dest_frame->visible_data(VideoFrame::kAPlane)[p] = source_data[p * 4 + 3];
    }
    return;
  }

  // Copy alpha values one-by-one if the destination stride != source width.
  for (int h = 0; h < source_size.height(); ++h) {
    const uint8_t* const src_ptr = &source_data[4 * h * source_size.width()];
    uint8_t* dest_ptr =
        &dest_frame->visible_data(VideoFrame::kAPlane)[h * dest_stride];
    for (int pixel = 0; pixel < 4 * source_size.width(); pixel += 4)
      *(dest_ptr++) = src_ptr[pixel + 3];
  }
}

}  // namespace

namespace content {

// Implementation VideoCapturerSource that is owned by
// MediaStreamVideoCapturerSource and delegates the Start/Stop calls to
// CanvasCaptureHandler.
// This class is single threaded and pinned to main render thread.
class VideoCapturerSource : public media::VideoCapturerSource {
 public:
  explicit VideoCapturerSource(base::WeakPtr<CanvasCaptureHandler>
                                   canvas_handler,
                               double frame_rate)
      : frame_rate_(frame_rate),
        canvas_handler_(canvas_handler) {}

 protected:
  void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      double max_requested_frame_rate,
      const VideoCaptureDeviceFormatsCB& callback) override {
    DCHECK(main_render_thread_checker_.CalledOnValidThread());
    const blink::WebSize& size = canvas_handler_->GetSourceSize();
    media::VideoCaptureFormats formats;
    formats.push_back(
        media::VideoCaptureFormat(gfx::Size(size.width, size.height),
                                  frame_rate_, media::PIXEL_FORMAT_I420));
    formats.push_back(
        media::VideoCaptureFormat(gfx::Size(size.width, size.height),
                                  frame_rate_, media::PIXEL_FORMAT_YV12A));
    callback.Run(formats);
  }
  void StartCapture(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& frame_callback,
                    const RunningCallback& running_callback) override {
    DCHECK(main_render_thread_checker_.CalledOnValidThread());
    canvas_handler_->StartVideoCapture(params, frame_callback,
                                       running_callback);
  }
  void RequestRefreshFrame() override {
    DCHECK(main_render_thread_checker_.CalledOnValidThread());
    canvas_handler_->RequestRefreshFrame();
  }
  void StopCapture() override {
    DCHECK(main_render_thread_checker_.CalledOnValidThread());
    if (canvas_handler_.get())
      canvas_handler_->StopVideoCapture();
  }

 private:
  const double frame_rate_;
  // Bound to Main Render thread.
  base::ThreadChecker main_render_thread_checker_;
  // CanvasCaptureHandler is owned by CanvasDrawListener in blink and might be
  // destroyed before StopCapture() call.
  base::WeakPtr<CanvasCaptureHandler> canvas_handler_;
};

class CanvasCaptureHandler::CanvasCaptureHandlerDelegate {
 public:
  explicit CanvasCaptureHandlerDelegate(
      media::VideoCapturerSource::VideoCaptureDeliverFrameCB new_frame_callback)
      : new_frame_callback_(new_frame_callback),
        weak_ptr_factory_(this) {
    io_thread_checker_.DetachFromThread();
  }
  ~CanvasCaptureHandlerDelegate() {
    DCHECK(io_thread_checker_.CalledOnValidThread());
  }

  void SendNewFrameOnIOThread(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& current_time) {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    new_frame_callback_.Run(video_frame, current_time);
  }

  base::WeakPtr<CanvasCaptureHandlerDelegate> GetWeakPtrForIOThread() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const media::VideoCapturerSource::VideoCaptureDeliverFrameCB
      new_frame_callback_;
  // Bound to IO thread.
  base::ThreadChecker io_thread_checker_;
  base::WeakPtrFactory<CanvasCaptureHandlerDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CanvasCaptureHandlerDelegate);
};

CanvasCaptureHandler::CanvasCaptureHandler(
    const blink::WebSize& size,
    double frame_rate,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    blink::WebMediaStreamTrack* track)
    : ask_for_new_frame_(false),
      size_(size),
      io_task_runner_(io_task_runner),
      weak_ptr_factory_(this) {
  std::unique_ptr<media::VideoCapturerSource> video_source(
      new VideoCapturerSource(weak_ptr_factory_.GetWeakPtr(), frame_rate));
  AddVideoCapturerSourceToVideoTrack(std::move(video_source), track);
}

CanvasCaptureHandler::~CanvasCaptureHandler() {
  DVLOG(3) << __FUNCTION__;
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  io_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());
}

// static
CanvasCaptureHandler* CanvasCaptureHandler::CreateCanvasCaptureHandler(
    const blink::WebSize& size,
    double frame_rate,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    blink::WebMediaStreamTrack* track) {
  // Save histogram data so we can see how much CanvasCapture is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(WEBKIT_CANVAS_CAPTURE_STREAM);

  return new CanvasCaptureHandler(size, frame_rate, io_task_runner, track);
}

void CanvasCaptureHandler::sendNewFrame(const SkImage* image) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  CreateNewFrame(image);
}

bool CanvasCaptureHandler::needsNewFrame() const {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  return ask_for_new_frame_;
}

void CanvasCaptureHandler::StartVideoCapture(
    const media::VideoCaptureParams& params,
    const media::VideoCapturerSource::VideoCaptureDeliverFrameCB&
        new_frame_callback,
    const media::VideoCapturerSource::RunningCallback& running_callback) {
  DVLOG(3) << __FUNCTION__ << " requested "
           << media::VideoCaptureFormat::ToString(params.requested_format);
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(params.requested_format.IsValid());

  // TODO(emircan): Accomodate to the given frame rate constraints here.
  capture_format_ = params.requested_format;
  delegate_.reset(new CanvasCaptureHandlerDelegate(new_frame_callback));
  DCHECK(delegate_);
  ask_for_new_frame_ = true;
  running_callback.Run(true);
}

void CanvasCaptureHandler::RequestRefreshFrame() {
  DVLOG(3) << __FUNCTION__;
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (last_frame_ && delegate_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CanvasCaptureHandler::CanvasCaptureHandlerDelegate::
                       SendNewFrameOnIOThread,
                   delegate_->GetWeakPtrForIOThread(), last_frame_,
                   base::TimeTicks::Now()));
  }
}

void CanvasCaptureHandler::StopVideoCapture() {
  DVLOG(3) << __FUNCTION__;
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  ask_for_new_frame_ = false;
  io_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());
}

void CanvasCaptureHandler::CreateNewFrame(const SkImage* image) {
  DVLOG(4) << __FUNCTION__;
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(image);

  const gfx::Size size(image->width(), image->height());
  if (size != last_size) {
    temp_data_.resize(
        media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_ARGB, size));
    row_bytes_ =
        media::VideoFrame::RowBytes(0, media::PIXEL_FORMAT_ARGB, size.width());
    image_info_ =
        SkImageInfo::Make(size.width(), size.height(), kBGRA_8888_SkColorType,
                          kUnpremul_SkAlphaType);
    last_size = size;
  }

  if(!image->readPixels(image_info_, &temp_data_[0], row_bytes_, 0, 0)) {
    DLOG(ERROR) << "Couldn't read SkImage pixels";
    return;
  }

  const bool isOpaque = image->isOpaque();
  const base::TimeTicks timestamp = base::TimeTicks::Now();
  scoped_refptr<media::VideoFrame> video_frame = frame_pool_.CreateFrame(
      isOpaque ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_YV12A, size,
      gfx::Rect(size), size, timestamp - base::TimeTicks());
  DCHECK(video_frame);

  libyuv::ARGBToI420(temp_data_.data(), row_bytes_,
                     video_frame->visible_data(media::VideoFrame::kYPlane),
                     video_frame->stride(media::VideoFrame::kYPlane),
                     video_frame->visible_data(media::VideoFrame::kUPlane),
                     video_frame->stride(media::VideoFrame::kUPlane),
                     video_frame->visible_data(media::VideoFrame::kVPlane),
                     video_frame->stride(media::VideoFrame::kVPlane),
                     size.width(), size.height());
  if (!isOpaque) {
    // TODO(emircan): Use https://code.google.com/p/libyuv/issues/detail?id=572
    // when it becomes available.
    CopyAlphaChannelIntoVideoFrame(temp_data_.data(), size, video_frame);
  }

  last_frame_ = video_frame;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CanvasCaptureHandler::CanvasCaptureHandlerDelegate::
                     SendNewFrameOnIOThread,
                 delegate_->GetWeakPtrForIOThread(), video_frame, timestamp));
}

void CanvasCaptureHandler::AddVideoCapturerSourceToVideoTrack(
    std::unique_ptr<media::VideoCapturerSource> source,
    blink::WebMediaStreamTrack* web_track) {
  std::string str_track_id;
  base::Base64Encode(base::RandBytesAsString(64), &str_track_id);
  const blink::WebString track_id = base::UTF8ToUTF16(str_track_id);
  blink::WebMediaStreamSource webkit_source;
  std::unique_ptr<MediaStreamVideoSource> media_stream_source(
      new MediaStreamVideoCapturerSource(
          MediaStreamSource::SourceStoppedCallback(), std::move(source)));
  webkit_source.initialize(track_id, blink::WebMediaStreamSource::TypeVideo,
                           track_id, false);
  webkit_source.setExtraData(media_stream_source.get());

  web_track->initialize(webkit_source);
  blink::WebMediaConstraints constraints;
  constraints.initialize();
  web_track->setTrackData(new MediaStreamVideoTrack(
      media_stream_source.release(), constraints,
      MediaStreamVideoSource::ConstraintsCallback(), true));
}

}  // namespace content
