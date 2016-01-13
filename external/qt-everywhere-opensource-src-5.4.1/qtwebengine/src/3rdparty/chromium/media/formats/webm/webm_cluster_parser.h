// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_CLUSTER_PARSER_H_
#define MEDIA_FORMATS_WEBM_WEBM_CLUSTER_PARSER_H_

#include <deque>
#include <map>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/formats/webm/webm_parser.h"
#include "media/formats/webm/webm_tracks_parser.h"

namespace media {

class MEDIA_EXPORT WebMClusterParser : public WebMParserClient {
 public:
  typedef StreamParser::TrackId TrackId;
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;
  typedef std::map<TrackId, const BufferQueue> TextBufferQueueMap;

  // Arbitrarily-chosen numbers to estimate the duration of a buffer if none is
  // set and there is not enough information to get a better estimate.
  // TODO(wolenetz/acolwell): Parse audio codebook to determine missing audio
  // frame durations. See http://crbug.com/351166.
  enum {
    kDefaultAudioBufferDurationInMs = 23,  // Common 1k samples @44.1kHz
    kDefaultVideoBufferDurationInMs = 42  // Low 24fps to reduce stalls
  };

 private:
  // Helper class that manages per-track state.
  class Track {
   public:
    Track(int track_num,
          bool is_video,
          base::TimeDelta default_duration,
          const LogCB& log_cb);
    ~Track();

    int track_num() const { return track_num_; }

    // If a buffer is currently held aside pending duration calculation, returns
    // its decode timestamp. Otherwise, returns kInfiniteDuration().
    base::TimeDelta GetReadyUpperBound();

    // Prepares |ready_buffers_| for retrieval. Prior to calling,
    // |ready_buffers_| must be empty. Moves all |buffers_| with timestamp
    // before |before_timestamp| to |ready_buffers_|, preserving their order.
    void ExtractReadyBuffers(const base::TimeDelta before_timestamp);

    const BufferQueue& ready_buffers() const { return ready_buffers_; }

    // If |last_added_buffer_missing_duration_| is set, updates its duration
    // relative to |buffer|'s timestamp, and adds it to |buffers_| and unsets
    // |last_added_buffer_missing_duration_|. Then, if |buffer| is missing
    // duration, saves |buffer| into |last_added_buffer_missing_duration_|, or
    // otherwise adds |buffer| to |buffers_|.
    bool AddBuffer(const scoped_refptr<StreamParserBuffer>& buffer);

    // If |last_added_buffer_missing_duration_| is set, updates its duration to
    // be non-kNoTimestamp() value of |estimated_next_frame_duration_| or an
    // arbitrary default, then adds it to |buffers_| and unsets
    // |last_added_buffer_missing_duration_|. (This method helps stream parser
    // emit all buffers in a media segment before signaling end of segment.)
    void ApplyDurationEstimateIfNeeded();

    // Clears |ready_buffers_| (use ExtractReadyBuffers() to fill it again).
    // Leaves as-is |buffers_| and any possibly held-aside buffer that is
    // missing duration.
    void ClearReadyBuffers();

    // Clears all buffer state, including any possibly held-aside buffer that
    // was missing duration, and all contents of |buffers_| and
    // |ready_buffers_|.
    void Reset();

    // Helper function used to inspect block data to determine if the
    // block is a keyframe.
    // |data| contains the bytes in the block.
    // |size| indicates the number of bytes in |data|.
    bool IsKeyframe(const uint8* data, int size) const;

    base::TimeDelta default_duration() const { return default_duration_; }

   private:
    // Helper that sanity-checks |buffer| duration, updates
    // |estimated_next_frame_duration_|, and adds |buffer| to |buffers_|.
    // Returns false if |buffer| failed sanity check and therefore was not added
    // to |buffers_|. Returns true otherwise.
    bool QueueBuffer(const scoped_refptr<StreamParserBuffer>& buffer);

    // Helper that calculates the buffer duration to use in
    // ApplyDurationEstimateIfNeeded().
    base::TimeDelta GetDurationEstimate();

    int track_num_;
    bool is_video_;

    // Parsed track buffers, each with duration and in (decode) timestamp order,
    // that have not yet been extracted into |ready_buffers_|. Note that up to
    // one additional buffer missing duration may be tracked by
    // |last_added_buffer_missing_duration_|.
    BufferQueue buffers_;
    scoped_refptr<StreamParserBuffer> last_added_buffer_missing_duration_;

    // Buffers in (decode) timestamp order that were previously parsed into and
    // extracted from |buffers_|. Buffers are moved from |buffers_| to
    // |ready_buffers_| by ExtractReadyBuffers() if they are below a specified
    // upper bound timestamp. Track users can therefore extract only those
    // parsed buffers which are "ready" for emission (all before some maximum
    // timestamp).
    BufferQueue ready_buffers_;

    // If kNoTimestamp(), then |estimated_next_frame_duration_| will be used.
    base::TimeDelta default_duration_;

    // If kNoTimestamp(), then a default value will be used. This estimate is
    // the maximum duration seen or derived so far for this track, and is valid
    // only if |default_duration_| is kNoTimestamp().
    base::TimeDelta estimated_next_frame_duration_;

    LogCB log_cb_;
  };

  typedef std::map<int, Track> TextTrackMap;

 public:
  WebMClusterParser(int64 timecode_scale,
                    int audio_track_num,
                    base::TimeDelta audio_default_duration,
                    int video_track_num,
                    base::TimeDelta video_default_duration,
                    const WebMTracksParser::TextTracks& text_tracks,
                    const std::set<int64>& ignored_tracks,
                    const std::string& audio_encryption_key_id,
                    const std::string& video_encryption_key_id,
                    const LogCB& log_cb);
  virtual ~WebMClusterParser();

  // Resets the parser state so it can accept a new cluster.
  void Reset();

  // Parses a WebM cluster element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8* buf, int size);

  base::TimeDelta cluster_start_time() const { return cluster_start_time_; }

  // Get the current ready buffers resulting from Parse().
  // If the parse reached the end of cluster and the last buffer was held aside
  // due to missing duration, the buffer is given an estimated duration and
  // included in the result.
  // Otherwise, if there are is a buffer held aside due to missing duration for
  // any of the tracks, no buffers with same or greater (decode) timestamp will
  // be included in the buffers.
  // The returned deques are cleared by Parse() or Reset() and updated by the
  // next calls to Get{Audio,Video}Buffers().
  // If no Parse() or Reset() has occurred since the last call to Get{Audio,
  // Video,Text}Buffers(), then the previous BufferQueue& is returned again
  // without any recalculation.
  const BufferQueue& GetAudioBuffers();
  const BufferQueue& GetVideoBuffers();

  // Constructs and returns a subset of |text_track_map_| containing only
  // tracks with non-empty buffer queues produced by the last Parse() and
  // filtered to exclude any buffers that have (decode) timestamp same or
  // greater than the lowest (decode) timestamp across all tracks of any buffer
  // held aside due to missing duration (unless the end of cluster has been
  // reached).
  // The returned map is cleared by Parse() or Reset() and updated by the next
  // call to GetTextBuffers().
  // If no Parse() or Reset() has occurred since the last call to
  // GetTextBuffers(), then the previous TextBufferQueueMap& is returned again
  // without any recalculation.
  const TextBufferQueueMap& GetTextBuffers();

  // Returns true if the last Parse() call stopped at the end of a cluster.
  bool cluster_ended() const { return cluster_ended_; }

 private:
  // WebMParserClient methods.
  virtual WebMParserClient* OnListStart(int id) OVERRIDE;
  virtual bool OnListEnd(int id) OVERRIDE;
  virtual bool OnUInt(int id, int64 val) OVERRIDE;
  virtual bool OnBinary(int id, const uint8* data, int size) OVERRIDE;

  bool ParseBlock(bool is_simple_block, const uint8* buf, int size,
                  const uint8* additional, int additional_size, int duration,
                  int64 discard_padding);
  bool OnBlock(bool is_simple_block, int track_num, int timecode, int duration,
               int flags, const uint8* data, int size,
               const uint8* additional, int additional_size,
               int64 discard_padding);

  // Resets the Track objects associated with each text track.
  void ResetTextTracks();

  // Clears the the ready buffers associated with each text track.
  void ClearTextTrackReadyBuffers();

  // Helper method for Get{Audio,Video,Text}Buffers() that recomputes
  // |ready_buffer_upper_bound_| and calls ExtractReadyBuffers() on each track.
  // If |cluster_ended_| is true, first applies duration estimate if needed for
  // |audio_| and |video_| and sets |ready_buffer_upper_bound_| to
  // kInfiniteDuration(). Otherwise, sets |ready_buffer_upper_bound_| to the
  // minimum upper bound across |audio_| and |video_|. (Text tracks can have no
  // buffers missing duration, so they are not involved in calculating the upper
  // bound.)
  // Parse() or Reset() must be called between calls to UpdateReadyBuffers() to
  // clear each track's ready buffers and to reset |ready_buffer_upper_bound_|
  // to kNoTimestamp().
  void UpdateReadyBuffers();

  // Search for the indicated track_num among the text tracks.  Returns NULL
  // if that track num is not a text track.
  Track* FindTextTrack(int track_num);

  double timecode_multiplier_;  // Multiplier used to convert timecodes into
                                // microseconds.
  std::set<int64> ignored_tracks_;
  std::string audio_encryption_key_id_;
  std::string video_encryption_key_id_;

  WebMListParser parser_;

  int64 last_block_timecode_;
  scoped_ptr<uint8[]> block_data_;
  int block_data_size_;
  int64 block_duration_;
  int64 block_add_id_;
  scoped_ptr<uint8[]> block_additional_data_;
  int block_additional_data_size_;
  int64 discard_padding_;
  bool discard_padding_set_;

  int64 cluster_timecode_;
  base::TimeDelta cluster_start_time_;
  bool cluster_ended_;

  Track audio_;
  Track video_;
  TextTrackMap text_track_map_;

  // Subset of |text_track_map_| maintained by GetTextBuffers(), and cleared by
  // ClearTextTrackReadyBuffers(). Callers of GetTextBuffers() get a const-ref
  // to this member.
  TextBufferQueueMap text_buffers_map_;

  // Limits the range of buffers returned by Get{Audio,Video,Text}Buffers() to
  // this exclusive upper bound. Set to kNoTimestamp(), meaning not yet
  // calculated, by Reset() and Parse(). If kNoTimestamp(), then
  // Get{Audio,Video,Text}Buffers() will calculate it to be the minimum (decode)
  // timestamp across all tracks' |last_buffer_missing_duration_|, or
  // kInfiniteDuration() if no buffers are currently missing duration.
  base::TimeDelta ready_buffer_upper_bound_;

  LogCB log_cb_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMClusterParser);
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_CLUSTER_PARSER_H_
