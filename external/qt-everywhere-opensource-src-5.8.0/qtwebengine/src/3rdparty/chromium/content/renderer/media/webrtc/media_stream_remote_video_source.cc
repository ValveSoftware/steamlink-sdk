// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_remote_video_source.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/trace_event/trace_event.h"
#include "content/renderer/media/webrtc/track_observer.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/webrtc/media/base/videoframe.h"
#include "third_party/webrtc/media/base/videosinkinterface.h"

namespace content {

// Internal class used for receiving frames from the webrtc track on a
// libjingle thread and forward it to the IO-thread.
class MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate
    : public base::RefCountedThreadSafe<RemoteVideoSourceDelegate>,
      public rtc::VideoSinkInterface<cricket::VideoFrame> {
 public:
  RemoteVideoSourceDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      const VideoCaptureDeliverFrameCB& new_frame_callback);

 protected:
  friend class base::RefCountedThreadSafe<RemoteVideoSourceDelegate>;
  ~RemoteVideoSourceDelegate() override;

  // Implements rtc::VideoSinkInterface used for receiving video frames
  // from the PeerConnection video track. May be called on a libjingle internal
  // thread.
  void OnFrame(const cricket::VideoFrame& frame) override;

  void DoRenderFrameOnIOThread(
      const scoped_refptr<media::VideoFrame>& video_frame);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // |frame_callback_| is accessed on the IO thread.
  VideoCaptureDeliverFrameCB frame_callback_;

  // Timestamp of the first received frame.
  base::TimeDelta start_timestamp_;
  // WebRTC Chromium timestamp diff
  const base::TimeDelta time_diff_;
};

MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate::
    RemoteVideoSourceDelegate(
        scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
        const VideoCaptureDeliverFrameCB& new_frame_callback)
    : io_task_runner_(io_task_runner),
      frame_callback_(new_frame_callback),
      start_timestamp_(media::kNoTimestamp()),
      // TODO(qiangchen): There can be two differences between clocks: 1)
      // the offset, 2) the rate (i.e., one clock runs faster than the other).
      // See http://crbug/516700
      time_diff_(base::TimeTicks::Now() - base::TimeTicks() -
                 base::TimeDelta::FromMicroseconds(rtc::TimeMicros())) {}

MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::~RemoteVideoSourceDelegate() {
}

namespace {
void DoNothing(const scoped_refptr<rtc::RefCountInterface>& ref) {}
}  // anonymous

void MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate::OnFrame(
    const cricket::VideoFrame& incoming_frame) {
  const base::TimeDelta incoming_timestamp = base::TimeDelta::FromMicroseconds(
      incoming_frame.GetTimeStamp() / rtc::kNumNanosecsPerMicrosec);
  const base::TimeTicks render_time =
      base::TimeTicks() + incoming_timestamp + time_diff_;

  TRACE_EVENT1("webrtc", "RemoteVideoSourceDelegate::RenderFrame",
               "Ideal Render Instant", render_time.ToInternalValue());

  CHECK_NE(media::kNoTimestamp(), incoming_timestamp);
  if (start_timestamp_ == media::kNoTimestamp())
    start_timestamp_ = incoming_timestamp;
  const base::TimeDelta elapsed_timestamp =
      incoming_timestamp - start_timestamp_;

  scoped_refptr<media::VideoFrame> video_frame;
  scoped_refptr<webrtc::VideoFrameBuffer> buffer(
      incoming_frame.video_frame_buffer());

  if (buffer->native_handle() != NULL) {
    video_frame =
        static_cast<media::VideoFrame*>(buffer->native_handle());
    video_frame->set_timestamp(elapsed_timestamp);
  } else {
    // Note that the GetCopyWithRotationApplied returns a pointer to a
    // frame owned by incoming_frame.
    buffer =
        incoming_frame.GetCopyWithRotationApplied()->video_frame_buffer();
    gfx::Size size(buffer->width(), buffer->height());

    // Make a shallow copy. Both |frame| and |video_frame| will share a single
    // reference counted frame buffer. Const cast and hope no one will overwrite
    // the data.
    // TODO(magjed): Update media::VideoFrame to support const data so we don't
    // need to const cast here.
    video_frame = media::VideoFrame::WrapExternalYuvData(
        media::PIXEL_FORMAT_YV12, size, gfx::Rect(size), size,
        buffer->StrideY(),
        buffer->StrideU(),
        buffer->StrideV(),
        const_cast<uint8_t*>(buffer->DataY()),
        const_cast<uint8_t*>(buffer->DataU()),
        const_cast<uint8_t*>(buffer->DataV()),
        elapsed_timestamp);
    if (!video_frame)
      return;
    // The bind ensures that we keep a reference to the underlying buffer.
    video_frame->AddDestructionObserver(base::Bind(&DoNothing, buffer));
  }

  video_frame->metadata()->SetTimeTicks(
      media::VideoFrameMetadata::REFERENCE_TIME, render_time);

  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RemoteVideoSourceDelegate::DoRenderFrameOnIOThread,
                            this, video_frame));
}

void MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::DoRenderFrameOnIOThread(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("webrtc", "RemoteVideoSourceDelegate::DoRenderFrameOnIOThread");
  // TODO(hclam): Give the estimated capture time.
  frame_callback_.Run(video_frame, base::TimeTicks());
}

MediaStreamRemoteVideoSource::MediaStreamRemoteVideoSource(
    std::unique_ptr<TrackObserver> observer)
    : observer_(std::move(observer)) {
  // The callback will be automatically cleared when 'observer_' goes out of
  // scope and no further callbacks will occur.
  observer_->SetCallback(base::Bind(&MediaStreamRemoteVideoSource::OnChanged,
      base::Unretained(this)));
}

MediaStreamRemoteVideoSource::~MediaStreamRemoteVideoSource() {
  DCHECK(CalledOnValidThread());
  DCHECK(!observer_);
}

void MediaStreamRemoteVideoSource::OnSourceTerminated() {
  DCHECK(CalledOnValidThread());
  StopSourceImpl();
}

void MediaStreamRemoteVideoSource::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height,
    double max_requested_frame_rate,
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(CalledOnValidThread());
  media::VideoCaptureFormats formats;
  // Since the remote end is free to change the resolution at any point in time
  // the supported formats are unknown.
  callback.Run(formats);
}

void MediaStreamRemoteVideoSource::StartSourceImpl(
    const media::VideoCaptureFormat& format,
    const blink::WebMediaConstraints& constraints,
    const VideoCaptureDeliverFrameCB& frame_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!delegate_.get());
  delegate_ = new RemoteVideoSourceDelegate(io_task_runner(), frame_callback);
  scoped_refptr<webrtc::VideoTrackInterface> video_track(
      static_cast<webrtc::VideoTrackInterface*>(observer_->track().get()));
  video_track->AddOrUpdateSink(delegate_.get(), rtc::VideoSinkWants());
  OnStartDone(MEDIA_DEVICE_OK);
}

void MediaStreamRemoteVideoSource::StopSourceImpl() {
  DCHECK(CalledOnValidThread());
  // StopSourceImpl is called either when MediaStreamTrack.stop is called from
  // JS or blink gc the MediaStreamSource object or when OnSourceTerminated()
  // is called. Garbage collection will happen after the PeerConnection no
  // longer receives the video track.
  if (!observer_)
    return;
  DCHECK(state() != MediaStreamVideoSource::ENDED);
  scoped_refptr<webrtc::VideoTrackInterface> video_track(
      static_cast<webrtc::VideoTrackInterface*>(observer_->track().get()));
  video_track->RemoveSink(delegate_.get());
  // This removes the references to the webrtc video track.
  observer_.reset();
}

rtc::VideoSinkInterface<cricket::VideoFrame>*
MediaStreamRemoteVideoSource::SinkInterfaceForTest() {
  return delegate_.get();
}

void MediaStreamRemoteVideoSource::OnChanged(
    webrtc::MediaStreamTrackInterface::TrackState state) {
  DCHECK(CalledOnValidThread());
  switch (state) {
    case webrtc::MediaStreamTrackInterface::kLive:
      SetReadyState(blink::WebMediaStreamSource::ReadyStateLive);
      break;
    case webrtc::MediaStreamTrackInterface::kEnded:
      SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace content
