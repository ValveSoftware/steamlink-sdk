// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_track.h"

#include <utility>

#include "base/bind.h"
#include "media/base/bind_to_current_loop.h"

namespace content {

namespace {
void ResetCallback(scoped_ptr<VideoCaptureDeliverFrameCB> callback) {
  // |callback| will be deleted when this exits.
}
}  // namespace

// MediaStreamVideoTrack::FrameDeliverer is a helper class used for registering
// VideoCaptureDeliverFrameCB on the main render thread to receive video frames
// on the IO-thread.
// Frames are only delivered to the sinks if the track is enabled. If the track
// is disabled, a black frame is instead forwarded to the sinks at the same
// frame rate.
class MediaStreamVideoTrack::FrameDeliverer
    : public base::RefCountedThreadSafe<FrameDeliverer> {
 public:
  FrameDeliverer(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
      bool enabled);

  void SetEnabled(bool enabled);

  // Add |callback| to receive video frames on the IO-thread.
  // Must be called on the main render thread.
  void AddCallback(void* id, const VideoCaptureDeliverFrameCB& callback);

  // Removes |callback| associated with |id| from receiving video frames if |id|
  // has been added. It is ok to call RemoveCallback even if the |id| has not
  // been added. Note that the added callback will be reset on the main thread.
  // Must be called on the main render thread.
  void RemoveCallback(void* id);

  // Triggers all registered callbacks with |frame|, |format| and
  // |estimated_capture_time| as parameters. Must be called on the IO-thread.
  void DeliverFrameOnIO(const scoped_refptr<media::VideoFrame>& frame,
                        const media::VideoCaptureFormat& format,
                        const base::TimeTicks& estimated_capture_time);

 private:
  friend class base::RefCountedThreadSafe<FrameDeliverer>;
  virtual ~FrameDeliverer();
  void AddCallbackOnIO(void* id, const VideoCaptureDeliverFrameCB& callback);
  void RemoveCallbackOnIO(
      void* id, const scoped_refptr<base::MessageLoopProxy>& message_loop);

  void SetEnabledOnIO(bool enabled);
  // Returns |black_frame_| where the size and time stamp is set to the same as
  // as in |reference_frame|.
  const scoped_refptr<media::VideoFrame>& GetBlackFrame(
      const scoped_refptr<media::VideoFrame>& reference_frame);

  // Used to DCHECK that AddCallback and RemoveCallback are called on the main
  // render thread.
  base::ThreadChecker thread_checker_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  bool enabled_;
  scoped_refptr<media::VideoFrame> black_frame_;

  typedef std::pair<void*, VideoCaptureDeliverFrameCB> VideoIdCallbackPair;
  std::vector<VideoIdCallbackPair> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(FrameDeliverer);
};

MediaStreamVideoTrack::FrameDeliverer::FrameDeliverer(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop, bool enabled)
    : io_message_loop_(io_message_loop),
      enabled_(enabled) {
  DCHECK(io_message_loop_);
}

MediaStreamVideoTrack::FrameDeliverer::~FrameDeliverer() {
  DCHECK(callbacks_.empty());
}

void MediaStreamVideoTrack::FrameDeliverer::AddCallback(
    void* id,
    const VideoCaptureDeliverFrameCB& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&FrameDeliverer::AddCallbackOnIO,
                 this, id, callback));
}

void MediaStreamVideoTrack::FrameDeliverer::AddCallbackOnIO(
    void* id,
    const VideoCaptureDeliverFrameCB& callback) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  callbacks_.push_back(std::make_pair(id, callback));
}

void MediaStreamVideoTrack::FrameDeliverer::RemoveCallback(void* id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&FrameDeliverer::RemoveCallbackOnIO,
                 this, id, base::MessageLoopProxy::current()));
}

void MediaStreamVideoTrack::FrameDeliverer::RemoveCallbackOnIO(
    void* id, const scoped_refptr<base::MessageLoopProxy>& message_loop) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  std::vector<VideoIdCallbackPair>::iterator it = callbacks_.begin();
  for (; it != callbacks_.end(); ++it) {
    if (it->first == id) {
      // Callback is copied to heap and then deleted on the target thread.
      scoped_ptr<VideoCaptureDeliverFrameCB> callback;
      callback.reset(new VideoCaptureDeliverFrameCB(it->second));
      callbacks_.erase(it);
      message_loop->PostTask(
          FROM_HERE, base::Bind(&ResetCallback, base::Passed(&callback)));
      return;
    }
  }
}

void MediaStreamVideoTrack::FrameDeliverer::SetEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&FrameDeliverer::SetEnabledOnIO,
                 this, enabled));
}

void MediaStreamVideoTrack::FrameDeliverer::SetEnabledOnIO(bool enabled) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  enabled_ = enabled;
  if (enabled_)
    black_frame_ = NULL;
}

void MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnIO(
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format,
    const base::TimeTicks& estimated_capture_time) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  const scoped_refptr<media::VideoFrame>& video_frame =
      enabled_ ? frame : GetBlackFrame(frame);

  for (std::vector<VideoIdCallbackPair>::iterator it = callbacks_.begin();
       it != callbacks_.end(); ++it) {
    it->second.Run(video_frame, format, estimated_capture_time);
  }
}

const scoped_refptr<media::VideoFrame>&
MediaStreamVideoTrack::FrameDeliverer::GetBlackFrame(
    const scoped_refptr<media::VideoFrame>& reference_frame) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (!black_frame_ ||
      black_frame_->natural_size() != reference_frame->natural_size())
    black_frame_ =
        media::VideoFrame::CreateBlackFrame(reference_frame->natural_size());

  black_frame_->set_timestamp(reference_frame->timestamp());
  return black_frame_;
}

// static
blink::WebMediaStreamTrack MediaStreamVideoTrack::CreateVideoTrack(
    MediaStreamVideoSource* source,
    const blink::WebMediaConstraints& constraints,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled) {
  blink::WebMediaStreamTrack track;
  track.initialize(source->owner());
  track.setExtraData(new MediaStreamVideoTrack(source,
                                               constraints,
                                               callback,
                                               enabled));
  return track;
}

// static
MediaStreamVideoTrack* MediaStreamVideoTrack::GetVideoTrack(
     const blink::WebMediaStreamTrack& track) {
  return static_cast<MediaStreamVideoTrack*>(track.extraData());
}

MediaStreamVideoTrack::MediaStreamVideoTrack(
    MediaStreamVideoSource* source,
    const blink::WebMediaConstraints& constraints,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled)
    : MediaStreamTrack(NULL, true),
      frame_deliverer_(
          new MediaStreamVideoTrack::FrameDeliverer(source->io_message_loop(),
                                                    enabled)),
      constraints_(constraints),
      source_(source) {
  DCHECK(!constraints.isNull());
  source->AddTrack(this,
                   base::Bind(
                       &MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnIO,
                       frame_deliverer_),
                   constraints, callback);
}

MediaStreamVideoTrack::~MediaStreamVideoTrack() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(sinks_.empty());
  Stop();
  DVLOG(3) << "~MediaStreamVideoTrack()";
}

void MediaStreamVideoTrack::AddSink(
    MediaStreamVideoSink* sink, const VideoCaptureDeliverFrameCB& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end());
  sinks_.push_back(sink);
  frame_deliverer_->AddCallback(sink, callback);
}

void MediaStreamVideoTrack::RemoveSink(MediaStreamVideoSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<MediaStreamVideoSink*>::iterator it =
      std::find(sinks_.begin(), sinks_.end(), sink);
  DCHECK(it != sinks_.end());
  sinks_.erase(it);
  frame_deliverer_->RemoveCallback(sink);
}

void MediaStreamVideoTrack::SetEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  MediaStreamTrack::SetEnabled(enabled);

  frame_deliverer_->SetEnabled(enabled);
  for (std::vector<MediaStreamVideoSink*>::const_iterator it = sinks_.begin();
       it != sinks_.end(); ++it) {
    (*it)->OnEnabledChanged(enabled);
  }
}

void MediaStreamVideoTrack::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (source_) {
    source_->RemoveTrack(this);
    source_ = NULL;
  }
  OnReadyStateChanged(blink::WebMediaStreamSource::ReadyStateEnded);
}

void MediaStreamVideoTrack::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (std::vector<MediaStreamVideoSink*>::const_iterator it = sinks_.begin();
       it != sinks_.end(); ++it) {
    (*it)->OnReadyStateChanged(state);
  }
}

}  // namespace content
