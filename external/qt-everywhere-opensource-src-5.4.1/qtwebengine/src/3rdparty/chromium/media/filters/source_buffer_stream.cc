// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_stream.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "media/base/audio_splicer.h"

namespace media {

typedef StreamParser::BufferQueue BufferQueue;

// Buffers with the same timestamp are only allowed under certain conditions.
// More precisely, it is allowed in all situations except when the previous
// frame is not a key frame and the current is a key frame.
// Examples of situations where DTS of two consecutive frames can be equal:
// - Video: VP8 Alt-Ref frames.
// - Video: IPBPBP...: DTS for I frame and for P frame can be equal.
// - Text track cues that start at same time.
// Returns true if |prev_is_keyframe| and |current_is_keyframe| indicate a
// same timestamp situation that is allowed. False is returned otherwise.
static bool AllowSameTimestamp(
    bool prev_is_keyframe, bool current_is_keyframe,
    SourceBufferStream::Type type) {
  return prev_is_keyframe || !current_is_keyframe;
}

// Returns the config ID of |buffer| if |buffer| has no splice buffers or
// |index| is out of range.  Otherwise returns the config ID for the fade out
// preroll buffer at position |index|.
static int GetConfigId(StreamParserBuffer* buffer, size_t index) {
  return index < buffer->splice_buffers().size()
             ? buffer->splice_buffers()[index]->GetConfigId()
             : buffer->GetConfigId();
}

// Helper class representing a range of buffered data. All buffers in a
// SourceBufferRange are ordered sequentially in presentation order with no
// gaps.
class SourceBufferRange {
 public:
  // Returns the maximum distance in time between any buffer seen in this
  // stream. Used to estimate the duration of a buffer if its duration is not
  // known.
  typedef base::Callback<base::TimeDelta()> InterbufferDistanceCB;

  // Creates a source buffer range with |new_buffers|. |new_buffers| cannot be
  // empty and the front of |new_buffers| must be a keyframe.
  // |media_segment_start_time| refers to the starting timestamp for the media
  // segment to which these buffers belong.
  SourceBufferRange(SourceBufferStream::Type type,
                    const BufferQueue& new_buffers,
                    base::TimeDelta media_segment_start_time,
                    const InterbufferDistanceCB& interbuffer_distance_cb);

  // Appends |buffers| to the end of the range and updates |keyframe_map_| as
  // it encounters new keyframes. Assumes |buffers| belongs at the end of the
  // range.
  void AppendBuffersToEnd(const BufferQueue& buffers);
  bool CanAppendBuffersToEnd(const BufferQueue& buffers) const;

  // Appends the buffers from |range| into this range.
  // The first buffer in |range| must come directly after the last buffer
  // in this range.
  // If |transfer_current_position| is true, |range|'s |next_buffer_index_|
  // is transfered to this SourceBufferRange.
  void AppendRangeToEnd(const SourceBufferRange& range,
                        bool transfer_current_position);
  bool CanAppendRangeToEnd(const SourceBufferRange& range) const;

  // Updates |next_buffer_index_| to point to the Buffer containing |timestamp|.
  // Assumes |timestamp| is valid and in this range.
  void Seek(base::TimeDelta timestamp);

  // Updates |next_buffer_index_| to point to next keyframe after or equal to
  // |timestamp|.
  void SeekAheadTo(base::TimeDelta timestamp);

  // Updates |next_buffer_index_| to point to next keyframe strictly after
  // |timestamp|.
  void SeekAheadPast(base::TimeDelta timestamp);

  // Seeks to the beginning of the range.
  void SeekToStart();

  // Finds the next keyframe from |buffers_| after |timestamp| (or at
  // |timestamp| if |is_exclusive| is false) and creates and returns a new
  // SourceBufferRange with the buffers from that keyframe onward.
  // The buffers in the new SourceBufferRange are moved out of this range. If
  // there is no keyframe after |timestamp|, SplitRange() returns null and this
  // range is unmodified.
  SourceBufferRange* SplitRange(base::TimeDelta timestamp, bool is_exclusive);

  // Deletes the buffers from this range starting at |timestamp|, exclusive if
  // |is_exclusive| is true, inclusive otherwise.
  // Resets |next_buffer_index_| if the buffer at |next_buffer_index_| was
  // deleted, and deletes the |keyframe_map_| entries for the buffers that
  // were removed.
  // |deleted_buffers| contains the buffers that were deleted from this range,
  // starting at the buffer that had been at |next_buffer_index_|.
  // Returns true if everything in the range was deleted. Otherwise
  // returns false.
  bool TruncateAt(base::TimeDelta timestamp,
                  BufferQueue* deleted_buffers, bool is_exclusive);
  // Deletes all buffers in range.
  void DeleteAll(BufferQueue* deleted_buffers);

  // Deletes a GOP from the front or back of the range and moves these
  // buffers into |deleted_buffers|. Returns the number of bytes deleted from
  // the range (i.e. the size in bytes of |deleted_buffers|).
  int DeleteGOPFromFront(BufferQueue* deleted_buffers);
  int DeleteGOPFromBack(BufferQueue* deleted_buffers);

  // Gets the range of GOP to secure at least |bytes_to_free| from
  // [|start_timestamp|, |end_timestamp|).
  // Returns the size of the buffers to secure if the buffers of
  // [|start_timestamp|, |end_removal_timestamp|) is removed.
  // Will not update |end_removal_timestamp| if the returned size is 0.
  int GetRemovalGOP(
      base::TimeDelta start_timestamp, base::TimeDelta end_timestamp,
      int bytes_to_free, base::TimeDelta* end_removal_timestamp);

  // Indicates whether the GOP at the beginning or end of the range contains the
  // next buffer position.
  bool FirstGOPContainsNextBufferPosition() const;
  bool LastGOPContainsNextBufferPosition() const;

  // Updates |out_buffer| with the next buffer in presentation order. Seek()
  // must be called before calls to GetNextBuffer(), and buffers are returned
  // in order from the last call to Seek(). Returns true if |out_buffer| is
  // filled with a valid buffer, false if there is not enough data to fulfill
  // the request.
  bool GetNextBuffer(scoped_refptr<StreamParserBuffer>* out_buffer);
  bool HasNextBuffer() const;

  // Returns the config ID for the buffer that will be returned by
  // GetNextBuffer().
  int GetNextConfigId() const;

  // Returns true if the range knows the position of the next buffer it should
  // return, i.e. it has been Seek()ed. This does not necessarily mean that it
  // has the next buffer yet.
  bool HasNextBufferPosition() const;

  // Resets this range to an "unseeked" state.
  void ResetNextBufferPosition();

  // Returns the timestamp of the next buffer that will be returned from
  // GetNextBuffer(), or kNoTimestamp() if the timestamp is unknown.
  base::TimeDelta GetNextTimestamp() const;

  // Returns the start timestamp of the range.
  base::TimeDelta GetStartTimestamp() const;

  // Returns the timestamp of the last buffer in the range.
  base::TimeDelta GetEndTimestamp() const;

  // Returns the timestamp for the end of the buffered region in this range.
  // This is an approximation if the duration for the last buffer in the range
  // is unset.
  base::TimeDelta GetBufferedEndTimestamp() const;

  // Gets the timestamp for the keyframe that is after |timestamp|. If
  // there isn't a keyframe in the range after |timestamp| then kNoTimestamp()
  // is returned. If |timestamp| is in the "gap" between the value  returned by
  // GetStartTimestamp() and the timestamp on the first buffer in |buffers_|,
  // then |timestamp| is returned.
  base::TimeDelta NextKeyframeTimestamp(base::TimeDelta timestamp);

  // Gets the timestamp for the closest keyframe that is <= |timestamp|. If
  // there isn't a keyframe before |timestamp| or |timestamp| is outside
  // this range, then kNoTimestamp() is returned.
  base::TimeDelta KeyframeBeforeTimestamp(base::TimeDelta timestamp);

  // Returns whether a buffer with a starting timestamp of |timestamp| would
  // belong in this range. This includes a buffer that would be appended to
  // the end of the range.
  bool BelongsToRange(base::TimeDelta timestamp) const;

  // Returns true if the range has enough data to seek to the specified
  // |timestamp|, false otherwise.
  bool CanSeekTo(base::TimeDelta timestamp) const;

  // Returns true if this range's buffered timespan completely overlaps the
  // buffered timespan of |range|.
  bool CompletelyOverlaps(const SourceBufferRange& range) const;

  // Returns true if the end of this range contains buffers that overlaps with
  // the beginning of |range|.
  bool EndOverlaps(const SourceBufferRange& range) const;

  // Returns true if |timestamp| is the timestamp of the next buffer in
  // sequence after |buffers_.back()|, false otherwise.
  bool IsNextInSequence(base::TimeDelta timestamp, bool is_keyframe) const;

  // Adds all buffers which overlap [start, end) to the end of |buffers|.  If
  // no buffers exist in the range returns false, true otherwise.
  bool GetBuffersInRange(base::TimeDelta start, base::TimeDelta end,
                         BufferQueue* buffers);

  int size_in_bytes() const { return size_in_bytes_; }

 private:
  typedef std::map<base::TimeDelta, int> KeyframeMap;

  // Seeks the range to the next keyframe after |timestamp|. If
  // |skip_given_timestamp| is true, the seek will go to a keyframe with a
  // timestamp strictly greater than |timestamp|.
  void SeekAhead(base::TimeDelta timestamp, bool skip_given_timestamp);

  // Returns an iterator in |buffers_| pointing to the buffer at |timestamp|.
  // If |skip_given_timestamp| is true, this returns the first buffer with
  // timestamp greater than |timestamp|.
  BufferQueue::iterator GetBufferItrAt(
      base::TimeDelta timestamp, bool skip_given_timestamp);

  // Returns an iterator in |keyframe_map_| pointing to the next keyframe after
  // |timestamp|. If |skip_given_timestamp| is true, this returns the first
  // keyframe with a timestamp strictly greater than |timestamp|.
  KeyframeMap::iterator GetFirstKeyframeAt(
      base::TimeDelta timestamp, bool skip_given_timestamp);

  // Returns an iterator in |keyframe_map_| pointing to the first keyframe
  // before or at |timestamp|.
  KeyframeMap::iterator GetFirstKeyframeBefore(base::TimeDelta timestamp);

  // Helper method to delete buffers in |buffers_| starting at
  // |starting_point|, an iterator in |buffers_|.
  // Returns true if everything in the range was removed. Returns
  // false if the range still contains buffers.
  bool TruncateAt(const BufferQueue::iterator& starting_point,
                  BufferQueue* deleted_buffers);

  // Frees the buffers in |buffers_| from [|start_point|,|ending_point|) and
  // updates the |size_in_bytes_| accordingly. Does not update |keyframe_map_|.
  void FreeBufferRange(const BufferQueue::iterator& starting_point,
                       const BufferQueue::iterator& ending_point);

  // Returns the distance in time estimating how far from the beginning or end
  // of this range a buffer can be to considered in the range.
  base::TimeDelta GetFudgeRoom() const;

  // Returns the approximate duration of a buffer in this range.
  base::TimeDelta GetApproximateDuration() const;

  // Type of this stream.
  const SourceBufferStream::Type type_;

  // An ordered list of buffers in this range.
  BufferQueue buffers_;

  // Maps keyframe timestamps to its index position in |buffers_|.
  KeyframeMap keyframe_map_;

  // Index base of all positions in |keyframe_map_|. In other words, the
  // real position of entry |k| of |keyframe_map_| in the range is:
  //   keyframe_map_[k] - keyframe_map_index_base_
  int keyframe_map_index_base_;

  // Index into |buffers_| for the next buffer to be returned by
  // GetNextBuffer(), set to -1 before Seek().
  int next_buffer_index_;

  // If the first buffer in this range is the beginning of a media segment,
  // |media_segment_start_time_| is the time when the media segment begins.
  // |media_segment_start_time_| may be <= the timestamp of the first buffer in
  // |buffers_|. |media_segment_start_time_| is kNoTimestamp() if this range
  // does not start at the beginning of a media segment, which can only happen
  // garbage collection or after an end overlap that results in a split range
  // (we don't have a way of knowing the media segment timestamp for the new
  // range).
  base::TimeDelta media_segment_start_time_;

  // Called to get the largest interbuffer distance seen so far in the stream.
  InterbufferDistanceCB interbuffer_distance_cb_;

  // Stores the amount of memory taken up by the data in |buffers_|.
  int size_in_bytes_;

  DISALLOW_COPY_AND_ASSIGN(SourceBufferRange);
};

}  // namespace media

// Helper method that returns true if |ranges| is sorted in increasing order,
// false otherwise.
static bool IsRangeListSorted(
    const std::list<media::SourceBufferRange*>& ranges) {
  base::TimeDelta prev = media::kNoTimestamp();
  for (std::list<media::SourceBufferRange*>::const_iterator itr =
       ranges.begin(); itr != ranges.end(); ++itr) {
    if (prev != media::kNoTimestamp() && prev >= (*itr)->GetStartTimestamp())
      return false;
    prev = (*itr)->GetEndTimestamp();
  }
  return true;
}

// Comparison operators for std::upper_bound() and std::lower_bound().
static bool CompareTimeDeltaToStreamParserBuffer(
    const base::TimeDelta& decode_timestamp,
    const scoped_refptr<media::StreamParserBuffer>& buffer) {
  return decode_timestamp < buffer->GetDecodeTimestamp();
}
static bool CompareStreamParserBufferToTimeDelta(
    const scoped_refptr<media::StreamParserBuffer>& buffer,
    const base::TimeDelta& decode_timestamp) {
  return buffer->GetDecodeTimestamp() < decode_timestamp;
}

// Returns an estimate of how far from the beginning or end of a range a buffer
// can be to still be considered in the range, given the |approximate_duration|
// of a buffer in the stream.
static base::TimeDelta ComputeFudgeRoom(base::TimeDelta approximate_duration) {
  // Because we do not know exactly when is the next timestamp, any buffer
  // that starts within 2x the approximate duration of a buffer is considered
  // within this range.
  return 2 * approximate_duration;
}

// An arbitrarily-chosen number to estimate the duration of a buffer if none
// is set and there's not enough information to get a better estimate.
static int kDefaultBufferDurationInMs = 125;

// The amount of time the beginning of the buffered data can differ from the
// start time in order to still be considered the start of stream.
static base::TimeDelta kSeekToStartFudgeRoom() {
  return base::TimeDelta::FromMilliseconds(1000);
}
// The maximum amount of data in bytes the stream will keep in memory.
// 12MB: approximately 5 minutes of 320Kbps content.
// 150MB: approximately 5 minutes of 4Mbps content.
static int kDefaultAudioMemoryLimit = 12 * 1024 * 1024;
static int kDefaultVideoMemoryLimit = 150 * 1024 * 1024;

namespace media {

SourceBufferStream::SourceBufferStream(const AudioDecoderConfig& audio_config,
                                       const LogCB& log_cb,
                                       bool splice_frames_enabled)
    : log_cb_(log_cb),
      current_config_index_(0),
      append_config_index_(0),
      seek_pending_(false),
      end_of_stream_(false),
      seek_buffer_timestamp_(kNoTimestamp()),
      selected_range_(NULL),
      media_segment_start_time_(kNoTimestamp()),
      range_for_next_append_(ranges_.end()),
      new_media_segment_(false),
      last_appended_buffer_timestamp_(kNoTimestamp()),
      last_appended_buffer_is_keyframe_(false),
      last_output_buffer_timestamp_(kNoTimestamp()),
      max_interbuffer_distance_(kNoTimestamp()),
      memory_limit_(kDefaultAudioMemoryLimit),
      config_change_pending_(false),
      splice_buffers_index_(0),
      pending_buffers_complete_(false),
      splice_frames_enabled_(splice_frames_enabled) {
  DCHECK(audio_config.IsValidConfig());
  audio_configs_.push_back(audio_config);
}

SourceBufferStream::SourceBufferStream(const VideoDecoderConfig& video_config,
                                       const LogCB& log_cb,
                                       bool splice_frames_enabled)
    : log_cb_(log_cb),
      current_config_index_(0),
      append_config_index_(0),
      seek_pending_(false),
      end_of_stream_(false),
      seek_buffer_timestamp_(kNoTimestamp()),
      selected_range_(NULL),
      media_segment_start_time_(kNoTimestamp()),
      range_for_next_append_(ranges_.end()),
      new_media_segment_(false),
      last_appended_buffer_timestamp_(kNoTimestamp()),
      last_appended_buffer_is_keyframe_(false),
      last_output_buffer_timestamp_(kNoTimestamp()),
      max_interbuffer_distance_(kNoTimestamp()),
      memory_limit_(kDefaultVideoMemoryLimit),
      config_change_pending_(false),
      splice_buffers_index_(0),
      pending_buffers_complete_(false),
      splice_frames_enabled_(splice_frames_enabled) {
  DCHECK(video_config.IsValidConfig());
  video_configs_.push_back(video_config);
}

SourceBufferStream::SourceBufferStream(const TextTrackConfig& text_config,
                                       const LogCB& log_cb,
                                       bool splice_frames_enabled)
    : log_cb_(log_cb),
      current_config_index_(0),
      append_config_index_(0),
      text_track_config_(text_config),
      seek_pending_(false),
      end_of_stream_(false),
      seek_buffer_timestamp_(kNoTimestamp()),
      selected_range_(NULL),
      media_segment_start_time_(kNoTimestamp()),
      range_for_next_append_(ranges_.end()),
      new_media_segment_(false),
      last_appended_buffer_timestamp_(kNoTimestamp()),
      last_appended_buffer_is_keyframe_(false),
      last_output_buffer_timestamp_(kNoTimestamp()),
      max_interbuffer_distance_(kNoTimestamp()),
      memory_limit_(kDefaultAudioMemoryLimit),
      config_change_pending_(false),
      splice_buffers_index_(0),
      pending_buffers_complete_(false),
      splice_frames_enabled_(splice_frames_enabled) {}

SourceBufferStream::~SourceBufferStream() {
  while (!ranges_.empty()) {
    delete ranges_.front();
    ranges_.pop_front();
  }
}

void SourceBufferStream::OnNewMediaSegment(
    base::TimeDelta media_segment_start_time) {
  DCHECK(!end_of_stream_);
  media_segment_start_time_ = media_segment_start_time;
  new_media_segment_ = true;

  RangeList::iterator last_range = range_for_next_append_;
  range_for_next_append_ = FindExistingRangeFor(media_segment_start_time);

  // Only reset |last_appended_buffer_timestamp_| if this new media segment is
  // not adjacent to the previous media segment appended to the stream.
  if (range_for_next_append_ == ranges_.end() ||
      !AreAdjacentInSequence(last_appended_buffer_timestamp_,
                             media_segment_start_time)) {
    last_appended_buffer_timestamp_ = kNoTimestamp();
    last_appended_buffer_is_keyframe_ = false;
  } else if (last_range != ranges_.end()) {
    DCHECK(last_range == range_for_next_append_);
  }
}

bool SourceBufferStream::Append(const BufferQueue& buffers) {
  TRACE_EVENT2("media", "SourceBufferStream::Append",
               "stream type", GetStreamTypeName(),
               "buffers to append", buffers.size());

  DCHECK(!buffers.empty());
  DCHECK(media_segment_start_time_ != kNoTimestamp());
  DCHECK(media_segment_start_time_ <= buffers.front()->GetDecodeTimestamp());
  DCHECK(!end_of_stream_);

  // New media segments must begin with a keyframe.
  if (new_media_segment_ && !buffers.front()->IsKeyframe()) {
    MEDIA_LOG(log_cb_) << "Media segment did not begin with keyframe.";
    return false;
  }

  // Buffers within a media segment should be monotonically increasing.
  if (!IsMonotonicallyIncreasing(buffers))
    return false;

  if (media_segment_start_time_ < base::TimeDelta() ||
      buffers.front()->GetDecodeTimestamp() < base::TimeDelta()) {
    MEDIA_LOG(log_cb_)
        << "Cannot append a media segment with negative timestamps.";
    return false;
  }

  if (!IsNextTimestampValid(buffers.front()->GetDecodeTimestamp(),
                            buffers.front()->IsKeyframe())) {
    MEDIA_LOG(log_cb_) << "Invalid same timestamp construct detected at time "
                       << buffers.front()->GetDecodeTimestamp().InSecondsF();

    return false;
  }

  UpdateMaxInterbufferDistance(buffers);
  SetConfigIds(buffers);

  // Save a snapshot of stream state before range modifications are made.
  base::TimeDelta next_buffer_timestamp = GetNextBufferTimestamp();
  BufferQueue deleted_buffers;

  PrepareRangesForNextAppend(buffers, &deleted_buffers);

  // If there's a range for |buffers|, insert |buffers| accordingly. Otherwise,
  // create a new range with |buffers|.
  if (range_for_next_append_ != ranges_.end()) {
    (*range_for_next_append_)->AppendBuffersToEnd(buffers);
    last_appended_buffer_timestamp_ = buffers.back()->GetDecodeTimestamp();
    last_appended_buffer_is_keyframe_ = buffers.back()->IsKeyframe();
  } else {
    base::TimeDelta new_range_start_time = std::min(
        media_segment_start_time_, buffers.front()->GetDecodeTimestamp());
    const BufferQueue* buffers_for_new_range = &buffers;
    BufferQueue trimmed_buffers;

    // If the new range is not being created because of a new media
    // segment, then we must make sure that we start with a keyframe.
    // This can happen if the GOP in the previous append gets destroyed
    // by a Remove() call.
    if (!new_media_segment_) {
      BufferQueue::const_iterator itr = buffers.begin();

      // Scan past all the non-keyframes.
      while (itr != buffers.end() && !(*itr)->IsKeyframe()) {
        ++itr;
      }

      // If we didn't find a keyframe, then update the last appended
      // buffer state and return.
      if (itr == buffers.end()) {
        last_appended_buffer_timestamp_ = buffers.back()->GetDecodeTimestamp();
        last_appended_buffer_is_keyframe_ = buffers.back()->IsKeyframe();
        return true;
      } else if (itr != buffers.begin()) {
        // Copy the first keyframe and everything after it into
        // |trimmed_buffers|.
        trimmed_buffers.assign(itr, buffers.end());
        buffers_for_new_range = &trimmed_buffers;
      }

      new_range_start_time =
          buffers_for_new_range->front()->GetDecodeTimestamp();
    }

    range_for_next_append_ =
        AddToRanges(new SourceBufferRange(
            GetType(), *buffers_for_new_range, new_range_start_time,
            base::Bind(&SourceBufferStream::GetMaxInterbufferDistance,
                       base::Unretained(this))));
    last_appended_buffer_timestamp_ =
        buffers_for_new_range->back()->GetDecodeTimestamp();
    last_appended_buffer_is_keyframe_ =
        buffers_for_new_range->back()->IsKeyframe();
  }

  new_media_segment_ = false;

  MergeWithAdjacentRangeIfNecessary(range_for_next_append_);

  // Seek to try to fulfill a previous call to Seek().
  if (seek_pending_) {
    DCHECK(!selected_range_);
    DCHECK(deleted_buffers.empty());
    Seek(seek_buffer_timestamp_);
  }

  if (!deleted_buffers.empty()) {
    base::TimeDelta start_of_deleted =
        deleted_buffers.front()->GetDecodeTimestamp();

    DCHECK(track_buffer_.empty() ||
           track_buffer_.back()->GetDecodeTimestamp() < start_of_deleted)
        << "decode timestamp "
        << track_buffer_.back()->GetDecodeTimestamp().InSecondsF() << " sec"
        << ", start_of_deleted " << start_of_deleted.InSecondsF()<< " sec";

    track_buffer_.insert(track_buffer_.end(), deleted_buffers.begin(),
                         deleted_buffers.end());
  }

  // Prune any extra buffers in |track_buffer_| if new keyframes
  // are appended to the range covered by |track_buffer_|.
  if (!track_buffer_.empty()) {
    base::TimeDelta keyframe_timestamp =
        FindKeyframeAfterTimestamp(track_buffer_.front()->GetDecodeTimestamp());
    if (keyframe_timestamp != kNoTimestamp())
      PruneTrackBuffer(keyframe_timestamp);
  }

  SetSelectedRangeIfNeeded(next_buffer_timestamp);

  GarbageCollectIfNeeded();

  DCHECK(IsRangeListSorted(ranges_));
  DCHECK(OnlySelectedRangeIsSeeked());
  return true;
}

void SourceBufferStream::Remove(base::TimeDelta start, base::TimeDelta end,
                                base::TimeDelta duration) {
  DVLOG(1) << __FUNCTION__ << "(" << start.InSecondsF()
           << ", " << end.InSecondsF()
           << ", " << duration.InSecondsF() << ")";
  DCHECK(start >= base::TimeDelta()) << start.InSecondsF();
  DCHECK(start < end) << "start " << start.InSecondsF()
                      << " end " << end.InSecondsF();
  DCHECK(duration != kNoTimestamp());

  base::TimeDelta remove_end_timestamp = duration;
  base::TimeDelta keyframe_timestamp = FindKeyframeAfterTimestamp(end);
  if (keyframe_timestamp != kNoTimestamp()) {
    remove_end_timestamp = keyframe_timestamp;
  } else if (end < remove_end_timestamp) {
    remove_end_timestamp = end;
  }

  BufferQueue deleted_buffers;
  RemoveInternal(start, remove_end_timestamp, false, &deleted_buffers);

  if (!deleted_buffers.empty())
    SetSelectedRangeIfNeeded(deleted_buffers.front()->GetDecodeTimestamp());
}

void SourceBufferStream::RemoveInternal(
    base::TimeDelta start, base::TimeDelta end, bool is_exclusive,
    BufferQueue* deleted_buffers) {
  DVLOG(1) << __FUNCTION__ << "(" << start.InSecondsF()
           << ", " << end.InSecondsF()
           << ", " << is_exclusive << ")";

  DCHECK(start >= base::TimeDelta());
  DCHECK(start < end) << "start " << start.InSecondsF()
                      << " end " << end.InSecondsF();
  DCHECK(deleted_buffers);

  RangeList::iterator itr = ranges_.begin();

  while (itr != ranges_.end()) {
    SourceBufferRange* range = *itr;
    if (range->GetStartTimestamp() >= end)
      break;

    // Split off any remaining end piece and add it to |ranges_|.
    SourceBufferRange* new_range = range->SplitRange(end, is_exclusive);
    if (new_range) {
      itr = ranges_.insert(++itr, new_range);
      --itr;

      // Update the selected range if the next buffer position was transferred
      // to |new_range|.
      if (new_range->HasNextBufferPosition())
        SetSelectedRange(new_range);
    }

    // Truncate the current range so that it only contains data before
    // the removal range.
    BufferQueue saved_buffers;
    bool delete_range = range->TruncateAt(start, &saved_buffers, is_exclusive);

    // Check to see if the current playback position was removed and
    // update the selected range appropriately.
    if (!saved_buffers.empty()) {
      DCHECK(!range->HasNextBufferPosition());
      DCHECK(deleted_buffers->empty());

      *deleted_buffers = saved_buffers;
    }

    if (range == selected_range_ && !range->HasNextBufferPosition())
      SetSelectedRange(NULL);

    // If the current range now is completely covered by the removal
    // range then delete it and move on.
    if (delete_range) {
      DeleteAndRemoveRange(&itr);
      continue;
    }

    // Clear |range_for_next_append_| if we determine that the removal
    // operation makes it impossible for the next append to be added
    // to the current range.
    if (range_for_next_append_ != ranges_.end() &&
        *range_for_next_append_ == range &&
        last_appended_buffer_timestamp_ != kNoTimestamp()) {
      base::TimeDelta potential_next_append_timestamp =
          last_appended_buffer_timestamp_ +
          base::TimeDelta::FromInternalValue(1);

      if (!range->BelongsToRange(potential_next_append_timestamp)) {
        DVLOG(1) << "Resetting range_for_next_append_ since the next append"
                 <<  " can't add to the current range.";
        range_for_next_append_ =
            FindExistingRangeFor(potential_next_append_timestamp);
      }
    }

    // Move on to the next range.
    ++itr;
  }

  DCHECK(IsRangeListSorted(ranges_));
  DCHECK(OnlySelectedRangeIsSeeked());
  DVLOG(1) << __FUNCTION__ << " : done";
}

void SourceBufferStream::ResetSeekState() {
  SetSelectedRange(NULL);
  track_buffer_.clear();
  config_change_pending_ = false;
  last_output_buffer_timestamp_ = kNoTimestamp();
  splice_buffers_index_ = 0;
  pending_buffer_ = NULL;
  pending_buffers_complete_ = false;
}

bool SourceBufferStream::ShouldSeekToStartOfBuffered(
    base::TimeDelta seek_timestamp) const {
  if (ranges_.empty())
    return false;
  base::TimeDelta beginning_of_buffered =
      ranges_.front()->GetStartTimestamp();
  return (seek_timestamp <= beginning_of_buffered &&
          beginning_of_buffered < kSeekToStartFudgeRoom());
}

bool SourceBufferStream::IsMonotonicallyIncreasing(
    const BufferQueue& buffers) const {
  DCHECK(!buffers.empty());
  base::TimeDelta prev_timestamp = last_appended_buffer_timestamp_;
  bool prev_is_keyframe = last_appended_buffer_is_keyframe_;
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    base::TimeDelta current_timestamp = (*itr)->GetDecodeTimestamp();
    bool current_is_keyframe = (*itr)->IsKeyframe();
    DCHECK(current_timestamp != kNoTimestamp());

    if (prev_timestamp != kNoTimestamp()) {
      if (current_timestamp < prev_timestamp) {
        MEDIA_LOG(log_cb_) << "Buffers were not monotonically increasing.";
        return false;
      }

      if (current_timestamp == prev_timestamp &&
          !AllowSameTimestamp(prev_is_keyframe, current_is_keyframe,
                              GetType())) {
        MEDIA_LOG(log_cb_) << "Unexpected combination of buffers with the"
                           << " same timestamp detected at "
                           << current_timestamp.InSecondsF();
        return false;
      }
    }

    prev_timestamp = current_timestamp;
    prev_is_keyframe = current_is_keyframe;
  }
  return true;
}

bool SourceBufferStream::IsNextTimestampValid(
    base::TimeDelta next_timestamp, bool next_is_keyframe) const {
  return (last_appended_buffer_timestamp_ != next_timestamp) ||
      new_media_segment_ ||
      AllowSameTimestamp(last_appended_buffer_is_keyframe_, next_is_keyframe,
                         GetType());
}


bool SourceBufferStream::OnlySelectedRangeIsSeeked() const {
  for (RangeList::const_iterator itr = ranges_.begin();
       itr != ranges_.end(); ++itr) {
    if ((*itr)->HasNextBufferPosition() && (*itr) != selected_range_)
      return false;
  }
  return !selected_range_ || selected_range_->HasNextBufferPosition();
}

void SourceBufferStream::UpdateMaxInterbufferDistance(
    const BufferQueue& buffers) {
  DCHECK(!buffers.empty());
  base::TimeDelta prev_timestamp = last_appended_buffer_timestamp_;
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    base::TimeDelta current_timestamp = (*itr)->GetDecodeTimestamp();
    DCHECK(current_timestamp != kNoTimestamp());

    if (prev_timestamp != kNoTimestamp()) {
      base::TimeDelta interbuffer_distance = current_timestamp - prev_timestamp;
      if (max_interbuffer_distance_ == kNoTimestamp()) {
        max_interbuffer_distance_ = interbuffer_distance;
      } else {
        max_interbuffer_distance_ =
            std::max(max_interbuffer_distance_, interbuffer_distance);
      }
    }
    prev_timestamp = current_timestamp;
  }
}

void SourceBufferStream::SetConfigIds(const BufferQueue& buffers) {
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    (*itr)->SetConfigId(append_config_index_);
  }
}

void SourceBufferStream::GarbageCollectIfNeeded() {
  // Compute size of |ranges_|.
  int ranges_size = 0;
  for (RangeList::iterator itr = ranges_.begin(); itr != ranges_.end(); ++itr)
    ranges_size += (*itr)->size_in_bytes();

  // Return if we're under or at the memory limit.
  if (ranges_size <= memory_limit_)
    return;

  int bytes_to_free = ranges_size - memory_limit_;

  // Begin deleting after the last appended buffer.
  int bytes_freed = FreeBuffersAfterLastAppended(bytes_to_free);

  // Begin deleting from the front.
  if (bytes_to_free - bytes_freed > 0)
    bytes_freed += FreeBuffers(bytes_to_free - bytes_freed, false);

  // Begin deleting from the back.
  if (bytes_to_free - bytes_freed > 0)
    FreeBuffers(bytes_to_free - bytes_freed, true);
}

int SourceBufferStream::FreeBuffersAfterLastAppended(int total_bytes_to_free) {
  base::TimeDelta next_buffer_timestamp = GetNextBufferTimestamp();
  if (last_appended_buffer_timestamp_ == kNoTimestamp() ||
      next_buffer_timestamp == kNoTimestamp() ||
      last_appended_buffer_timestamp_ >= next_buffer_timestamp) {
    return 0;
  }

  base::TimeDelta remove_range_start = last_appended_buffer_timestamp_;
  if (last_appended_buffer_is_keyframe_)
    remove_range_start += GetMaxInterbufferDistance();

  base::TimeDelta remove_range_start_keyframe = FindKeyframeAfterTimestamp(
      remove_range_start);
  if (remove_range_start_keyframe != kNoTimestamp())
    remove_range_start = remove_range_start_keyframe;
  if (remove_range_start >= next_buffer_timestamp)
    return 0;

  base::TimeDelta remove_range_end;
  int bytes_freed = GetRemovalRange(
      remove_range_start, next_buffer_timestamp, total_bytes_to_free,
      &remove_range_end);
  if (bytes_freed > 0)
    Remove(remove_range_start, remove_range_end, next_buffer_timestamp);
  return bytes_freed;
}

int SourceBufferStream::GetRemovalRange(
    base::TimeDelta start_timestamp, base::TimeDelta end_timestamp,
    int total_bytes_to_free, base::TimeDelta* removal_end_timestamp) {
  DCHECK(start_timestamp >= base::TimeDelta()) << start_timestamp.InSecondsF();
  DCHECK(start_timestamp < end_timestamp)
      << "start " << start_timestamp.InSecondsF()
      << ", end " << end_timestamp.InSecondsF();

  int bytes_to_free = total_bytes_to_free;
  int bytes_freed = 0;

  for (RangeList::iterator itr = ranges_.begin();
       itr != ranges_.end() && bytes_to_free > 0; ++itr) {
    SourceBufferRange* range = *itr;
    if (range->GetStartTimestamp() >= end_timestamp)
      break;
    if (range->GetEndTimestamp() < start_timestamp)
      continue;

    int bytes_removed = range->GetRemovalGOP(
        start_timestamp, end_timestamp, bytes_to_free, removal_end_timestamp);
    bytes_to_free -= bytes_removed;
    bytes_freed += bytes_removed;
  }
  return bytes_freed;
}

int SourceBufferStream::FreeBuffers(int total_bytes_to_free,
                                    bool reverse_direction) {
  TRACE_EVENT2("media", "SourceBufferStream::FreeBuffers",
               "total bytes to free", total_bytes_to_free,
               "reverse direction", reverse_direction);

  DCHECK_GT(total_bytes_to_free, 0);
  int bytes_to_free = total_bytes_to_free;
  int bytes_freed = 0;

  // This range will save the last GOP appended to |range_for_next_append_|
  // if the buffers surrounding it get deleted during garbage collection.
  SourceBufferRange* new_range_for_append = NULL;

  while (!ranges_.empty() && bytes_to_free > 0) {
    SourceBufferRange* current_range = NULL;
    BufferQueue buffers;
    int bytes_deleted = 0;

    if (reverse_direction) {
      current_range = ranges_.back();
      if (current_range->LastGOPContainsNextBufferPosition()) {
        DCHECK_EQ(current_range, selected_range_);
        break;
      }
      bytes_deleted = current_range->DeleteGOPFromBack(&buffers);
    } else {
      current_range = ranges_.front();
      if (current_range->FirstGOPContainsNextBufferPosition()) {
        DCHECK_EQ(current_range, selected_range_);
        break;
      }
      bytes_deleted = current_range->DeleteGOPFromFront(&buffers);
    }

    // Check to see if we've just deleted the GOP that was last appended.
    base::TimeDelta end_timestamp = buffers.back()->GetDecodeTimestamp();
    if (end_timestamp == last_appended_buffer_timestamp_) {
      DCHECK(last_appended_buffer_timestamp_ != kNoTimestamp());
      DCHECK(!new_range_for_append);
      // Create a new range containing these buffers.
      new_range_for_append = new SourceBufferRange(
          GetType(), buffers, kNoTimestamp(),
          base::Bind(&SourceBufferStream::GetMaxInterbufferDistance,
                     base::Unretained(this)));
      range_for_next_append_ = ranges_.end();
    } else {
      bytes_to_free -= bytes_deleted;
      bytes_freed += bytes_deleted;
    }

    if (current_range->size_in_bytes() == 0) {
      DCHECK_NE(current_range, selected_range_);
      DCHECK(range_for_next_append_ == ranges_.end() ||
             *range_for_next_append_ != current_range);
      delete current_range;
      reverse_direction ? ranges_.pop_back() : ranges_.pop_front();
    }
  }

  // Insert |new_range_for_append| into |ranges_|, if applicable.
  if (new_range_for_append) {
    range_for_next_append_ = AddToRanges(new_range_for_append);
    DCHECK(range_for_next_append_ != ranges_.end());

    // Check to see if we need to merge |new_range_for_append| with the range
    // before or after it. |new_range_for_append| is created whenever the last
    // GOP appended is encountered, regardless of whether any buffers after it
    // are ultimately deleted. Merging is necessary if there were no buffers
    // (or very few buffers) deleted after creating |new_range_for_append|.
    if (range_for_next_append_ != ranges_.begin()) {
      RangeList::iterator range_before_next = range_for_next_append_;
      --range_before_next;
      MergeWithAdjacentRangeIfNecessary(range_before_next);
    }
    MergeWithAdjacentRangeIfNecessary(range_for_next_append_);
  }
  return bytes_freed;
}

void SourceBufferStream::PrepareRangesForNextAppend(
    const BufferQueue& new_buffers, BufferQueue* deleted_buffers) {
  DCHECK(deleted_buffers);

  bool temporarily_select_range = false;
  if (!track_buffer_.empty()) {
    base::TimeDelta tb_timestamp = track_buffer_.back()->GetDecodeTimestamp();
    base::TimeDelta seek_timestamp = FindKeyframeAfterTimestamp(tb_timestamp);
    if (seek_timestamp != kNoTimestamp() &&
        seek_timestamp < new_buffers.front()->GetDecodeTimestamp() &&
        range_for_next_append_ != ranges_.end() &&
        (*range_for_next_append_)->BelongsToRange(seek_timestamp)) {
      DCHECK(tb_timestamp < seek_timestamp);
      DCHECK(!selected_range_);
      DCHECK(!(*range_for_next_append_)->HasNextBufferPosition());

      // If there are GOPs between the end of the track buffer and the
      // beginning of the new buffers, then temporarily seek the range
      // so that the buffers between these two times will be deposited in
      // |deleted_buffers| as if they were part of the current playback
      // position.
      // TODO(acolwell): Figure out a more elegant way to do this.
      SeekAndSetSelectedRange(*range_for_next_append_, seek_timestamp);
      temporarily_select_range = true;
    }
  }

  // Handle splices between the existing buffers and the new buffers.  If a
  // splice is generated the timestamp and duration of the first buffer in
  // |new_buffers| will be modified.
  if (splice_frames_enabled_)
    GenerateSpliceFrame(new_buffers);

  base::TimeDelta prev_timestamp = last_appended_buffer_timestamp_;
  bool prev_is_keyframe = last_appended_buffer_is_keyframe_;
  base::TimeDelta next_timestamp = new_buffers.front()->GetDecodeTimestamp();
  bool next_is_keyframe = new_buffers.front()->IsKeyframe();

  if (prev_timestamp != kNoTimestamp() && prev_timestamp != next_timestamp) {
    // Clean up the old buffers between the last appended buffer and the
    // beginning of |new_buffers|.
    RemoveInternal(prev_timestamp, next_timestamp, true, deleted_buffers);
  }

  // Make the delete range exclusive if we are dealing with an allowed same
  // timestamp situation. This prevents the first buffer in the current append
  // from deleting the last buffer in the previous append if both buffers
  // have the same timestamp.
  //
  // The delete range should never be exclusive if a splice frame was generated
  // because we don't generate splice frames for same timestamp situations.
  DCHECK(new_buffers.front()->splice_timestamp() !=
         new_buffers.front()->timestamp());
  const bool is_exclusive =
      new_buffers.front()->splice_buffers().empty() &&
      prev_timestamp == next_timestamp &&
      AllowSameTimestamp(prev_is_keyframe, next_is_keyframe, GetType());

  // Delete the buffers that |new_buffers| overlaps.
  base::TimeDelta start = new_buffers.front()->GetDecodeTimestamp();
  base::TimeDelta end = new_buffers.back()->GetDecodeTimestamp();
  base::TimeDelta duration = new_buffers.back()->duration();

  if (duration != kNoTimestamp() && duration > base::TimeDelta()) {
    end += duration;
  } else {
    // TODO(acolwell): Ensure all buffers actually have proper
    // duration info so that this hack isn't needed.
    // http://crbug.com/312836
    end += base::TimeDelta::FromInternalValue(1);
  }

  RemoveInternal(start, end, is_exclusive, deleted_buffers);

  // Restore the range seek state if necessary.
  if (temporarily_select_range)
    SetSelectedRange(NULL);
}

bool SourceBufferStream::AreAdjacentInSequence(
    base::TimeDelta first_timestamp, base::TimeDelta second_timestamp) const {
  return first_timestamp < second_timestamp &&
      second_timestamp <=
      first_timestamp + ComputeFudgeRoom(GetMaxInterbufferDistance());
}

void SourceBufferStream::PruneTrackBuffer(const base::TimeDelta timestamp) {
  // If we don't have the next timestamp, we don't have anything to delete.
  if (timestamp == kNoTimestamp())
    return;

  while (!track_buffer_.empty() &&
         track_buffer_.back()->GetDecodeTimestamp() >= timestamp) {
    track_buffer_.pop_back();
  }
}

void SourceBufferStream::MergeWithAdjacentRangeIfNecessary(
    const RangeList::iterator& range_with_new_buffers_itr) {
  DCHECK(range_with_new_buffers_itr != ranges_.end());

  SourceBufferRange* range_with_new_buffers = *range_with_new_buffers_itr;
  RangeList::iterator next_range_itr = range_with_new_buffers_itr;
  ++next_range_itr;

  if (next_range_itr == ranges_.end() ||
      !range_with_new_buffers->CanAppendRangeToEnd(**next_range_itr)) {
    return;
  }

  bool transfer_current_position = selected_range_ == *next_range_itr;
  range_with_new_buffers->AppendRangeToEnd(**next_range_itr,
                                           transfer_current_position);
  // Update |selected_range_| pointer if |range| has become selected after
  // merges.
  if (transfer_current_position)
    SetSelectedRange(range_with_new_buffers);

  if (next_range_itr == range_for_next_append_)
    range_for_next_append_ = range_with_new_buffers_itr;

  DeleteAndRemoveRange(&next_range_itr);
}

void SourceBufferStream::Seek(base::TimeDelta timestamp) {
  DCHECK(timestamp >= base::TimeDelta());
  ResetSeekState();

  if (ShouldSeekToStartOfBuffered(timestamp)) {
    ranges_.front()->SeekToStart();
    SetSelectedRange(ranges_.front());
    seek_pending_ = false;
    return;
  }

  seek_buffer_timestamp_ = timestamp;
  seek_pending_ = true;

  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->CanSeekTo(timestamp))
      break;
  }

  if (itr == ranges_.end())
    return;

  SeekAndSetSelectedRange(*itr, timestamp);
  seek_pending_ = false;
}

bool SourceBufferStream::IsSeekPending() const {
  return !(end_of_stream_ && IsEndSelected()) && seek_pending_;
}

void SourceBufferStream::OnSetDuration(base::TimeDelta duration) {
  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->GetEndTimestamp() > duration)
      break;
  }
  if (itr == ranges_.end())
    return;

  // Need to partially truncate this range.
  if ((*itr)->GetStartTimestamp() < duration) {
    bool delete_range = (*itr)->TruncateAt(duration, NULL, false);
    if ((*itr == selected_range_) && !selected_range_->HasNextBufferPosition())
      SetSelectedRange(NULL);

    if (delete_range) {
      DeleteAndRemoveRange(&itr);
    } else {
      ++itr;
    }
  }

  // Delete all ranges that begin after |duration|.
  while (itr != ranges_.end()) {
    // If we're about to delete the selected range, also reset the seek state.
    DCHECK((*itr)->GetStartTimestamp() >= duration);
    if (*itr == selected_range_)
      ResetSeekState();
    DeleteAndRemoveRange(&itr);
  }
}

SourceBufferStream::Status SourceBufferStream::GetNextBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  if (!pending_buffer_) {
    const SourceBufferStream::Status status = GetNextBufferInternal(out_buffer);
    if (status != SourceBufferStream::kSuccess || !SetPendingBuffer(out_buffer))
      return status;
  }

  if (!pending_buffer_->splice_buffers().empty())
    return HandleNextBufferWithSplice(out_buffer);

  DCHECK(pending_buffer_->preroll_buffer());
  return HandleNextBufferWithPreroll(out_buffer);
}

SourceBufferStream::Status SourceBufferStream::HandleNextBufferWithSplice(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  const BufferQueue& splice_buffers = pending_buffer_->splice_buffers();
  const size_t last_splice_buffer_index = splice_buffers.size() - 1;

  // Are there any splice buffers left to hand out?  The last buffer should be
  // handed out separately since it represents the first post-splice buffer.
  if (splice_buffers_index_ < last_splice_buffer_index) {
    // Account for config changes which occur between fade out buffers.
    if (current_config_index_ !=
        splice_buffers[splice_buffers_index_]->GetConfigId()) {
      config_change_pending_ = true;
      DVLOG(1) << "Config change (splice buffer config ID does not match).";
      return SourceBufferStream::kConfigChange;
    }

    // Every pre splice buffer must have the same splice_timestamp().
    DCHECK(pending_buffer_->splice_timestamp() ==
           splice_buffers[splice_buffers_index_]->splice_timestamp());

    // No pre splice buffers should have preroll.
    DCHECK(!splice_buffers[splice_buffers_index_]->preroll_buffer());

    *out_buffer = splice_buffers[splice_buffers_index_++];
    return SourceBufferStream::kSuccess;
  }

  // Did we hand out the last pre-splice buffer on the previous call?
  if (!pending_buffers_complete_) {
    DCHECK_EQ(splice_buffers_index_, last_splice_buffer_index);
    pending_buffers_complete_ = true;
    config_change_pending_ = true;
    DVLOG(1) << "Config change (forced for fade in of splice frame).";
    return SourceBufferStream::kConfigChange;
  }

  // All pre-splice buffers have been handed out and a config change completed,
  // so hand out the final buffer for fade in.  Because a config change is
  // always issued prior to handing out this buffer, any changes in config id
  // have been inherently handled.
  DCHECK(pending_buffers_complete_);
  DCHECK_EQ(splice_buffers_index_, splice_buffers.size() - 1);
  DCHECK(splice_buffers.back()->splice_timestamp() == kNoTimestamp());
  *out_buffer = splice_buffers.back();
  pending_buffer_ = NULL;

  // If the last splice buffer has preroll, hand off to the preroll handler.
  return SetPendingBuffer(out_buffer) ? HandleNextBufferWithPreroll(out_buffer)
                                      : SourceBufferStream::kSuccess;
}

SourceBufferStream::Status SourceBufferStream::HandleNextBufferWithPreroll(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  // Any config change should have already been handled.
  DCHECK_EQ(current_config_index_, pending_buffer_->GetConfigId());

  // Check if the preroll buffer has already been handed out.
  if (!pending_buffers_complete_) {
    pending_buffers_complete_ = true;
    *out_buffer = pending_buffer_->preroll_buffer();
    return SourceBufferStream::kSuccess;
  }

  // Preroll complete, hand out the final buffer.
  *out_buffer = pending_buffer_;
  pending_buffer_ = NULL;
  return SourceBufferStream::kSuccess;
}

SourceBufferStream::Status SourceBufferStream::GetNextBufferInternal(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  CHECK(!config_change_pending_);

  if (!track_buffer_.empty()) {
    DCHECK(!selected_range_);
    scoped_refptr<StreamParserBuffer>& next_buffer = track_buffer_.front();

    // If the next buffer is an audio splice frame, the next effective config id
    // comes from the first splice buffer.
    if (GetConfigId(next_buffer, 0) != current_config_index_) {
      config_change_pending_ = true;
      DVLOG(1) << "Config change (track buffer config ID does not match).";
      return kConfigChange;
    }

    *out_buffer = next_buffer;
    track_buffer_.pop_front();
    last_output_buffer_timestamp_ = (*out_buffer)->GetDecodeTimestamp();

    // If the track buffer becomes empty, then try to set the selected range
    // based on the timestamp of this buffer being returned.
    if (track_buffer_.empty())
      SetSelectedRangeIfNeeded(last_output_buffer_timestamp_);

    return kSuccess;
  }

  if (!selected_range_ || !selected_range_->HasNextBuffer()) {
    if (end_of_stream_ && IsEndSelected())
      return kEndOfStream;
    return kNeedBuffer;
  }

  if (selected_range_->GetNextConfigId() != current_config_index_) {
    config_change_pending_ = true;
    DVLOG(1) << "Config change (selected range config ID does not match).";
    return kConfigChange;
  }

  CHECK(selected_range_->GetNextBuffer(out_buffer));
  last_output_buffer_timestamp_ = (*out_buffer)->GetDecodeTimestamp();
  return kSuccess;
}

base::TimeDelta SourceBufferStream::GetNextBufferTimestamp() {
  if (!track_buffer_.empty())
    return track_buffer_.front()->GetDecodeTimestamp();

  if (!selected_range_)
    return kNoTimestamp();

  DCHECK(selected_range_->HasNextBufferPosition());
  return selected_range_->GetNextTimestamp();
}

base::TimeDelta SourceBufferStream::GetEndBufferTimestamp() {
  if (!selected_range_)
    return kNoTimestamp();
  return selected_range_->GetEndTimestamp();
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::FindExistingRangeFor(base::TimeDelta start_timestamp) {
  for (RangeList::iterator itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->BelongsToRange(start_timestamp))
      return itr;
  }
  return ranges_.end();
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::AddToRanges(SourceBufferRange* new_range) {
  base::TimeDelta start_timestamp = new_range->GetStartTimestamp();
  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->GetStartTimestamp() > start_timestamp)
      break;
  }
  return ranges_.insert(itr, new_range);
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::GetSelectedRangeItr() {
  DCHECK(selected_range_);
  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if (*itr == selected_range_)
      break;
  }
  DCHECK(itr != ranges_.end());
  return itr;
}

void SourceBufferStream::SeekAndSetSelectedRange(
    SourceBufferRange* range, base::TimeDelta seek_timestamp) {
  if (range)
    range->Seek(seek_timestamp);
  SetSelectedRange(range);
}

void SourceBufferStream::SetSelectedRange(SourceBufferRange* range) {
  DVLOG(1) << __FUNCTION__ << " : " << selected_range_ << " -> " << range;
  if (selected_range_)
    selected_range_->ResetNextBufferPosition();
  DCHECK(!range || range->HasNextBufferPosition());
  selected_range_ = range;
}

Ranges<base::TimeDelta> SourceBufferStream::GetBufferedTime() const {
  Ranges<base::TimeDelta> ranges;
  for (RangeList::const_iterator itr = ranges_.begin();
       itr != ranges_.end(); ++itr) {
    ranges.Add((*itr)->GetStartTimestamp(), (*itr)->GetBufferedEndTimestamp());
  }
  return ranges;
}

base::TimeDelta SourceBufferStream::GetBufferedDuration() const {
  if (ranges_.empty())
    return base::TimeDelta();

  return ranges_.back()->GetBufferedEndTimestamp();
}

void SourceBufferStream::MarkEndOfStream() {
  DCHECK(!end_of_stream_);
  end_of_stream_ = true;
}

void SourceBufferStream::UnmarkEndOfStream() {
  DCHECK(end_of_stream_);
  end_of_stream_ = false;
}

bool SourceBufferStream::IsEndSelected() const {
  if (ranges_.empty())
    return true;

  if (seek_pending_)
    return seek_buffer_timestamp_ >= ranges_.back()->GetBufferedEndTimestamp();

  return selected_range_ == ranges_.back();
}

const AudioDecoderConfig& SourceBufferStream::GetCurrentAudioDecoderConfig() {
  if (config_change_pending_)
    CompleteConfigChange();
  return audio_configs_[current_config_index_];
}

const VideoDecoderConfig& SourceBufferStream::GetCurrentVideoDecoderConfig() {
  if (config_change_pending_)
    CompleteConfigChange();
  return video_configs_[current_config_index_];
}

const TextTrackConfig& SourceBufferStream::GetCurrentTextTrackConfig() {
  return text_track_config_;
}

base::TimeDelta SourceBufferStream::GetMaxInterbufferDistance() const {
  if (max_interbuffer_distance_ == kNoTimestamp())
    return base::TimeDelta::FromMilliseconds(kDefaultBufferDurationInMs);
  return max_interbuffer_distance_;
}

bool SourceBufferStream::UpdateAudioConfig(const AudioDecoderConfig& config) {
  DCHECK(!audio_configs_.empty());
  DCHECK(video_configs_.empty());
  DVLOG(3) << "UpdateAudioConfig.";

  if (audio_configs_[0].codec() != config.codec()) {
    MEDIA_LOG(log_cb_) << "Audio codec changes not allowed.";
    return false;
  }

  if (audio_configs_[0].is_encrypted() != config.is_encrypted()) {
    MEDIA_LOG(log_cb_) << "Audio encryption changes not allowed.";
    return false;
  }

  // Check to see if the new config matches an existing one.
  for (size_t i = 0; i < audio_configs_.size(); ++i) {
    if (config.Matches(audio_configs_[i])) {
      append_config_index_ = i;
      return true;
    }
  }

  // No matches found so let's add this one to the list.
  append_config_index_ = audio_configs_.size();
  DVLOG(2) << "New audio config - index: " << append_config_index_;
  audio_configs_.resize(audio_configs_.size() + 1);
  audio_configs_[append_config_index_] = config;
  return true;
}

bool SourceBufferStream::UpdateVideoConfig(const VideoDecoderConfig& config) {
  DCHECK(!video_configs_.empty());
  DCHECK(audio_configs_.empty());
  DVLOG(3) << "UpdateVideoConfig.";

  if (video_configs_[0].codec() != config.codec()) {
    MEDIA_LOG(log_cb_) << "Video codec changes not allowed.";
    return false;
  }

  if (video_configs_[0].is_encrypted() != config.is_encrypted()) {
    MEDIA_LOG(log_cb_) << "Video encryption changes not allowed.";
    return false;
  }

  // Check to see if the new config matches an existing one.
  for (size_t i = 0; i < video_configs_.size(); ++i) {
    if (config.Matches(video_configs_[i])) {
      append_config_index_ = i;
      return true;
    }
  }

  // No matches found so let's add this one to the list.
  append_config_index_ = video_configs_.size();
  DVLOG(2) << "New video config - index: " << append_config_index_;
  video_configs_.resize(video_configs_.size() + 1);
  video_configs_[append_config_index_] = config;
  return true;
}

void SourceBufferStream::CompleteConfigChange() {
  config_change_pending_ = false;

  if (pending_buffer_) {
    current_config_index_ =
        GetConfigId(pending_buffer_, splice_buffers_index_);
    return;
  }

  if (!track_buffer_.empty()) {
    current_config_index_ = GetConfigId(track_buffer_.front(), 0);
    return;
  }

  if (selected_range_ && selected_range_->HasNextBuffer())
    current_config_index_ = selected_range_->GetNextConfigId();
}

void SourceBufferStream::SetSelectedRangeIfNeeded(
    const base::TimeDelta timestamp) {
  DVLOG(1) << __FUNCTION__ << "(" << timestamp.InSecondsF() << ")";

  if (selected_range_) {
    DCHECK(track_buffer_.empty());
    return;
  }

  if (!track_buffer_.empty()) {
    DCHECK(!selected_range_);
    return;
  }

  base::TimeDelta start_timestamp = timestamp;

  // If the next buffer timestamp is not known then use a timestamp just after
  // the timestamp on the last buffer returned by GetNextBuffer().
  if (start_timestamp == kNoTimestamp()) {
    if (last_output_buffer_timestamp_ == kNoTimestamp())
      return;

    start_timestamp = last_output_buffer_timestamp_ +
        base::TimeDelta::FromInternalValue(1);
  }

  base::TimeDelta seek_timestamp =
      FindNewSelectedRangeSeekTimestamp(start_timestamp);

  // If we don't have buffered data to seek to, then return.
  if (seek_timestamp == kNoTimestamp())
    return;

  DCHECK(track_buffer_.empty());
  SeekAndSetSelectedRange(*FindExistingRangeFor(seek_timestamp),
                          seek_timestamp);
}

base::TimeDelta SourceBufferStream::FindNewSelectedRangeSeekTimestamp(
    const base::TimeDelta start_timestamp) {
  DCHECK(start_timestamp != kNoTimestamp());
  DCHECK(start_timestamp >= base::TimeDelta());

  RangeList::iterator itr = ranges_.begin();

  for (; itr != ranges_.end(); ++itr) {
    if ((*itr)->GetEndTimestamp() >= start_timestamp) {
      break;
    }
  }

  if (itr == ranges_.end())
    return kNoTimestamp();

  // First check for a keyframe timestamp >= |start_timestamp|
  // in the current range.
  base::TimeDelta keyframe_timestamp =
      (*itr)->NextKeyframeTimestamp(start_timestamp);

  if (keyframe_timestamp != kNoTimestamp())
    return keyframe_timestamp;

  // If a keyframe was not found then look for a keyframe that is
  // "close enough" in the current or next range.
  base::TimeDelta end_timestamp =
      start_timestamp + ComputeFudgeRoom(GetMaxInterbufferDistance());
  DCHECK(start_timestamp < end_timestamp);

  // Make sure the current range doesn't start beyond |end_timestamp|.
  if ((*itr)->GetStartTimestamp() >= end_timestamp)
    return kNoTimestamp();

  keyframe_timestamp = (*itr)->KeyframeBeforeTimestamp(end_timestamp);

  // Check to see if the keyframe is within the acceptable range
  // (|start_timestamp|, |end_timestamp|].
  if (keyframe_timestamp != kNoTimestamp() &&
      start_timestamp < keyframe_timestamp  &&
      keyframe_timestamp <= end_timestamp) {
    return keyframe_timestamp;
  }

  // If |end_timestamp| is within this range, then no other checks are
  // necessary.
  if (end_timestamp <= (*itr)->GetEndTimestamp())
    return kNoTimestamp();

  // Move on to the next range.
  ++itr;

  // Return early if the next range does not contain |end_timestamp|.
  if (itr == ranges_.end() || (*itr)->GetStartTimestamp() >= end_timestamp)
    return kNoTimestamp();

  keyframe_timestamp = (*itr)->KeyframeBeforeTimestamp(end_timestamp);

  // Check to see if the keyframe is within the acceptable range
  // (|start_timestamp|, |end_timestamp|].
  if (keyframe_timestamp != kNoTimestamp() &&
      start_timestamp < keyframe_timestamp  &&
      keyframe_timestamp <= end_timestamp) {
    return keyframe_timestamp;
  }

  return kNoTimestamp();
}

base::TimeDelta SourceBufferStream::FindKeyframeAfterTimestamp(
    const base::TimeDelta timestamp) {
  DCHECK(timestamp != kNoTimestamp());

  RangeList::iterator itr = FindExistingRangeFor(timestamp);

  if (itr == ranges_.end())
    return kNoTimestamp();

  // First check for a keyframe timestamp >= |timestamp|
  // in the current range.
  return (*itr)->NextKeyframeTimestamp(timestamp);
}

std::string SourceBufferStream::GetStreamTypeName() const {
  switch (GetType()) {
    case kAudio:
      return "AUDIO";
    case kVideo:
      return "VIDEO";
    case kText:
      return "TEXT";
  }
  NOTREACHED();
  return "";
}

SourceBufferStream::Type SourceBufferStream::GetType() const {
  if (!audio_configs_.empty())
    return kAudio;
  if (!video_configs_.empty())
    return kVideo;
  DCHECK_NE(text_track_config_.kind(), kTextNone);
  return kText;
}

void SourceBufferStream::DeleteAndRemoveRange(RangeList::iterator* itr) {
  DVLOG(1) << __FUNCTION__;

  DCHECK(*itr != ranges_.end());
  if (**itr == selected_range_) {
    DVLOG(1) << __FUNCTION__ << " deleting selected range.";
    SetSelectedRange(NULL);
  }

  if (*itr == range_for_next_append_) {
    DVLOG(1) << __FUNCTION__ << " deleting range_for_next_append_.";
    range_for_next_append_ = ranges_.end();
    last_appended_buffer_timestamp_ = kNoTimestamp();
    last_appended_buffer_is_keyframe_ = false;
  }

  delete **itr;
  *itr = ranges_.erase(*itr);
}

void SourceBufferStream::GenerateSpliceFrame(const BufferQueue& new_buffers) {
  DCHECK(!new_buffers.empty());

  // Splice frames are only supported for audio.
  if (GetType() != kAudio)
    return;

  // Find the overlapped range (if any).
  const base::TimeDelta splice_timestamp = new_buffers.front()->timestamp();
  RangeList::iterator range_itr = FindExistingRangeFor(splice_timestamp);
  if (range_itr == ranges_.end())
    return;

  const base::TimeDelta max_splice_end_timestamp =
      splice_timestamp + base::TimeDelta::FromMilliseconds(
                             AudioSplicer::kCrossfadeDurationInMilliseconds);

  // Find all buffers involved before the splice point.
  BufferQueue pre_splice_buffers;
  if (!(*range_itr)->GetBuffersInRange(
          splice_timestamp, max_splice_end_timestamp, &pre_splice_buffers)) {
    return;
  }

  // If there are gaps in the timeline, it's possible that we only find buffers
  // after the splice point but within the splice range.  For simplicity, we do
  // not generate splice frames in this case.
  //
  // We also do not want to generate splices if the first new buffer replaces an
  // existing buffer exactly.
  if (pre_splice_buffers.front()->timestamp() >= splice_timestamp)
    return;

  // If any |pre_splice_buffers| are already splices or preroll, do not generate
  // a splice.
  for (size_t i = 0; i < pre_splice_buffers.size(); ++i) {
    const BufferQueue& original_splice_buffers =
        pre_splice_buffers[i]->splice_buffers();
    if (!original_splice_buffers.empty()) {
      DVLOG(1) << "Can't generate splice: overlapped buffers contain a "
                  "pre-existing splice.";
      return;
    }

    if (pre_splice_buffers[i]->preroll_buffer()) {
      DVLOG(1) << "Can't generate splice: overlapped buffers contain preroll.";
      return;
    }
  }

  // Don't generate splice frames which represent less than two frames, since we
  // need at least that much to generate a crossfade.  Per the spec, make this
  // check using the sample rate of the overlapping buffers.
  const base::TimeDelta splice_duration =
      pre_splice_buffers.back()->timestamp() +
      pre_splice_buffers.back()->duration() - splice_timestamp;
  const base::TimeDelta minimum_splice_duration = base::TimeDelta::FromSecondsD(
      2.0 / audio_configs_[append_config_index_].samples_per_second());
  if (splice_duration < minimum_splice_duration) {
    DVLOG(1) << "Can't generate splice: not enough samples for crossfade; have "
             << splice_duration.InMicroseconds() << " us, but need "
             << minimum_splice_duration.InMicroseconds() << " us.";
    return;
  }

  new_buffers.front()->ConvertToSpliceBuffer(pre_splice_buffers);
}

SourceBufferRange::SourceBufferRange(
    SourceBufferStream::Type type, const BufferQueue& new_buffers,
    base::TimeDelta media_segment_start_time,
    const InterbufferDistanceCB& interbuffer_distance_cb)
    : type_(type),
      keyframe_map_index_base_(0),
      next_buffer_index_(-1),
      media_segment_start_time_(media_segment_start_time),
      interbuffer_distance_cb_(interbuffer_distance_cb),
      size_in_bytes_(0) {
  CHECK(!new_buffers.empty());
  DCHECK(new_buffers.front()->IsKeyframe());
  DCHECK(!interbuffer_distance_cb.is_null());
  AppendBuffersToEnd(new_buffers);
}

void SourceBufferRange::AppendBuffersToEnd(const BufferQueue& new_buffers) {
  DCHECK(buffers_.empty() || CanAppendBuffersToEnd(new_buffers));
  DCHECK(media_segment_start_time_ == kNoTimestamp() ||
         media_segment_start_time_ <=
             new_buffers.front()->GetDecodeTimestamp());
  for (BufferQueue::const_iterator itr = new_buffers.begin();
       itr != new_buffers.end();
       ++itr) {
    DCHECK((*itr)->GetDecodeTimestamp() != kNoTimestamp());
    buffers_.push_back(*itr);
    size_in_bytes_ += (*itr)->data_size();

    if ((*itr)->IsKeyframe()) {
      keyframe_map_.insert(
          std::make_pair((*itr)->GetDecodeTimestamp(),
                         buffers_.size() - 1 + keyframe_map_index_base_));
    }
  }
}

void SourceBufferRange::Seek(base::TimeDelta timestamp) {
  DCHECK(CanSeekTo(timestamp));
  DCHECK(!keyframe_map_.empty());

  KeyframeMap::iterator result = GetFirstKeyframeBefore(timestamp);
  next_buffer_index_ = result->second - keyframe_map_index_base_;
  DCHECK_LT(next_buffer_index_, static_cast<int>(buffers_.size()));
}

void SourceBufferRange::SeekAheadTo(base::TimeDelta timestamp) {
  SeekAhead(timestamp, false);
}

void SourceBufferRange::SeekAheadPast(base::TimeDelta timestamp) {
  SeekAhead(timestamp, true);
}

void SourceBufferRange::SeekAhead(base::TimeDelta timestamp,
                                  bool skip_given_timestamp) {
  DCHECK(!keyframe_map_.empty());

  KeyframeMap::iterator result =
      GetFirstKeyframeAt(timestamp, skip_given_timestamp);

  // If there isn't a keyframe after |timestamp|, then seek to end and return
  // kNoTimestamp to signal such.
  if (result == keyframe_map_.end()) {
    next_buffer_index_ = -1;
    return;
  }
  next_buffer_index_ = result->second - keyframe_map_index_base_;
  DCHECK_LT(next_buffer_index_, static_cast<int>(buffers_.size()));
}

void SourceBufferRange::SeekToStart() {
  DCHECK(!buffers_.empty());
  next_buffer_index_ = 0;
}

SourceBufferRange* SourceBufferRange::SplitRange(
    base::TimeDelta timestamp, bool is_exclusive) {
  CHECK(!buffers_.empty());

  // Find the first keyframe after |timestamp|. If |is_exclusive|, do not
  // include keyframes at |timestamp|.
  KeyframeMap::iterator new_beginning_keyframe =
      GetFirstKeyframeAt(timestamp, is_exclusive);

  // If there is no keyframe after |timestamp|, we can't split the range.
  if (new_beginning_keyframe == keyframe_map_.end())
    return NULL;

  // Remove the data beginning at |keyframe_index| from |buffers_| and save it
  // into |removed_buffers|.
  int keyframe_index =
      new_beginning_keyframe->second - keyframe_map_index_base_;
  DCHECK_LT(keyframe_index, static_cast<int>(buffers_.size()));
  BufferQueue::iterator starting_point = buffers_.begin() + keyframe_index;
  BufferQueue removed_buffers(starting_point, buffers_.end());

  base::TimeDelta new_range_start_timestamp = kNoTimestamp();
  if (GetStartTimestamp() < buffers_.front()->GetDecodeTimestamp() &&
      timestamp < removed_buffers.front()->GetDecodeTimestamp()) {
    // The split is in the gap between |media_segment_start_time_| and
    // the first buffer of the new range so we should set the start
    // time of the new range to |timestamp| so we preserve part of the
    // gap in the new range.
    new_range_start_timestamp = timestamp;
  }

  keyframe_map_.erase(new_beginning_keyframe, keyframe_map_.end());
  FreeBufferRange(starting_point, buffers_.end());

  // Create a new range with |removed_buffers|.
  SourceBufferRange* split_range =
      new SourceBufferRange(
          type_, removed_buffers, new_range_start_timestamp,
          interbuffer_distance_cb_);

  // If the next buffer position is now in |split_range|, update the state of
  // this range and |split_range| accordingly.
  if (next_buffer_index_ >= static_cast<int>(buffers_.size())) {
    split_range->next_buffer_index_ = next_buffer_index_ - keyframe_index;
    ResetNextBufferPosition();
  }

  return split_range;
}

BufferQueue::iterator SourceBufferRange::GetBufferItrAt(
    base::TimeDelta timestamp,
    bool skip_given_timestamp) {
  return skip_given_timestamp
             ? std::upper_bound(buffers_.begin(),
                                buffers_.end(),
                                timestamp,
                                CompareTimeDeltaToStreamParserBuffer)
             : std::lower_bound(buffers_.begin(),
                                buffers_.end(),
                                timestamp,
                                CompareStreamParserBufferToTimeDelta);
}

SourceBufferRange::KeyframeMap::iterator
SourceBufferRange::GetFirstKeyframeAt(base::TimeDelta timestamp,
                                      bool skip_given_timestamp) {
  return skip_given_timestamp ?
      keyframe_map_.upper_bound(timestamp) :
      keyframe_map_.lower_bound(timestamp);
}

SourceBufferRange::KeyframeMap::iterator
SourceBufferRange::GetFirstKeyframeBefore(base::TimeDelta timestamp) {
  KeyframeMap::iterator result = keyframe_map_.lower_bound(timestamp);
  // lower_bound() returns the first element >= |timestamp|, so we want the
  // previous element if it did not return the element exactly equal to
  // |timestamp|.
  if (result != keyframe_map_.begin() &&
      (result == keyframe_map_.end() || result->first != timestamp)) {
    --result;
  }
  return result;
}

void SourceBufferRange::DeleteAll(BufferQueue* removed_buffers) {
  TruncateAt(buffers_.begin(), removed_buffers);
}

bool SourceBufferRange::TruncateAt(
    base::TimeDelta timestamp, BufferQueue* removed_buffers,
    bool is_exclusive) {
  // Find the place in |buffers_| where we will begin deleting data.
  BufferQueue::iterator starting_point =
      GetBufferItrAt(timestamp, is_exclusive);
  return TruncateAt(starting_point, removed_buffers);
}

int SourceBufferRange::DeleteGOPFromFront(BufferQueue* deleted_buffers) {
  DCHECK(!FirstGOPContainsNextBufferPosition());
  DCHECK(deleted_buffers);

  int buffers_deleted = 0;
  int total_bytes_deleted = 0;

  KeyframeMap::iterator front = keyframe_map_.begin();
  DCHECK(front != keyframe_map_.end());

  // Delete the keyframe at the start of |keyframe_map_|.
  keyframe_map_.erase(front);

  // Now we need to delete all the buffers that depend on the keyframe we've
  // just deleted.
  int end_index = keyframe_map_.size() > 0 ?
      keyframe_map_.begin()->second - keyframe_map_index_base_ :
      buffers_.size();

  // Delete buffers from the beginning of the buffered range up until (but not
  // including) the next keyframe.
  for (int i = 0; i < end_index; i++) {
    int bytes_deleted = buffers_.front()->data_size();
    size_in_bytes_ -= bytes_deleted;
    total_bytes_deleted += bytes_deleted;
    deleted_buffers->push_back(buffers_.front());
    buffers_.pop_front();
    ++buffers_deleted;
  }

  // Update |keyframe_map_index_base_| to account for the deleted buffers.
  keyframe_map_index_base_ += buffers_deleted;

  if (next_buffer_index_ > -1) {
    next_buffer_index_ -= buffers_deleted;
    DCHECK_GE(next_buffer_index_, 0);
  }

  // Invalidate media segment start time if we've deleted the first buffer of
  // the range.
  if (buffers_deleted > 0)
    media_segment_start_time_ = kNoTimestamp();

  return total_bytes_deleted;
}

int SourceBufferRange::DeleteGOPFromBack(BufferQueue* deleted_buffers) {
  DCHECK(!LastGOPContainsNextBufferPosition());
  DCHECK(deleted_buffers);

  // Remove the last GOP's keyframe from the |keyframe_map_|.
  KeyframeMap::iterator back = keyframe_map_.end();
  DCHECK_GT(keyframe_map_.size(), 0u);
  --back;

  // The index of the first buffer in the last GOP is equal to the new size of
  // |buffers_| after that GOP is deleted.
  size_t goal_size = back->second - keyframe_map_index_base_;
  keyframe_map_.erase(back);

  int total_bytes_deleted = 0;
  while (buffers_.size() != goal_size) {
    int bytes_deleted = buffers_.back()->data_size();
    size_in_bytes_ -= bytes_deleted;
    total_bytes_deleted += bytes_deleted;
    // We're removing buffers from the back, so push each removed buffer to the
    // front of |deleted_buffers| so that |deleted_buffers| are in nondecreasing
    // order.
    deleted_buffers->push_front(buffers_.back());
    buffers_.pop_back();
  }

  return total_bytes_deleted;
}

int SourceBufferRange::GetRemovalGOP(
    base::TimeDelta start_timestamp, base::TimeDelta end_timestamp,
    int total_bytes_to_free, base::TimeDelta* removal_end_timestamp) {
  int bytes_to_free = total_bytes_to_free;
  int bytes_removed = 0;

  KeyframeMap::iterator gop_itr = GetFirstKeyframeAt(start_timestamp, false);
  if (gop_itr == keyframe_map_.end())
    return 0;
  int keyframe_index = gop_itr->second - keyframe_map_index_base_;
  BufferQueue::iterator buffer_itr = buffers_.begin() + keyframe_index;
  KeyframeMap::iterator gop_end = keyframe_map_.end();
  if (end_timestamp < GetBufferedEndTimestamp())
    gop_end = GetFirstKeyframeBefore(end_timestamp);

  // Check if the removal range is within a GOP and skip the loop if so.
  // [keyframe]...[start_timestamp]...[end_timestamp]...[keyframe]
  KeyframeMap::iterator gop_itr_prev = gop_itr;
  if (gop_itr_prev != keyframe_map_.begin() && --gop_itr_prev == gop_end)
    gop_end = gop_itr;

  while (gop_itr != gop_end && bytes_to_free > 0) {
    ++gop_itr;

    int gop_size = 0;
    int next_gop_index = gop_itr == keyframe_map_.end() ?
        buffers_.size() : gop_itr->second - keyframe_map_index_base_;
    BufferQueue::iterator next_gop_start = buffers_.begin() + next_gop_index;
    for (; buffer_itr != next_gop_start; ++buffer_itr)
      gop_size += (*buffer_itr)->data_size();

    bytes_removed += gop_size;
    bytes_to_free -= gop_size;
  }
  if (bytes_removed > 0) {
    *removal_end_timestamp = gop_itr == keyframe_map_.end() ?
        GetBufferedEndTimestamp() : gop_itr->first;
  }
  return bytes_removed;
}

bool SourceBufferRange::FirstGOPContainsNextBufferPosition() const {
  if (!HasNextBufferPosition())
    return false;

  // If there is only one GOP, it must contain the next buffer position.
  if (keyframe_map_.size() == 1u)
    return true;

  KeyframeMap::const_iterator second_gop = keyframe_map_.begin();
  ++second_gop;
  return next_buffer_index_ < second_gop->second - keyframe_map_index_base_;
}

bool SourceBufferRange::LastGOPContainsNextBufferPosition() const {
  if (!HasNextBufferPosition())
    return false;

  // If there is only one GOP, it must contain the next buffer position.
  if (keyframe_map_.size() == 1u)
    return true;

  KeyframeMap::const_iterator last_gop = keyframe_map_.end();
  --last_gop;
  return last_gop->second - keyframe_map_index_base_ <= next_buffer_index_;
}

void SourceBufferRange::FreeBufferRange(
    const BufferQueue::iterator& starting_point,
    const BufferQueue::iterator& ending_point) {
  for (BufferQueue::iterator itr = starting_point;
       itr != ending_point; ++itr) {
    size_in_bytes_ -= (*itr)->data_size();
    DCHECK_GE(size_in_bytes_, 0);
  }
  buffers_.erase(starting_point, ending_point);
}

bool SourceBufferRange::TruncateAt(
    const BufferQueue::iterator& starting_point, BufferQueue* removed_buffers) {
  DCHECK(!removed_buffers || removed_buffers->empty());

  // Return if we're not deleting anything.
  if (starting_point == buffers_.end())
    return buffers_.empty();

  // Reset the next buffer index if we will be deleting the buffer that's next
  // in sequence.
  if (HasNextBufferPosition()) {
    base::TimeDelta next_buffer_timestamp = GetNextTimestamp();
    if (next_buffer_timestamp == kNoTimestamp() ||
        next_buffer_timestamp >= (*starting_point)->GetDecodeTimestamp()) {
      if (HasNextBuffer() && removed_buffers) {
        int starting_offset = starting_point - buffers_.begin();
        int next_buffer_offset = next_buffer_index_ - starting_offset;
        DCHECK_GE(next_buffer_offset, 0);
        BufferQueue saved(starting_point + next_buffer_offset, buffers_.end());
        removed_buffers->swap(saved);
      }
      ResetNextBufferPosition();
    }
  }

  // Remove keyframes from |starting_point| onward.
  KeyframeMap::iterator starting_point_keyframe =
      keyframe_map_.lower_bound((*starting_point)->GetDecodeTimestamp());
  keyframe_map_.erase(starting_point_keyframe, keyframe_map_.end());

  // Remove everything from |starting_point| onward.
  FreeBufferRange(starting_point, buffers_.end());
  return buffers_.empty();
}

bool SourceBufferRange::GetNextBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  if (!HasNextBuffer())
    return false;

  *out_buffer = buffers_[next_buffer_index_];
  next_buffer_index_++;
  return true;
}

bool SourceBufferRange::HasNextBuffer() const {
  return next_buffer_index_ >= 0 &&
      next_buffer_index_ < static_cast<int>(buffers_.size());
}

int SourceBufferRange::GetNextConfigId() const {
  DCHECK(HasNextBuffer());
  // If the next buffer is an audio splice frame, the next effective config id
  // comes from the first fade out preroll buffer.
  return GetConfigId(buffers_[next_buffer_index_], 0);
}

base::TimeDelta SourceBufferRange::GetNextTimestamp() const {
  DCHECK(!buffers_.empty());
  DCHECK(HasNextBufferPosition());

  if (next_buffer_index_ >= static_cast<int>(buffers_.size())) {
    return kNoTimestamp();
  }

  return buffers_[next_buffer_index_]->GetDecodeTimestamp();
}

bool SourceBufferRange::HasNextBufferPosition() const {
  return next_buffer_index_ >= 0;
}

void SourceBufferRange::ResetNextBufferPosition() {
  next_buffer_index_ = -1;
}

void SourceBufferRange::AppendRangeToEnd(const SourceBufferRange& range,
                                         bool transfer_current_position) {
  DCHECK(CanAppendRangeToEnd(range));
  DCHECK(!buffers_.empty());

  if (transfer_current_position && range.next_buffer_index_ >= 0)
    next_buffer_index_ = range.next_buffer_index_ + buffers_.size();

  AppendBuffersToEnd(range.buffers_);
}

bool SourceBufferRange::CanAppendRangeToEnd(
    const SourceBufferRange& range) const {
  return CanAppendBuffersToEnd(range.buffers_);
}

bool SourceBufferRange::CanAppendBuffersToEnd(
    const BufferQueue& buffers) const {
  DCHECK(!buffers_.empty());
  return IsNextInSequence(buffers.front()->GetDecodeTimestamp(),
                          buffers.front()->IsKeyframe());
}

bool SourceBufferRange::BelongsToRange(base::TimeDelta timestamp) const {
  DCHECK(!buffers_.empty());

  return (IsNextInSequence(timestamp, false) ||
          (GetStartTimestamp() <= timestamp && timestamp <= GetEndTimestamp()));
}

bool SourceBufferRange::CanSeekTo(base::TimeDelta timestamp) const {
  base::TimeDelta start_timestamp =
      std::max(base::TimeDelta(), GetStartTimestamp() - GetFudgeRoom());
  return !keyframe_map_.empty() && start_timestamp <= timestamp &&
      timestamp < GetBufferedEndTimestamp();
}

bool SourceBufferRange::CompletelyOverlaps(
    const SourceBufferRange& range) const {
  return GetStartTimestamp() <= range.GetStartTimestamp() &&
      GetEndTimestamp() >= range.GetEndTimestamp();
}

bool SourceBufferRange::EndOverlaps(const SourceBufferRange& range) const {
  return range.GetStartTimestamp() <= GetEndTimestamp() &&
      GetEndTimestamp() < range.GetEndTimestamp();
}

base::TimeDelta SourceBufferRange::GetStartTimestamp() const {
  DCHECK(!buffers_.empty());
  base::TimeDelta start_timestamp = media_segment_start_time_;
  if (start_timestamp == kNoTimestamp())
    start_timestamp = buffers_.front()->GetDecodeTimestamp();
  return start_timestamp;
}

base::TimeDelta SourceBufferRange::GetEndTimestamp() const {
  DCHECK(!buffers_.empty());
  return buffers_.back()->GetDecodeTimestamp();
}

base::TimeDelta SourceBufferRange::GetBufferedEndTimestamp() const {
  DCHECK(!buffers_.empty());
  base::TimeDelta duration = buffers_.back()->duration();
  if (duration == kNoTimestamp() || duration == base::TimeDelta())
    duration = GetApproximateDuration();
  return GetEndTimestamp() + duration;
}

base::TimeDelta SourceBufferRange::NextKeyframeTimestamp(
    base::TimeDelta timestamp) {
  DCHECK(!keyframe_map_.empty());

  if (timestamp < GetStartTimestamp() || timestamp >= GetBufferedEndTimestamp())
    return kNoTimestamp();

  KeyframeMap::iterator itr = GetFirstKeyframeAt(timestamp, false);
  if (itr == keyframe_map_.end())
    return kNoTimestamp();

  // If the timestamp is inside the gap between the start of the media
  // segment and the first buffer, then just pretend there is a
  // keyframe at the specified timestamp.
  if (itr == keyframe_map_.begin() &&
      timestamp > media_segment_start_time_ &&
      timestamp < itr->first) {
    return timestamp;
  }

  return itr->first;
}

base::TimeDelta SourceBufferRange::KeyframeBeforeTimestamp(
    base::TimeDelta timestamp) {
  DCHECK(!keyframe_map_.empty());

  if (timestamp < GetStartTimestamp() || timestamp >= GetBufferedEndTimestamp())
    return kNoTimestamp();

  return GetFirstKeyframeBefore(timestamp)->first;
}

bool SourceBufferRange::IsNextInSequence(
    base::TimeDelta timestamp, bool is_keyframe) const {
  base::TimeDelta end = buffers_.back()->GetDecodeTimestamp();
  if (end < timestamp &&
      (type_ == SourceBufferStream::kText ||
          timestamp <= end + GetFudgeRoom())) {
    return true;
  }

  return timestamp == end && AllowSameTimestamp(
      buffers_.back()->IsKeyframe(), is_keyframe, type_);
}

base::TimeDelta SourceBufferRange::GetFudgeRoom() const {
  return ComputeFudgeRoom(GetApproximateDuration());
}

base::TimeDelta SourceBufferRange::GetApproximateDuration() const {
  base::TimeDelta max_interbuffer_distance = interbuffer_distance_cb_.Run();
  DCHECK(max_interbuffer_distance != kNoTimestamp());
  return max_interbuffer_distance;
}

bool SourceBufferRange::GetBuffersInRange(base::TimeDelta start,
                                          base::TimeDelta end,
                                          BufferQueue* buffers) {
  // Find the nearest buffer with a decode timestamp <= start.
  const base::TimeDelta first_timestamp = KeyframeBeforeTimestamp(start);
  if (first_timestamp == kNoTimestamp())
    return false;

  // Find all buffers involved in the range.
  const size_t previous_size = buffers->size();
  for (BufferQueue::iterator it = GetBufferItrAt(first_timestamp, false);
       it != buffers_.end();
       ++it) {
    const scoped_refptr<StreamParserBuffer>& buffer = *it;
    // Buffers without duration are not supported, so bail if we encounter any.
    if (buffer->duration() == kNoTimestamp() ||
        buffer->duration() <= base::TimeDelta()) {
      return false;
    }
    if (buffer->end_of_stream() || buffer->timestamp() >= end)
      break;
    if (buffer->timestamp() + buffer->duration() <= start)
      continue;
    buffers->push_back(buffer);
  }
  return previous_size < buffers->size();
}

bool SourceBufferStream::SetPendingBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  DCHECK(*out_buffer);
  DCHECK(!pending_buffer_);

  const bool have_splice_buffers = !(*out_buffer)->splice_buffers().empty();
  const bool have_preroll_buffer = !!(*out_buffer)->preroll_buffer();

  if (!have_splice_buffers && !have_preroll_buffer)
    return false;

  DCHECK_NE(have_splice_buffers, have_preroll_buffer);
  splice_buffers_index_ = 0;
  pending_buffer_.swap(*out_buffer);
  pending_buffers_complete_ = false;
  return true;
}

}  // namespace media
