// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_stream.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "media/base/data_buffer.h"
#include "media/base/media_log.h"
#include "media/base/test_helpers.h"
#include "media/base/text_track_config.h"
#include "media/filters/webvtt_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

typedef StreamParser::BufferQueue BufferQueue;

static const int kDefaultFramesPerSecond = 30;
static const int kDefaultKeyframesPerSecond = 6;
static const uint8 kDataA = 0x11;
static const uint8 kDataB = 0x33;
static const int kDataSize = 1;

class SourceBufferStreamTest : public testing::Test {
 protected:
  SourceBufferStreamTest()
      : accurate_durations_(false) {
    video_config_ = TestVideoConfig::Normal();
    SetStreamInfo(kDefaultFramesPerSecond, kDefaultKeyframesPerSecond);
    stream_.reset(new SourceBufferStream(video_config_, log_cb(), true));
  }

  void SetMemoryLimit(int buffers_of_data) {
    stream_->set_memory_limit_for_testing(buffers_of_data * kDataSize);
  }

  void SetStreamInfo(int frames_per_second, int keyframes_per_second) {
    frames_per_second_ = frames_per_second;
    keyframes_per_second_ = keyframes_per_second;
    frame_duration_ = ConvertToFrameDuration(frames_per_second);
  }

  void SetTextStream() {
    video_config_ = TestVideoConfig::Invalid();
    TextTrackConfig config(kTextSubtitles, "", "", "");
    stream_.reset(new SourceBufferStream(config, LogCB(), true));
    SetStreamInfo(2, 2);
  }

  void SetAudioStream() {
    video_config_ = TestVideoConfig::Invalid();
    accurate_durations_ = true;
    audio_config_.Initialize(kCodecVorbis,
                             kSampleFormatPlanarF32,
                             CHANNEL_LAYOUT_STEREO,
                             1000,
                             NULL,
                             0,
                             false,
                             false,
                             base::TimeDelta(),
                             0);
    stream_.reset(new SourceBufferStream(audio_config_, LogCB(), true));

    // Equivalent to 2ms per frame.
    SetStreamInfo(500, 500);
  }

  void NewSegmentAppend(int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  base::TimeDelta(), true, &kDataA, kDataSize);
  }

  void NewSegmentAppend(int starting_position, int number_of_buffers,
                        const uint8* data) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  base::TimeDelta(), true, data, kDataSize);
  }

  void NewSegmentAppend_OffsetFirstBuffer(
      int starting_position, int number_of_buffers,
      base::TimeDelta first_buffer_offset) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  first_buffer_offset, true, &kDataA, kDataSize);
  }

  void NewSegmentAppend_ExpectFailure(
      int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  base::TimeDelta(), false, &kDataA, kDataSize);
  }

  void AppendBuffers(int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, false,
                  base::TimeDelta(), true, &kDataA, kDataSize);
  }

  void AppendBuffers(int starting_position, int number_of_buffers,
                     const uint8* data) {
    AppendBuffers(starting_position, number_of_buffers, false,
                  base::TimeDelta(), true, data, kDataSize);
  }

  void NewSegmentAppend(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, true, kNoTimestamp(), false, true);
  }

  void NewSegmentAppend(base::TimeDelta start_timestamp,
                        const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, true, start_timestamp, false, true);
  }

  void AppendBuffers(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, false, kNoTimestamp(), false, true);
  }

  void NewSegmentAppendOneByOne(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, true, kNoTimestamp(), true, true);
  }

  void AppendBuffersOneByOne(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, false, kNoTimestamp(), true, true);
  }

  void NewSegmentAppend_ExpectFailure(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, true, kNoTimestamp(), false, false);
  }

  void AppendBuffers_ExpectFailure(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, false, kNoTimestamp(), false, false);
  }

  void Seek(int position) {
    stream_->Seek(position * frame_duration_);
  }

  void SeekToTimestamp(base::TimeDelta timestamp) {
    stream_->Seek(timestamp);
  }

  void RemoveInMs(int start, int end, int duration) {
    Remove(base::TimeDelta::FromMilliseconds(start),
           base::TimeDelta::FromMilliseconds(end),
           base::TimeDelta::FromMilliseconds(duration));
  }

  void Remove(base::TimeDelta start, base::TimeDelta end,
              base::TimeDelta duration) {
    stream_->Remove(start, end, duration);
  }

  int GetRemovalRangeInMs(int start, int end, int bytes_to_free,
                          int* removal_end) {
    base::TimeDelta removal_end_timestamp =
        base::TimeDelta::FromMilliseconds(*removal_end);
    int bytes_removed = stream_->GetRemovalRange(
        base::TimeDelta::FromMilliseconds(start),
        base::TimeDelta::FromMilliseconds(end), bytes_to_free,
        &removal_end_timestamp);
    *removal_end = removal_end_timestamp.InMilliseconds();
    return bytes_removed;
  }

  void CheckExpectedRanges(const std::string& expected) {
    Ranges<base::TimeDelta> r = stream_->GetBufferedTime();

    std::stringstream ss;
    ss << "{ ";
    for (size_t i = 0; i < r.size(); ++i) {
      int64 start = (r.start(i) / frame_duration_);
      int64 end = (r.end(i) / frame_duration_) - 1;
      ss << "[" << start << "," << end << ") ";
    }
    ss << "}";
    EXPECT_EQ(expected, ss.str());
  }

  void CheckExpectedRangesByTimestamp(const std::string& expected) {
    Ranges<base::TimeDelta> r = stream_->GetBufferedTime();

    std::stringstream ss;
    ss << "{ ";
    for (size_t i = 0; i < r.size(); ++i) {
      int64 start = r.start(i).InMilliseconds();
      int64 end = r.end(i).InMilliseconds();
      ss << "[" << start << "," << end << ") ";
    }
    ss << "}";
    EXPECT_EQ(expected, ss.str());
  }

  void CheckExpectedBuffers(
      int starting_position, int ending_position) {
    CheckExpectedBuffers(starting_position, ending_position, false, NULL, 0);
  }

  void CheckExpectedBuffers(
      int starting_position, int ending_position, bool expect_keyframe) {
    CheckExpectedBuffers(starting_position, ending_position, expect_keyframe,
                         NULL, 0);
  }

  void CheckExpectedBuffers(
      int starting_position, int ending_position, const uint8* data) {
    CheckExpectedBuffers(starting_position, ending_position, false, data,
                         kDataSize);
  }

  void CheckExpectedBuffers(
      int starting_position, int ending_position, const uint8* data,
      bool expect_keyframe) {
    CheckExpectedBuffers(starting_position, ending_position, expect_keyframe,
                         data, kDataSize);
  }

  void CheckExpectedBuffers(
      int starting_position, int ending_position, bool expect_keyframe,
      const uint8* expected_data, int expected_size) {
    int current_position = starting_position;
    for (; current_position <= ending_position; current_position++) {
      scoped_refptr<StreamParserBuffer> buffer;
      SourceBufferStream::Status status = stream_->GetNextBuffer(&buffer);

      EXPECT_NE(status, SourceBufferStream::kConfigChange);
      if (status != SourceBufferStream::kSuccess)
        break;

      if (expect_keyframe && current_position == starting_position)
        EXPECT_TRUE(buffer->IsKeyframe());

      if (expected_data) {
        const uint8* actual_data = buffer->data();
        const int actual_size = buffer->data_size();
        EXPECT_EQ(expected_size, actual_size);
        for (int i = 0; i < std::min(actual_size, expected_size); i++) {
          EXPECT_EQ(expected_data[i], actual_data[i]);
        }
      }

      EXPECT_EQ(buffer->GetDecodeTimestamp() / frame_duration_,
                current_position);
    }

    EXPECT_EQ(ending_position + 1, current_position);
  }

  void CheckExpectedBuffers(const std::string& expected) {
    std::vector<std::string> timestamps;
    base::SplitString(expected, ' ', &timestamps);
    std::stringstream ss;
    const SourceBufferStream::Type type = stream_->GetType();
    base::TimeDelta active_splice_timestamp = kNoTimestamp();
    for (size_t i = 0; i < timestamps.size(); i++) {
      scoped_refptr<StreamParserBuffer> buffer;
      SourceBufferStream::Status status = stream_->GetNextBuffer(&buffer);

      if (i > 0)
        ss << " ";

      if (status == SourceBufferStream::kConfigChange) {
        switch (type) {
          case SourceBufferStream::kVideo:
            stream_->GetCurrentVideoDecoderConfig();
            break;
          case SourceBufferStream::kAudio:
            stream_->GetCurrentAudioDecoderConfig();
            break;
          case SourceBufferStream::kText:
            stream_->GetCurrentTextTrackConfig();
            break;
        }

        if (timestamps[i] == "C")
          EXPECT_EQ(SourceBufferStream::kConfigChange, status);

        ss << "C";
        continue;
      }

      EXPECT_EQ(SourceBufferStream::kSuccess, status);
      if (status != SourceBufferStream::kSuccess)
        break;

      ss << buffer->GetDecodeTimestamp().InMilliseconds();

      // Handle preroll buffers.
      if (EndsWith(timestamps[i], "P", true)) {
        ASSERT_TRUE(buffer->IsKeyframe());
        scoped_refptr<StreamParserBuffer> preroll_buffer;
        preroll_buffer.swap(buffer);

        // When a preroll buffer is encountered we should be able to request one
        // more buffer.  The first buffer should match the timestamp and config
        // of the second buffer, except that its discard_padding() should be its
        // duration.
        ASSERT_EQ(SourceBufferStream::kSuccess,
                  stream_->GetNextBuffer(&buffer));
        ASSERT_EQ(buffer->GetConfigId(), preroll_buffer->GetConfigId());
        ASSERT_EQ(buffer->track_id(), preroll_buffer->track_id());
        ASSERT_EQ(buffer->timestamp(), preroll_buffer->timestamp());
        ASSERT_EQ(buffer->GetDecodeTimestamp(),
                  preroll_buffer->GetDecodeTimestamp());
        ASSERT_EQ(kInfiniteDuration(), preroll_buffer->discard_padding().first);
        ASSERT_EQ(base::TimeDelta(), preroll_buffer->discard_padding().second);
        ASSERT_TRUE(buffer->IsKeyframe());

        ss << "P";
      } else if (buffer->IsKeyframe()) {
        ss << "K";
      }

      // Until the last splice frame is seen, indicated by a matching timestamp,
      // all buffers must have the same splice_timestamp().
      if (buffer->timestamp() == active_splice_timestamp) {
        ASSERT_EQ(buffer->splice_timestamp(), kNoTimestamp());
      } else {
        ASSERT_TRUE(active_splice_timestamp == kNoTimestamp() ||
                    active_splice_timestamp == buffer->splice_timestamp());
      }

      active_splice_timestamp = buffer->splice_timestamp();
    }
    EXPECT_EQ(expected, ss.str());
  }

  void CheckNoNextBuffer() {
    scoped_refptr<StreamParserBuffer> buffer;
    EXPECT_EQ(SourceBufferStream::kNeedBuffer, stream_->GetNextBuffer(&buffer));
  }

  void CheckVideoConfig(const VideoDecoderConfig& config) {
    const VideoDecoderConfig& actual = stream_->GetCurrentVideoDecoderConfig();
    EXPECT_TRUE(actual.Matches(config))
        << "Expected: " << config.AsHumanReadableString()
        << "\nActual: " << actual.AsHumanReadableString();
  }

  void CheckAudioConfig(const AudioDecoderConfig& config) {
    const AudioDecoderConfig& actual = stream_->GetCurrentAudioDecoderConfig();
    EXPECT_TRUE(actual.Matches(config))
        << "Expected: " << config.AsHumanReadableString()
        << "\nActual: " << actual.AsHumanReadableString();
  }

  const LogCB log_cb() {
    return base::Bind(&SourceBufferStreamTest::DebugMediaLog,
                      base::Unretained(this));
  }

  base::TimeDelta frame_duration() const { return frame_duration_; }

  scoped_ptr<SourceBufferStream> stream_;
  VideoDecoderConfig video_config_;
  AudioDecoderConfig audio_config_;

 private:
  base::TimeDelta ConvertToFrameDuration(int frames_per_second) {
    return base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond / frames_per_second);
  }

  void AppendBuffers(int starting_position,
                     int number_of_buffers,
                     bool begin_media_segment,
                     base::TimeDelta first_buffer_offset,
                     bool expect_success,
                     const uint8* data,
                     int size) {
    if (begin_media_segment)
      stream_->OnNewMediaSegment(starting_position * frame_duration_);

    int keyframe_interval = frames_per_second_ / keyframes_per_second_;

    BufferQueue queue;
    for (int i = 0; i < number_of_buffers; i++) {
      int position = starting_position + i;
      bool is_keyframe = position % keyframe_interval == 0;
      // Buffer type and track ID are meaningless to these tests.
      scoped_refptr<StreamParserBuffer> buffer =
          StreamParserBuffer::CopyFrom(data, size, is_keyframe,
                                       DemuxerStream::AUDIO, 0);
      base::TimeDelta timestamp = frame_duration_ * position;

      if (i == 0)
        timestamp += first_buffer_offset;
      buffer->SetDecodeTimestamp(timestamp);

      // Simulate an IBB...BBP pattern in which all B-frames reference both
      // the I- and P-frames. For a GOP with playback order 12345, this would
      // result in a decode timestamp order of 15234.
      base::TimeDelta presentation_timestamp;
      if (is_keyframe) {
        presentation_timestamp = timestamp;
      } else if ((position - 1) % keyframe_interval == 0) {
        // This is the P-frame (first frame following the I-frame)
        presentation_timestamp =
            (timestamp + frame_duration_ * (keyframe_interval - 2));
      } else {
        presentation_timestamp = timestamp - frame_duration_;
      }
      buffer->set_timestamp(presentation_timestamp);
      if (accurate_durations_)
        buffer->set_duration(frame_duration_);

      queue.push_back(buffer);
    }
    if (!queue.empty())
      EXPECT_EQ(expect_success, stream_->Append(queue));
  }

  // StringToBufferQueue() allows for the generation of StreamParserBuffers from
  // coded strings of timestamps separated by spaces.  Supported syntax:
  //
  // ##:
  // Generates a StreamParserBuffer with decode timestamp ##.  E.g., "0 1 2 3".
  //
  // ##K:
  // Indicates the buffer with timestamp ## reflects a keyframe.  E.g., "0K 1".
  //
  // S(a# ... y# z#)
  // Indicates a splice frame buffer should be created with timestamp z#.  The
  // preceding timestamps a# ... y# will be treated as the fade out preroll for
  // the splice frame.  If a timestamp within the preroll ends with C the config
  // id to use for that and subsequent preroll appends is incremented by one.
  // The config id for non-splice frame appends will not be affected.
  BufferQueue StringToBufferQueue(const std::string& buffers_to_append) {
    std::vector<std::string> timestamps;
    base::SplitString(buffers_to_append, ' ', &timestamps);

    CHECK_GT(timestamps.size(), 0u);

    bool splice_frame = false;
    size_t splice_config_id = stream_->append_config_index_;
    BufferQueue pre_splice_buffers;
    BufferQueue buffers;
    for (size_t i = 0; i < timestamps.size(); i++) {
      bool is_keyframe = false;
      bool has_preroll = false;
      bool last_splice_frame = false;
      // Handle splice frame starts.
      if (StartsWithASCII(timestamps[i], "S(", true)) {
        CHECK(!splice_frame);
        splice_frame = true;
        // Remove the "S(" off of the token.
        timestamps[i] = timestamps[i].substr(2, timestamps[i].length());
      }
      if (splice_frame && EndsWith(timestamps[i], ")", true)) {
        splice_frame = false;
        last_splice_frame = true;
        // Remove the ")" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }
      // Handle config changes within the splice frame.
      if (splice_frame && EndsWith(timestamps[i], "C", true)) {
        splice_config_id++;
        CHECK(splice_config_id < stream_->audio_configs_.size() ||
              splice_config_id < stream_->video_configs_.size());
        // Remove the "C" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }
      if (EndsWith(timestamps[i], "K", true)) {
        is_keyframe = true;
        // Remove the "K" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }
      // Handle preroll buffers.
      if (EndsWith(timestamps[i], "P", true)) {
        is_keyframe = true;
        has_preroll = true;
        // Remove the "P" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }

      int time_in_ms;
      CHECK(base::StringToInt(timestamps[i], &time_in_ms));

      // Create buffer. Buffer type and track ID are meaningless to these tests.
      scoped_refptr<StreamParserBuffer> buffer =
          StreamParserBuffer::CopyFrom(&kDataA, kDataSize, is_keyframe,
                                       DemuxerStream::AUDIO, 0);
      base::TimeDelta timestamp =
          base::TimeDelta::FromMilliseconds(time_in_ms);
      buffer->set_timestamp(timestamp);
      if (accurate_durations_)
        buffer->set_duration(frame_duration_);
      buffer->SetDecodeTimestamp(timestamp);

      // Simulate preroll buffers by just generating another buffer and sticking
      // it as the preroll.
      if (has_preroll) {
        scoped_refptr<StreamParserBuffer> preroll_buffer =
            StreamParserBuffer::CopyFrom(
                &kDataA, kDataSize, is_keyframe, DemuxerStream::AUDIO, 0);
        preroll_buffer->set_duration(frame_duration_);
        buffer->SetPrerollBuffer(preroll_buffer);
      }

      if (splice_frame) {
        if (!pre_splice_buffers.empty()) {
          // Enforce strictly monotonically increasing timestamps.
          CHECK_GT(
              timestamp.InMicroseconds(),
              pre_splice_buffers.back()->GetDecodeTimestamp().InMicroseconds());
        }
        buffer->SetConfigId(splice_config_id);
        pre_splice_buffers.push_back(buffer);
        continue;
      }

      if (last_splice_frame) {
        // Require at least one additional buffer for a splice.
        CHECK(!pre_splice_buffers.empty());
        buffer->SetConfigId(splice_config_id);
        buffer->ConvertToSpliceBuffer(pre_splice_buffers);
        pre_splice_buffers.clear();
      }

      buffers.push_back(buffer);
    }
    return buffers;
  }

  void AppendBuffers(const std::string& buffers_to_append,
                     bool start_new_segment,
                     base::TimeDelta segment_start_timestamp,
                     bool one_by_one,
                     bool expect_success) {
    BufferQueue buffers = StringToBufferQueue(buffers_to_append);

    if (start_new_segment) {
      base::TimeDelta start_timestamp = segment_start_timestamp;
      if (start_timestamp == kNoTimestamp())
        start_timestamp = buffers[0]->GetDecodeTimestamp();

      ASSERT_TRUE(start_timestamp <= buffers[0]->GetDecodeTimestamp());

      stream_->OnNewMediaSegment(start_timestamp);
    }

    if (!one_by_one) {
      EXPECT_EQ(expect_success, stream_->Append(buffers));
      return;
    }

    // Append each buffer one by one.
    for (size_t i = 0; i < buffers.size(); i++) {
      BufferQueue wrapper;
      wrapper.push_back(buffers[i]);
      EXPECT_TRUE(stream_->Append(wrapper));
    }
  }

  void DebugMediaLog(const std::string& log) {
    DVLOG(1) << log;
  }

  int frames_per_second_;
  int keyframes_per_second_;
  base::TimeDelta frame_duration_;
  // TODO(dalecurtis): It's silly to have this, all tests should use accurate
  // durations instead.  However, currently all tests are written with an
  // expectation of 0 duration, so it's an involved change.
  bool accurate_durations_;
  DISALLOW_COPY_AND_ASSIGN(SourceBufferStreamTest);
};

TEST_F(SourceBufferStreamTest, Append_SingleRange) {
  // Append 15 buffers at positions 0 through 14.
  NewSegmentAppend(0, 15);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 14);
}

TEST_F(SourceBufferStreamTest, Append_SingleRange_OneBufferAtATime) {
  // Append 15 buffers starting at position 0, one buffer at a time.
  NewSegmentAppend(0, 1);
  for (int i = 1; i < 15; i++)
    AppendBuffers(i, 1);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 14);
}

TEST_F(SourceBufferStreamTest, Append_DisjointRanges) {
  // Append 5 buffers at positions 0 through 4.
  NewSegmentAppend(0, 5);

  // Append 10 buffers at positions 15 through 24.
  NewSegmentAppend(15, 10);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,4) [15,24) }");
  // Check buffers in ranges.
  Seek(0);
  CheckExpectedBuffers(0, 4);
  Seek(15);
  CheckExpectedBuffers(15, 24);
}

TEST_F(SourceBufferStreamTest, Append_AdjacentRanges) {
  // Append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10);

  // Append 11 buffers at positions 15 through 25.
  NewSegmentAppend(15, 11);

  // Append 5 buffers at positions 10 through 14 to bridge the gap.
  NewSegmentAppend(10, 5);

  // Check expected range.
  CheckExpectedRanges("{ [0,25) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 25);
}

TEST_F(SourceBufferStreamTest, Append_DoesNotBeginWithKeyframe) {
  // Append fails because the range doesn't begin with a keyframe.
  NewSegmentAppend_ExpectFailure(3, 2);

  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10);

  // Check expected range.
  CheckExpectedRanges("{ [5,14) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 14);

  // Append fails because the range doesn't begin with a keyframe.
  NewSegmentAppend_ExpectFailure(17, 3);

  CheckExpectedRanges("{ [5,14) }");
  Seek(5);
  CheckExpectedBuffers(5, 14);
}

TEST_F(SourceBufferStreamTest, Append_DoesNotBeginWithKeyframe_Adjacent) {
  // Append 8 buffers at positions 0 through 7.
  NewSegmentAppend(0, 8);

  // Now start a new media segment at position 8. Append should fail because
  // the media segment does not begin with a keyframe.
  NewSegmentAppend_ExpectFailure(8, 2);

  // Check expected range.
  CheckExpectedRanges("{ [0,7) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 7);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap) {
  // Append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5);

  // Append 15 buffers at positions 0 through 14.
  NewSegmentAppend(0, 15);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 14);
}

TEST_F(SourceBufferStreamTest,
       Complete_Overlap_AfterSegmentTimestampAndBeforeFirstBufferTimestamp) {
  // Append a segment with a start timestamp of 0, but the first
  // buffer starts at 30ms. This can happen in muxed content where the
  // audio starts before the first frame.
  NewSegmentAppend(base::TimeDelta::FromMilliseconds(0), "30K 60K 90K 120K");

  CheckExpectedRangesByTimestamp("{ [0,150) }");

  // Completely overlap the old buffers, with a segment that starts
  // after the old segment start timestamp, but before the timestamp
  // of the first buffer in the segment.
  NewSegmentAppend("20K 50K 80K 110K");

  // Verify that the buffered ranges are updated properly and we don't crash.
  CheckExpectedRangesByTimestamp("{ [20,150) }");

  SeekToTimestamp(base::TimeDelta::FromMilliseconds(20));
  CheckExpectedBuffers("20K 50K 80K 110K 120K");
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_EdgeCase) {
  // Make each frame a keyframe so that it's okay to overlap frames at any point
  // (instead of needing to respect keyframe boundaries).
  SetStreamInfo(30, 30);

  // Append 6 buffers at positions 6 through 11.
  NewSegmentAppend(6, 6);

  // Append 8 buffers at positions 5 through 12.
  NewSegmentAppend(5, 8);

  // Check expected range.
  CheckExpectedRanges("{ [5,12) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 12);
}

TEST_F(SourceBufferStreamTest, Start_Overlap) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 5);

  // Append 6 buffers at positions 10 through 15.
  NewSegmentAppend(10, 6);

  // Check expected range.
  CheckExpectedRanges("{ [5,15) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 15);
}

TEST_F(SourceBufferStreamTest, End_Overlap) {
  // Append 10 buffers at positions 10 through 19.
  NewSegmentAppend(10, 10);

  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10);

  // Check expected range.
  CheckExpectedRanges("{ [5,19) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 19);
}

TEST_F(SourceBufferStreamTest, End_Overlap_Several) {
  // Append 10 buffers at positions 10 through 19.
  NewSegmentAppend(10, 10);

  // Append 8 buffers at positions 5 through 12.
  NewSegmentAppend(5, 8);

  // Check expected ranges: stream should not have kept buffers 13 and 14
  // because the keyframe on which they depended was overwritten.
  CheckExpectedRanges("{ [5,12) [15,19) }");

  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 12);
  CheckNoNextBuffer();

  Seek(19);
  CheckExpectedBuffers(15, 19);
}

// Test an end overlap edge case where a single buffer overlaps the
// beginning of a range.
// old  : *0K*   30   60   90   120K  150
// new  : *0K*
// after: *0K*                 *120K* 150K
// track:
TEST_F(SourceBufferStreamTest, End_Overlap_SingleBuffer) {
  // Seek to start of stream.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));

  NewSegmentAppend("0K 30 60 90 120K 150");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  NewSegmentAppend("0K");
  CheckExpectedRangesByTimestamp("{ [0,30) [120,180) }");

  CheckExpectedBuffers("0K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Several) {
  // Append 2 buffers at positions 5 through 6.
  NewSegmentAppend(5, 2);

  // Append 2 buffers at positions 10 through 11.
  NewSegmentAppend(10, 2);

  // Append 2 buffers at positions 15 through 16.
  NewSegmentAppend(15, 2);

  // Check expected ranges.
  CheckExpectedRanges("{ [5,6) [10,11) [15,16) }");

  // Append buffers at positions 0 through 19.
  NewSegmentAppend(0, 20);

  // Check expected range.
  CheckExpectedRanges("{ [0,19) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 19);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Several_Then_Merge) {
  // Append 2 buffers at positions 5 through 6.
  NewSegmentAppend(5, 2);

  // Append 2 buffers at positions 10 through 11.
  NewSegmentAppend(10, 2);

  // Append 2 buffers at positions 15 through 16.
  NewSegmentAppend(15, 2);

  // Append 2 buffers at positions 20 through 21.
  NewSegmentAppend(20, 2);

  // Append buffers at positions 0 through 19.
  NewSegmentAppend(0, 20);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,21) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 21);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to buffer at position 5.
  Seek(5);

  // Replace old data with new data.
  NewSegmentAppend(5, 10, &kDataB);

  // Check ranges are correct.
  CheckExpectedRanges("{ [5,14) }");

  // Check that data has been replaced with new data.
  CheckExpectedBuffers(5, 14, &kDataB);
}

// This test is testing that a client can append data to SourceBufferStream that
// overlaps the range from which the client is currently grabbing buffers. We
// would expect that the SourceBufferStream would return old data until it hits
// the keyframe of the new data, after which it will return the new data.
TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected_TrackBuffer) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Do a complete overlap by appending 20 buffers at positions 0 through 19.
  NewSegmentAppend(0, 20, &kDataB);

  // Check range is correct.
  CheckExpectedRanges("{ [0,19) }");

  // Expect old data up until next keyframe in new data.
  CheckExpectedBuffers(6, 9, &kDataA);
  CheckExpectedBuffers(10, 10, &kDataB, true);

  // Expect rest of data to be new.
  CheckExpectedBuffers(11, 19, &kDataB);

  // Seek back to beginning; all data should be new.
  Seek(0);
  CheckExpectedBuffers(0, 19, &kDataB);

  // Check range continues to be correct.
  CheckExpectedRanges("{ [0,19) }");
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected_EdgeCase) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  NewSegmentAppend(5, 10, &kDataB);

  // Check ranges are correct.
  CheckExpectedRanges("{ [5,14) }");

  // Expect old data up until next keyframe in new data.
  CheckExpectedBuffers(6, 9, &kDataA);
  CheckExpectedBuffers(10, 10, &kDataB, true);

  // Expect rest of data to be new.
  CheckExpectedBuffers(11, 14, &kDataB);

  // Seek back to beginning; all data should be new.
  Seek(5);
  CheckExpectedBuffers(5, 14, &kDataB);

  // Check range continues to be correct.
  CheckExpectedRanges("{ [5,14) }");
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected_Multiple) {
  static const uint8 kDataC = 0x55;
  static const uint8 kDataD = 0x77;

  // Append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  NewSegmentAppend(5, 5, &kDataB);

  // Then replace it again with different data.
  NewSegmentAppend(5, 5, &kDataC);

  // Now append 5 new buffers at positions 10 through 14.
  NewSegmentAppend(10, 5, &kDataC);

  // Now replace all the data entirely.
  NewSegmentAppend(5, 10, &kDataD);

  // Expect buffers 6 through 9 to be DataA, and the remaining
  // buffers to be kDataD.
  CheckExpectedBuffers(6, 9, &kDataA);
  CheckExpectedBuffers(10, 14, &kDataD);

  // At this point we cannot fulfill request.
  CheckNoNextBuffer();

  // Seek back to beginning; all data should be new.
  Seek(5);
  CheckExpectedBuffers(5, 14, &kDataD);
}

TEST_F(SourceBufferStreamTest, Start_Overlap_Selected) {
  // Append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10, &kDataA);

  // Seek to position 5, then add buffers to overlap data at that position.
  Seek(5);
  NewSegmentAppend(5, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Because we seeked to a keyframe, the next buffers should all be new data.
  CheckExpectedBuffers(5, 14, &kDataB);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 4, &kDataA);
  CheckExpectedBuffers(5, 14, &kDataB);
}

TEST_F(SourceBufferStreamTest, Start_Overlap_Selected_TrackBuffer) {
  // Append 15 buffers at positions 0 through 14.
  NewSegmentAppend(0, 15, &kDataA);

  // Seek to 10 and get buffer.
  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now append 10 buffers of new data at positions 10 through 19.
  NewSegmentAppend(10, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,19) }");

  // The next 4 buffers should be a from the old buffer, followed by a keyframe
  // from the new data.
  CheckExpectedBuffers(11, 14, &kDataA);
  CheckExpectedBuffers(15, 15, &kDataB, true);

  // The rest of the buffers should be new data.
  CheckExpectedBuffers(16, 19, &kDataB);

  // Now seek to the beginning; positions 0 through 9 should be the original
  // data, positions 10 through 19 should be the new data.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataA);
  CheckExpectedBuffers(10, 19, &kDataB);

  // Make sure range is still correct.
  CheckExpectedRanges("{ [0,19) }");
}

TEST_F(SourceBufferStreamTest, Start_Overlap_Selected_EdgeCase) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10, &kDataA);

  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now replace the last 5 buffers with new data.
  NewSegmentAppend(10, 5, &kDataB);

  // The next 4 buffers should be the origial data, held in the track buffer.
  CheckExpectedBuffers(11, 14, &kDataA);

  // The next buffer is at position 15, so we should fail to fulfill the
  // request.
  CheckNoNextBuffer();

  // Now append data at 15 through 19 and check to make sure it's correct.
  NewSegmentAppend(15, 5, &kDataB);
  CheckExpectedBuffers(15, 19, &kDataB);

  // Seek to beginning of buffered range and check buffers.
  Seek(5);
  CheckExpectedBuffers(5, 9, &kDataA);
  CheckExpectedBuffers(10, 19, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [5,19) }");
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer is a keyframe that's being overlapped by new
// buffers.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           *A*a a a a A a a a a
// new  :  B b b b b B b b b b
// after:  B b b b b*B*b b b b A a a a a
TEST_F(SourceBufferStreamTest, End_Overlap_Selected) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to position 5.
  Seek(5);

  // Now append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Because we seeked to a keyframe, the next buffers should be new.
  CheckExpectedBuffers(5, 9, &kDataB);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is after the newly appended buffers.
// In this particular case, the end overlap does not require a split.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A a a a a A a a*a*a|
// new  :  B b b b b B b b b b
// after: |B b b b b B b b b b A a a*a*a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_AfterEndOfNew_1) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to position 10, then move to position 13.
  Seek(10);
  CheckExpectedBuffers(10, 12, &kDataA);

  // Now append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Make sure rest of data is as expected.
  CheckExpectedBuffers(13, 14, &kDataA);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is after the newly appended buffers.
// In this particular case, the end overlap requires a split, and the next
// buffer is in the split range.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A a a a a A a a*a*a|
// new  :  B b b b b B b b
// after: |B b b b b B b b|   |A a a*a*a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_AfterEndOfNew_2) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to position 10, then move to position 13.
  Seek(10);
  CheckExpectedBuffers(10, 12, &kDataA);

  // Now append 8 buffers at positions 0 through 7.
  NewSegmentAppend(0, 8, &kDataB);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,7) [10,14) }");

  // Make sure rest of data is as expected.
  CheckExpectedBuffers(13, 14, &kDataA);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 7, &kDataB);
  CheckNoNextBuffer();

  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is after the newly appended buffers.
// In this particular case, the end overlap requires a split, and the next
// buffer was in between the end of the new data and the split range.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A a a*a*a A a a a a|
// new  :  B b b b b B b b
// after: |B b b b b B b b|   |A a a a a|
// track:                 |a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_AfterEndOfNew_3) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to position 5, then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 8 buffers at positions 0 through 7.
  NewSegmentAppend(0, 8, &kDataB);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,7) [10,14) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(8, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe.
  CheckExpectedBuffers(10, 10, &kDataA, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 7, &kDataB);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is overlapped by the new buffers.
// In this particular case, the end overlap does not require a split.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A a a*a*a A a a a a|
// new  :  B b b b b B b b b b
// after: |B b b b b B b b b b A a a a a|
// track:                 |a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_OverlappedByNew_1) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to position 5, then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(8, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe.
  CheckExpectedBuffers(10, 10, &kDataA, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is overlapped by the new buffers.
// In this particular case, the end overlap requires a split, and the next
// keyframe after the track buffer is in the split range.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A*a*a a a A a a a a|
// new  :  B b b b b B b
// after: |B b b b b B b|     |A a a a a|
// track:             |a a a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_OverlappedByNew_2) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 7 buffers at positions 0 through 6.
  NewSegmentAppend(0, 7, &kDataB);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,6) [10,14) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(6, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe.
  CheckExpectedBuffers(10, 10, &kDataA, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 6, &kDataB);
  CheckNoNextBuffer();

  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is overlapped by the new buffers.
// In this particular case, the end overlap requires a split, and the next
// keyframe after the track buffer is in the range with the new buffers.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A*a*a a a A a a a a A a a a a|
// new  :  B b b b b B b b b b B b b
// after: |B b b b b B b b b b B b b|   |A a a a a|
// track:             |a a a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_OverlappedByNew_3) {
  // Append 15 buffers at positions 5 through 19.
  NewSegmentAppend(5, 15, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 13 buffers at positions 0 through 12.
  NewSegmentAppend(0, 13, &kDataB);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,12) [15,19) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(6, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe
  // from the new data.
  CheckExpectedBuffers(10, 10, &kDataB, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 12, &kDataB);
  CheckNoNextBuffer();

  Seek(15);
  CheckExpectedBuffers(15, 19, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and there is no keyframe after the end of the new buffers.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A*a*a a a|
// new  :  B b b b b B
// after: |B b b b b B|
// track:             |a a a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_NoKeyframeAfterNew) {
  // Append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 6 buffers at positions 0 through 5.
  NewSegmentAppend(0, 6, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,5) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(6, 9, &kDataA);

  // Now there's no data to fulfill the request.
  CheckNoNextBuffer();

  // Let's fill in the gap, buffers 6 through 10.
  AppendBuffers(6, 5, &kDataB);

  // We should be able to get the next buffer.
  CheckExpectedBuffers(10, 10, &kDataB);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and there is no keyframe after the end of the new buffers, then the
// range gets split.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :                     |A a a a a A*a*|
// new  :            B b b b b B b b b b B
// after:           |B b b b b B b b b b B|
// new  :  A a a a a A
// after: |A a a a a A|       |B b b b b B|
// track:                                 |a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_NoKeyframeAfterNew2) {
  // Append 7 buffers at positions 10 through 16.
  NewSegmentAppend(10, 7, &kDataA);

  // Seek to position 15, then move to position 16.
  Seek(15);
  CheckExpectedBuffers(15, 15, &kDataA);

  // Now append 11 buffers at positions 5 through 15.
  NewSegmentAppend(5, 11, &kDataB);
  CheckExpectedRanges("{ [5,15) }");

  // Now do another end-overlap to split the range into two parts, where the
  // 2nd range should have the next buffer position.
  NewSegmentAppend(0, 6, &kDataA);
  CheckExpectedRanges("{ [0,5) [10,15) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(16, 16, &kDataA);

  // Now there's no data to fulfill the request.
  CheckNoNextBuffer();

  // Add data to the 2nd range, should not be able to fulfill the next read
  // until we've added a keyframe.
  NewSegmentAppend(15, 1, &kDataB);
  CheckNoNextBuffer();
  for (int i = 16; i <= 19; i++) {
    AppendBuffers(i, 1, &kDataB);
    CheckNoNextBuffer();
  }

  // Now append a keyframe.
  AppendBuffers(20, 1, &kDataB);

  // We should be able to get the next buffer.
  CheckExpectedBuffers(20, 20, &kDataB, true);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next keyframe in a separate range.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A*a*a a a|          |A a a a a|
// new  :  B b b b b B
// after: |B b b b b B|                  |A a a a a|
// track:             |a a a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_NoKeyframeAfterNew3) {
  // Append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5, &kDataA);

  // Append 5 buffers at positions 15 through 19.
  NewSegmentAppend(15, 5, &kDataA);

  // Check expected range.
  CheckExpectedRanges("{ [5,9) [15,19) }");

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 6 buffers at positions 0 through 5.
  NewSegmentAppend(0, 6, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,5) [15,19) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(6, 9, &kDataA);

  // Now there's no data to fulfill the request.
  CheckNoNextBuffer();

  // Let's fill in the gap, buffers 6 through 14.
  AppendBuffers(6, 9, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,19) }");

  // We should be able to get the next buffer.
  CheckExpectedBuffers(10, 14, &kDataB);

  // We should be able to get the next buffer.
  CheckExpectedBuffers(15, 19, &kDataA);
}

// This test covers the case when new buffers overlap the middle of a selected
// range. This tests the case when there is no split and the next buffer is a
// keyframe.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :  A a a a a*A*a a a a A a a a a
// new  :            B b b b b
// after:  A a a a a*B*b b b b A a a a a
TEST_F(SourceBufferStreamTest, Middle_Overlap_Selected_1) {
  // Append 15 buffers at positions 0 through 14.
  NewSegmentAppend(0, 15, &kDataA);

  // Seek to position 5.
  Seek(5);

  // Now append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Check for next data; should be new data.
  CheckExpectedBuffers(5, 9, &kDataB);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 4, &kDataA);
  CheckExpectedBuffers(5, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case when new buffers overlap the middle of a selected
// range. This tests the case when there is no split and the next buffer is
// after the new buffers.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :  A a a a a A a a a a A*a*a a a
// new  :            B b b b b
// after:  A a a a a B b b b b A*a*a a a
TEST_F(SourceBufferStreamTest, Middle_Overlap_Selected_2) {
  // Append 15 buffers at positions 0 through 14.
  NewSegmentAppend(0, 15, &kDataA);

  // Seek to 10 then move to position 11.
  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Make sure data is correct.
  CheckExpectedBuffers(11, 14, &kDataA);
  Seek(0);
  CheckExpectedBuffers(0, 4, &kDataA);
  CheckExpectedBuffers(5, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case when new buffers overlap the middle of a selected
// range. This tests the case when there is a split and the next buffer is
// before the new buffers.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :  A a*a*a a A a a a a A a a a a
// new  :            B b b
// after:  A a*a*a a B b b|   |A a a a a
TEST_F(SourceBufferStreamTest, Middle_Overlap_Selected_3) {
  // Append 15 buffers at positions 0 through 14.
  NewSegmentAppend(0, 15, &kDataA);

  // Seek to beginning then move to position 2.
  Seek(0);
  CheckExpectedBuffers(0, 1, &kDataA);

  // Now append 3 buffers at positions 5 through 7.
  NewSegmentAppend(5, 3, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,7) [10,14) }");

  // Make sure data is correct.
  CheckExpectedBuffers(2, 4, &kDataA);
  CheckExpectedBuffers(5, 7, &kDataB);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case when new buffers overlap the middle of a selected
// range. This tests the case when there is a split and the next buffer is after
// the new buffers but before the split range.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :  A a a a a A a a*a*a A a a a a
// new  :            B b b
// after: |A a a a a B b b|   |A a a a a|
// track:                 |a a|
TEST_F(SourceBufferStreamTest, Middle_Overlap_Selected_4) {
  // Append 15 buffers at positions 0 through 14.
  NewSegmentAppend(0, 15, &kDataA);

  // Seek to 5 then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 3 buffers at positions 5 through 7.
  NewSegmentAppend(5, 3, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,7) [10,14) }");

  // Buffers 8 and 9 should be in the track buffer.
  CheckExpectedBuffers(8, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe.
  CheckExpectedBuffers(10, 10, &kDataA, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 4, &kDataA);
  CheckExpectedBuffers(5, 7, &kDataB);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
}

TEST_F(SourceBufferStreamTest, Overlap_OneByOne) {
  // Append 5 buffers starting at 10ms, 30ms apart.
  NewSegmentAppendOneByOne("10K 40 70 100 130");

  // The range ends at 160, accounting for the last buffer's duration.
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Overlap with 10 buffers starting at the beginning, appended one at a
  // time.
  NewSegmentAppend(0, 1, &kDataB);
  for (int i = 1; i < 10; i++)
    AppendBuffers(i, 1, &kDataB);

  // All data should be replaced.
  Seek(0);
  CheckExpectedRanges("{ [0,9) }");
  CheckExpectedBuffers(0, 9, &kDataB);
}

TEST_F(SourceBufferStreamTest, Overlap_OneByOne_DeleteGroup) {
  NewSegmentAppendOneByOne("10K 40 70 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 130ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(130));

  // Overlap with a new segment from 0 to 120ms.
  NewSegmentAppendOneByOne("0K 120");

  // Next buffer should still be 130ms.
  CheckExpectedBuffers("130K");

  // Check the final buffers is correct.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));
  CheckExpectedBuffers("0K 120 130K");
}

TEST_F(SourceBufferStreamTest, Overlap_OneByOne_BetweenMediaSegments) {
  // Append 5 buffers starting at 110ms, 30ms apart.
  NewSegmentAppendOneByOne("110K 140 170 200 230");
  CheckExpectedRangesByTimestamp("{ [110,260) }");

  // Now append 2 media segments from 0ms to 210ms, 30ms apart. Note that the
  // old keyframe 110ms falls in between these two segments.
  NewSegmentAppendOneByOne("0K 30 60 90");
  NewSegmentAppendOneByOne("120K 150 180 210");
  CheckExpectedRangesByTimestamp("{ [0,240) }");

  // Check the final buffers is correct; the keyframe at 110ms should be
  // deleted.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));
  CheckExpectedBuffers("0K 30 60 90 120K 150 180 210");
}

// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer) {
  NewSegmentAppendOneByOne("10K 40 70 100K 125 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(70));
  CheckExpectedBuffers("10K 40");

  // Overlap with a new segment from 0 to 120ms.
  NewSegmentAppendOneByOne("0K 30 60 90 120K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Should return frames 70ms and 100ms from the track buffer, then switch
  // to the new data at 120K, then switch back to the old data at 130K. The
  // frame at 125ms that depended on keyframe 100ms should have been deleted.
  CheckExpectedBuffers("70 100K 120K 130K");

  // Check the final result: should not include data from the track buffer.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));
  CheckExpectedBuffers("0K 30 60 90 120K 130K");
}

// Overlap the next keyframe after the end of the track buffer with a new
// keyframe.
// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
// track:             70   100K
// new  :                     110K    130
// after: 0K   30   60   90  *110K*   130
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer2) {
  NewSegmentAppendOneByOne("10K 40 70 100K 125 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(70));
  CheckExpectedBuffers("10K 40");

  // Overlap with a new segment from 0 to 120ms; 70ms and 100ms go in track
  // buffer.
  NewSegmentAppendOneByOne("0K 30 60 90 120K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Now overlap the keyframe at 120ms.
  NewSegmentAppendOneByOne("110K 130");

  // Should expect buffers 70ms and 100ms from the track buffer. Then it should
  // return the keyframe after the track buffer, which is at 110ms.
  CheckExpectedBuffers("70 100K 110K 130");
}

// Overlap the next keyframe after the end of the track buffer without a
// new keyframe.
// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
// track:             70   100K
// new  :        50K   80   110          140
// after: 0K   30   50K   80   110   140 * (waiting for keyframe)
// track:               70   100K   120K   130K
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer3) {
  NewSegmentAppendOneByOne("10K 40 70 100K 125 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(70));
  CheckExpectedBuffers("10K 40");

  // Overlap with a new segment from 0 to 120ms; 70ms and 100ms go in track
  // buffer.
  NewSegmentAppendOneByOne("0K 30 60 90 120K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Now overlap the keyframe at 120ms. There's no keyframe after 70ms, so 120ms
  // and 130ms go into the track buffer.
  NewSegmentAppendOneByOne("50K 80 110 140");

  // Should have all the buffers from the track buffer, then stall.
  CheckExpectedBuffers("70 100K 120K 130K");
  CheckNoNextBuffer();

  // Appending a keyframe should fulfill the read.
  AppendBuffersOneByOne("150K");
  CheckExpectedBuffers("150K");
  CheckNoNextBuffer();
}

// Overlap the next keyframe after the end of the track buffer with a keyframe
// that comes before the end of the track buffer.
// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
// track:             70   100K
// new  :              80K  110          140
// after: 0K   30   60   *80K*  110   140
// track:               70
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer4) {
  NewSegmentAppendOneByOne("10K 40 70 100K 125 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(70));
  CheckExpectedBuffers("10K 40");

  // Overlap with a new segment from 0 to 120ms; 70ms and 100ms go in track
  // buffer.
  NewSegmentAppendOneByOne("0K 30 60 90 120K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Now append a keyframe at 80ms.
  NewSegmentAppendOneByOne("80K 110 140");

  CheckExpectedBuffers("70 80K 110 140");
  CheckNoNextBuffer();
}

// Overlap the next keyframe after the end of the track buffer with a keyframe
// that comes before the end of the track buffer, when the selected stream was
// waiting for the next keyframe.
// old  :   10K  40  *70*  100K
// new  : 0K   30   60   90   120
// after: 0K   30   60   90   120 * (waiting for keyframe)
// track:             70   100K
// new  :              80K  110          140
// after: 0K   30   60   *80K*  110   140
// track:               70
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer5) {
  NewSegmentAppendOneByOne("10K 40 70 100K");
  CheckExpectedRangesByTimestamp("{ [10,130) }");

  // Seek to 70ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(70));
  CheckExpectedBuffers("10K 40");

  // Overlap with a new segment from 0 to 120ms; 70ms and 100ms go in track
  // buffer.
  NewSegmentAppendOneByOne("0K 30 60 90 120");
  CheckExpectedRangesByTimestamp("{ [0,150) }");

  // Now append a keyframe at 80ms. The buffer at 100ms should be deleted from
  // the track buffer.
  NewSegmentAppendOneByOne("80K 110 140");

  CheckExpectedBuffers("70 80K 110 140");
  CheckNoNextBuffer();
}

// Test that appending to a different range while there is data in
// the track buffer doesn't affect the selected range or track buffer state.
// old  :   10K  40  *70*  100K  125  130K ... 200K 230
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K ... 200K 230
// track:             70   100K
// old  : 0K   30   60   90  *120K*   130K ... 200K 230
// new  :                                               260K 290
// after: 0K   30   60   90  *120K*   130K ... 200K 230 260K 290
// track:             70   100K
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer6) {
  NewSegmentAppendOneByOne("10K 40 70 100K 125 130K");
  NewSegmentAppendOneByOne("200K 230");
  CheckExpectedRangesByTimestamp("{ [10,160) [200,260) }");

  // Seek to 70ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(70));
  CheckExpectedBuffers("10K 40");

  // Overlap with a new segment from 0 to 120ms.
  NewSegmentAppendOneByOne("0K 30 60 90 120K");
  CheckExpectedRangesByTimestamp("{ [0,160) [200,260) }");

  // Verify that 70 gets read out of the track buffer.
  CheckExpectedBuffers("70");

  // Append more data to the unselected range.
  NewSegmentAppendOneByOne("260K 290");
  CheckExpectedRangesByTimestamp("{ [0,160) [200,320) }");

  CheckExpectedBuffers("100K 120K 130K");
  CheckNoNextBuffer();

  // Check the final result: should not include data from the track buffer.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));
  CheckExpectedBuffers("0K 30 60 90 120K 130K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Seek_Keyframe) {
  // Append 6 buffers at positions 0 through 5.
  NewSegmentAppend(0, 6);

  // Seek to beginning.
  Seek(0);
  CheckExpectedBuffers(0, 5, true);
}

TEST_F(SourceBufferStreamTest, Seek_NonKeyframe) {
  // Append 15 buffers at positions 0 through 14.
  NewSegmentAppend(0, 15);

  // Seek to buffer at position 13.
  Seek(13);

  // Expect seeking back to the nearest keyframe.
  CheckExpectedBuffers(10, 14, true);

  // Seek to buffer at position 3.
  Seek(3);

  // Expect seeking back to the nearest keyframe.
  CheckExpectedBuffers(0, 3, true);
}

TEST_F(SourceBufferStreamTest, Seek_NotBuffered) {
  // Seek to beginning.
  Seek(0);

  // Try to get buffer; nothing's appended.
  CheckNoNextBuffer();

  // Append 2 buffers at positions 0.
  NewSegmentAppend(0, 2);
  Seek(0);
  CheckExpectedBuffers(0, 1);

  // Try to get buffer out of range.
  Seek(2);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Seek_InBetweenTimestamps) {
  // Append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10);

  base::TimeDelta bump = frame_duration() / 4;
  CHECK(bump > base::TimeDelta());

  // Seek to buffer a little after position 5.
  stream_->Seek(5 * frame_duration() + bump);
  CheckExpectedBuffers(5, 5, true);

  // Seek to buffer a little before position 5.
  stream_->Seek(5 * frame_duration() - bump);
  CheckExpectedBuffers(0, 0, true);
}

// This test will do a complete overlap of an existing range in order to add
// buffers to the track buffers. Then the test does a seek to another part of
// the stream. The SourceBufferStream should clear its internal track buffer in
// response to the Seek().
TEST_F(SourceBufferStreamTest, Seek_After_TrackBuffer_Filled) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Do a complete overlap by appending 20 buffers at positions 0 through 19.
  NewSegmentAppend(0, 20, &kDataB);

  // Check range is correct.
  CheckExpectedRanges("{ [0,19) }");

  // Seek to beginning; all data should be new.
  Seek(0);
  CheckExpectedBuffers(0, 19, &kDataB);

  // Check range continues to be correct.
  CheckExpectedRanges("{ [0,19) }");
}

TEST_F(SourceBufferStreamTest, Seek_StartOfSegment) {
  base::TimeDelta bump = frame_duration() / 4;
  CHECK(bump > base::TimeDelta());

  // Append 5 buffers at position (5 + |bump|) through 9, where the media
  // segment begins at position 5.
  Seek(5);
  NewSegmentAppend_OffsetFirstBuffer(5, 5, bump);
  scoped_refptr<StreamParserBuffer> buffer;

  // GetNextBuffer() should return the next buffer at position (5 + |bump|).
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kSuccess);
  EXPECT_EQ(buffer->GetDecodeTimestamp(), 5 * frame_duration() + bump);

  // Check rest of buffers.
  CheckExpectedBuffers(6, 9);

  // Seek to position 15.
  Seek(15);

  // Append 5 buffers at positions (15 + |bump|) through 19, where the media
  // segment begins at 15.
  NewSegmentAppend_OffsetFirstBuffer(15, 5, bump);

  // GetNextBuffer() should return the next buffer at position (15 + |bump|).
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kSuccess);
  EXPECT_EQ(buffer->GetDecodeTimestamp(), 15 * frame_duration() + bump);

  // Check rest of buffers.
  CheckExpectedBuffers(16, 19);
}

TEST_F(SourceBufferStreamTest, Seek_BeforeStartOfSegment) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10);

  // Seek to a time before the first buffer in the range.
  Seek(0);

  // Should return buffers from the beginning of the range.
  CheckExpectedBuffers(5, 14);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_CompleteOverlap) {
  // Append 5 buffers at positions 0 through 4.
  NewSegmentAppend(0, 4);

  // Append 5 buffers at positions 10 through 14, and seek to the beginning of
  // this range.
  NewSegmentAppend(10, 5);
  Seek(10);

  // Now seek to the beginning of the first range.
  Seek(0);

  // Completely overlap the old seek point.
  NewSegmentAppend(5, 15);

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_CompleteOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewSegmentAppend(0, 2);

  // Append 5 buffers at positions 15 through 19 and seek to beginning of the
  // range.
  NewSegmentAppend(15, 5);
  Seek(15);

  // Now seek position 5.
  Seek(5);

  // Completely overlap the old seek point.
  NewSegmentAppend(10, 15);

  // The seek at position 5 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_MiddleOverlap) {
  // Append 2 buffers at positions 0 through 1.
  NewSegmentAppend(0, 2);

  // Append 15 buffers at positions 5 through 19 and seek to position 15.
  NewSegmentAppend(5, 15);
  Seek(15);

  // Now seek to the beginning of the stream.
  Seek(0);

  // Overlap the middle of the range such that there are now three ranges.
  NewSegmentAppend(10, 3);
  CheckExpectedRanges("{ [0,1) [5,12) [15,19) }");

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_MiddleOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewSegmentAppend(0, 2);

  // Append 15 buffers at positions 10 through 24 and seek to position 20.
  NewSegmentAppend(10, 15);
  Seek(20);

  // Now seek to position 5.
  Seek(5);

  // Overlap the middle of the range such that it is now split into two ranges.
  NewSegmentAppend(15, 3);
  CheckExpectedRanges("{ [0,1) [10,17) [20,24) }");

  // The seek at position 5 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_StartOverlap) {
  // Append 2 buffers at positions 0 through 1.
  NewSegmentAppend(0, 2);

  // Append 15 buffers at positions 5 through 19 and seek to position 15.
  NewSegmentAppend(5, 15);
  Seek(15);

  // Now seek to the beginning of the stream.
  Seek(0);

  // Start overlap the old seek point.
  NewSegmentAppend(10, 10);

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_StartOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewSegmentAppend(0, 2);

  // Append 15 buffers at positions 10 through 24 and seek to position 20.
  NewSegmentAppend(10, 15);
  Seek(20);

  // Now seek to position 5.
  Seek(5);

  // Start overlap the old seek point.
  NewSegmentAppend(15, 10);

  // The seek at time 0 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_EndOverlap) {
  // Append 5 buffers at positions 0 through 4.
  NewSegmentAppend(0, 4);

  // Append 15 buffers at positions 10 through 24 and seek to start of range.
  NewSegmentAppend(10, 15);
  Seek(10);

  // Now seek to the beginning of the stream.
  Seek(0);

  // End overlap the old seek point.
  NewSegmentAppend(5, 10);

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_EndOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewSegmentAppend(0, 2);

  // Append 15 buffers at positions 15 through 29 and seek to start of range.
  NewSegmentAppend(15, 15);
  Seek(15);

  // Now seek to position 5
  Seek(5);

  // End overlap the old seek point.
  NewSegmentAppend(10, 10);

  // The seek at time 0 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GetNextBuffer_AfterMerges) {
  // Append 5 buffers at positions 10 through 14.
  NewSegmentAppend(10, 5);

  // Seek to buffer at position 12.
  Seek(12);

  // Append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5);

  // Make sure ranges are merged.
  CheckExpectedRanges("{ [5,14) }");

  // Make sure the next buffer is correct.
  CheckExpectedBuffers(10, 10);

  // Append 5 buffers at positions 15 through 19.
  NewSegmentAppend(15, 5);
  CheckExpectedRanges("{ [5,19) }");

  // Make sure the remaining next buffers are correct.
  CheckExpectedBuffers(11, 14);
}

TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenAppend) {
  // Append 4 buffers at positions 0 through 3.
  NewSegmentAppend(0, 4);

  // Seek to buffer at position 0 and get all buffers.
  Seek(0);
  CheckExpectedBuffers(0, 3);

  // Next buffer is at position 4, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Append 2 buffers at positions 4 through 5.
  AppendBuffers(4, 2);
  CheckExpectedBuffers(4, 5);
}

// This test covers the case where new buffers start-overlap a range whose next
// buffer is not buffered.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenStartOverlap) {
  // Append 10 buffers at positions 0 through 9 and exhaust the buffers.
  NewSegmentAppend(0, 10, &kDataA);
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataA);

  // Next buffer is at position 10, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Append 6 buffers at positons 5 through 10. This is to test that doing a
  // start-overlap successfully fulfills the read at position 10, even though
  // position 10 was unbuffered.
  NewSegmentAppend(5, 6, &kDataB);
  CheckExpectedBuffers(10, 10, &kDataB);

  // Then add 5 buffers from positions 11 though 15.
  AppendBuffers(11, 5, &kDataB);

  // Check the next 4 buffers are correct, which also effectively seeks to
  // position 15.
  CheckExpectedBuffers(11, 14, &kDataB);

  // Replace the next buffer at position 15 with another start overlap.
  NewSegmentAppend(15, 2, &kDataA);
  CheckExpectedBuffers(15, 16, &kDataA);
}

// Tests a start overlap that occurs right at the timestamp of the last output
// buffer that was returned by GetNextBuffer(). This test verifies that
// GetNextBuffer() skips to second GOP in the newly appended data instead
// of returning two buffers with the same timestamp.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenStartOverlap2) {
  NewSegmentAppend("0K 30 60 90 120");

  Seek(0);
  CheckExpectedBuffers("0K 30 60 90 120");
  CheckNoNextBuffer();

  // Append a keyframe with the same timestamp as the last buffer output.
  NewSegmentAppend("120K");
  CheckNoNextBuffer();

  // Append the rest of the segment and make sure that buffers are returned
  // from the first GOP after 120.
  AppendBuffers("150 180 210K 240");
  CheckExpectedBuffers("210K 240");

  // Seek to the beginning and verify the contents of the source buffer.
  Seek(0);
  CheckExpectedBuffers("0K 30 60 90 120K 150 180 210K 240");
  CheckNoNextBuffer();
}

// This test covers the case where new buffers completely overlap a range
// whose next buffer is not buffered.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenCompleteOverlap) {
  // Append 5 buffers at positions 10 through 14 and exhaust the buffers.
  NewSegmentAppend(10, 5, &kDataA);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);

  // Next buffer is at position 15, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Do a complete overlap and test that this successfully fulfills the read
  // at position 15.
  NewSegmentAppend(5, 11, &kDataB);
  CheckExpectedBuffers(15, 15, &kDataB);

  // Then add 5 buffers from positions 16 though 20.
  AppendBuffers(16, 5, &kDataB);

  // Check the next 4 buffers are correct, which also effectively seeks to
  // position 20.
  CheckExpectedBuffers(16, 19, &kDataB);

  // Do a complete overlap and replace the buffer at position 20.
  NewSegmentAppend(0, 21, &kDataA);
  CheckExpectedBuffers(20, 20, &kDataA);
}

// This test covers the case where a range is stalled waiting for its next
// buffer, then an end-overlap causes the end of the range to be deleted.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenEndOverlap) {
  // Append 5 buffers at positions 10 through 14 and exhaust the buffers.
  NewSegmentAppend(10, 5, &kDataA);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckExpectedRanges("{ [10,14) }");

  // Next buffer is at position 15, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Do an end overlap that causes the latter half of the range to be deleted.
  NewSegmentAppend(5, 6, &kDataB);
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [5,10) }");

  // Fill in the gap. Getting the next buffer should still stall at position 15.
  for (int i = 11; i <= 14; i++) {
    AppendBuffers(i, 1, &kDataB);
    CheckNoNextBuffer();
  }

  // Append the buffer at position 15 and check to make sure all is correct.
  AppendBuffers(15, 1);
  CheckExpectedBuffers(15, 15);
  CheckExpectedRanges("{ [5,15) }");
}

// This test is testing the "next buffer" logic after a complete overlap. In
// this scenario, when the track buffer is exhausted, there is no buffered data
// to fulfill the request. The SourceBufferStream should be able to fulfill the
// request when the data is later appended, and should not lose track of the
// "next buffer" position.
TEST_F(SourceBufferStreamTest, GetNextBuffer_Overlap_Selected_Complete) {
  // Append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  NewSegmentAppend(5, 5, &kDataB);

  // Expect old data up until next keyframe in new data.
  CheckExpectedBuffers(6, 9, &kDataA);

  // Next buffer is at position 10, so should not be able to fulfill the
  // request.
  CheckNoNextBuffer();

  // Now add 5 new buffers at positions 10 through 14.
  AppendBuffers(10, 5, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataB);
}

TEST_F(SourceBufferStreamTest, PresentationTimestampIndependence) {
  // Append 20 buffers at position 0.
  NewSegmentAppend(0, 20);
  Seek(0);

  int last_keyframe_idx = -1;
  base::TimeDelta last_keyframe_presentation_timestamp;
  base::TimeDelta last_p_frame_presentation_timestamp;

  // Check for IBB...BBP pattern.
  for (int i = 0; i < 20; i++) {
    scoped_refptr<StreamParserBuffer> buffer;
    ASSERT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kSuccess);

    if (buffer->IsKeyframe()) {
      EXPECT_EQ(buffer->timestamp(), buffer->GetDecodeTimestamp());
      last_keyframe_idx = i;
      last_keyframe_presentation_timestamp = buffer->timestamp();
    } else if (i == last_keyframe_idx + 1) {
      ASSERT_NE(last_keyframe_idx, -1);
      last_p_frame_presentation_timestamp = buffer->timestamp();
      EXPECT_LT(last_keyframe_presentation_timestamp,
                last_p_frame_presentation_timestamp);
    } else {
      EXPECT_GT(buffer->timestamp(), last_keyframe_presentation_timestamp);
      EXPECT_LT(buffer->timestamp(), last_p_frame_presentation_timestamp);
      EXPECT_LT(buffer->timestamp(), buffer->GetDecodeTimestamp());
    }
  }
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteFront) {
  // Set memory limit to 20 buffers.
  SetMemoryLimit(20);

  // Append 20 buffers at positions 0 through 19.
  NewSegmentAppend(0, 1, &kDataA);
  for (int i = 1; i < 20; i++)
    AppendBuffers(i, 1, &kDataA);

  // None of the buffers should trigger garbage collection, so all data should
  // be there as expected.
  CheckExpectedRanges("{ [0,19) }");
  Seek(0);
  CheckExpectedBuffers(0, 19, &kDataA);

  // Seek to the middle of the stream.
  Seek(10);

  // Append 5 buffers to the end of the stream.
  AppendBuffers(20, 5, &kDataA);

  // GC should have deleted the first 5 buffers.
  CheckExpectedRanges("{ [5,24) }");
  CheckExpectedBuffers(10, 24, &kDataA);
  Seek(5);
  CheckExpectedBuffers(5, 9, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteFrontGOPsAtATime) {
  // Set memory limit to 20 buffers.
  SetMemoryLimit(20);

  // Append 20 buffers at positions 0 through 19.
  NewSegmentAppend(0, 20, &kDataA);

  // Seek to position 10.
  Seek(10);

  // Add one buffer to put the memory over the cap.
  AppendBuffers(20, 1, &kDataA);

  // GC should have deleted the first 5 buffers so that the range still begins
  // with a keyframe.
  CheckExpectedRanges("{ [5,20) }");
  CheckExpectedBuffers(10, 20, &kDataA);
  Seek(5);
  CheckExpectedBuffers(5, 9, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteBack) {
  // Set memory limit to 5 buffers.
  SetMemoryLimit(5);

  // Seek to position 0.
  Seek(0);

  // Append 20 buffers at positions 0 through 19.
  NewSegmentAppend(0, 20, &kDataA);

  // Should leave the first 5 buffers from 0 to 4 and the last GOP appended.
  CheckExpectedRanges("{ [0,4) [15,19) }");
  CheckExpectedBuffers(0, 4, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteFrontAndBack) {
  // Set memory limit to 3 buffers.
  SetMemoryLimit(3);

  // Seek to position 15.
  Seek(15);

  // Append 40 buffers at positions 0 through 39.
  NewSegmentAppend(0, 40, &kDataA);

  // Should leave the GOP containing the seek position and the last GOP
  // appended.
  CheckExpectedRanges("{ [15,19) [35,39) }");
  CheckExpectedBuffers(15, 19, &kDataA);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteSeveralRanges) {
  // Append 5 buffers at positions 0 through 4.
  NewSegmentAppend(0, 5);

  // Append 5 buffers at positions 10 through 14.
  NewSegmentAppend(10, 5);

  // Append 5 buffers at positions 20 through 24.
  NewSegmentAppend(20, 5);

  // Append 5 buffers at positions 30 through 34.
  NewSegmentAppend(30, 5);

  CheckExpectedRanges("{ [0,4) [10,14) [20,24) [30,34) }");

  // Seek to position 21.
  Seek(20);
  CheckExpectedBuffers(20, 20);

  // Set memory limit to 1 buffer.
  SetMemoryLimit(1);

  // Append 5 buffers at positions 40 through 44. This will trigger GC.
  NewSegmentAppend(40, 5);

  // Should delete everything except the GOP containing the current buffer and
  // the last GOP appended.
  CheckExpectedRanges("{ [20,24) [40,44) }");
  CheckExpectedBuffers(21, 24);
  CheckNoNextBuffer();

  // Continue appending into the last range to make sure it didn't break.
  AppendBuffers(45, 10);
  // Should only save last GOP appended.
  CheckExpectedRanges("{ [20,24) [50,54) }");

  // Make sure appending before and after the ranges didn't somehow break.
  SetMemoryLimit(100);
  NewSegmentAppend(0, 10);
  CheckExpectedRanges("{ [0,9) [20,24) [50,54) }");
  Seek(0);
  CheckExpectedBuffers(0, 9);

  NewSegmentAppend(90, 10);
  CheckExpectedRanges("{ [0,9) [20,24) [50,54) [90,99) }");
  Seek(50);
  CheckExpectedBuffers(50, 54);
  CheckNoNextBuffer();
  Seek(90);
  CheckExpectedBuffers(90, 99);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteAfterLastAppend) {
  // Set memory limit to 10 buffers.
  SetMemoryLimit(10);

  // Append 1 GOP starting at 310ms, 30ms apart.
  NewSegmentAppend("310K 340 370");

  // Append 2 GOPs starting at 490ms, 30ms apart.
  NewSegmentAppend("490K 520 550 580K 610 640");

  CheckExpectedRangesByTimestamp("{ [310,400) [490,670) }");

  // Seek to the GOP at 580ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(580));

  // Append 2 GOPs before the existing ranges.
  // So the ranges before GC are "{ [100,280) [310,400) [490,670) }".
  NewSegmentAppend("100K 130 160 190K 220 250K");

  // Should save the newly appended GOPs.
  CheckExpectedRangesByTimestamp("{ [100,280) [580,670) }");
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteAfterLastAppendMerged) {
  // Set memory limit to 10 buffers.
  SetMemoryLimit(10);

  // Append 3 GOPs starting at 400ms, 30ms apart.
  NewSegmentAppend("400K 430 460 490K 520 550 580K 610 640");

  // Seek to the GOP at 580ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(580));

  // Append 2 GOPs starting at 220ms, and they will be merged with the existing
  // range.  So the range before GC is "{ [220,670) }".
  NewSegmentAppend("220K 250 280 310K 340 370");

  // Should save the newly appended GOPs.
  CheckExpectedRangesByTimestamp("{ [220,400) [580,670) }");
}

TEST_F(SourceBufferStreamTest, GarbageCollection_NoSeek) {
  // Set memory limit to 20 buffers.
  SetMemoryLimit(20);

  // Append 25 buffers at positions 0 through 24.
  NewSegmentAppend(0, 25, &kDataA);

  // GC deletes the first 5 buffers to keep the memory limit within cap.
  CheckExpectedRanges("{ [5,24) }");
  CheckNoNextBuffer();
  Seek(5);
  CheckExpectedBuffers(5, 24, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_PendingSeek) {
  // Append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10, &kDataA);

  // Append 5 buffers at positions 25 through 29.
  NewSegmentAppend(25, 5, &kDataA);

  // Seek to position 15.
  Seek(15);
  CheckNoNextBuffer();

  CheckExpectedRanges("{ [0,9) [25,29) }");

  // Set memory limit to 5 buffers.
  SetMemoryLimit(5);

  // Append 5 buffers as positions 30 to 34 to trigger GC.
  AppendBuffers(30, 5, &kDataA);

  // The current algorithm will delete from the beginning until the memory is
  // under cap.
  CheckExpectedRanges("{ [30,34) }");

  // Expand memory limit again so that GC won't be triggered.
  SetMemoryLimit(100);

  // Append data to fulfill seek.
  NewSegmentAppend(15, 5, &kDataA);

  // Check to make sure all is well.
  CheckExpectedRanges("{ [15,19) [30,34) }");
  CheckExpectedBuffers(15, 19, &kDataA);
  Seek(30);
  CheckExpectedBuffers(30, 34, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_NeedsMoreData) {
  // Set memory limit to 15 buffers.
  SetMemoryLimit(15);

  // Append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10, &kDataA);

  // Advance next buffer position to 10.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataA);
  CheckNoNextBuffer();

  // Append 20 buffers at positions 15 through 34.
  NewSegmentAppend(15, 20, &kDataA);

  // GC should have saved the keyframe before the current seek position and the
  // data closest to the current seek position. It will also save the last GOP
  // appended.
  CheckExpectedRanges("{ [5,9) [15,19) [30,34) }");

  // Now fulfill the seek at position 10. This will make GC delete the data
  // before position 10 to keep it within cap.
  NewSegmentAppend(10, 5, &kDataA);
  CheckExpectedRanges("{ [10,19) [30,34) }");
  CheckExpectedBuffers(10, 19, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_TrackBuffer) {
  // Set memory limit to 3 buffers.
  SetMemoryLimit(3);

  // Seek to position 15.
  Seek(15);

  // Append 18 buffers at positions 0 through 17.
  NewSegmentAppend(0, 18, &kDataA);

  // Should leave GOP containing seek position.
  CheckExpectedRanges("{ [15,17) }");

  // Seek ahead to position 16.
  CheckExpectedBuffers(15, 15, &kDataA);

  // Completely overlap the existing buffers.
  NewSegmentAppend(0, 20, &kDataB);

  // Because buffers 16 and 17 are not keyframes, they are moved to the track
  // buffer upon overlap. The source buffer (i.e. not the track buffer) is now
  // waiting for the next keyframe.
  CheckExpectedRanges("{ [15,19) }");
  CheckExpectedBuffers(16, 17, &kDataA);
  CheckNoNextBuffer();

  // Now add a keyframe at position 20.
  AppendBuffers(20, 5, &kDataB);

  // Should garbage collect such that there are 5 frames remaining, starting at
  // the keyframe.
  CheckExpectedRanges("{ [20,24) }");
  CheckExpectedBuffers(20, 24, &kDataB);
  CheckNoNextBuffer();
}

// Test saving the last GOP appended when this GOP is the only GOP in its range.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP) {
  // Set memory limit to 3 and make sure the 4-byte GOP is not garbage
  // collected.
  SetMemoryLimit(3);
  NewSegmentAppend("0K 30 60 90");
  CheckExpectedRangesByTimestamp("{ [0,120) }");

  // Make sure you can continue appending data to this GOP; again, GC should not
  // wipe out anything.
  AppendBuffers("120");
  CheckExpectedRangesByTimestamp("{ [0,150) }");

  // Set memory limit to 100 and append a 2nd range after this without
  // triggering GC.
  SetMemoryLimit(100);
  NewSegmentAppend("200K 230 260 290K 320 350");
  CheckExpectedRangesByTimestamp("{ [0,150) [200,380) }");

  // Seek to 290ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(290));

  // Now set memory limit to 3 and append a GOP in a separate range after the
  // selected range. Because it is after 290ms, this tests that the GOP is saved
  // when deleting from the back.
  SetMemoryLimit(3);
  NewSegmentAppend("500K 530 560 590");

  // Should save GOP with 290ms and last GOP appended.
  CheckExpectedRangesByTimestamp("{ [290,380) [500,620) }");

  // Continue appending to this GOP after GC.
  AppendBuffers("620");
  CheckExpectedRangesByTimestamp("{ [290,380) [500,650) }");
}

// Test saving the last GOP appended when this GOP is in the middle of a
// non-selected range.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Middle) {
  // Append 3 GOPs starting at 0ms, 30ms apart.
  NewSegmentAppend("0K 30 60 90K 120 150 180K 210 240");
  CheckExpectedRangesByTimestamp("{ [0,270) }");

  // Now set the memory limit to 1 and overlap the middle of the range with a
  // new GOP.
  SetMemoryLimit(1);
  NewSegmentAppend("80K 110 140");

  // This whole GOP should be saved, and should be able to continue appending
  // data to it.
  CheckExpectedRangesByTimestamp("{ [80,170) }");
  AppendBuffers("170");
  CheckExpectedRangesByTimestamp("{ [80,200) }");

  // Set memory limit to 100 and append a 2nd range after this without
  // triggering GC.
  SetMemoryLimit(100);
  NewSegmentAppend("400K 430 460 490K 520 550 580K 610 640");
  CheckExpectedRangesByTimestamp("{ [80,200) [400,670) }");

  // Seek to 80ms to make the first range the selected range.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(80));

  // Now set memory limit to 3 and append a GOP in the middle of the second
  // range. Because it is after the selected range, this tests that the GOP is
  // saved when deleting from the back.
  SetMemoryLimit(3);
  NewSegmentAppend("500K 530 560 590");

  // Should save the GOP containing the seek point and GOP that was last
  // appended.
  CheckExpectedRangesByTimestamp("{ [80,200) [500,620) }");

  // Continue appending to this GOP after GC.
  AppendBuffers("620");
  CheckExpectedRangesByTimestamp("{ [80,200) [500,650) }");
}

// Test saving the last GOP appended when the GOP containing the next buffer is
// adjacent to the last GOP appended.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Selected1) {
  // Append 3 GOPs at 0ms, 90ms, and 180ms.
  NewSegmentAppend("0K 30 60 90K 120 150 180K 210 240");
  CheckExpectedRangesByTimestamp("{ [0,270) }");

  // Seek to the GOP at 90ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(90));

  // Set the memory limit to 1, then overlap the GOP at 0.
  SetMemoryLimit(1);
  NewSegmentAppend("0K 30 60");

  // Should save the GOP at 0ms and 90ms.
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Seek to 0 and check all buffers.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));
  CheckExpectedBuffers("0K 30 60 90K 120 150");
  CheckNoNextBuffer();

  // Now seek back to 90ms and append a GOP at 180ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(90));
  NewSegmentAppend("180K 210 240");

  // Should save the GOP at 90ms and the GOP at 180ms.
  CheckExpectedRangesByTimestamp("{ [90,270) }");
  CheckExpectedBuffers("90K 120 150 180K 210 240");
  CheckNoNextBuffer();
}

// Test saving the last GOP appended when it is at the beginning or end of the
// selected range. This tests when the last GOP appended is before or after the
// GOP containing the next buffer, but not directly adjacent to this GOP.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Selected2) {
  // Append 4 GOPs starting at positions 0ms, 90ms, 180ms, 270ms.
  NewSegmentAppend("0K 30 60 90K 120 150 180K 210 240 270K 300 330");
  CheckExpectedRangesByTimestamp("{ [0,360) }");

  // Seek to the last GOP at 270ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(270));

  // Set the memory limit to 1, then overlap the GOP at 90ms.
  SetMemoryLimit(1);
  NewSegmentAppend("90K 120 150");

  // Should save the GOP at 90ms and the GOP at 270ms.
  CheckExpectedRangesByTimestamp("{ [90,180) [270,360) }");

  // Set memory limit to 100 and add 3 GOPs to the end of the selected range
  // at 360ms, 450ms, and 540ms.
  SetMemoryLimit(100);
  NewSegmentAppend("360K 390 420 450K 480 510 540K 570 600");
  CheckExpectedRangesByTimestamp("{ [90,180) [270,630) }");

  // Constrain the memory limit again and overlap the GOP at 450ms to test
  // deleting from the back.
  SetMemoryLimit(1);
  NewSegmentAppend("450K 480 510");

  // Should save GOP at 270ms and the GOP at 450ms.
  CheckExpectedRangesByTimestamp("{ [270,360) [450,540) }");
}

// Test saving the last GOP appended when it is the same as the GOP containing
// the next buffer.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Selected3) {
  // Seek to start of stream.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));

  // Append 3 GOPs starting at 0ms, 90ms, 180ms.
  NewSegmentAppend("0K 30 60 90K 120 150 180K 210 240");
  CheckExpectedRangesByTimestamp("{ [0,270) }");

  // Set the memory limit to 1 then begin appending the start of a GOP starting
  // at 0ms.
  SetMemoryLimit(1);
  NewSegmentAppend("0K 30");

  // Should save the newly appended GOP, which is also the next GOP that will be
  // returned from the seek request.
  CheckExpectedRangesByTimestamp("{ [0,60) }");

  // Check the buffers in the range.
  CheckExpectedBuffers("0K 30");
  CheckNoNextBuffer();

  // Continue appending to this buffer.
  AppendBuffers("60 90");

  // Should still save the rest of this GOP and should be able to fulfill the
  // read.
  CheckExpectedRangesByTimestamp("{ [0,120) }");
  CheckExpectedBuffers("60 90");
  CheckNoNextBuffer();
}

// Currently disabled because of bug: crbug.com/140875.
TEST_F(SourceBufferStreamTest, DISABLED_GarbageCollection_WaitingForKeyframe) {
  // Set memory limit to 10 buffers.
  SetMemoryLimit(10);

  // Append 5 buffers at positions 10 through 14 and exhaust the buffers.
  NewSegmentAppend(10, 5, &kDataA);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckExpectedRanges("{ [10,14) }");

  // We are now stalled at position 15.
  CheckNoNextBuffer();

  // Do an end overlap that causes the latter half of the range to be deleted.
  NewSegmentAppend(5, 6, &kDataA);
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [5,10) }");

  // Append buffers from position 20 to 29. This should trigger GC.
  NewSegmentAppend(20, 10, &kDataA);

  // GC should keep the keyframe before the seek position 15, and the next 9
  // buffers closest to the seek position.
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [10,10) [20,28) }");

  // Fulfill the seek by appending one buffer at 15.
  NewSegmentAppend(15, 1, &kDataA);
  CheckExpectedBuffers(15, 15, &kDataA);
  CheckExpectedRanges("{ [15,15) [20,28) }");
}

// Test the performance of garbage collection.
TEST_F(SourceBufferStreamTest, GarbageCollection_Performance) {
  // Force |keyframes_per_second_| to be equal to kDefaultFramesPerSecond.
  SetStreamInfo(kDefaultFramesPerSecond, kDefaultFramesPerSecond);

  const int kBuffersToKeep = 1000;
  SetMemoryLimit(kBuffersToKeep);

  int buffers_appended = 0;

  NewSegmentAppend(0, kBuffersToKeep);
  buffers_appended += kBuffersToKeep;

  const int kBuffersToAppend = 1000;
  const int kGarbageCollections = 3;
  for (int i = 0; i < kGarbageCollections; ++i) {
    AppendBuffers(buffers_appended, kBuffersToAppend);
    buffers_appended += kBuffersToAppend;
  }
}

TEST_F(SourceBufferStreamTest, GetRemovalRange_BytesToFree) {
  // Append 2 GOPs starting at 300ms, 30ms apart.
  NewSegmentAppend("300K 330 360 390K 420 450");

  // Append 2 GOPs starting at 600ms, 30ms apart.
  NewSegmentAppend("600K 630 660 690K 720 750");

  // Append 2 GOPs starting at 900ms, 30ms apart.
  NewSegmentAppend("900K 930 960 990K 1020 1050");

  CheckExpectedRangesByTimestamp("{ [300,480) [600,780) [900,1080) }");

  int remove_range_end = -1;
  int bytes_removed = -1;

  // Size 0.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 0, &remove_range_end);
  EXPECT_EQ(-1, remove_range_end);
  EXPECT_EQ(0, bytes_removed);

  // Smaller than the size of GOP.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 1, &remove_range_end);
  EXPECT_EQ(390, remove_range_end);
  // Remove as the size of GOP.
  EXPECT_EQ(3, bytes_removed);

  // The same size with a GOP.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 3, &remove_range_end);
  EXPECT_EQ(390, remove_range_end);
  EXPECT_EQ(3, bytes_removed);

  // The same size with a range.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 6, &remove_range_end);
  EXPECT_EQ(480, remove_range_end);
  EXPECT_EQ(6, bytes_removed);

  // A frame larger than a range.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 7, &remove_range_end);
  EXPECT_EQ(690, remove_range_end);
  EXPECT_EQ(9, bytes_removed);

  // The same size with two ranges.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 12, &remove_range_end);
  EXPECT_EQ(780, remove_range_end);
  EXPECT_EQ(12, bytes_removed);

  // Larger than two ranges.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 14, &remove_range_end);
  EXPECT_EQ(990, remove_range_end);
  EXPECT_EQ(15, bytes_removed);

  // The same size with the whole ranges.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 18, &remove_range_end);
  EXPECT_EQ(1080, remove_range_end);
  EXPECT_EQ(18, bytes_removed);

  // Larger than the whole ranges.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 20, &remove_range_end);
  EXPECT_EQ(1080, remove_range_end);
  EXPECT_EQ(18, bytes_removed);
}

TEST_F(SourceBufferStreamTest, GetRemovalRange_Range) {
  // Append 2 GOPs starting at 300ms, 30ms apart.
  NewSegmentAppend("300K 330 360 390K 420 450");

  // Append 2 GOPs starting at 600ms, 30ms apart.
  NewSegmentAppend("600K 630 660 690K 720 750");

  // Append 2 GOPs starting at 900ms, 30ms apart.
  NewSegmentAppend("900K 930 960 990K 1020 1050");

  CheckExpectedRangesByTimestamp("{ [300,480) [600,780) [900,1080) }");

  int remove_range_end = -1;
  int bytes_removed = -1;

  // Within a GOP and no keyframe.
  bytes_removed = GetRemovalRangeInMs(630, 660, 20, &remove_range_end);
  EXPECT_EQ(-1, remove_range_end);
  EXPECT_EQ(0, bytes_removed);

  // Across a GOP and no keyframe.
  bytes_removed = GetRemovalRangeInMs(630, 750, 20, &remove_range_end);
  EXPECT_EQ(-1, remove_range_end);
  EXPECT_EQ(0, bytes_removed);

  // The same size with a range.
  bytes_removed = GetRemovalRangeInMs(600, 780, 20, &remove_range_end);
  EXPECT_EQ(780, remove_range_end);
  EXPECT_EQ(6, bytes_removed);

  // One frame larger than a range.
  bytes_removed = GetRemovalRangeInMs(570, 810, 20, &remove_range_end);
  EXPECT_EQ(780, remove_range_end);
  EXPECT_EQ(6, bytes_removed);

  // Facing the other ranges.
  bytes_removed = GetRemovalRangeInMs(480, 900, 20, &remove_range_end);
  EXPECT_EQ(780, remove_range_end);
  EXPECT_EQ(6, bytes_removed);

  // In the midle of the other ranges, but not including any GOP.
  bytes_removed = GetRemovalRangeInMs(420, 960, 20, &remove_range_end);
  EXPECT_EQ(780, remove_range_end);
  EXPECT_EQ(6, bytes_removed);

  // In the middle of the other ranges.
  bytes_removed = GetRemovalRangeInMs(390, 990, 20, &remove_range_end);
  EXPECT_EQ(990, remove_range_end);
  EXPECT_EQ(12, bytes_removed);

  // A frame smaller than the whole ranges.
  bytes_removed = GetRemovalRangeInMs(330, 1050, 20, &remove_range_end);
  EXPECT_EQ(990, remove_range_end);
  EXPECT_EQ(12, bytes_removed);

  // The same with the whole ranges.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 20, &remove_range_end);
  EXPECT_EQ(1080, remove_range_end);
  EXPECT_EQ(18, bytes_removed);

  // Larger than the whole ranges.
  bytes_removed = GetRemovalRangeInMs(270, 1110, 20, &remove_range_end);
  EXPECT_EQ(1080, remove_range_end);
  EXPECT_EQ(18, bytes_removed);
}

TEST_F(SourceBufferStreamTest, ConfigChange_Basic) {
  VideoDecoderConfig new_config = TestVideoConfig::Large();
  ASSERT_FALSE(new_config.Matches(video_config_));

  Seek(0);
  CheckVideoConfig(video_config_);

  // Append 5 buffers at positions 0 through 4
  NewSegmentAppend(0, 5, &kDataA);

  CheckVideoConfig(video_config_);

  // Signal a config change.
  stream_->UpdateVideoConfig(new_config);

  // Make sure updating the config doesn't change anything since new_config
  // should not be associated with the buffer GetNextBuffer() will return.
  CheckVideoConfig(video_config_);

  // Append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5, &kDataB);

  // Consume the buffers associated with the initial config.
  scoped_refptr<StreamParserBuffer> buffer;
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kSuccess);
    CheckVideoConfig(video_config_);
  }

  // Verify the next attempt to get a buffer will signal that a config change
  // has happened.
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kConfigChange);

  // Verify that the new config is now returned.
  CheckVideoConfig(new_config);

  // Consume the remaining buffers associated with the new config.
  for (int i = 0; i < 5; i++) {
    CheckVideoConfig(new_config);
    EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kSuccess);
  }
}

TEST_F(SourceBufferStreamTest, ConfigChange_Seek) {
  scoped_refptr<StreamParserBuffer> buffer;
  VideoDecoderConfig new_config = TestVideoConfig::Large();

  Seek(0);
  NewSegmentAppend(0, 5, &kDataA);
  stream_->UpdateVideoConfig(new_config);
  NewSegmentAppend(5, 5, &kDataB);

  // Seek to the start of the buffers with the new config and make sure a
  // config change is signalled.
  CheckVideoConfig(video_config_);
  Seek(5);
  CheckVideoConfig(video_config_);
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kConfigChange);
  CheckVideoConfig(new_config);
  CheckExpectedBuffers(5, 9, &kDataB);


  // Seek to the start which has a different config. Don't fetch any buffers and
  // seek back to buffers with the current config. Make sure a config change
  // isn't signalled in this case.
  CheckVideoConfig(new_config);
  Seek(0);
  Seek(7);
  CheckExpectedBuffers(5, 9, &kDataB);


  // Seek to the start and make sure a config change is signalled.
  CheckVideoConfig(new_config);
  Seek(0);
  CheckVideoConfig(new_config);
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kConfigChange);
  CheckVideoConfig(video_config_);
  CheckExpectedBuffers(0, 4, &kDataA);
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration) {
  // Append 2 buffers at positions 5 through 6.
  NewSegmentAppend(5, 2);

  // Append 2 buffers at positions 10 through 11.
  NewSegmentAppend(10, 2);

  // Append 2 buffers at positions 15 through 16.
  NewSegmentAppend(15, 2);

  // Check expected ranges.
  CheckExpectedRanges("{ [5,6) [10,11) [15,16) }");

  // Set duration to be between buffers 6 and 10.
  stream_->OnSetDuration(frame_duration() * 8);

  // Should truncate the data after 6.
  CheckExpectedRanges("{ [5,6) }");

  // Adding data past the previous duration should still work.
  NewSegmentAppend(0, 20);
  CheckExpectedRanges("{ [0,19) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_EdgeCase) {
  // Append 10 buffers at positions 10 through 19.
  NewSegmentAppend(10, 10);

  // Append 5 buffers at positions 25 through 29.
  NewSegmentAppend(25, 5);

  // Check expected ranges.
  CheckExpectedRanges("{ [10,19) [25,29) }");

  // Set duration to be right before buffer 25.
  stream_->OnSetDuration(frame_duration() * 25);

  // Should truncate the last range.
  CheckExpectedRanges("{ [10,19) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_DeletePartialRange) {
  // Append 5 buffers at positions 0 through 4.
  NewSegmentAppend(0, 5);

  // Append 10 buffers at positions 10 through 19.
  NewSegmentAppend(10, 10);

  // Append 5 buffers at positions 25 through 29.
  NewSegmentAppend(25, 5);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,4) [10,19) [25,29) }");

  // Set duration to be between buffers 13 and 14.
  stream_->OnSetDuration(frame_duration() * 14);

  // Should truncate the data after 13.
  CheckExpectedRanges("{ [0,4) [10,13) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_DeleteSelectedRange) {
  // Append 2 buffers at positions 5 through 6.
  NewSegmentAppend(5, 2);

  // Append 2 buffers at positions 10 through 11.
  NewSegmentAppend(10, 2);

  // Append 2 buffers at positions 15 through 16.
  NewSegmentAppend(15, 2);

  // Check expected ranges.
  CheckExpectedRanges("{ [5,6) [10,11) [15,16) }");

  // Seek to 10.
  Seek(10);

  // Set duration to be after position 3.
  stream_->OnSetDuration(frame_duration() * 4);

  // Expect everything to be deleted, and should not have next buffer anymore.
  CheckNoNextBuffer();
  CheckExpectedRanges("{ }");

  // Appending data at position 10 should not fulfill the seek.
  // (If the duration is set to be something smaller than the current seek
  // point, then the seek point is reset and the SourceBufferStream waits
  // for a new seek request. Therefore even if the data is re-appended, it
  // should not fulfill the old seek.)
  NewSegmentAppend(0, 15);
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [0,14) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_DeletePartialSelectedRange) {
  // Append 5 buffers at positions 0 through 4.
  NewSegmentAppend(0, 5);

  // Append 20 buffers at positions 10 through 29.
  NewSegmentAppend(10, 20);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,4) [10,29) }");

  // Seek to position 10.
  Seek(10);

  // Set duration to be between buffers 24 and 25.
  stream_->OnSetDuration(frame_duration() * 25);

  // Should truncate the data after 24.
  CheckExpectedRanges("{ [0,4) [10,24) }");

  // The seek position should not be lost.
  CheckExpectedBuffers(10, 10);

  // Now set the duration immediately after buffer 10.
  stream_->OnSetDuration(frame_duration() * 11);

  // Seek position should be reset.
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [0,4) [10,10) }");
}

// Test the case where duration is set while the stream parser buffers
// already start passing the data to decoding pipeline. Selected range,
// when invalidated by getting truncated, should be updated to NULL
// accordingly so that successive append operations keep working.
TEST_F(SourceBufferStreamTest, SetExplicitDuration_UpdateSelectedRange) {
  // Seek to start of stream.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));

  NewSegmentAppend("0K 30 60 90");

  // Read out the first few buffers.
  CheckExpectedBuffers("0K 30");

  // Set duration to be right before buffer 1.
  stream_->OnSetDuration(base::TimeDelta::FromMilliseconds(60));

  // Verify that there is no next buffer.
  CheckNoNextBuffer();

  // We should be able to append new buffers at this point.
  NewSegmentAppend("120K 150");

  CheckExpectedRangesByTimestamp("{ [0,60) [120,180) }");
}

TEST_F(SourceBufferStreamTest,
       SetExplicitDuration_AfterSegmentTimestampAndBeforeFirstBufferTimestamp) {

  NewSegmentAppend("0K 30K 60K");

  // Append a segment with a start timestamp of 200, but the first
  // buffer starts at 230ms. This can happen in muxed content where the
  // audio starts before the first frame.
  NewSegmentAppend(base::TimeDelta::FromMilliseconds(200),
                   "230K 260K 290K 320K");

  NewSegmentAppend("400K 430K 460K");

  CheckExpectedRangesByTimestamp("{ [0,90) [200,350) [400,490) }");

  stream_->OnSetDuration(base::TimeDelta::FromMilliseconds(120));

  // Verify that the buffered ranges are updated properly and we don't crash.
  CheckExpectedRangesByTimestamp("{ [0,90) }");
}

// Test the case were the current playback position is at the end of the
// buffered data and several overlaps occur that causes the selected
// range to get split and then merged back into a single range.
TEST_F(SourceBufferStreamTest, OverlapSplitAndMergeWhileWaitingForMoreData) {
  // Seek to start of stream.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));

  NewSegmentAppend("0K 30 60 90 120K 150");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Read all the buffered data.
  CheckExpectedBuffers("0K 30 60 90 120K 150");
  CheckNoNextBuffer();

  // Append data over the current GOP so that a keyframe is needed before
  // playback can continue from the current position.
  NewSegmentAppend("120K 150");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Append buffers that cause the range to get split.
  NewSegmentAppend("0K 30");
  CheckExpectedRangesByTimestamp("{ [0,60) [120,180) }");

  // Append buffers that cause the ranges to get merged.
  AppendBuffers("60 90");

  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Verify that we still don't have a next buffer.
  CheckNoNextBuffer();

  // Add more data to the end and verify that this new data is read correctly.
  NewSegmentAppend("180K 210");
  CheckExpectedRangesByTimestamp("{ [0,240) }");
  CheckExpectedBuffers("180K 210");
}

// Verify that non-keyframes with the same timestamp in the same
// append are handled correctly.
TEST_F(SourceBufferStreamTest, SameTimestamp_Video_SingleAppend) {
  Seek(0);
  NewSegmentAppend("0K 30 30 60 90 120K 150");
  CheckExpectedBuffers("0K 30 30 60 90 120K 150");
}

// Verify that non-keyframes with the same timestamp can occur
// in different appends.
TEST_F(SourceBufferStreamTest, SameTimestamp_Video_TwoAppends) {
  Seek(0);
  NewSegmentAppend("0K 30");
  AppendBuffers("30 60 90 120K 150");
  CheckExpectedBuffers("0K 30 30 60 90 120K 150");
}

// Verify that a non-keyframe followed by a keyframe with the same timestamp
// is not allowed.
TEST_F(SourceBufferStreamTest, SameTimestamp_Video_Invalid_1) {
  Seek(0);
  NewSegmentAppend("0K 30");
  AppendBuffers_ExpectFailure("30K 60");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_Video_Invalid_2) {
  Seek(0);
  NewSegmentAppend_ExpectFailure("0K 30 30K 60");
}

// Verify that a keyframe followed by a non-keyframe with the same timestamp
// is allowed.
TEST_F(SourceBufferStreamTest, SameTimestamp_VideoKeyFrame_TwoAppends) {
  Seek(0);
  NewSegmentAppend("0K 30K");
  AppendBuffers("30 60");
  CheckExpectedBuffers("0K 30K 30 60");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_VideoKeyFrame_SingleAppend) {
  Seek(0);
  NewSegmentAppend("0K 30K 30 60");
  CheckExpectedBuffers("0K 30K 30 60");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_Video_Overlap_1) {
  Seek(0);
  NewSegmentAppend("0K 30 60 60 90 120K 150");

  NewSegmentAppend("60K 91 121K 151");
  CheckExpectedBuffers("0K 30 60K 91 121K 151");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_Video_Overlap_2) {
  Seek(0);
  NewSegmentAppend("0K 30 60 60 90 120K 150");
  NewSegmentAppend("0K 30 61");
  CheckExpectedBuffers("0K 30 61 120K 150");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_Video_Overlap_3) {
  Seek(0);
  NewSegmentAppend("0K 20 40 60 80 100K 101 102 103K");
  NewSegmentAppend("0K 20 40 60 80 90");
  CheckExpectedBuffers("0K 20 40 60 80 90 100K 101 102 103K");
  AppendBuffers("90 110K 150");
  Seek(0);
  CheckExpectedBuffers("0K 20 40 60 80 90 90 110K 150");
  CheckNoNextBuffer();
  CheckExpectedRangesByTimestamp("{ [0,190) }");
}

// Test all the valid same timestamp cases for audio.
TEST_F(SourceBufferStreamTest, SameTimestamp_Audio) {
  AudioDecoderConfig config(kCodecMP3, kSampleFormatF32, CHANNEL_LAYOUT_STEREO,
                            44100, NULL, 0, false);
  stream_.reset(new SourceBufferStream(config, log_cb(), true));
  Seek(0);
  NewSegmentAppend("0K 0K 30K 30 60 60");
  CheckExpectedBuffers("0K 0K 30K 30 60 60");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_Audio_Invalid_1) {
  AudioDecoderConfig config(kCodecMP3, kSampleFormatF32, CHANNEL_LAYOUT_STEREO,
                            44100, NULL, 0, false);
  stream_.reset(new SourceBufferStream(config, log_cb(), true));
  Seek(0);
  NewSegmentAppend_ExpectFailure("0K 30 30K 60");
}

// If seeking past any existing range and the seek is pending
// because no data has been provided for that position,
// the stream position can be considered as the end of stream.
TEST_F(SourceBufferStreamTest, EndSelected_During_PendingSeek) {
  // Append 15 buffers at positions 0 through 14.
  NewSegmentAppend(0, 15);

  Seek(20);
  EXPECT_TRUE(stream_->IsSeekPending());
  stream_->MarkEndOfStream();
  EXPECT_FALSE(stream_->IsSeekPending());
}

// If there is a pending seek between 2 existing ranges,
// the end of the stream has not been reached.
TEST_F(SourceBufferStreamTest, EndNotSelected_During_PendingSeek) {
  // Append:
  // - 10 buffers at positions 0 through 9.
  // - 10 buffers at positions 30 through 39
  NewSegmentAppend(0, 10);
  NewSegmentAppend(30, 10);

  Seek(20);
  EXPECT_TRUE(stream_->IsSeekPending());
  stream_->MarkEndOfStream();
  EXPECT_TRUE(stream_->IsSeekPending());
}


// Removing exact start & end of a range.
TEST_F(SourceBufferStreamTest, Remove_WholeRange1) {
  Seek(0);
  NewSegmentAppend("10K 40 70K 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");
  RemoveInMs(10, 160, 160);
  CheckExpectedRangesByTimestamp("{ }");
}

// Removal range starts before range and ends exactly at end.
TEST_F(SourceBufferStreamTest, Remove_WholeRange2) {
  Seek(0);
  NewSegmentAppend("10K 40 70K 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");
  RemoveInMs(0, 160, 160);
  CheckExpectedRangesByTimestamp("{ }");
}

// Removal range starts at the start of a range and ends beyond the
// range end.
TEST_F(SourceBufferStreamTest, Remove_WholeRange3) {
  Seek(0);
  NewSegmentAppend("10K 40 70K 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");
  RemoveInMs(10, 200, 200);
  CheckExpectedRangesByTimestamp("{ }");
}

// Removal range starts before range start and ends after the range end.
TEST_F(SourceBufferStreamTest, Remove_WholeRange4) {
  Seek(0);
  NewSegmentAppend("10K 40 70K 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");
  RemoveInMs(0, 200, 200);
  CheckExpectedRangesByTimestamp("{ }");
}

// Removes multiple ranges.
TEST_F(SourceBufferStreamTest, Remove_WholeRange5) {
  Seek(0);
  NewSegmentAppend("10K 40 70K 100 130K");
  NewSegmentAppend("1000K 1030 1060K 1090 1120K");
  NewSegmentAppend("2000K 2030 2060K 2090 2120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) [2000,2150) }");
  RemoveInMs(10, 3000, 3000);
  CheckExpectedRangesByTimestamp("{ }");
}

// Verifies a [0-infinity) range removes everything.
TEST_F(SourceBufferStreamTest, Remove_ZeroToInfinity) {
  Seek(0);
  NewSegmentAppend("10K 40 70K 100 130K");
  NewSegmentAppend("1000K 1030 1060K 1090 1120K");
  NewSegmentAppend("2000K 2030 2060K 2090 2120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) [2000,2150) }");
  Remove(base::TimeDelta(), kInfiniteDuration(), kInfiniteDuration());
  CheckExpectedRangesByTimestamp("{ }");
}

// Removal range starts at the beginning of the range and ends in the
// middle of the range. This test verifies that full GOPs are removed.
TEST_F(SourceBufferStreamTest, Remove_Partial1) {
  Seek(0);
  NewSegmentAppend("10K 40 70K 100 130K");
  NewSegmentAppend("1000K 1030 1060K 1090 1120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) }");
  RemoveInMs(0, 80, 2200);
  CheckExpectedRangesByTimestamp("{ [130,160) [1000,1150) }");
}

// Removal range starts in the middle of a range and ends at the exact
// end of the range.
TEST_F(SourceBufferStreamTest, Remove_Partial2) {
  Seek(0);
  NewSegmentAppend("10K 40 70K 100 130K");
  NewSegmentAppend("1000K 1030 1060K 1090 1120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) }");
  RemoveInMs(40, 160, 2200);
  CheckExpectedRangesByTimestamp("{ [10,40) [1000,1150) }");
}

// Removal range starts and ends within a range.
TEST_F(SourceBufferStreamTest, Remove_Partial3) {
  Seek(0);
  NewSegmentAppend("10K 40 70K 100 130K");
  NewSegmentAppend("1000K 1030 1060K 1090 1120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) }");
  RemoveInMs(40, 120, 2200);
  CheckExpectedRangesByTimestamp("{ [10,40) [130,160) [1000,1150) }");
}

// Removal range starts in the middle of one range and ends in the
// middle of another range.
TEST_F(SourceBufferStreamTest, Remove_Partial4) {
  Seek(0);
  NewSegmentAppend("10K 40 70K 100 130K");
  NewSegmentAppend("1000K 1030 1060K 1090 1120K");
  NewSegmentAppend("2000K 2030 2060K 2090 2120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) [2000,2150) }");
  RemoveInMs(40, 2030, 2200);
  CheckExpectedRangesByTimestamp("{ [10,40) [2060,2150) }");
}

// Test behavior when the current position is removed and new buffers
// are appended over the removal range.
TEST_F(SourceBufferStreamTest, Remove_CurrentPosition) {
  Seek(0);
  NewSegmentAppend("0K 30 60 90K 120 150 180K 210 240 270K 300 330");
  CheckExpectedRangesByTimestamp("{ [0,360) }");
  CheckExpectedBuffers("0K 30 60 90K 120");

  // Remove a range that includes the next buffer (i.e., 150).
  RemoveInMs(150, 210, 360);
  CheckExpectedRangesByTimestamp("{ [0,150) [270,360) }");

  // Verify that no next buffer is returned.
  CheckNoNextBuffer();

  // Append some buffers to fill the gap that was created.
  NewSegmentAppend("120K 150 180 210K 240");
  CheckExpectedRangesByTimestamp("{ [0,360) }");

  // Verify that buffers resume at the next keyframe after the
  // current position.
  CheckExpectedBuffers("210K 240 270K 300 330");
}

// Test behavior when buffers in the selected range before the current position
// are removed.
TEST_F(SourceBufferStreamTest, Remove_BeforeCurrentPosition) {
  Seek(0);
  NewSegmentAppend("0K 30 60 90K 120 150 180K 210 240 270K 300 330");
  CheckExpectedRangesByTimestamp("{ [0,360) }");
  CheckExpectedBuffers("0K 30 60 90K 120");

  // Remove a range that is before the current playback position.
  RemoveInMs(0, 90, 360);
  CheckExpectedRangesByTimestamp("{ [90,360) }");

  CheckExpectedBuffers("150 180K 210 240 270K 300 330");
}

// Test removing the entire range for the current media segment
// being appended.
TEST_F(SourceBufferStreamTest, Remove_MidSegment) {
  Seek(0);
  NewSegmentAppend("0K 30 60 90 120K 150 180 210");
  CheckExpectedRangesByTimestamp("{ [0,240) }");

  NewSegmentAppend("0K 30");

  CheckExpectedBuffers("0K");

  CheckExpectedRangesByTimestamp("{ [0,60) [120,240) }");

  // Remove the entire range that is being appended to.
  RemoveInMs(0, 60, 240);

  // Verify that there is no next buffer since it was removed.
  CheckNoNextBuffer();

  CheckExpectedRangesByTimestamp("{ [120,240) }");

  // Continue appending frames for the current GOP.
  AppendBuffers("60 90");

  // Verify that the non-keyframes are not added.
  CheckExpectedRangesByTimestamp("{ [120,240) }");

  // Finish the previous GOP and start the next one.
  AppendBuffers("120 150K 180");

  // Verify that new GOP replaces the existing range.
  CheckExpectedRangesByTimestamp("{ [150,210) }");


  SeekToTimestamp(base::TimeDelta::FromMilliseconds(150));
  CheckExpectedBuffers("150K 180");
  CheckNoNextBuffer();
}

// Test removing the current GOP being appended, while not removing
// the entire range the GOP belongs to.
TEST_F(SourceBufferStreamTest, Remove_GOPBeingAppended) {
  Seek(0);
  NewSegmentAppend("0K 30 60 90 120K 150 180");
  CheckExpectedRangesByTimestamp("{ [0,210) }");

  // Remove the current GOP being appended.
  RemoveInMs(120, 150, 240);
  CheckExpectedRangesByTimestamp("{ [0,120) }");

  // Continue appending the current GOP and the next one.
  AppendBuffers("210 240K 270 300");

  // Verify that the non-keyframe in the previous GOP does
  // not effect any existing ranges and a new range is started at the
  // beginning of the next GOP.
  CheckExpectedRangesByTimestamp("{ [0,120) [240,330) }");

  // Verify the buffers in the ranges.
  CheckExpectedBuffers("0K 30 60 90");
  CheckNoNextBuffer();
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(240));
  CheckExpectedBuffers("240K 270 300");
}

TEST_F(SourceBufferStreamTest, Remove_WholeGOPBeingAppended) {
  Seek(0);
  NewSegmentAppend("0K 30 60 90");
  CheckExpectedRangesByTimestamp("{ [0,120) }");

  // Remove the keyframe of the current GOP being appended.
  RemoveInMs(0, 30, 120);
  CheckExpectedRangesByTimestamp("{ }");

  // Continue appending the current GOP.
  AppendBuffers("210 240");

  CheckExpectedRangesByTimestamp("{ }");

  // Append the beginning of the next GOP.
  AppendBuffers("270K 300");

  // Verify that the new range is started at the
  // beginning of the next GOP.
  CheckExpectedRangesByTimestamp("{ [270,330) }");

  // Verify the buffers in the ranges.
  CheckNoNextBuffer();
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(270));
  CheckExpectedBuffers("270K 300");
}

TEST_F(SourceBufferStreamTest,
       Remove_PreviousAppendDestroyedAndOverwriteExistingRange) {
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(90));

  NewSegmentAppend("90K 120 150");
  CheckExpectedRangesByTimestamp("{ [90,180) }");

  // Append a segment before the previously appended data.
  NewSegmentAppend("0K 30 60");

  // Verify that the ranges get merged.
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Remove the data from the last append.
  RemoveInMs(0, 90, 360);
  CheckExpectedRangesByTimestamp("{ [90,180) }");

  // Append a new segment that follows the removed segment and
  // starts at the beginning of the range left over from the
  // remove.
  NewSegmentAppend("90K 121 151");
  CheckExpectedBuffers("90K 121 151");
}

TEST_F(SourceBufferStreamTest, Remove_GapAtBeginningOfMediaSegment) {
  Seek(0);

  // Append a media segment that has a gap at the beginning of it.
  NewSegmentAppend(base::TimeDelta::FromMilliseconds(0),
                   "30K 60 90 120K 150");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Remove the gap that doesn't contain any buffers.
  RemoveInMs(0, 10, 180);
  CheckExpectedRangesByTimestamp("{ [10,180) }");

  // Verify we still get the first buffer still since only part of
  // the gap was removed.
  // TODO(acolwell/wolenetz): Consider not returning a buffer at this
  // point since the current seek position has been explicitly
  // removed but didn't happen to remove any buffers.
  // http://crbug.com/384016
  CheckExpectedBuffers("30K");

  // Remove a range that includes the first GOP.
  RemoveInMs(0, 60, 180);

  // Verify that no buffer is returned because the current buffer
  // position has been removed.
  CheckNoNextBuffer();

  CheckExpectedRangesByTimestamp("{ [120,180) }");
}

TEST_F(SourceBufferStreamTest, Text_Append_SingleRange) {
  SetTextStream();
  NewSegmentAppend("0K 500K 1000K");
  CheckExpectedRangesByTimestamp("{ [0,1500) }");

  Seek(0);
  CheckExpectedBuffers("0K 500K 1000K");
}

TEST_F(SourceBufferStreamTest, Text_Append_DisjointAfter) {
  SetTextStream();
  NewSegmentAppend("0K 500K 1000K");
  CheckExpectedRangesByTimestamp("{ [0,1500) }");
  NewSegmentAppend("3000K 3500K 4000K");
  CheckExpectedRangesByTimestamp("{ [0,4500) }");

  Seek(0);
  CheckExpectedBuffers("0K 500K 1000K 3000K 3500K 4000K");
}

TEST_F(SourceBufferStreamTest, Text_Append_DisjointBefore) {
  SetTextStream();
  NewSegmentAppend("3000K 3500K 4000K");
  CheckExpectedRangesByTimestamp("{ [3000,4500) }");
  NewSegmentAppend("0K 500K 1000K");
  CheckExpectedRangesByTimestamp("{ [0,4500) }");

  Seek(0);
  CheckExpectedBuffers("0K 500K 1000K 3000K 3500K 4000K");
}

TEST_F(SourceBufferStreamTest, Text_CompleteOverlap) {
  SetTextStream();
  NewSegmentAppend("3000K 3500K 4000K");
  CheckExpectedRangesByTimestamp("{ [3000,4500) }");
  NewSegmentAppend("0K 501K 1001K 1501K 2001K 2501K "
                   "3001K 3501K 4001K 4501K 5001K");
  CheckExpectedRangesByTimestamp("{ [0,5502) }");

  Seek(0);
  CheckExpectedBuffers("0K 501K 1001K 1501K 2001K 2501K "
                       "3001K 3501K 4001K 4501K 5001K");
}

TEST_F(SourceBufferStreamTest, Text_OverlapAfter) {
  SetTextStream();
  NewSegmentAppend("0K 500K 1000K 1500K 2000K");
  CheckExpectedRangesByTimestamp("{ [0,2500) }");
  NewSegmentAppend("1499K 2001K 2501K 3001K");
  CheckExpectedRangesByTimestamp("{ [0,3503) }");

  Seek(0);
  CheckExpectedBuffers("0K 500K 1000K 1499K 2001K 2501K 3001K");
}

TEST_F(SourceBufferStreamTest, Text_OverlapBefore) {
  SetTextStream();
  NewSegmentAppend("1500K 2000K 2500K 3000K 3500K");
  CheckExpectedRangesByTimestamp("{ [1500,4000) }");
  NewSegmentAppend("0K 501K 1001K 1501K 2001K");
  CheckExpectedRangesByTimestamp("{ [0,4001) }");

  Seek(0);
  CheckExpectedBuffers("0K 501K 1001K 1501K 2001K 2500K 3000K 3500K");
}

TEST_F(SourceBufferStreamTest, SpliceFrame_Basic) {
  Seek(0);
  NewSegmentAppend("0K S(3K 6 9 10) 15 20 S(25K 30 35) 40");
  CheckExpectedBuffers("0K 3K 6 9 C 10 15 20 25K 30 C 35 40");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, SpliceFrame_SeekClearsSplice) {
  Seek(0);
  NewSegmentAppend("0K S(3K 6 9 10) 15K 20");
  CheckExpectedBuffers("0K 3K 6");

  SeekToTimestamp(base::TimeDelta::FromMilliseconds(15));
  CheckExpectedBuffers("15K 20");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, SpliceFrame_SeekClearsSpliceFromTrackBuffer) {
  Seek(0);
  NewSegmentAppend("0K 2K S(3K 6 9 10) 15K 20");
  CheckExpectedBuffers("0K 2K");

  // Overlap the existing segment.
  NewSegmentAppend("5K 15K 20");
  CheckExpectedBuffers("3K 6");

  SeekToTimestamp(base::TimeDelta::FromMilliseconds(15));
  CheckExpectedBuffers("15K 20");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, SpliceFrame_ConfigChangeWithinSplice) {
  VideoDecoderConfig new_config = TestVideoConfig::Large();
  ASSERT_FALSE(new_config.Matches(video_config_));

  // Add a new video config, then reset the config index back to the original.
  stream_->UpdateVideoConfig(new_config);
  stream_->UpdateVideoConfig(video_config_);

  Seek(0);
  CheckVideoConfig(video_config_);
  NewSegmentAppend("0K S(3K 6C 9 10) 15");

  CheckExpectedBuffers("0K 3K C");
  CheckVideoConfig(new_config);
  CheckExpectedBuffers("6 9 C");
  CheckExpectedBuffers("10 C");
  CheckVideoConfig(video_config_);
  CheckExpectedBuffers("15");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, SpliceFrame_BasicFromTrackBuffer) {
  Seek(0);
  NewSegmentAppend("0K 5K S(8K 9 10) 20");
  CheckExpectedBuffers("0K 5K");

  // Overlap the existing segment.
  NewSegmentAppend("5K 20");
  CheckExpectedBuffers("8K 9 C 10 20");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       SpliceFrame_ConfigChangeWithinSpliceFromTrackBuffer) {
  VideoDecoderConfig new_config = TestVideoConfig::Large();
  ASSERT_FALSE(new_config.Matches(video_config_));

  // Add a new video config, then reset the config index back to the original.
  stream_->UpdateVideoConfig(new_config);
  stream_->UpdateVideoConfig(video_config_);

  Seek(0);
  CheckVideoConfig(video_config_);
  NewSegmentAppend("0K 5K S(7K 8C 9 10) 20");
  CheckExpectedBuffers("0K 5K");

  // Overlap the existing segment.
  NewSegmentAppend("5K 20");
  CheckExpectedBuffers("7K C");
  CheckVideoConfig(new_config);
  CheckExpectedBuffers("8 9 C");
  CheckExpectedBuffers("10 C");
  CheckVideoConfig(video_config_);
  CheckExpectedBuffers("20");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_Basic) {
  SetAudioStream();
  Seek(0);
  NewSegmentAppend("0K 2K 4K 6K 8K 10K 12K");
  NewSegmentAppend("11K 13K 15K 17K");
  CheckExpectedBuffers("0K 2K 4K 6K 8K 10K 12K C 11K 13K 15K 17K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_NoExactSplices) {
  SetAudioStream();
  Seek(0);
  NewSegmentAppend("0K 2K 4K 6K 8K 10K 12K");
  NewSegmentAppend("10K 14K");
  CheckExpectedBuffers("0K 2K 4K 6K 8K 10K 14K");
  CheckNoNextBuffer();
}

// Do not allow splices on top of splices.
TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_NoDoubleSplice) {
  SetAudioStream();
  Seek(0);
  NewSegmentAppend("0K 2K 4K 6K 8K 10K 12K");
  NewSegmentAppend("11K 13K 15K 17K");

  // Verify the splice was created.
  CheckExpectedBuffers("0K 2K 4K 6K 8K 10K 12K C 11K 13K 15K 17K");
  CheckNoNextBuffer();
  Seek(0);

  // Create a splice before the first splice which would include it.
  NewSegmentAppend("9K");

  // A splice on top of a splice should result in a discard of the original
  // splice and no new splice frame being generated.
  CheckExpectedBuffers("0K 2K 4K 6K 8K 9K 13K 15K 17K");
  CheckNoNextBuffer();
}

// Test that a splice is not created if an end timestamp and start timestamp
// overlap.
TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_NoSplice) {
  SetAudioStream();
  Seek(0);
  NewSegmentAppend("0K 2K 4K 6K 8K 10K");
  NewSegmentAppend("12K 14K 16K 18K");
  CheckExpectedBuffers("0K 2K 4K 6K 8K 10K 12K 14K 16K 18K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_CorrectMediaSegmentStartTime) {
  SetAudioStream();
  Seek(0);
  NewSegmentAppend("0K 2K 4K");
  CheckExpectedRangesByTimestamp("{ [0,6) }");
  NewSegmentAppend("6K 8K 10K");
  CheckExpectedRangesByTimestamp("{ [0,12) }");
  NewSegmentAppend("1K 4K");
  CheckExpectedRangesByTimestamp("{ [0,12) }");
  CheckExpectedBuffers("0K 2K 4K C 1K 4K 6K 8K 10K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_ConfigChange) {
  SetAudioStream();

  AudioDecoderConfig new_config(kCodecVorbis,
                                kSampleFormatPlanarF32,
                                CHANNEL_LAYOUT_MONO,
                                1000,
                                NULL,
                                0,
                                false);
  ASSERT_NE(new_config.channel_layout(), audio_config_.channel_layout());

  Seek(0);
  CheckAudioConfig(audio_config_);
  NewSegmentAppend("0K 2K 4K 6K");
  stream_->UpdateAudioConfig(new_config);
  NewSegmentAppend("5K 8K 12K");
  CheckExpectedBuffers("0K 2K 4K 6K C 5K 8K 12K");
  CheckAudioConfig(new_config);
  CheckNoNextBuffer();
}

// Ensure splices are not created if there are not enough frames to crossfade.
TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_NoTinySplices) {
  SetAudioStream();
  Seek(0);

  // Overlap the range [0, 2) with [1, 3).  Since each frame has a duration of
  // 2ms this results in an overlap of 1ms between the ranges.  A splice frame
  // should not be generated since it requires at least 2 frames, or 2ms in this
  // case, of data to crossfade.
  NewSegmentAppend("0K");
  CheckExpectedRangesByTimestamp("{ [0,2) }");
  NewSegmentAppend("1K");
  CheckExpectedRangesByTimestamp("{ [0,3) }");
  CheckExpectedBuffers("0K 1K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_Preroll) {
  SetAudioStream();
  Seek(0);
  NewSegmentAppend("0K 2K 4K 6K 8K 10K 12K");
  NewSegmentAppend("11P 13K 15K 17K");
  CheckExpectedBuffers("0K 2K 4K 6K 8K 10K 12K C 11P 13K 15K 17K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_PrerollFrame) {
  Seek(0);
  NewSegmentAppend("0K 3P 6K");
  CheckExpectedBuffers("0K 3P 6K");
  CheckNoNextBuffer();
}

// TODO(vrk): Add unit tests where keyframes are unaligned between streams.
// (crbug.com/133557)

// TODO(vrk): Add unit tests with end of stream being called at interesting
// times.

}  // namespace media
