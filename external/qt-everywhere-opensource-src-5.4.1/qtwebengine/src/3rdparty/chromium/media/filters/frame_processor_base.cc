// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/frame_processor_base.h"

#include <cstdlib>

#include "base/stl_util.h"
#include "media/base/buffers.h"

namespace media {

MseTrackBuffer::MseTrackBuffer(ChunkDemuxerStream* stream)
    : last_decode_timestamp_(kNoTimestamp()),
      last_frame_duration_(kNoTimestamp()),
      highest_presentation_timestamp_(kNoTimestamp()),
      needs_random_access_point_(true),
      stream_(stream) {
  DCHECK(stream_);
}

MseTrackBuffer::~MseTrackBuffer() {
  DVLOG(2) << __FUNCTION__ << "()";
}

void MseTrackBuffer::Reset() {
  DVLOG(2) << __FUNCTION__ << "()";

  last_decode_timestamp_ = kNoTimestamp();
  last_frame_duration_ = kNoTimestamp();
  highest_presentation_timestamp_ = kNoTimestamp();
  needs_random_access_point_ = true;
}

void MseTrackBuffer::SetHighestPresentationTimestampIfIncreased(
    base::TimeDelta timestamp) {
  if (highest_presentation_timestamp_ == kNoTimestamp() ||
      timestamp > highest_presentation_timestamp_) {
    highest_presentation_timestamp_ = timestamp;
  }
}

FrameProcessorBase::FrameProcessorBase()
    : sequence_mode_(false),
      group_start_timestamp_(kNoTimestamp()) {}

FrameProcessorBase::~FrameProcessorBase() {
  DVLOG(2) << __FUNCTION__ << "()";

  STLDeleteValues(&track_buffers_);
}

void FrameProcessorBase::SetGroupStartTimestampIfInSequenceMode(
    base::TimeDelta timestamp_offset) {
  DVLOG(2) << __FUNCTION__ << "(" << timestamp_offset.InSecondsF() << ")";
  DCHECK(kNoTimestamp() != timestamp_offset);
  if (sequence_mode_)
    group_start_timestamp_ = timestamp_offset;

  // Changes to timestampOffset should invalidate the preroll buffer.
  audio_preroll_buffer_ = NULL;
}

bool FrameProcessorBase::AddTrack(StreamParser::TrackId id,
                                  ChunkDemuxerStream* stream) {
  DVLOG(2) << __FUNCTION__ << "(): id=" << id;

  MseTrackBuffer* existing_track = FindTrack(id);
  DCHECK(!existing_track);
  if (existing_track)
    return false;

  track_buffers_[id] = new MseTrackBuffer(stream);
  return true;
}

bool FrameProcessorBase::UpdateTrack(StreamParser::TrackId old_id,
                                     StreamParser::TrackId new_id) {
  DVLOG(2) << __FUNCTION__ << "() : old_id=" << old_id << ", new_id=" << new_id;

  if (old_id == new_id || !FindTrack(old_id) || FindTrack(new_id))
    return false;

  track_buffers_[new_id] = track_buffers_[old_id];
  CHECK_EQ(1u, track_buffers_.erase(old_id));
  return true;
}

void FrameProcessorBase::SetAllTrackBuffersNeedRandomAccessPoint() {
  for (TrackBufferMap::iterator itr = track_buffers_.begin();
       itr != track_buffers_.end();
       ++itr) {
    itr->second->set_needs_random_access_point(true);
  }
}

void FrameProcessorBase::Reset() {
  DVLOG(2) << __FUNCTION__ << "()";
  for (TrackBufferMap::iterator itr = track_buffers_.begin();
       itr != track_buffers_.end(); ++itr) {
    itr->second->Reset();
  }
}

MseTrackBuffer* FrameProcessorBase::FindTrack(StreamParser::TrackId id) {
  TrackBufferMap::iterator itr = track_buffers_.find(id);
  if (itr == track_buffers_.end())
    return NULL;

  return itr->second;
}

void FrameProcessorBase::NotifyNewMediaSegmentStarting(
    base::TimeDelta segment_timestamp) {
  DVLOG(2) << __FUNCTION__ << "(" << segment_timestamp.InSecondsF() << ")";

  for (TrackBufferMap::iterator itr = track_buffers_.begin();
       itr != track_buffers_.end();
       ++itr) {
    itr->second->stream()->OnNewMediaSegment(segment_timestamp);
  }
}

bool FrameProcessorBase::HandlePartialAppendWindowTrimming(
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    const scoped_refptr<StreamParserBuffer>& buffer) {
  DCHECK(buffer->duration() > base::TimeDelta());
  DCHECK_EQ(DemuxerStream::AUDIO, buffer->type());

  const base::TimeDelta frame_end_timestamp =
      buffer->timestamp() + buffer->duration();

  // Ignore any buffers which start after |append_window_start| or end after
  // |append_window_end|.  For simplicity, even those that start before
  // |append_window_start|.
  if (buffer->timestamp() > append_window_start ||
      frame_end_timestamp > append_window_end) {
    // TODO(dalecurtis): Partial append window trimming could also be done
    // around |append_window_end|, but is not necessary since splice frames
    // cover overlaps there.
    return false;
  }

  // If the buffer is entirely before |append_window_start|, save it as preroll
  // for the first buffer which overlaps |append_window_start|.
  if (buffer->timestamp() < append_window_start &&
      frame_end_timestamp <= append_window_start) {
    audio_preroll_buffer_ = buffer;
    return false;
  }

  // There's nothing to be done if we have no preroll and the buffer starts on
  // the append window start.
  if (buffer->timestamp() == append_window_start && !audio_preroll_buffer_)
    return false;

  // See if a partial discard can be done around |append_window_start|.
  DCHECK(buffer->timestamp() <= append_window_start);
  DCHECK(buffer->IsKeyframe());
  DVLOG(1) << "Truncating buffer which overlaps append window start."
           << " presentation_timestamp " << buffer->timestamp().InSecondsF()
           << " append_window_start " << append_window_start.InSecondsF();

  // If this isn't the first buffer discarded by the append window, try to use
  // the last buffer discarded for preroll.  This ensures that the partially
  // trimmed buffer can be correctly decoded.
  if (audio_preroll_buffer_) {
    // We only want to use the preroll buffer if it directly precedes (less than
    // one sample apart) the current buffer.
    const int64 delta = std::abs((audio_preroll_buffer_->timestamp() +
                                  audio_preroll_buffer_->duration() -
                                  buffer->timestamp()).InMicroseconds());
    if (delta < sample_duration_.InMicroseconds()) {
      buffer->SetPrerollBuffer(audio_preroll_buffer_);
    } else {
      // TODO(dalecurtis): Add a MEDIA_LOG() for when this is dropped unused.
    }
    audio_preroll_buffer_ = NULL;
  }

  // Decrease the duration appropriately.  We only need to shorten the buffer if
  // it overlaps |append_window_start|.
  if (buffer->timestamp() < append_window_start) {
    buffer->set_discard_padding(std::make_pair(
        append_window_start - buffer->timestamp(), base::TimeDelta()));
    buffer->set_duration(frame_end_timestamp - append_window_start);
  }

  // Adjust the timestamp of this buffer forward to |append_window_start|.  The
  // timestamps are always set, even if |buffer|'s timestamp is already set to
  // |append_window_start|, to ensure the preroll buffer is setup correctly.
  buffer->set_timestamp(append_window_start);
  buffer->SetDecodeTimestamp(append_window_start);
  return true;
}

void FrameProcessorBase::OnPossibleAudioConfigUpdate(
    const AudioDecoderConfig& config) {
  DCHECK(config.IsValidConfig());

  // Always clear the preroll buffer when a config update is received.
  audio_preroll_buffer_ = NULL;

  if (config.Matches(current_audio_config_))
    return;

  current_audio_config_ = config;
  sample_duration_ = base::TimeDelta::FromSecondsD(
      1.0 / current_audio_config_.samples_per_second());
}

}  // namespace media
