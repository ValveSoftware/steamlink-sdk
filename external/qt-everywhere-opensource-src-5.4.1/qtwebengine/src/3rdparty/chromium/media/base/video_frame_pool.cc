// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_pool.h"

#include <list>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"

namespace media {

class VideoFramePool::PoolImpl
    : public base::RefCountedThreadSafe<VideoFramePool::PoolImpl> {
 public:
  PoolImpl();

  // Returns a frame from the pool that matches the specified
  // parameters or creates a new frame if no suitable frame exists in
  // the pool. The pool is drained if no matching frame is found.
  scoped_refptr<VideoFrame> CreateFrame(VideoFrame::Format format,
                                        const gfx::Size& coded_size,
                                        const gfx::Rect& visible_rect,
                                        const gfx::Size& natural_size,
                                        base::TimeDelta timestamp);

  // Shuts down the frame pool and releases all frames in |frames_|.
  // Once this is called frames will no longer be inserted back into
  // |frames_|.
  void Shutdown();

  size_t GetPoolSizeForTesting() const { return frames_.size(); }

 private:
  friend class base::RefCountedThreadSafe<VideoFramePool::PoolImpl>;
  ~PoolImpl();

  // Called when the frame wrapper gets destroyed.
  // |frame| is the actual frame that was wrapped and is placed
  // in |frames_| by this function so it can be reused.
  void FrameReleased(const scoped_refptr<VideoFrame>& frame);

  base::Lock lock_;
  bool is_shutdown_;
  std::list<scoped_refptr<VideoFrame> > frames_;

  DISALLOW_COPY_AND_ASSIGN(PoolImpl);
};

VideoFramePool::PoolImpl::PoolImpl() : is_shutdown_(false) {}

VideoFramePool::PoolImpl::~PoolImpl() {
  DCHECK(is_shutdown_);
}

scoped_refptr<VideoFrame> VideoFramePool::PoolImpl::CreateFrame(
    VideoFrame::Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!is_shutdown_);

  scoped_refptr<VideoFrame> frame;

  while (!frame && !frames_.empty()) {
      scoped_refptr<VideoFrame> pool_frame = frames_.front();
      frames_.pop_front();

      if (pool_frame->format() == format &&
          pool_frame->coded_size() == coded_size &&
          pool_frame->visible_rect() == visible_rect &&
          pool_frame->natural_size() == natural_size) {
        frame = pool_frame;
        frame->set_timestamp(timestamp);
        break;
      }
  }

  if (!frame) {
    frame = VideoFrame::CreateFrame(
        format, coded_size, visible_rect, natural_size, timestamp);
  }

  return VideoFrame::WrapVideoFrame(
      frame, frame->visible_rect(), frame->natural_size(),
      base::Bind(&VideoFramePool::PoolImpl::FrameReleased, this, frame));
}

void VideoFramePool::PoolImpl::Shutdown() {
  base::AutoLock auto_lock(lock_);
  is_shutdown_ = true;
  frames_.clear();
}

void VideoFramePool::PoolImpl::FrameReleased(
    const scoped_refptr<VideoFrame>& frame) {
  base::AutoLock auto_lock(lock_);
  if (is_shutdown_)
    return;

  frames_.push_back(frame);
}

VideoFramePool::VideoFramePool() : pool_(new PoolImpl()) {
}

VideoFramePool::~VideoFramePool() {
  pool_->Shutdown();
}

scoped_refptr<VideoFrame> VideoFramePool::CreateFrame(
    VideoFrame::Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  return pool_->CreateFrame(format, coded_size, visible_rect, natural_size,
                            timestamp);
}

size_t VideoFramePool::GetPoolSizeForTesting() const {
  return pool_->GetPoolSizeForTesting();
}

}  // namespace media
