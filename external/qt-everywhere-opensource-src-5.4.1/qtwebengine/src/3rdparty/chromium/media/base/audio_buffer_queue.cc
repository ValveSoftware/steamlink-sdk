// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_buffer_queue.h"

#include <algorithm>

#include "base/logging.h"
#include "media/base/audio_bus.h"
#include "media/base/buffers.h"

namespace media {

AudioBufferQueue::AudioBufferQueue() { Clear(); }
AudioBufferQueue::~AudioBufferQueue() {}

void AudioBufferQueue::Clear() {
  buffers_.clear();
  current_buffer_ = buffers_.begin();
  current_buffer_offset_ = 0;
  frames_ = 0;
  current_time_ = kNoTimestamp();
}

void AudioBufferQueue::Append(const scoped_refptr<AudioBuffer>& buffer_in) {
  // If we have just written the first buffer, update |current_time_| to be the
  // start time.
  if (buffers_.empty() && buffer_in->timestamp() != kNoTimestamp()) {
    current_time_ = buffer_in->timestamp();
  }

  // Add the buffer to the queue. Inserting into deque invalidates all
  // iterators, so point to the first buffer.
  buffers_.push_back(buffer_in);
  current_buffer_ = buffers_.begin();

  // Update the |frames_| counter since we have added frames.
  frames_ += buffer_in->frame_count();
  CHECK_GT(frames_, 0);  // make sure it doesn't overflow.
}

int AudioBufferQueue::ReadFrames(int frames,
                                 int dest_frame_offset,
                                 AudioBus* dest) {
  DCHECK_GE(dest->frames(), frames + dest_frame_offset);
  return InternalRead(frames, true, 0, dest_frame_offset, dest);
}

int AudioBufferQueue::PeekFrames(int frames,
                                 int source_frame_offset,
                                 int dest_frame_offset,
                                 AudioBus* dest) {
  DCHECK_GE(dest->frames(), frames);
  return InternalRead(
      frames, false, source_frame_offset, dest_frame_offset, dest);
}

void AudioBufferQueue::SeekFrames(int frames) {
  // Perform seek only if we have enough bytes in the queue.
  CHECK_LE(frames, frames_);
  int taken = InternalRead(frames, true, 0, 0, NULL);
  DCHECK_EQ(taken, frames);
}

int AudioBufferQueue::InternalRead(int frames,
                                   bool advance_position,
                                   int source_frame_offset,
                                   int dest_frame_offset,
                                   AudioBus* dest) {
  // Counts how many frames are actually read from the buffer queue.
  int taken = 0;
  BufferQueue::iterator current_buffer = current_buffer_;
  int current_buffer_offset = current_buffer_offset_;

  int frames_to_skip = source_frame_offset;
  while (taken < frames) {
    // |current_buffer| is valid since the first time this buffer is appended
    // with data. Make sure there is data to be processed.
    if (current_buffer == buffers_.end())
      break;

    scoped_refptr<AudioBuffer> buffer = *current_buffer;

    int remaining_frames_in_buffer =
        buffer->frame_count() - current_buffer_offset;

    if (frames_to_skip > 0) {
      // If there are frames to skip, do it first. May need to skip into
      // subsequent buffers.
      int skipped = std::min(remaining_frames_in_buffer, frames_to_skip);
      current_buffer_offset += skipped;
      frames_to_skip -= skipped;
    } else {
      // Find the right amount to copy from the current buffer. We shall copy no
      // more than |frames| frames in total and each single step copies no more
      // than the current buffer size.
      int copied = std::min(frames - taken, remaining_frames_in_buffer);

      // if |dest| is NULL, there's no need to copy.
      if (dest) {
        buffer->ReadFrames(
            copied, current_buffer_offset, dest_frame_offset + taken, dest);
      }

      // Increase total number of frames copied, which regulates when to end
      // this loop.
      taken += copied;

      // We have read |copied| frames from the current buffer. Advance the
      // offset.
      current_buffer_offset += copied;
    }

    // Has the buffer has been consumed?
    if (current_buffer_offset == buffer->frame_count()) {
      if (advance_position) {
        // Next buffer may not have timestamp, so we need to update current
        // timestamp before switching to the next buffer.
        UpdateCurrentTime(current_buffer, current_buffer_offset);
      }

      // If we are at the last buffer, no more data to be copied, so stop.
      BufferQueue::iterator next = current_buffer + 1;
      if (next == buffers_.end())
        break;

      // Advances the iterator.
      current_buffer = next;
      current_buffer_offset = 0;
    }
  }

  if (advance_position) {
    // Update the appropriate values since |taken| frames have been copied out.
    frames_ -= taken;
    DCHECK_GE(frames_, 0);
    DCHECK(current_buffer_ != buffers_.end() || frames_ == 0);

    UpdateCurrentTime(current_buffer, current_buffer_offset);

    // Remove any buffers before the current buffer as there is no going
    // backwards.
    buffers_.erase(buffers_.begin(), current_buffer);
    current_buffer_ = buffers_.begin();
    current_buffer_offset_ = current_buffer_offset;
  }

  return taken;
}

void AudioBufferQueue::UpdateCurrentTime(BufferQueue::iterator buffer,
                                         int offset) {
  if (buffer != buffers_.end() && (*buffer)->timestamp() != kNoTimestamp()) {
    double time_offset = ((*buffer)->duration().InMicroseconds() * offset) /
                         static_cast<double>((*buffer)->frame_count());
    current_time_ =
        (*buffer)->timestamp() + base::TimeDelta::FromMicroseconds(
                                     static_cast<int64>(time_offset + 0.5));
  }
}

}  // namespace media
