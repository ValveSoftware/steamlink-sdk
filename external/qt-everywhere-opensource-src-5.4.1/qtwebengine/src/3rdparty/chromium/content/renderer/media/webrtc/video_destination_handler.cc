// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/video_destination_handler.h"

#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_registry_interface.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "media/video/capture/video_capture_types.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "url/gurl.h"

namespace content {

class PpFrameWriter::FrameWriterDelegate
    : public base::RefCountedThreadSafe<FrameWriterDelegate> {
 public:
  FrameWriterDelegate(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop_proxy,
      const VideoCaptureDeliverFrameCB& new_frame_callback);

  void DeliverFrame(const scoped_refptr<media::VideoFrame>& frame,
                    const media::VideoCaptureFormat& format);
 private:
  friend class base::RefCountedThreadSafe<FrameWriterDelegate>;
  virtual ~FrameWriterDelegate();

  void DeliverFrameOnIO(const scoped_refptr<media::VideoFrame>& frame,
                        const media::VideoCaptureFormat& format);

  scoped_refptr<base::MessageLoopProxy> io_message_loop_;
  VideoCaptureDeliverFrameCB new_frame_callback_;
};

PpFrameWriter::FrameWriterDelegate::FrameWriterDelegate(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop_proxy,
    const VideoCaptureDeliverFrameCB& new_frame_callback)
    : io_message_loop_(io_message_loop_proxy),
      new_frame_callback_(new_frame_callback) {
}

PpFrameWriter::FrameWriterDelegate::~FrameWriterDelegate() {
}

void PpFrameWriter::FrameWriterDelegate::DeliverFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format) {
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&FrameWriterDelegate::DeliverFrameOnIO,
                 this, frame, format));
}

void PpFrameWriter::FrameWriterDelegate::DeliverFrameOnIO(
     const scoped_refptr<media::VideoFrame>& frame,
     const media::VideoCaptureFormat& format) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  // The local time when this frame is generated is unknown so give a null
  // value to |estimated_capture_time|.
  new_frame_callback_.Run(frame, format, base::TimeTicks());
}

PpFrameWriter::PpFrameWriter() {
  DVLOG(3) << "PpFrameWriter ctor";
}

PpFrameWriter::~PpFrameWriter() {
  DVLOG(3) << "PpFrameWriter dtor";
}

void PpFrameWriter::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height,
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "PpFrameWriter::GetCurrentSupportedFormats()";
  // Since the input is free to change the resolution at any point in time
  // the supported formats are unknown.
  media::VideoCaptureFormats formats;
  callback.Run(formats);
}

void PpFrameWriter::StartSourceImpl(
    const media::VideoCaptureParams& params,
    const VideoCaptureDeliverFrameCB& frame_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!delegate_);
  DVLOG(3) << "PpFrameWriter::StartSourceImpl()";
  delegate_ = new FrameWriterDelegate(io_message_loop(), frame_callback);
  OnStartDone(true);
}

void PpFrameWriter::StopSourceImpl() {
  DCHECK(CalledOnValidThread());
}

void PpFrameWriter::PutFrame(PPB_ImageData_Impl* image_data,
                             int64 time_stamp_ns) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "PpFrameWriter::PutFrame()";

  if (!image_data) {
    LOG(ERROR) << "PpFrameWriter::PutFrame - Called with NULL image_data.";
    return;
  }
  ImageDataAutoMapper mapper(image_data);
  if (!mapper.is_valid()) {
    LOG(ERROR) << "PpFrameWriter::PutFrame - "
               << "The image could not be mapped and is unusable.";
    return;
  }
  const SkBitmap* bitmap = image_data->GetMappedBitmap();
  if (!bitmap) {
    LOG(ERROR) << "PpFrameWriter::PutFrame - "
               << "The image_data's mapped bitmap is NULL.";
    return;
  }

  const gfx::Size frame_size(bitmap->width(), bitmap->height());

  if (state() != MediaStreamVideoSource::STARTED)
    return;

  const base::TimeDelta timestamp = base::TimeDelta::FromMicroseconds(
      time_stamp_ns / base::Time::kNanosecondsPerMicrosecond);

  // TODO(perkj): It would be more efficient to use I420 here. Using YV12 will
  // force a copy into a tightly packed I420 frame in
  // WebRtcVideoCapturerAdapter before the frame is delivered to libJingle.
  // crbug/359587.
  scoped_refptr<media::VideoFrame> new_frame =
      frame_pool_.CreateFrame(media::VideoFrame::YV12, frame_size,
                              gfx::Rect(frame_size), frame_size, timestamp);
  media::VideoCaptureFormat format(
      frame_size,
      MediaStreamVideoSource::kDefaultFrameRate,
      media::PIXEL_FORMAT_YV12);

  libyuv::BGRAToI420(reinterpret_cast<uint8*>(bitmap->getPixels()),
                     bitmap->rowBytes(),
                     new_frame->data(media::VideoFrame::kYPlane),
                     new_frame->stride(media::VideoFrame::kYPlane),
                     new_frame->data(media::VideoFrame::kUPlane),
                     new_frame->stride(media::VideoFrame::kUPlane),
                     new_frame->data(media::VideoFrame::kVPlane),
                     new_frame->stride(media::VideoFrame::kVPlane),
                     frame_size.width(), frame_size.height());

  delegate_->DeliverFrame(new_frame, format);
}

// PpFrameWriterProxy is a helper class to make sure the user won't use
// PpFrameWriter after it is released (IOW its owner - WebMediaStreamSource -
// is released).
class PpFrameWriterProxy : public FrameWriterInterface {
 public:
  explicit PpFrameWriterProxy(const base::WeakPtr<PpFrameWriter>& writer)
      : writer_(writer) {
    DCHECK(writer_ != NULL);
  }

  virtual ~PpFrameWriterProxy() {}

  virtual void PutFrame(PPB_ImageData_Impl* image_data,
                        int64 time_stamp_ns) OVERRIDE {
    writer_->PutFrame(image_data, time_stamp_ns);
  }

 private:
  base::WeakPtr<PpFrameWriter> writer_;

  DISALLOW_COPY_AND_ASSIGN(PpFrameWriterProxy);
};

bool VideoDestinationHandler::Open(
    MediaStreamRegistryInterface* registry,
    const std::string& url,
    FrameWriterInterface** frame_writer) {
  DVLOG(3) << "VideoDestinationHandler::Open";
  blink::WebMediaStream stream;
  if (registry) {
    stream = registry->GetMediaStream(url);
  } else {
    stream =
        blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(GURL(url));
  }
  if (stream.isNull()) {
    LOG(ERROR) << "VideoDestinationHandler::Open - invalid url: " << url;
    return false;
  }

  // Create a new native video track and add it to |stream|.
  std::string track_id;
  // According to spec, a media stream source's id should be unique per
  // application. There's no easy way to strictly achieve that. The id
  // generated with this method should be unique for most of the cases but
  // theoretically it's possible we can get an id that's duplicated with the
  // existing sources.
  base::Base64Encode(base::RandBytesAsString(64), &track_id);

  PpFrameWriter* writer = new PpFrameWriter();

  // Create a new webkit video track.
  blink::WebMediaStreamSource webkit_source;
  blink::WebMediaStreamSource::Type type =
      blink::WebMediaStreamSource::TypeVideo;
  blink::WebString webkit_track_id = base::UTF8ToUTF16(track_id);
  webkit_source.initialize(webkit_track_id, type, webkit_track_id);
  webkit_source.setExtraData(writer);

  blink::WebMediaConstraints constraints;
  constraints.initialize();
  bool track_enabled = true;

  stream.addTrack(MediaStreamVideoTrack::CreateVideoTrack(
      writer, constraints, MediaStreamVideoSource::ConstraintsCallback(),
      track_enabled));

  *frame_writer = new PpFrameWriterProxy(writer->AsWeakPtr());
  return true;
}

}  // namespace content
