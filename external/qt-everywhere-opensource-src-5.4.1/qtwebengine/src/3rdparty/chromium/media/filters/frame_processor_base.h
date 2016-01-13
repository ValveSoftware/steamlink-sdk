// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FRAME_PROCESSOR_BASE_H_
#define MEDIA_FILTERS_FRAME_PROCESSOR_BASE_H_

#include <map>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/filters/chunk_demuxer.h"

namespace media {

// Helper class to capture per-track details needed by a frame processor. Some
// of this information may be duplicated in the short-term in the associated
// ChunkDemuxerStream and SourceBufferStream for a track.
// This parallels the MSE spec each of a SourceBuffer's Track Buffers at
// http://www.w3.org/TR/media-source/#track-buffers.
class MseTrackBuffer {
 public:
  explicit MseTrackBuffer(ChunkDemuxerStream* stream);
  ~MseTrackBuffer();

  // Get/set |last_decode_timestamp_|.
  base::TimeDelta last_decode_timestamp() const {
    return last_decode_timestamp_;
  }
  void set_last_decode_timestamp(base::TimeDelta timestamp) {
    last_decode_timestamp_ = timestamp;
  }

  // Get/set |last_frame_duration_|.
  base::TimeDelta last_frame_duration() const {
    return last_frame_duration_;
  }
  void set_last_frame_duration(base::TimeDelta duration) {
    last_frame_duration_ = duration;
  }

  // Gets |highest_presentation_timestamp_|.
  base::TimeDelta highest_presentation_timestamp() const {
    return highest_presentation_timestamp_;
  }

  // Get/set |needs_random_access_point_|.
  bool needs_random_access_point() const {
    return needs_random_access_point_;
  }
  void set_needs_random_access_point(bool needs_random_access_point) {
    needs_random_access_point_ = needs_random_access_point;
  }

  // Gets a pointer to this track's ChunkDemuxerStream.
  ChunkDemuxerStream* stream() const { return stream_; }

  // Unsets |last_decode_timestamp_|, unsets |last_frame_duration_|,
  // unsets |highest_presentation_timestamp_|, and sets
  // |needs_random_access_point_| to true.
  void Reset();

  // If |highest_presentation_timestamp_| is unset or |timestamp| is greater
  // than |highest_presentation_timestamp_|, sets
  // |highest_presentation_timestamp_| to |timestamp|. Note that bidirectional
  // prediction between coded frames can cause |timestamp| to not be
  // monotonically increasing even though the decode timestamps are
  // monotonically increasing.
  void SetHighestPresentationTimestampIfIncreased(base::TimeDelta timestamp);

 private:
  // The decode timestamp of the last coded frame appended in the current coded
  // frame group. Initially kNoTimestamp(), meaning "unset".
  base::TimeDelta last_decode_timestamp_;

  // The coded frame duration of the last coded frame appended in the current
  // coded frame group. Initially kNoTimestamp(), meaning "unset".
  base::TimeDelta last_frame_duration_;

  // The highest presentation timestamp encountered in a coded frame appended
  // in the current coded frame group. Initially kNoTimestamp(), meaning
  // "unset".
  base::TimeDelta highest_presentation_timestamp_;

  // Keeps track of whether the track buffer is waiting for a random access
  // point coded frame. Initially set to true to indicate that a random access
  // point coded frame is needed before anything can be added to the track
  // buffer.
  bool needs_random_access_point_;

  // Pointer to the stream associated with this track. The stream is not owned
  // by |this|.
  ChunkDemuxerStream* const stream_;

  DISALLOW_COPY_AND_ASSIGN(MseTrackBuffer);
};

// Abstract interface for helper class implementation of Media Source
// Extension's coded frame processing algorithm.
// TODO(wolenetz): Once the new FrameProcessor implementation stabilizes, remove
// LegacyFrameProcessor and fold this interface into FrameProcessor. See
// http://crbug.com/249422.
class MEDIA_EXPORT FrameProcessorBase {
 public:
  // TODO(wolenetz/acolwell): Ensure that all TrackIds are coherent and unique
  // for each track buffer. For now, special track identifiers are used for each
  // of audio and video here, and text TrackIds are assumed to be non-negative.
  // See http://crbug.com/341581.
  enum {
    kAudioTrackId = -2,
    kVideoTrackId = -3
  };

  virtual ~FrameProcessorBase();

  // Get/set the current append mode, which if true means "sequence" and if
  // false means "segments".
  // See http://www.w3.org/TR/media-source/#widl-SourceBuffer-mode.
  bool sequence_mode() { return sequence_mode_; }
  virtual void SetSequenceMode(bool sequence_mode) = 0;

  // Processes buffers in |audio_buffers|, |video_buffers|, and |text_map|.
  // Returns true on success or false on failure which indicates decode error.
  // |append_window_start| and |append_window_end| correspond to the MSE spec's
  // similarly named source buffer attributes that are used in coded frame
  // processing.
  // |*new_media_segment| tracks whether the next buffers processed within the
  // append window represent the start of a new media segment. This method may
  // both use and update this flag.
  // Uses |*timestamp_offset| according to the coded frame processing algorithm,
  // including updating it as required in 'sequence' mode frame processing.
  virtual bool ProcessFrames(const StreamParser::BufferQueue& audio_buffers,
                             const StreamParser::BufferQueue& video_buffers,
                             const StreamParser::TextBufferQueueMap& text_map,
                             base::TimeDelta append_window_start,
                             base::TimeDelta append_window_end,
                             bool* new_media_segment,
                             base::TimeDelta* timestamp_offset) = 0;

  // Signals the frame processor to update its group start timestamp to be
  // |timestamp_offset| if it is in sequence append mode.
  void SetGroupStartTimestampIfInSequenceMode(base::TimeDelta timestamp_offset);

  // Adds a new track with unique track ID |id|.
  // If |id| has previously been added, returns false to indicate error.
  // Otherwise, returns true, indicating future ProcessFrames() will emit
  // frames for the track |id| to |stream|.
  bool AddTrack(StreamParser::TrackId id, ChunkDemuxerStream* stream);

  // Updates the internal mapping of TrackId to track buffer for the track
  // buffer formerly associated with |old_id| to be associated with |new_id|.
  // Returns false to indicate failure due to either no existing track buffer
  // for |old_id| or collision with previous track buffer already mapped to
  // |new_id|. Otherwise returns true.
  bool UpdateTrack(StreamParser::TrackId old_id, StreamParser::TrackId new_id);

  // Sets the need random access point flag on all track buffers to true.
  void SetAllTrackBuffersNeedRandomAccessPoint();

  // Resets state for the coded frame processing algorithm as described in steps
  // 2-5 of the MSE Reset Parser State algorithm described at
  // http://www.w3.org/TR/media-source/#sourcebuffer-reset-parser-state
  void Reset();

  // Must be called when the audio config is updated.  Used to manage when
  // the preroll buffer is cleared and the allowed "fudge" factor between
  // preroll buffers.
  void OnPossibleAudioConfigUpdate(const AudioDecoderConfig& config);

 protected:
  typedef std::map<StreamParser::TrackId, MseTrackBuffer*> TrackBufferMap;

  FrameProcessorBase();

  // If |track_buffers_| contains |id|, returns a pointer to the associated
  // MseTrackBuffer. Otherwise, returns NULL.
  MseTrackBuffer* FindTrack(StreamParser::TrackId id);

  // Signals all track buffers' streams that a new media segment is starting
  // with timestamp |segment_timestamp|.
  void NotifyNewMediaSegmentStarting(base::TimeDelta segment_timestamp);

  // Handles partial append window trimming of |buffer|.  Returns true if the
  // given |buffer| can be partially trimmed or have preroll added; otherwise,
  // returns false.
  //
  // If |buffer| overlaps |append_window_start|, the portion of |buffer| before
  // |append_window_start| will be marked for post-decode discard.  Further, if
  // |audio_preroll_buffer_| exists and abuts |buffer|, it will be set as
  // preroll on |buffer| and |audio_preroll_buffer_| will be cleared.  If the
  // preroll buffer does not abut |buffer|, it will be discarded, but not used.
  //
  // If |buffer| lies entirely before |append_window_start|, and thus would
  // normally be discarded, |audio_preroll_buffer_| will be set to |buffer| and
  // the method will return false.
  bool HandlePartialAppendWindowTrimming(
      base::TimeDelta append_window_start,
      base::TimeDelta append_window_end,
      const scoped_refptr<StreamParserBuffer>& buffer);

  // The AppendMode of the associated SourceBuffer.
  // See SetSequenceMode() for interpretation of |sequence_mode_|.
  // Per http://www.w3.org/TR/media-source/#widl-SourceBuffer-mode:
  // Controls how a sequence of media segments are handled. This is initially
  // set to false ("segments").
  bool sequence_mode_;

  // TrackId-indexed map of each track's stream.
  TrackBufferMap track_buffers_;

  // Tracks the MSE coded frame processing variable of same name.
  // Initially kNoTimestamp(), meaning "unset".
  // Note: LegacyFrameProcessor does not use this member; it's here to reduce
  // short-term plumbing of SetGroupStartTimestampIfInSequenceMode() until
  // LegacyFrameProcessor is removed.
  base::TimeDelta group_start_timestamp_;

 private:
  // The last audio buffer seen by the frame processor that was removed because
  // it was entirely before the start of the append window.
  scoped_refptr<StreamParserBuffer> audio_preroll_buffer_;

  // The AudioDecoderConfig associated with buffers handed to ProcessFrames().
  AudioDecoderConfig current_audio_config_;
  base::TimeDelta sample_duration_;

  DISALLOW_COPY_AND_ASSIGN(FrameProcessorBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FRAME_PROCESSOR_BASE_H_
