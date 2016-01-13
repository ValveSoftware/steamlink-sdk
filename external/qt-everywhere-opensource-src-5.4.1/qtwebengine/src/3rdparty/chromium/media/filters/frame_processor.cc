// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/frame_processor.h"

#include "base/stl_util.h"
#include "media/base/buffers.h"
#include "media/base/stream_parser_buffer.h"

namespace media {

FrameProcessor::FrameProcessor(const UpdateDurationCB& update_duration_cb)
    : update_duration_cb_(update_duration_cb) {
  DVLOG(2) << __FUNCTION__ << "()";
  DCHECK(!update_duration_cb.is_null());
}

FrameProcessor::~FrameProcessor() {
  DVLOG(2) << __FUNCTION__;
}

void FrameProcessor::SetSequenceMode(bool sequence_mode) {
  DVLOG(2) << __FUNCTION__ << "(" << sequence_mode << ")";

  // Per April 1, 2014 MSE spec editor's draft:
  // https://dvcs.w3.org/hg/html-media/raw-file/d471a4412040/media-source/media-source.html#widl-SourceBuffer-mode
  // Step 7: If the new mode equals "sequence", then set the group start
  // timestamp to the group end timestamp.
  if (sequence_mode) {
    DCHECK(kNoTimestamp() != group_end_timestamp_);
    group_start_timestamp_ = group_end_timestamp_;
  }

  // Step 8: Update the attribute to new mode.
  sequence_mode_ = sequence_mode;
}

bool FrameProcessor::ProcessFrames(
    const StreamParser::BufferQueue& audio_buffers,
    const StreamParser::BufferQueue& video_buffers,
    const StreamParser::TextBufferQueueMap& text_map,
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    bool* new_media_segment,
    base::TimeDelta* timestamp_offset) {
  StreamParser::BufferQueue frames;
  if (!MergeBufferQueues(audio_buffers, video_buffers, text_map, &frames)) {
    DVLOG(2) << "Parse error discovered while merging parser's buffers";
    return false;
  }

  DCHECK(!frames.empty());

  // Implements the coded frame processing algorithm's outer loop for step 1.
  // Note that ProcessFrame() implements an inner loop for a single frame that
  // handles "jump to the Loop Top step to restart processing of the current
  // coded frame" per April 1, 2014 MSE spec editor's draft:
  // https://dvcs.w3.org/hg/html-media/raw-file/d471a4412040/media-source/
  //     media-source.html#sourcebuffer-coded-frame-processing
  // 1. For each coded frame in the media segment run the following steps:
  for (StreamParser::BufferQueue::const_iterator frames_itr = frames.begin();
       frames_itr != frames.end(); ++frames_itr) {
    if (!ProcessFrame(*frames_itr, append_window_start, append_window_end,
                      timestamp_offset, new_media_segment)) {
      return false;
    }
  }

  // 2. - 4. Are handled by the WebMediaPlayer / Pipeline / Media Element.

  // Step 5:
  update_duration_cb_.Run(group_end_timestamp_);

  return true;
}

bool FrameProcessor::ProcessFrame(
    const scoped_refptr<StreamParserBuffer>& frame,
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    base::TimeDelta* timestamp_offset,
    bool* new_media_segment) {
  // Implements the loop within step 1 of the coded frame processing algorithm
  // for a single input frame per April 1, 2014 MSE spec editor's draft:
  // https://dvcs.w3.org/hg/html-media/raw-file/d471a4412040/media-source/
  //     media-source.html#sourcebuffer-coded-frame-processing

  while (true) {
    // 1. Loop Top: Let presentation timestamp be a double precision floating
    //    point representation of the coded frame's presentation timestamp in
    //    seconds.
    // 2. Let decode timestamp be a double precision floating point
    //    representation of the coded frame's decode timestamp in seconds.
    // 3. Let frame duration be a double precision floating point representation
    //    of the coded frame's duration in seconds.
    // We use base::TimeDelta instead of double.
    base::TimeDelta presentation_timestamp = frame->timestamp();
    base::TimeDelta decode_timestamp = frame->GetDecodeTimestamp();
    base::TimeDelta frame_duration = frame->duration();

    DVLOG(3) << __FUNCTION__ << ": Processing frame "
             << "Type=" << frame->type()
             << ", TrackID=" << frame->track_id()
             << ", PTS=" << presentation_timestamp.InSecondsF()
             << ", DTS=" << decode_timestamp.InSecondsF()
             << ", DUR=" << frame_duration.InSecondsF();

    // Sanity check the timestamps.
    if (presentation_timestamp == kNoTimestamp()) {
      DVLOG(2) << __FUNCTION__ << ": Unknown frame PTS";
      return false;
    }
    if (decode_timestamp == kNoTimestamp()) {
      DVLOG(2) << __FUNCTION__ << ": Unknown frame DTS";
      return false;
    }
    if (decode_timestamp > presentation_timestamp) {
      // TODO(wolenetz): Determine whether DTS>PTS should really be allowed. See
      // http://crbug.com/354518.
      DVLOG(2) << __FUNCTION__ << ": WARNING: Frame DTS("
               << decode_timestamp.InSecondsF() << ") > PTS("
               << presentation_timestamp.InSecondsF() << ")";
    }

    // TODO(acolwell/wolenetz): All stream parsers must emit valid (positive)
    // frame durations. For now, we allow non-negative frame duration.
    // See http://crbug.com/351166.
    if (frame_duration == kNoTimestamp()) {
      DVLOG(2) << __FUNCTION__ << ": Frame missing duration (kNoTimestamp())";
      return false;
    }
    if (frame_duration <  base::TimeDelta()) {
      DVLOG(2) << __FUNCTION__ << ": Frame duration negative: "
               << frame_duration.InSecondsF();
      return false;
    }

    // 4. If mode equals "sequence" and group start timestamp is set, then run
    //    the following steps:
    if (sequence_mode_ && group_start_timestamp_ != kNoTimestamp()) {
      // 4.1. Set timestampOffset equal to group start timestamp -
      //      presentation timestamp.
      *timestamp_offset = group_start_timestamp_ - presentation_timestamp;

      DVLOG(3) << __FUNCTION__ << ": updated timestampOffset is now "
               << timestamp_offset->InSecondsF();

      // 4.2. Set group end timestamp equal to group start timestamp.
      group_end_timestamp_ = group_start_timestamp_;

      // 4.3. Set the need random access point flag on all track buffers to
      //      true.
      SetAllTrackBuffersNeedRandomAccessPoint();

      // 4.4. Unset group start timestamp.
      group_start_timestamp_ = kNoTimestamp();
    }

    // 5. If timestampOffset is not 0, then run the following steps:
    if (*timestamp_offset != base::TimeDelta()) {
      // 5.1. Add timestampOffset to the presentation timestamp.
      // Note: |frame| PTS is only updated if it survives discontinuity
      // processing.
      presentation_timestamp += *timestamp_offset;

      // 5.2. Add timestampOffset to the decode timestamp.
      // Frame DTS is only updated if it survives discontinuity processing.
      decode_timestamp += *timestamp_offset;
    }

    // 6. Let track buffer equal the track buffer that the coded frame will be
    //    added to.

    // Remap audio and video track types to their special singleton identifiers.
    StreamParser::TrackId track_id = kAudioTrackId;
    switch (frame->type()) {
      case DemuxerStream::AUDIO:
        break;
      case DemuxerStream::VIDEO:
        track_id = kVideoTrackId;
        break;
      case DemuxerStream::TEXT:
        track_id = frame->track_id();
        break;
      case DemuxerStream::UNKNOWN:
      case DemuxerStream::NUM_TYPES:
        DCHECK(false) << ": Invalid frame type " << frame->type();
        return false;
    }

    MseTrackBuffer* track_buffer = FindTrack(track_id);
    if (!track_buffer) {
      DVLOG(2) << __FUNCTION__ << ": Unknown track: type=" << frame->type()
               << ", frame processor track id=" << track_id
               << ", parser track id=" << frame->track_id();
      return false;
    }

    // 7. If last decode timestamp for track buffer is set and decode timestamp
    //    is less than last decode timestamp
    //    OR
    //    If last decode timestamp for track buffer is set and the difference
    //    between decode timestamp and last decode timestamp is greater than 2
    //    times last frame duration:
    base::TimeDelta last_decode_timestamp =
        track_buffer->last_decode_timestamp();
    if (last_decode_timestamp != kNoTimestamp()) {
      base::TimeDelta dts_delta = decode_timestamp - last_decode_timestamp;
      if (dts_delta < base::TimeDelta() ||
          dts_delta > 2 * track_buffer->last_frame_duration()) {
        // 7.1. If mode equals "segments": Set group end timestamp to
        //      presentation timestamp.
        //      If mode equals "sequence": Set group start timestamp equal to
        //      the group end timestamp.
        if (!sequence_mode_) {
          group_end_timestamp_ = presentation_timestamp;
          // This triggers a discontinuity so we need to treat the next frames
          // appended within the append window as if they were the beginning of
          // a new segment.
          *new_media_segment = true;
        } else {
          DVLOG(3) << __FUNCTION__ << " : Sequence mode discontinuity, GETS: "
                   << group_end_timestamp_.InSecondsF();
          DCHECK(kNoTimestamp() != group_end_timestamp_);
          group_start_timestamp_ = group_end_timestamp_;
        }

        // 7.2. - 7.5.:
        Reset();

        // 7.6. Jump to the Loop Top step above to restart processing of the
        //      current coded frame.
        DVLOG(3) << __FUNCTION__ << ": Discontinuity: reprocessing frame";
        continue;
      }
    }

    // 9. Let frame end timestamp equal the sum of presentation timestamp and
    //    frame duration.
    const base::TimeDelta frame_end_timestamp =
        presentation_timestamp + frame_duration;

    // 10.  If presentation timestamp is less than appendWindowStart, then set
    //      the need random access point flag to true, drop the coded frame, and
    //      jump to the top of the loop to start processing the next coded
    //      frame.
    // Note: We keep the result of partial discard of a buffer that overlaps
    //      |append_window_start| and does not end after |append_window_end|.
    // 11. If frame end timestamp is greater than appendWindowEnd, then set the
    //     need random access point flag to true, drop the coded frame, and jump
    //     to the top of the loop to start processing the next coded frame.
    frame->set_timestamp(presentation_timestamp);
    frame->SetDecodeTimestamp(decode_timestamp);
    if (track_buffer->stream()->supports_partial_append_window_trimming() &&
        HandlePartialAppendWindowTrimming(append_window_start,
                                          append_window_end,
                                          frame)) {
      // If |frame| was shortened a discontinuity may exist, so treat the next
      // frames appended as if they were the beginning of a new media segment.
      if (frame->timestamp() != presentation_timestamp && !sequence_mode_)
        *new_media_segment = true;

      // |frame| has been partially trimmed or had preroll added.  Though
      // |frame|'s duration may have changed, do not update |frame_duration|
      // here, so |track_buffer|'s last frame duration update uses original
      // frame duration and reduces spurious discontinuity detection.
      decode_timestamp = frame->GetDecodeTimestamp();
      presentation_timestamp = frame->timestamp();

      // The end timestamp of the frame should be unchanged.
      DCHECK(frame_end_timestamp == presentation_timestamp + frame->duration());
    }

    if (presentation_timestamp < append_window_start ||
        frame_end_timestamp > append_window_end) {
      track_buffer->set_needs_random_access_point(true);
      DVLOG(3) << "Dropping frame that is outside append window.";

      if (!sequence_mode_) {
        // This also triggers a discontinuity so we need to treat the next
        // frames appended within the append window as if they were the
        // beginning of a new segment.
        *new_media_segment = true;
      }

      return true;
    }

    // Note: This step is relocated, versus April 1 spec, to allow append window
    // processing to first filter coded frames shifted by |timestamp_offset_| in
    // such a way that their PTS is negative.
    // 8. If the presentation timestamp or decode timestamp is less than the
    // presentation start time, then run the end of stream algorithm with the
    // error parameter set to "decode", and abort these steps.
    DCHECK(presentation_timestamp >= base::TimeDelta());
    if (decode_timestamp < base::TimeDelta()) {
      // B-frames may still result in negative DTS here after being shifted by
      // |timestamp_offset_|.
      DVLOG(2) << __FUNCTION__
               << ": frame PTS=" << presentation_timestamp.InSecondsF()
               << " has negative DTS=" << decode_timestamp.InSecondsF()
               << " after applying timestampOffset, handling any discontinuity,"
               << " and filtering against append window";
      return false;
    }

    // 12. If the need random access point flag on track buffer equals true,
    //     then run the following steps:
    if (track_buffer->needs_random_access_point()) {
      // 12.1. If the coded frame is not a random access point, then drop the
      //       coded frame and jump to the top of the loop to start processing
      //       the next coded frame.
      if (!frame->IsKeyframe()) {
        DVLOG(3) << __FUNCTION__
                 << ": Dropping frame that is not a random access point";
        return true;
      }

      // 12.2. Set the need random access point flag on track buffer to false.
      track_buffer->set_needs_random_access_point(false);
    }

    // We now have a processed buffer to append to the track buffer's stream.
    // If it is the first in a new media segment or following a discontinuity,
    // notify all the track buffers' streams that a new segment is beginning.
    if (*new_media_segment) {
      *new_media_segment = false;
      NotifyNewMediaSegmentStarting(decode_timestamp);
    }

    DVLOG(3) << __FUNCTION__ << ": Sending processed frame to stream, "
             << "PTS=" << presentation_timestamp.InSecondsF()
             << ", DTS=" << decode_timestamp.InSecondsF();

    // Steps 13-18:
    // TODO(wolenetz): Collect and emit more than one buffer at a time, if
    // possible. Also refactor SourceBufferStream to conform to spec GC timing.
    // See http://crbug.com/371197.
    StreamParser::BufferQueue buffer_to_append;
    buffer_to_append.push_back(frame);
    if (!track_buffer->stream()->Append(buffer_to_append)) {
      DVLOG(3) << __FUNCTION__ << ": Failure appending frame to stream";
      return false;
    }

    // 19. Set last decode timestamp for track buffer to decode timestamp.
    track_buffer->set_last_decode_timestamp(decode_timestamp);

    // 20. Set last frame duration for track buffer to frame duration.
    track_buffer->set_last_frame_duration(frame_duration);

    // 21. If highest presentation timestamp for track buffer is unset or frame
    //     end timestamp is greater than highest presentation timestamp, then
    //     set highest presentation timestamp for track buffer to frame end
    //     timestamp.
    track_buffer->SetHighestPresentationTimestampIfIncreased(
        frame_end_timestamp);

    // 22. If frame end timestamp is greater than group end timestamp, then set
    //     group end timestamp equal to frame end timestamp.
    if (frame_end_timestamp > group_end_timestamp_)
      group_end_timestamp_ = frame_end_timestamp;
    DCHECK(group_end_timestamp_ >= base::TimeDelta());

    return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace media
