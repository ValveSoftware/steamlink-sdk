// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_track_adapter.h"

#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "content/common/media/media_stream_options.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"

namespace {

bool ConstraintKeyExists(const blink::WebMediaConstraints& constraints,
                         const blink::WebString& name) {
  blink::WebString value_str;
  return constraints.getMandatoryConstraintValue(name, value_str) ||
      constraints.getOptionalConstraintValue(name, value_str);
}

}  // anonymouse namespace

namespace content {

// Simple help class used for receiving video frames on the IO-thread from
// a MediaStreamVideoTrack and forward the frames to a
// WebRtcVideoCapturerAdapter on libjingle's worker thread.
// WebRtcVideoCapturerAdapter implements a video capturer for libjingle.
class WebRtcVideoTrackAdapter::WebRtcVideoSourceAdapter
    : public base::RefCountedThreadSafe<WebRtcVideoSourceAdapter> {
 public:
  WebRtcVideoSourceAdapter(
      const scoped_refptr<base::MessageLoopProxy>& libjingle_worker_thread,
      const scoped_refptr<webrtc::VideoSourceInterface>& source,
      WebRtcVideoCapturerAdapter* capture_adapter);

  // WebRtcVideoTrackAdapter can be destroyed on the main render thread or
  // libjingles worker thread since it posts video frames on that thread. But
  // |video_source_| must be released on the main render thread before the
  // PeerConnectionFactory has been destroyed. The only way to ensure that is
  // to make sure |video_source_| is released when WebRtcVideoTrackAdapter() is
  // destroyed.
  void ReleaseSourceOnMainThread();

  void OnVideoFrameOnIO(const scoped_refptr<media::VideoFrame>& frame,
                        const media::VideoCaptureFormat& format,
                        const base::TimeTicks& estimated_capture_time);

 private:
  void OnVideoFrameOnWorkerThread(
      const scoped_refptr<media::VideoFrame>& frame,
      const media::VideoCaptureFormat& format,
      const base::TimeTicks& estimated_capture_time);
  friend class base::RefCountedThreadSafe<WebRtcVideoSourceAdapter>;
  virtual ~WebRtcVideoSourceAdapter();

  scoped_refptr<base::MessageLoopProxy> render_thread_message_loop_;

  // |render_thread_checker_| is bound to the main render thread.
  base::ThreadChecker render_thread_checker_;
  // Used to DCHECK that frames are called on the IO-thread.
  base::ThreadChecker io_thread_checker_;

  // Used for posting frames to libjingle's worker thread. Accessed on the
  // IO-thread.
  scoped_refptr<base::MessageLoopProxy> libjingle_worker_thread_;

  scoped_refptr<webrtc::VideoSourceInterface> video_source_;

  // Used to protect |capture_adapter_|. It is taken by libjingle's worker
  // thread for each video frame that is delivered but only taken on the
  // main render thread in ReleaseSourceOnMainThread() when
  // the owning WebRtcVideoTrackAdapter is being destroyed.
  base::Lock capture_adapter_stop_lock_;
  // |capture_adapter_| is owned by |video_source_|
  WebRtcVideoCapturerAdapter* capture_adapter_;
};

WebRtcVideoTrackAdapter::WebRtcVideoSourceAdapter::WebRtcVideoSourceAdapter(
    const scoped_refptr<base::MessageLoopProxy>& libjingle_worker_thread,
    const scoped_refptr<webrtc::VideoSourceInterface>& source,
    WebRtcVideoCapturerAdapter* capture_adapter)
    : render_thread_message_loop_(base::MessageLoopProxy::current()),
      libjingle_worker_thread_(libjingle_worker_thread),
      video_source_(source),
      capture_adapter_(capture_adapter) {
  io_thread_checker_.DetachFromThread();
}

WebRtcVideoTrackAdapter::WebRtcVideoSourceAdapter::~WebRtcVideoSourceAdapter() {
  DVLOG(3) << "~WebRtcVideoSourceAdapter()";
  DCHECK(!capture_adapter_);
  // This object can be destroyed on the main render thread or libjingles
  // worker thread since it posts video frames on that thread. But
  // |video_source_| must be released on the main render thread before the
  // PeerConnectionFactory has been destroyed. The only way to ensure that is
  // to make sure |video_source_| is released when WebRtcVideoTrackAdapter() is
  // destroyed.
}

void WebRtcVideoTrackAdapter::WebRtcVideoSourceAdapter::
ReleaseSourceOnMainThread() {
  DCHECK(render_thread_checker_.CalledOnValidThread());
  // Since frames are posted to the worker thread, this object might be deleted
  // on that thread. However, since |video_source_| was created on the render
  // thread, it should be released on the render thread.
  base::AutoLock auto_lock(capture_adapter_stop_lock_);
  // |video_source| owns |capture_adapter_|.
  capture_adapter_ = NULL;
  video_source_ = NULL;
}

void WebRtcVideoTrackAdapter::WebRtcVideoSourceAdapter::OnVideoFrameOnIO(
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format,
    const base::TimeTicks& estimated_capture_time) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  libjingle_worker_thread_->PostTask(
      FROM_HERE,
      base::Bind(&WebRtcVideoSourceAdapter::OnVideoFrameOnWorkerThread,
                 this, frame, format, estimated_capture_time));
}

void
WebRtcVideoTrackAdapter::WebRtcVideoSourceAdapter::OnVideoFrameOnWorkerThread(
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format,
    const base::TimeTicks& estimated_capture_time) {
  DCHECK(libjingle_worker_thread_->BelongsToCurrentThread());
  base::AutoLock auto_lock(capture_adapter_stop_lock_);
  if (capture_adapter_)
    capture_adapter_->OnFrameCaptured(frame);
}

WebRtcVideoTrackAdapter::WebRtcVideoTrackAdapter(
    const blink::WebMediaStreamTrack& track,
    PeerConnectionDependencyFactory* factory)
    : web_track_(track) {
  const blink::WebMediaConstraints& constraints =
      MediaStreamVideoTrack::GetVideoTrack(track)->constraints();

  bool is_screencast = ConstraintKeyExists(
      constraints, base::UTF8ToUTF16(kMediaStreamSource));
  WebRtcVideoCapturerAdapter* capture_adapter =
      factory->CreateVideoCapturer(is_screencast);

  // |video_source| owns |capture_adapter|
  scoped_refptr<webrtc::VideoSourceInterface> video_source(
      factory->CreateVideoSource(capture_adapter,
                                 track.source().constraints()));

  video_track_ = factory->CreateLocalVideoTrack(web_track_.id().utf8(),
                                                video_source.get());

  video_track_->set_enabled(web_track_.isEnabled());

  source_adapter_ = new WebRtcVideoSourceAdapter(
      factory->GetWebRtcWorkerThread(),
      video_source,
      capture_adapter);

  AddToVideoTrack(
      this,
      base::Bind(&WebRtcVideoSourceAdapter::OnVideoFrameOnIO,
                 source_adapter_),
      web_track_);

  DVLOG(3) << "WebRtcVideoTrackAdapter ctor() : is_screencast "
           << is_screencast;
}

WebRtcVideoTrackAdapter::~WebRtcVideoTrackAdapter() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "WebRtcVideoTrackAdapter dtor().";
  RemoveFromVideoTrack(this, web_track_);
  source_adapter_->ReleaseSourceOnMainThread();
}

void WebRtcVideoTrackAdapter::OnEnabledChanged(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  video_track_->set_enabled(enabled);
}

}  // namespace content
