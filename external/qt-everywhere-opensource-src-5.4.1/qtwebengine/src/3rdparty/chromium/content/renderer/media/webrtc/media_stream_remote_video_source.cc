// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_remote_video_source.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/renderer/media/native_handle_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "media/base/video_util.h"
#include "third_party/libjingle/source/talk/media/base/videoframe.h"

namespace content {

// Internal class used for receiving frames from the webrtc track on a
// libjingle thread and forward it to the IO-thread.
class MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate
    : public base::RefCountedThreadSafe<RemoteVideoSourceDelegate>,
      public webrtc::VideoRendererInterface {
 public:
  RemoteVideoSourceDelegate(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
      const VideoCaptureDeliverFrameCB& new_frame_callback);

 protected:
  friend class base::RefCountedThreadSafe<RemoteVideoSourceDelegate>;
  virtual ~RemoteVideoSourceDelegate();

  // Implements webrtc::VideoRendererInterface used for receiving video frames
  // from the PeerConnection video track. May be called on a libjingle internal
  // thread.
  virtual void SetSize(int width, int height) OVERRIDE;
  virtual void RenderFrame(const cricket::VideoFrame* frame) OVERRIDE;

  void DoRenderFrameOnIOThread(scoped_refptr<media::VideoFrame> video_frame,
                               const media::VideoCaptureFormat& format);
 private:
  // Bound to the render thread.
  base::ThreadChecker thread_checker_;

  scoped_refptr<base::MessageLoopProxy> io_message_loop_;
  // |frame_pool_| is only accessed on whatever
  // thread webrtc::VideoRendererInterface::RenderFrame is called on.
  media::VideoFramePool frame_pool_;

  // |frame_callback_| is accessed on the IO thread.
  VideoCaptureDeliverFrameCB frame_callback_;
};

MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::RemoteVideoSourceDelegate(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
    const VideoCaptureDeliverFrameCB& new_frame_callback)
    : io_message_loop_(io_message_loop),
      frame_callback_(new_frame_callback) {
}

MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::~RemoteVideoSourceDelegate() {
}

void MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::SetSize(int width, int height) {
}

void MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::RenderFrame(
    const cricket::VideoFrame* frame) {
  base::TimeDelta timestamp = base::TimeDelta::FromMicroseconds(
      frame->GetElapsedTime() / talk_base::kNumNanosecsPerMicrosec);

  scoped_refptr<media::VideoFrame> video_frame;
  if (frame->GetNativeHandle() != NULL) {
    NativeHandleImpl* handle =
        static_cast<NativeHandleImpl*>(frame->GetNativeHandle());
    video_frame = static_cast<media::VideoFrame*>(handle->GetHandle());
    video_frame->set_timestamp(timestamp);
  } else {
    gfx::Size size(frame->GetWidth(), frame->GetHeight());
    video_frame = frame_pool_.CreateFrame(
        media::VideoFrame::YV12, size, gfx::Rect(size), size, timestamp);

    // Non-square pixels are unsupported.
    DCHECK_EQ(frame->GetPixelWidth(), 1u);
    DCHECK_EQ(frame->GetPixelHeight(), 1u);

    int y_rows = frame->GetHeight();
    int uv_rows = frame->GetChromaHeight();
    CopyYPlane(
        frame->GetYPlane(), frame->GetYPitch(), y_rows, video_frame.get());
    CopyUPlane(
        frame->GetUPlane(), frame->GetUPitch(), uv_rows, video_frame.get());
    CopyVPlane(
        frame->GetVPlane(), frame->GetVPitch(), uv_rows, video_frame.get());
  }

  media::VideoPixelFormat pixel_format =
      (video_frame->format() == media::VideoFrame::YV12) ?
          media::PIXEL_FORMAT_YV12 : media::PIXEL_FORMAT_TEXTURE;

  media::VideoCaptureFormat format(
      gfx::Size(video_frame->natural_size().width(),
                video_frame->natural_size().height()),
                MediaStreamVideoSource::kDefaultFrameRate,
                pixel_format);

  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&RemoteVideoSourceDelegate::DoRenderFrameOnIOThread,
                 this, video_frame, format));
}

void MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::DoRenderFrameOnIOThread(
    scoped_refptr<media::VideoFrame> video_frame,
    const media::VideoCaptureFormat& format) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  // TODO(hclam): Give the estimated capture time.
  frame_callback_.Run(video_frame, format, base::TimeTicks());
}

MediaStreamRemoteVideoSource::MediaStreamRemoteVideoSource(
    webrtc::VideoTrackInterface* remote_track)
    : remote_track_(remote_track),
      last_state_(remote_track->state()) {
  remote_track_->RegisterObserver(this);
}

MediaStreamRemoteVideoSource::~MediaStreamRemoteVideoSource() {
  remote_track_->UnregisterObserver(this);
}

void MediaStreamRemoteVideoSource::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height,
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media::VideoCaptureFormats formats;
  // Since the remote end is free to change the resolution at any point in time
  // the supported formats are unknown.
  callback.Run(formats);
}

void MediaStreamRemoteVideoSource::StartSourceImpl(
    const media::VideoCaptureParams& params,
    const VideoCaptureDeliverFrameCB& frame_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!delegate_);
  delegate_ = new RemoteVideoSourceDelegate(io_message_loop(), frame_callback);
  remote_track_->AddRenderer(delegate_);
  OnStartDone(true);
}

void MediaStreamRemoteVideoSource::StopSourceImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(state() != MediaStreamVideoSource::ENDED);
  remote_track_->RemoveRenderer(delegate_);
}

webrtc::VideoRendererInterface*
MediaStreamRemoteVideoSource::RenderInterfaceForTest() {
  return delegate_;
}

void MediaStreamRemoteVideoSource::OnChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  webrtc::MediaStreamTrackInterface::TrackState state = remote_track_->state();
  if (state != last_state_) {
    last_state_ = state;
    switch (state) {
      case webrtc::MediaStreamTrackInterface::kInitializing:
        // Ignore the kInitializing state since there is no match in
        // WebMediaStreamSource::ReadyState.
        break;
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
}

}  // namespace content
