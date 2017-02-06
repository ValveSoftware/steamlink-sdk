// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_stream.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "media/base/data_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "media/base/text_track_config.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/webvtt_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace media {

typedef StreamParser::BufferQueue BufferQueue;

static const int kDefaultFramesPerSecond = 30;
static const int kDefaultKeyframesPerSecond = 6;
static const uint8_t kDataA = 0x11;
static const uint8_t kDataB = 0x33;
static const int kDataSize = 1;

// Matchers for verifying common media log entry strings.
MATCHER(ContainsSameTimestampAt30MillisecondsLog, "") {
  return CONTAINS_STRING(arg,
                         "Detected an append sequence with keyframe following "
                         "a non-keyframe, both with the same decode timestamp "
                         "of 0.03");
}

MATCHER_P(ContainsTrackBufferExhaustionSkipLog, skip_milliseconds, "") {
  return CONTAINS_STRING(arg,
                         "Media append that overlapped current playback "
                         "position caused time gap in playing VIDEO stream "
                         "because the next keyframe is " +
                             base::IntToString(skip_milliseconds) +
                             "ms beyond last overlapped frame. Media may "
                             "appear temporarily frozen.");
}

MATCHER_P2(ContainsGeneratedSpliceLog,
           duration_microseconds,
           time_microseconds,
           "") {
  return CONTAINS_STRING(arg, "Generated splice of overlap duration " +
                                  base::IntToString(duration_microseconds) +
                                  "us into new buffer at " +
                                  base::IntToString(time_microseconds) + "us.");
}

class SourceBufferStreamTest : public testing::Test {
 protected:
  SourceBufferStreamTest() : media_log_(new StrictMock<MockMediaLog>()) {
    video_config_ = TestVideoConfig::Normal();
    SetStreamInfo(kDefaultFramesPerSecond, kDefaultKeyframesPerSecond);
    stream_.reset(new SourceBufferStream(video_config_, media_log_, true));
  }

  void SetMemoryLimit(size_t buffers_of_data) {
    stream_->set_memory_limit(buffers_of_data * kDataSize);
  }

  void SetStreamInfo(int frames_per_second, int keyframes_per_second) {
    frames_per_second_ = frames_per_second;
    keyframes_per_second_ = keyframes_per_second;
    frame_duration_ = ConvertToFrameDuration(frames_per_second);
  }

  void SetTextStream() {
    video_config_ = TestVideoConfig::Invalid();
    TextTrackConfig config(kTextSubtitles, "", "", "");
    stream_.reset(new SourceBufferStream(config, media_log_, true));
    SetStreamInfo(2, 2);
  }

  void SetAudioStream() {
    video_config_ = TestVideoConfig::Invalid();
    audio_config_.Initialize(kCodecVorbis, kSampleFormatPlanarF32,
                             CHANNEL_LAYOUT_STEREO, 1000, EmptyExtraData(),
                             Unencrypted(), base::TimeDelta(), 0);
    stream_.reset(new SourceBufferStream(audio_config_, media_log_, true));

    // Equivalent to 2ms per frame.
    SetStreamInfo(500, 500);
  }

  void NewCodedFrameGroupAppend(int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  base::TimeDelta(), true, &kDataA, kDataSize);
  }

  void NewCodedFrameGroupAppend(int starting_position,
                                int number_of_buffers,
                                const uint8_t* data) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  base::TimeDelta(), true, data, kDataSize);
  }

  void NewCodedFrameGroupAppend_OffsetFirstBuffer(
      int starting_position,
      int number_of_buffers,
      base::TimeDelta first_buffer_offset) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  first_buffer_offset, true, &kDataA, kDataSize);
  }

  void NewCodedFrameGroupAppend_ExpectFailure(int starting_position,
                                              int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  base::TimeDelta(), false, &kDataA, kDataSize);
  }

  void AppendBuffers(int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, false,
                  base::TimeDelta(), true, &kDataA, kDataSize);
  }

  void AppendBuffers(int starting_position,
                     int number_of_buffers,
                     const uint8_t* data) {
    AppendBuffers(starting_position, number_of_buffers, false,
                  base::TimeDelta(), true, data, kDataSize);
  }

  void NewCodedFrameGroupAppend(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, true, kNoTimestamp(), false, true);
  }

  void NewCodedFrameGroupAppend(base::TimeDelta start_timestamp,
                                const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, true, start_timestamp, false, true);
  }

  void AppendBuffers(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, false, kNoTimestamp(), false, true);
  }

  void NewCodedFrameGroupAppendOneByOne(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, true, kNoTimestamp(), true, true);
  }

  void AppendBuffersOneByOne(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, false, kNoTimestamp(), true, true);
  }

  void NewCodedFrameGroupAppend_ExpectFailure(
      const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, true, kNoTimestamp(), false, false);
  }

  void AppendBuffers_ExpectFailure(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, false, kNoTimestamp(), false, false);
  }

  void Seek(int position) {
    stream_->Seek(position * frame_duration_);
  }

  void SeekToTimestampMs(int64_t timestamp_ms) {
    stream_->Seek(base::TimeDelta::FromMilliseconds(timestamp_ms));
  }

  bool GarbageCollectWithPlaybackAtBuffer(int position, int newDataBuffers) {
    return stream_->GarbageCollectIfNeeded(
        DecodeTimestamp::FromPresentationTime(position * frame_duration_),
        newDataBuffers * kDataSize);
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

  void SignalStartOfCodedFrameGroup(base::TimeDelta start_timestamp) {
    stream_->OnStartOfCodedFrameGroup(
        DecodeTimestamp::FromPresentationTime(start_timestamp));
  }

  int GetRemovalRangeInMs(int start, int end, int bytes_to_free,
                          int* removal_end) {
    DecodeTimestamp removal_end_timestamp =
        DecodeTimestamp::FromMilliseconds(*removal_end);
    int bytes_removed = stream_->GetRemovalRange(
        DecodeTimestamp::FromMilliseconds(start),
        DecodeTimestamp::FromMilliseconds(end), bytes_to_free,
        &removal_end_timestamp);
    *removal_end = removal_end_timestamp.InMilliseconds();
    return bytes_removed;
  }

  void CheckExpectedRanges(const std::string& expected) {
    Ranges<base::TimeDelta> r = stream_->GetBufferedTime();

    std::stringstream ss;
    ss << "{ ";
    for (size_t i = 0; i < r.size(); ++i) {
      int64_t start = (r.start(i) / frame_duration_);
      int64_t end = (r.end(i) / frame_duration_) - 1;
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
      int64_t start = r.start(i).InMilliseconds();
      int64_t end = r.end(i).InMilliseconds();
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

  void CheckExpectedBuffers(int starting_position,
                            int ending_position,
                            const uint8_t* data) {
    CheckExpectedBuffers(starting_position, ending_position, false, data,
                         kDataSize);
  }

  void CheckExpectedBuffers(int starting_position,
                            int ending_position,
                            const uint8_t* data,
                            bool expect_keyframe) {
    CheckExpectedBuffers(starting_position, ending_position, expect_keyframe,
                         data, kDataSize);
  }

  void CheckExpectedBuffers(int starting_position,
                            int ending_position,
                            bool expect_keyframe,
                            const uint8_t* expected_data,
                            int expected_size) {
    int current_position = starting_position;
    for (; current_position <= ending_position; current_position++) {
      scoped_refptr<StreamParserBuffer> buffer;
      SourceBufferStream::Status status = stream_->GetNextBuffer(&buffer);

      EXPECT_NE(status, SourceBufferStream::kConfigChange);
      if (status != SourceBufferStream::kSuccess)
        break;

      if (expect_keyframe && current_position == starting_position)
        EXPECT_TRUE(buffer->is_key_frame());

      if (expected_data) {
        const uint8_t* actual_data = buffer->data();
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
    std::vector<std::string> timestamps = base::SplitString(
        expected, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
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

        EXPECT_EQ("C", timestamps[i]);

        ss << "C";
        continue;
      }

      EXPECT_EQ(SourceBufferStream::kSuccess, status);
      if (status != SourceBufferStream::kSuccess)
        break;

      ss << buffer->timestamp().InMilliseconds();

      if (buffer->GetDecodeTimestamp() !=
          DecodeTimestamp::FromPresentationTime(buffer->timestamp())) {
        ss << "|" << buffer->GetDecodeTimestamp().InMilliseconds();
      }

      // Check duration if expected timestamp contains it.
      if (timestamps[i].find('D') != std::string::npos) {
        ss << "D" << buffer->duration().InMilliseconds();
      }

      // Check duration estimation if expected timestamp contains it.
      if (timestamps[i].find('E') != std::string::npos &&
          buffer->is_duration_estimated()) {
        ss << "E";
      }

      // Handle preroll buffers.
      if (base::EndsWith(timestamps[i], "P", base::CompareCase::SENSITIVE)) {
        ASSERT_TRUE(buffer->is_key_frame());
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
        ASSERT_TRUE(buffer->is_key_frame());

        ss << "P";
      } else if (buffer->is_key_frame()) {
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

  void CheckEOSReached() {
    scoped_refptr<StreamParserBuffer> buffer;
    EXPECT_EQ(SourceBufferStream::kEndOfStream,
              stream_->GetNextBuffer(&buffer));
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

  base::TimeDelta frame_duration() const { return frame_duration_; }

  std::unique_ptr<SourceBufferStream> stream_;
  VideoDecoderConfig video_config_;
  AudioDecoderConfig audio_config_;
  scoped_refptr<StrictMock<MockMediaLog>> media_log_;

 private:
  base::TimeDelta ConvertToFrameDuration(int frames_per_second) {
    return base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond / frames_per_second);
  }

  void AppendBuffers(int starting_position,
                     int number_of_buffers,
                     bool begin_coded_frame_group,
                     base::TimeDelta first_buffer_offset,
                     bool expect_success,
                     const uint8_t* data,
                     int size) {
    if (begin_coded_frame_group)
      stream_->OnStartOfCodedFrameGroup(DecodeTimestamp::FromPresentationTime(
          starting_position * frame_duration_));

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
      buffer->SetDecodeTimestamp(
          DecodeTimestamp::FromPresentationTime(timestamp));

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
      buffer->set_duration(frame_duration_);

      queue.push_back(buffer);
    }
    if (!queue.empty())
      EXPECT_EQ(expect_success, stream_->Append(queue));
  }

  void UpdateLastBufferDuration(DecodeTimestamp current_dts,
                                BufferQueue* buffers) {
    if (buffers->empty() || buffers->back()->duration() > base::TimeDelta())
      return;

    DecodeTimestamp last_dts = buffers->back()->GetDecodeTimestamp();
    DCHECK(current_dts >= last_dts);
    buffers->back()->set_duration(current_dts - last_dts);
  }

  // StringToBufferQueue() allows for the generation of StreamParserBuffers from
  // coded strings of timestamps separated by spaces.
  //
  // Supported syntax (options must be in this order):
  // pp[|dd][Dzz][E][P][K]
  //
  // pp:
  // Generates a StreamParserBuffer with decode and presentation timestamp xx.
  // E.g., "0 1 2 3".
  //
  // pp|dd:
  // Generates a StreamParserBuffer with presentation timestamp pp and decode
  // timestamp dd. E.g., "0|0 3|1 1|2 2|3".
  //
  // Dzz
  // Explicitly describe the duration of the buffer. zz specifies the duration
  // in milliseconds. If the duration isn't specified with this syntax, the
  // duration is derived using the timestamp delta between this buffer and the
  // next buffer. If not specified, the final buffer will simply copy the
  // duration of the previous buffer. If the queue only contains 1 buffer then
  // the duration must be explicitly specified with this format.
  // E.g. "0D10 10D20"
  //
  // E:
  // Indicates that the buffer should be marked as containing an *estimated*
  // duration. E.g., "0D20E 20 25E 30"
  //
  // P:
  // Indicates the buffer with will also have a preroll buffer
  // associated with it. The preroll buffer will just be dummy data.
  // E.g. "0P 5 10"
  //
  // K:
  // Indicates the buffer is a keyframe. E.g., "0K 1|2K 2|4D2K 6 8".
  //
  // S(a# ... y# z#)
  // Indicates a splice frame buffer should be created with timestamp z#.  The
  // preceding timestamps a# ... y# will be treated as the fade out preroll for
  // the splice frame.  If a timestamp within the preroll ends with C the config
  // id to use for that and subsequent preroll appends is incremented by one.
  // The config id for non-splice frame appends will not be affected.
  BufferQueue StringToBufferQueue(const std::string& buffers_to_append) {
    std::vector<std::string> timestamps = base::SplitString(
        buffers_to_append, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    CHECK_GT(timestamps.size(), 0u);

    bool splice_frame = false;
    size_t splice_config_id = stream_->append_config_index_;
    BufferQueue pre_splice_buffers;
    BufferQueue buffers;
    for (size_t i = 0; i < timestamps.size(); i++) {
      bool is_keyframe = false;
      bool has_preroll = false;
      bool last_splice_frame = false;
      bool is_duration_estimated = false;

      // Handle splice frame starts.
      if (base::StartsWith(timestamps[i], "S(", base::CompareCase::SENSITIVE)) {
        CHECK(!splice_frame);
        splice_frame = true;
        // Remove the "S(" off of the token.
        timestamps[i] = timestamps[i].substr(2, timestamps[i].length());
      }
      if (splice_frame &&
          base::EndsWith(timestamps[i], ")", base::CompareCase::SENSITIVE)) {
        splice_frame = false;
        last_splice_frame = true;
        // Remove the ")" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }
      // Handle config changes within the splice frame.
      if (splice_frame &&
          base::EndsWith(timestamps[i], "C", base::CompareCase::SENSITIVE)) {
        splice_config_id++;
        CHECK(splice_config_id < stream_->audio_configs_.size() ||
              splice_config_id < stream_->video_configs_.size());
        // Remove the "C" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }
      if (base::EndsWith(timestamps[i], "K", base::CompareCase::SENSITIVE)) {
        is_keyframe = true;
        // Remove the "K" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }
      // Handle preroll buffers.
      if (base::EndsWith(timestamps[i], "P", base::CompareCase::SENSITIVE)) {
        is_keyframe = true;
        has_preroll = true;
        // Remove the "P" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }

      if (base::EndsWith(timestamps[i], "E", base::CompareCase::SENSITIVE)) {
        is_duration_estimated = true;
        // Remove the "E" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }

      int duration_in_ms = 0;
      size_t duration_pos = timestamps[i].find('D');
      if (duration_pos != std::string::npos) {
        CHECK(base::StringToInt(timestamps[i].substr(duration_pos + 1),
                                &duration_in_ms));
        timestamps[i] = timestamps[i].substr(0, duration_pos);
      }

      std::vector<std::string> buffer_timestamps = base::SplitString(
          timestamps[i], "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

      if (buffer_timestamps.size() == 1)
        buffer_timestamps.push_back(buffer_timestamps[0]);

      CHECK_EQ(2u, buffer_timestamps.size());

      int pts_in_ms = 0;
      int dts_in_ms = 0;
      CHECK(base::StringToInt(buffer_timestamps[0], &pts_in_ms));
      CHECK(base::StringToInt(buffer_timestamps[1], &dts_in_ms));

      // Create buffer. Buffer type and track ID are meaningless to these tests.
      scoped_refptr<StreamParserBuffer> buffer =
          StreamParserBuffer::CopyFrom(&kDataA, kDataSize, is_keyframe,
                                       DemuxerStream::AUDIO, 0);
      buffer->set_timestamp(base::TimeDelta::FromMilliseconds(pts_in_ms));
      buffer->set_is_duration_estimated(is_duration_estimated);

      if (dts_in_ms != pts_in_ms) {
        buffer->SetDecodeTimestamp(
            DecodeTimestamp::FromMilliseconds(dts_in_ms));
      }

      if (duration_in_ms)
        buffer->set_duration(base::TimeDelta::FromMilliseconds(duration_in_ms));

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
        // Make sure that splice frames aren't used with content where decode
        // and presentation timestamps can differ. (i.e., B-frames)
        CHECK_EQ(buffer->GetDecodeTimestamp().InMicroseconds(),
                 buffer->timestamp().InMicroseconds());
        if (!pre_splice_buffers.empty()) {
          // Enforce strictly monotonically increasing timestamps.
          CHECK_GT(
              buffer->timestamp().InMicroseconds(),
              pre_splice_buffers.back()->timestamp().InMicroseconds());
          CHECK_GT(
              buffer->GetDecodeTimestamp().InMicroseconds(),
              pre_splice_buffers.back()->GetDecodeTimestamp().InMicroseconds());
        }
        buffer->SetConfigId(splice_config_id);
        UpdateLastBufferDuration(buffer->GetDecodeTimestamp(),
                                 &pre_splice_buffers);
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

      UpdateLastBufferDuration(buffer->GetDecodeTimestamp(), &buffers);
      buffers.push_back(buffer);
    }

    // If the last buffer doesn't have a duration, assume it is the
    // same as the second to last buffer.
    if (buffers.size() >= 2 &&
        buffers.back()->duration() <= base::TimeDelta()) {
      buffers.back()->set_duration(
          buffers[buffers.size() - 2]->duration());
    }

    return buffers;
  }

  void AppendBuffers(const std::string& buffers_to_append,
                     bool start_new_coded_frame_group,
                     base::TimeDelta coded_frame_group_start_timestamp,
                     bool one_by_one,
                     bool expect_success) {
    BufferQueue buffers = StringToBufferQueue(buffers_to_append);

    if (start_new_coded_frame_group) {
      base::TimeDelta start_timestamp = coded_frame_group_start_timestamp;
      if (start_timestamp == kNoTimestamp())
        start_timestamp = buffers[0]->timestamp();

      ASSERT_TRUE(start_timestamp <= buffers[0]->timestamp());

      stream_->OnStartOfCodedFrameGroup(
          DecodeTimestamp::FromPresentationTime(start_timestamp));
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

  int frames_per_second_;
  int keyframes_per_second_;
  base::TimeDelta frame_duration_;
  DISALLOW_COPY_AND_ASSIGN(SourceBufferStreamTest);
};

TEST_F(SourceBufferStreamTest, Append_SingleRange) {
  // Append 15 buffers at positions 0 through 14.
  NewCodedFrameGroupAppend(0, 15);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 14);
}

TEST_F(SourceBufferStreamTest, Append_SingleRange_OneBufferAtATime) {
  // Append 15 buffers starting at position 0, one buffer at a time.
  NewCodedFrameGroupAppend(0, 1);
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
  NewCodedFrameGroupAppend(0, 5);

  // Append 10 buffers at positions 15 through 24.
  NewCodedFrameGroupAppend(15, 10);

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
  NewCodedFrameGroupAppend(0, 10);

  // Append 11 buffers at positions 15 through 25.
  NewCodedFrameGroupAppend(15, 11);

  // Append 5 buffers at positions 10 through 14 to bridge the gap.
  NewCodedFrameGroupAppend(10, 5);

  // Check expected range.
  CheckExpectedRanges("{ [0,25) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 25);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap) {
  // Append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5);

  // Append 15 buffers at positions 0 through 14.
  NewCodedFrameGroupAppend(0, 15);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 14);
}

TEST_F(SourceBufferStreamTest,
       Complete_Overlap_AfterGroupTimestampAndBeforeFirstBufferTimestamp) {
  // Append a coded frame group with a start timestamp of 0, but the first
  // buffer starts at 30ms. This can happen in muxed content where the
  // audio starts before the first frame.
  NewCodedFrameGroupAppend(base::TimeDelta::FromMilliseconds(0),
                           "30K 60K 90K 120K");

  CheckExpectedRangesByTimestamp("{ [0,150) }");

  // Completely overlap the old buffers, with a coded frame group that starts
  // after the old coded frame group start timestamp, but before the timestamp
  // of the first buffer in the coded frame group.
  NewCodedFrameGroupAppend("20K 50K 80K 110D10K");

  // Verify that the buffered ranges are updated properly and we don't crash.
  CheckExpectedRangesByTimestamp("{ [20,150) }");

  SeekToTimestampMs(20);
  CheckExpectedBuffers("20K 50K 80K 110K 120K");
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_EdgeCase) {
  // Make each frame a keyframe so that it's okay to overlap frames at any point
  // (instead of needing to respect keyframe boundaries).
  SetStreamInfo(30, 30);

  // Append 6 buffers at positions 6 through 11.
  NewCodedFrameGroupAppend(6, 6);

  // Append 8 buffers at positions 5 through 12.
  NewCodedFrameGroupAppend(5, 8);

  // Check expected range.
  CheckExpectedRanges("{ [5,12) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 12);
}

TEST_F(SourceBufferStreamTest, Start_Overlap) {
  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 5);

  // Append 6 buffers at positions 10 through 15.
  NewCodedFrameGroupAppend(10, 6);

  // Check expected range.
  CheckExpectedRanges("{ [5,15) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 15);
}

TEST_F(SourceBufferStreamTest, End_Overlap) {
  // Append 10 buffers at positions 10 through 19.
  NewCodedFrameGroupAppend(10, 10);

  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 10);

  // Check expected range.
  CheckExpectedRanges("{ [5,19) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 19);
}

TEST_F(SourceBufferStreamTest, End_Overlap_Several) {
  // Append 10 buffers at positions 10 through 19.
  NewCodedFrameGroupAppend(10, 10);

  // Append 8 buffers at positions 5 through 12.
  NewCodedFrameGroupAppend(5, 8);

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
  SeekToTimestampMs(0);

  NewCodedFrameGroupAppend("0K 30 60 90 120K 150");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  NewCodedFrameGroupAppend("0D30K");
  CheckExpectedRangesByTimestamp("{ [0,30) [120,180) }");

  CheckExpectedBuffers("0K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Several) {
  // Append 2 buffers at positions 5 through 6.
  NewCodedFrameGroupAppend(5, 2);

  // Append 2 buffers at positions 10 through 11.
  NewCodedFrameGroupAppend(10, 2);

  // Append 2 buffers at positions 15 through 16.
  NewCodedFrameGroupAppend(15, 2);

  // Check expected ranges.
  CheckExpectedRanges("{ [5,6) [10,11) [15,16) }");

  // Append buffers at positions 0 through 19.
  NewCodedFrameGroupAppend(0, 20);

  // Check expected range.
  CheckExpectedRanges("{ [0,19) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 19);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Several_Then_Merge) {
  // Append 2 buffers at positions 5 through 6.
  NewCodedFrameGroupAppend(5, 2);

  // Append 2 buffers at positions 10 through 11.
  NewCodedFrameGroupAppend(10, 2);

  // Append 2 buffers at positions 15 through 16.
  NewCodedFrameGroupAppend(15, 2);

  // Append 2 buffers at positions 20 through 21.
  NewCodedFrameGroupAppend(20, 2);

  // Append buffers at positions 0 through 19.
  NewCodedFrameGroupAppend(0, 20);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,21) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 21);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected) {
  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to buffer at position 5.
  Seek(5);

  // Replace old data with new data.
  NewCodedFrameGroupAppend(5, 10, &kDataB);

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
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Do a complete overlap by appending 20 buffers at positions 0 through 19.
  NewCodedFrameGroupAppend(0, 20, &kDataB);

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
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  NewCodedFrameGroupAppend(5, 10, &kDataB);

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
  static const uint8_t kDataC = 0x55;
  static const uint8_t kDataD = 0x77;

  // Append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  NewCodedFrameGroupAppend(5, 5, &kDataB);

  // Then replace it again with different data.
  NewCodedFrameGroupAppend(5, 5, &kDataC);

  // Now append 5 new buffers at positions 10 through 14.
  NewCodedFrameGroupAppend(10, 5, &kDataC);

  // Now replace all the data entirely.
  NewCodedFrameGroupAppend(5, 10, &kDataD);

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
  NewCodedFrameGroupAppend(0, 10, &kDataA);

  // Seek to position 5, then add buffers to overlap data at that position.
  Seek(5);
  NewCodedFrameGroupAppend(5, 10, &kDataB);

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
  NewCodedFrameGroupAppend(0, 15, &kDataA);

  // Seek to 10 and get buffer.
  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now append 10 buffers of new data at positions 10 through 19.
  NewCodedFrameGroupAppend(10, 10, &kDataB);

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
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now replace the last 5 buffers with new data.
  NewCodedFrameGroupAppend(10, 5, &kDataB);

  // The next 4 buffers should be the origial data, held in the track buffer.
  CheckExpectedBuffers(11, 14, &kDataA);

  // The next buffer is at position 15, so we should fail to fulfill the
  // request.
  CheckNoNextBuffer();

  // Now append data at 15 through 19 and check to make sure it's correct.
  NewCodedFrameGroupAppend(15, 5, &kDataB);
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
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to position 5.
  Seek(5);

  // Now append 10 buffers at positions 0 through 9.
  NewCodedFrameGroupAppend(0, 10, &kDataB);

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
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to position 10, then move to position 13.
  Seek(10);
  CheckExpectedBuffers(10, 12, &kDataA);

  // Now append 10 buffers at positions 0 through 9.
  NewCodedFrameGroupAppend(0, 10, &kDataB);

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
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to position 10, then move to position 13.
  Seek(10);
  CheckExpectedBuffers(10, 12, &kDataA);

  // Now append 8 buffers at positions 0 through 7.
  NewCodedFrameGroupAppend(0, 8, &kDataB);

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
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to position 5, then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 8 buffers at positions 0 through 7.
  NewCodedFrameGroupAppend(0, 8, &kDataB);

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
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to position 5, then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 10 buffers at positions 0 through 9.
  NewCodedFrameGroupAppend(0, 10, &kDataB);

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
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 7 buffers at positions 0 through 6.
  NewCodedFrameGroupAppend(0, 7, &kDataB);

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
  NewCodedFrameGroupAppend(5, 15, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 13 buffers at positions 0 through 12.
  NewCodedFrameGroupAppend(0, 13, &kDataB);

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
  NewCodedFrameGroupAppend(5, 5, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 6 buffers at positions 0 through 5.
  NewCodedFrameGroupAppend(0, 6, &kDataB);

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
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(133));

  // Append 7 buffers at positions 10 through 16.
  NewCodedFrameGroupAppend(10, 7, &kDataA);

  // Seek to position 15, then move to position 16.
  Seek(15);
  CheckExpectedBuffers(15, 15, &kDataA);

  // Now append 11 buffers at positions 5 through 15.
  NewCodedFrameGroupAppend(5, 11, &kDataB);
  CheckExpectedRanges("{ [5,15) }");

  // Now do another end-overlap to split the range into two parts, where the
  // 2nd range should have the next buffer position.
  NewCodedFrameGroupAppend(0, 6, &kDataA);
  CheckExpectedRanges("{ [0,5) [10,15) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(16, 16, &kDataA);

  // Now there's no data to fulfill the request.
  CheckNoNextBuffer();

  // Add data to the 2nd range, should not be able to fulfill the next read
  // until we've added a keyframe.
  NewCodedFrameGroupAppend(15, 1, &kDataB);
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
  NewCodedFrameGroupAppend(5, 5, &kDataA);

  // Append 5 buffers at positions 15 through 19.
  NewCodedFrameGroupAppend(15, 5, &kDataA);

  // Check expected range.
  CheckExpectedRanges("{ [5,9) [15,19) }");

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 6 buffers at positions 0 through 5.
  NewCodedFrameGroupAppend(0, 6, &kDataB);

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
  NewCodedFrameGroupAppend(0, 15, &kDataA);

  // Seek to position 5.
  Seek(5);

  // Now append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5, &kDataB);

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
  NewCodedFrameGroupAppend(0, 15, &kDataA);

  // Seek to 10 then move to position 11.
  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5, &kDataB);

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
  NewCodedFrameGroupAppend(0, 15, &kDataA);

  // Seek to beginning then move to position 2.
  Seek(0);
  CheckExpectedBuffers(0, 1, &kDataA);

  // Now append 3 buffers at positions 5 through 7.
  NewCodedFrameGroupAppend(5, 3, &kDataB);

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
  NewCodedFrameGroupAppend(0, 15, &kDataA);

  // Seek to 5 then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 3 buffers at positions 5 through 7.
  NewCodedFrameGroupAppend(5, 3, &kDataB);

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
  NewCodedFrameGroupAppendOneByOne("10K 40 70 100 130");

  // The range ends at 160, accounting for the last buffer's duration.
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Overlap with 10 buffers starting at the beginning, appended one at a
  // time.
  NewCodedFrameGroupAppend(0, 1, &kDataB);
  for (int i = 1; i < 10; i++)
    AppendBuffers(i, 1, &kDataB);

  // All data should be replaced.
  Seek(0);
  CheckExpectedRanges("{ [0,9) }");
  CheckExpectedBuffers(0, 9, &kDataB);
}

TEST_F(SourceBufferStreamTest, Overlap_OneByOne_DeleteGroup) {
  NewCodedFrameGroupAppendOneByOne("10K 40 70 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 130ms.
  SeekToTimestampMs(130);

  // Overlap with a new coded frame group from 0 to 130ms.
  NewCodedFrameGroupAppendOneByOne("0K 120D10");

  // Next buffer should still be 130ms.
  CheckExpectedBuffers("130K");

  // Check the final buffers is correct.
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 120 130K");
}

TEST_F(SourceBufferStreamTest, Overlap_OneByOne_BetweenCodedFrameGroups) {
  // Append 5 buffers starting at 110ms, 30ms apart.
  NewCodedFrameGroupAppendOneByOne("110K 140 170 200 230");
  CheckExpectedRangesByTimestamp("{ [110,260) }");

  // Now append 2 coded frame groups from 0ms to 210ms, 30ms apart. Note that
  // the
  // old keyframe 110ms falls in between these two groups.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90");
  NewCodedFrameGroupAppendOneByOne("120K 150 180 210");
  CheckExpectedRangesByTimestamp("{ [0,240) }");

  // Check the final buffers is correct; the keyframe at 110ms should be
  // deleted.
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 30 60 90 120K 150 180 210");
}

// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer) {
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(50));

  NewCodedFrameGroupAppendOneByOne("10K 40 70 100K 125 130D30K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestampMs(70);
  CheckExpectedBuffers("10K 40");

  // Overlap with a new coded frame group from 0 to 130ms.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90 120D10K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Should return frame 70ms from the track buffer, then switch
  // to the new data at 120K, then switch back to the old data at 130K. The
  // frame at 125ms that depended on keyframe 100ms should have been deleted.
  CheckExpectedBuffers("70 120K 130K");

  // Check the final result: should not include data from the track buffer.
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 30 60 90 120K 130K");
}

// Overlap the next keyframe after the end of the track buffer with a new
// keyframe.
// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
// track:             70
// new  :                     110K    130
// after: 0K   30   60   90  *110K*   130
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer2) {
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(40));

  NewCodedFrameGroupAppendOneByOne("10K 40 70 100K 125 130D30K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestampMs(70);
  CheckExpectedBuffers("10K 40");

  // Overlap with a new coded frame group from 0 to 120ms; 70ms and 100ms go in
  // track
  // buffer.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90 120D10K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Now overlap the keyframe at 120ms.
  NewCodedFrameGroupAppendOneByOne("110K 130");

  // Should return frame 70ms from the track buffer. Then it should
  // return the keyframe after the track buffer, which is at 110ms.
  CheckExpectedBuffers("70 110K 130");
}

// Overlap the next keyframe after the end of the track buffer without a
// new keyframe.
// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
// track:             70
// new  :        50K   80   110          140
// after: 0K   30   50K   80   110   140 * (waiting for keyframe)
// track:             70
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer3) {
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(80));

  NewCodedFrameGroupAppendOneByOne("10K 40 70 100K 125 130D30K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestampMs(70);
  CheckExpectedBuffers("10K 40");

  // Overlap with a new coded frame group from 0 to 120ms; 70ms goes in track
  // buffer.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90 120D10K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Now overlap the keyframe at 120ms and 130ms.
  NewCodedFrameGroupAppendOneByOne("50K 80 110 140");
  CheckExpectedRangesByTimestamp("{ [0,170) }");

  // Should have all the buffers from the track buffer, then stall.
  CheckExpectedBuffers("70");
  CheckNoNextBuffer();

  // Appending a keyframe should fulfill the read.
  AppendBuffersOneByOne("150D30K");
  CheckExpectedBuffers("150K");
  CheckNoNextBuffer();
}

// Overlap the next keyframe after the end of the track buffer with a keyframe
// that comes before the end of the track buffer.
// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
// track:             70
// new  :              80K  110          140
// after: 0K   30   60   *80K*  110   140
// track:               70
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer4) {
  NewCodedFrameGroupAppendOneByOne("10K 40 70 100K 125 130D30K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestampMs(70);
  CheckExpectedBuffers("10K 40");

  // Overlap with a new coded frame group from 0 to 120ms; 70ms and 100ms go in
  // track
  // buffer.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90 120D10K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Now append a keyframe at 80ms.
  NewCodedFrameGroupAppendOneByOne("80K 110 140");

  CheckExpectedBuffers("70 80K 110 140");
  CheckNoNextBuffer();
}

// Overlap the next keyframe after the end of the track buffer with a keyframe
// that comes before the end of the track buffer, when the selected stream was
// waiting for the next keyframe.
// old  :   10K  40  *70*  100K
// new  : 0K   30   60   90   120
// after: 0K   30   60   90   120 * (waiting for keyframe)
// track:             70
// new  :              80K  110          140
// after: 0K   30   60   *80K*  110   140
// track:               70
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer5) {
  NewCodedFrameGroupAppendOneByOne("10K 40 70 100K");
  CheckExpectedRangesByTimestamp("{ [10,130) }");

  // Seek to 70ms.
  SeekToTimestampMs(70);
  CheckExpectedBuffers("10K 40");

  // Overlap with a new coded frame group from 0 to 120ms; 70ms goes in track
  // buffer.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90 120");
  CheckExpectedRangesByTimestamp("{ [0,150) }");

  // Now append a keyframe at 80ms.
  NewCodedFrameGroupAppendOneByOne("80K 110 140");

  CheckExpectedBuffers("70 80K 110 140");
  CheckNoNextBuffer();
}

// Test that appending to a different range while there is data in
// the track buffer doesn't affect the selected range or track buffer state.
// old  :   10K  40  *70*  100K  125  130K ... 200K 230
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K ... 200K 230
// track:             70
// old  : 0K   30   60   90  *120K*   130K ... 200K 230
// new  :                                               260K 290
// after: 0K   30   60   90  *120K*   130K ... 200K 230 260K 290
// track:             70
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer6) {
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(50));

  NewCodedFrameGroupAppendOneByOne("10K 40 70 100K 125 130D30K");
  NewCodedFrameGroupAppendOneByOne("200K 230");
  CheckExpectedRangesByTimestamp("{ [10,160) [200,260) }");

  // Seek to 70ms.
  SeekToTimestampMs(70);
  CheckExpectedBuffers("10K 40");

  // Overlap with a new coded frame group from 0 to 120ms.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90 120D10K");
  CheckExpectedRangesByTimestamp("{ [0,160) [200,260) }");

  // Verify that 70 gets read out of the track buffer.
  CheckExpectedBuffers("70");

  // Append more data to the unselected range.
  NewCodedFrameGroupAppendOneByOne("260K 290");
  CheckExpectedRangesByTimestamp("{ [0,160) [200,320) }");

  CheckExpectedBuffers("120K 130K");
  CheckNoNextBuffer();

  // Check the final result: should not include data from the track buffer.
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 30 60 90 120K 130K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Seek_Keyframe) {
  // Append 6 buffers at positions 0 through 5.
  NewCodedFrameGroupAppend(0, 6);

  // Seek to beginning.
  Seek(0);
  CheckExpectedBuffers(0, 5, true);
}

TEST_F(SourceBufferStreamTest, Seek_NonKeyframe) {
  // Append 15 buffers at positions 0 through 14.
  NewCodedFrameGroupAppend(0, 15);

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
  NewCodedFrameGroupAppend(0, 2);
  Seek(0);
  CheckExpectedBuffers(0, 1);

  // Try to get buffer out of range.
  Seek(2);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Seek_InBetweenTimestamps) {
  // Append 10 buffers at positions 0 through 9.
  NewCodedFrameGroupAppend(0, 10);

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
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Do a complete overlap by appending 20 buffers at positions 0 through 19.
  NewCodedFrameGroupAppend(0, 20, &kDataB);

  // Check range is correct.
  CheckExpectedRanges("{ [0,19) }");

  // Seek to beginning; all data should be new.
  Seek(0);
  CheckExpectedBuffers(0, 19, &kDataB);

  // Check range continues to be correct.
  CheckExpectedRanges("{ [0,19) }");
}

TEST_F(SourceBufferStreamTest, Seek_StartOfGroup) {
  base::TimeDelta bump = frame_duration() / 4;
  CHECK(bump > base::TimeDelta());

  // Append 5 buffers at position (5 + |bump|) through 9, where the coded frame
  // group begins at position 5.
  Seek(5);
  NewCodedFrameGroupAppend_OffsetFirstBuffer(5, 5, bump);
  scoped_refptr<StreamParserBuffer> buffer;

  // GetNextBuffer() should return the next buffer at position (5 + |bump|).
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kSuccess);
  EXPECT_EQ(buffer->GetDecodeTimestamp(),
            DecodeTimestamp::FromPresentationTime(5 * frame_duration() + bump));

  // Check rest of buffers.
  CheckExpectedBuffers(6, 9);

  // Seek to position 15.
  Seek(15);

  // Append 5 buffers at positions (15 + |bump|) through 19, where the coded
  // frame group begins at 15.
  NewCodedFrameGroupAppend_OffsetFirstBuffer(15, 5, bump);

  // GetNextBuffer() should return the next buffer at position (15 + |bump|).
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kSuccess);
  EXPECT_EQ(buffer->GetDecodeTimestamp(), DecodeTimestamp::FromPresentationTime(
      15 * frame_duration() + bump));

  // Check rest of buffers.
  CheckExpectedBuffers(16, 19);
}

TEST_F(SourceBufferStreamTest, Seek_BeforeStartOfGroup) {
  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 10);

  // Seek to a time before the first buffer in the range.
  Seek(0);

  // Should return buffers from the beginning of the range.
  CheckExpectedBuffers(5, 14);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_CompleteOverlap) {
  // Append 5 buffers at positions 0 through 4.
  NewCodedFrameGroupAppend(0, 4);

  // Append 5 buffers at positions 10 through 14, and seek to the beginning of
  // this range.
  NewCodedFrameGroupAppend(10, 5);
  Seek(10);

  // Now seek to the beginning of the first range.
  Seek(0);

  // Completely overlap the old seek point.
  NewCodedFrameGroupAppend(5, 15);

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_CompleteOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewCodedFrameGroupAppend(0, 2);

  // Append 5 buffers at positions 15 through 19 and seek to beginning of the
  // range.
  NewCodedFrameGroupAppend(15, 5);
  Seek(15);

  // Now seek position 5.
  Seek(5);

  // Completely overlap the old seek point.
  NewCodedFrameGroupAppend(10, 15);

  // The seek at position 5 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_MiddleOverlap) {
  // Append 2 buffers at positions 0 through 1.
  NewCodedFrameGroupAppend(0, 2);

  // Append 15 buffers at positions 5 through 19 and seek to position 15.
  NewCodedFrameGroupAppend(5, 15);
  Seek(15);

  // Now seek to the beginning of the stream.
  Seek(0);

  // Overlap the middle of the range such that there are now three ranges.
  NewCodedFrameGroupAppend(10, 3);
  CheckExpectedRanges("{ [0,1) [5,12) [15,19) }");

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_MiddleOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewCodedFrameGroupAppend(0, 2);

  // Append 15 buffers at positions 10 through 24 and seek to position 20.
  NewCodedFrameGroupAppend(10, 15);
  Seek(20);

  // Now seek to position 5.
  Seek(5);

  // Overlap the middle of the range such that it is now split into two ranges.
  NewCodedFrameGroupAppend(15, 3);
  CheckExpectedRanges("{ [0,1) [10,17) [20,24) }");

  // The seek at position 5 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_StartOverlap) {
  // Append 2 buffers at positions 0 through 1.
  NewCodedFrameGroupAppend(0, 2);

  // Append 15 buffers at positions 5 through 19 and seek to position 15.
  NewCodedFrameGroupAppend(5, 15);
  Seek(15);

  // Now seek to the beginning of the stream.
  Seek(0);

  // Start overlap the old seek point.
  NewCodedFrameGroupAppend(10, 10);

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_StartOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewCodedFrameGroupAppend(0, 2);

  // Append 15 buffers at positions 10 through 24 and seek to position 20.
  NewCodedFrameGroupAppend(10, 15);
  Seek(20);

  // Now seek to position 5.
  Seek(5);

  // Start overlap the old seek point.
  NewCodedFrameGroupAppend(15, 10);

  // The seek at time 0 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_EndOverlap) {
  // Append 5 buffers at positions 0 through 4.
  NewCodedFrameGroupAppend(0, 4);

  // Append 15 buffers at positions 10 through 24 and seek to start of range.
  NewCodedFrameGroupAppend(10, 15);
  Seek(10);

  // Now seek to the beginning of the stream.
  Seek(0);

  // End overlap the old seek point.
  NewCodedFrameGroupAppend(5, 10);

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_EndOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewCodedFrameGroupAppend(0, 2);

  // Append 15 buffers at positions 15 through 29 and seek to start of range.
  NewCodedFrameGroupAppend(15, 15);
  Seek(15);

  // Now seek to position 5
  Seek(5);

  // End overlap the old seek point.
  NewCodedFrameGroupAppend(10, 10);

  // The seek at time 0 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GetNextBuffer_AfterMerges) {
  // Append 5 buffers at positions 10 through 14.
  NewCodedFrameGroupAppend(10, 5);

  // Seek to buffer at position 12.
  Seek(12);

  // Append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5);

  // Make sure ranges are merged.
  CheckExpectedRanges("{ [5,14) }");

  // Make sure the next buffer is correct.
  CheckExpectedBuffers(10, 10);

  // Append 5 buffers at positions 15 through 19.
  NewCodedFrameGroupAppend(15, 5);
  CheckExpectedRanges("{ [5,19) }");

  // Make sure the remaining next buffers are correct.
  CheckExpectedBuffers(11, 14);
}

TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenAppend) {
  // Append 4 buffers at positions 0 through 3.
  NewCodedFrameGroupAppend(0, 4);

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
  NewCodedFrameGroupAppend(0, 10, &kDataA);
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataA);

  // Next buffer is at position 10, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Append 6 buffers at positons 5 through 10. This is to test that doing a
  // start-overlap successfully fulfills the read at position 10, even though
  // position 10 was unbuffered.
  NewCodedFrameGroupAppend(5, 6, &kDataB);
  CheckExpectedBuffers(10, 10, &kDataB);

  // Then add 5 buffers from positions 11 though 15.
  AppendBuffers(11, 5, &kDataB);

  // Check the next 4 buffers are correct, which also effectively seeks to
  // position 15.
  CheckExpectedBuffers(11, 14, &kDataB);

  // Replace the next buffer at position 15 with another start overlap.
  NewCodedFrameGroupAppend(15, 2, &kDataA);
  CheckExpectedBuffers(15, 16, &kDataA);
}

// Tests a start overlap that occurs right at the timestamp of the last output
// buffer that was returned by GetNextBuffer(). This test verifies that
// GetNextBuffer() skips to second GOP in the newly appended data instead
// of returning two buffers with the same timestamp.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenStartOverlap2) {
  NewCodedFrameGroupAppend("0K 30 60 90 120");

  Seek(0);
  CheckExpectedBuffers("0K 30 60 90 120");
  CheckNoNextBuffer();

  // Append a keyframe with the same timestamp as the last buffer output.
  NewCodedFrameGroupAppend("120D30K");
  CheckNoNextBuffer();

  // Append the rest of the coded frame group and make sure that buffers are
  // returned from the first GOP after 120.
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
  NewCodedFrameGroupAppend(10, 5, &kDataA);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);

  // Next buffer is at position 15, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Do a complete overlap and test that this successfully fulfills the read
  // at position 15.
  NewCodedFrameGroupAppend(5, 11, &kDataB);
  CheckExpectedBuffers(15, 15, &kDataB);

  // Then add 5 buffers from positions 16 though 20.
  AppendBuffers(16, 5, &kDataB);

  // Check the next 4 buffers are correct, which also effectively seeks to
  // position 20.
  CheckExpectedBuffers(16, 19, &kDataB);

  // Do a complete overlap and replace the buffer at position 20.
  NewCodedFrameGroupAppend(0, 21, &kDataA);
  CheckExpectedBuffers(20, 20, &kDataA);
}

// This test covers the case where a range is stalled waiting for its next
// buffer, then an end-overlap causes the end of the range to be deleted.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenEndOverlap) {
  // Append 5 buffers at positions 10 through 14 and exhaust the buffers.
  NewCodedFrameGroupAppend(10, 5, &kDataA);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckExpectedRanges("{ [10,14) }");

  // Next buffer is at position 15, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Do an end overlap that causes the latter half of the range to be deleted.
  NewCodedFrameGroupAppend(5, 6, &kDataB);
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
  NewCodedFrameGroupAppend(5, 5, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  NewCodedFrameGroupAppend(5, 5, &kDataB);

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
  NewCodedFrameGroupAppend(0, 20);
  Seek(0);

  int last_keyframe_idx = -1;
  base::TimeDelta last_keyframe_presentation_timestamp;
  base::TimeDelta last_p_frame_presentation_timestamp;

  // Check for IBB...BBP pattern.
  for (int i = 0; i < 20; i++) {
    scoped_refptr<StreamParserBuffer> buffer;
    ASSERT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kSuccess);

    if (buffer->is_key_frame()) {
      EXPECT_EQ(DecodeTimestamp::FromPresentationTime(buffer->timestamp()),
                buffer->GetDecodeTimestamp());
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
      EXPECT_LT(DecodeTimestamp::FromPresentationTime(buffer->timestamp()),
                buffer->GetDecodeTimestamp());
    }
  }
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteFront) {
  // Set memory limit to 20 buffers.
  SetMemoryLimit(20);

  // Append 20 buffers at positions 0 through 19.
  NewCodedFrameGroupAppend(0, 1, &kDataA);
  for (int i = 1; i < 20; i++)
    AppendBuffers(i, 1, &kDataA);

  // GC should be a no-op, since we are just under memory limit.
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(0, 0));
  CheckExpectedRanges("{ [0,19) }");
  Seek(0);
  CheckExpectedBuffers(0, 19, &kDataA);

  // Seek to the middle of the stream.
  Seek(10);

  // We are about to append 5 new buffers and current playback position is 10,
  // so the GC algorithm should be able to delete some old data from the front.
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(10, 5));
  CheckExpectedRanges("{ [5,19) }");

  // Append 5 buffers to the end of the stream.
  AppendBuffers(20, 5, &kDataA);
  CheckExpectedRanges("{ [5,24) }");

  CheckExpectedBuffers(10, 24, &kDataA);
  Seek(5);
  CheckExpectedBuffers(5, 9, &kDataA);
}

TEST_F(SourceBufferStreamTest,
       GarbageCollection_DeleteFront_PreserveSeekedGOP) {
  // Set memory limit to 15 buffers.
  SetMemoryLimit(15);

  NewCodedFrameGroupAppend("0K 10 20 30 40 50K 60 70 80 90");
  NewCodedFrameGroupAppend("1000K 1010 1020 1030 1040");

  // GC should be a no-op, since we are just under memory limit.
  EXPECT_TRUE(stream_->GarbageCollectIfNeeded(DecodeTimestamp(), 0));
  CheckExpectedRangesByTimestamp("{ [0,100) [1000,1050) }");

  // Seek to the near the end of the first range
  SeekToTimestampMs(95);

  // We are about to append 7 new buffers and current playback position is at
  // the end of the last GOP in the first range, so the GC algorithm should be
  // able to delete some old data from the front, but must not collect the last
  // GOP in that first range. Neither can it collect the last appended GOP
  // (which is the entire second range), so GC should return false since it
  // couldn't collect enough.
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(95), 7));
  CheckExpectedRangesByTimestamp("{ [50,100) [1000,1050) }");
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteFrontGOPsAtATime) {
  // Set memory limit to 20 buffers.
  SetMemoryLimit(20);

  // Append 20 buffers at positions 0 through 19.
  NewCodedFrameGroupAppend(0, 20, &kDataA);

  // Seek to position 10.
  Seek(10);
  CheckExpectedRanges("{ [0,19) }");

  // Add one buffer to put the memory over the cap.
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(10, 1));
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

  // Append 5 buffers at positions 15 through 19.
  NewCodedFrameGroupAppend(15, 5, &kDataA);
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(0, 0));

  // Append 5 buffers at positions 0 through 4.
  NewCodedFrameGroupAppend(0, 5, &kDataA);
  CheckExpectedRanges("{ [0,4) [15,19) }");

  // Seek to position 0.
  Seek(0);
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(0, 0));
  // Should leave the first 5 buffers from 0 to 4.
  CheckExpectedRanges("{ [0,4) }");
  CheckExpectedBuffers(0, 4, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteFrontAndBack) {
  // Set memory limit to 3 buffers.
  SetMemoryLimit(3);

  // Seek to position 15.
  Seek(15);

  // Append 40 buffers at positions 0 through 39.
  NewCodedFrameGroupAppend(0, 40, &kDataA);
  // GC will try to keep data between current playback position and last append
  // position. This will ensure that the last append position is 19 and will
  // allow GC algorithm to collect data outside of the range [15,19)
  NewCodedFrameGroupAppend(15, 5, &kDataA);
  CheckExpectedRanges("{ [0,39) }");

  // Should leave the GOP containing the current playback position 15 and the
  // last append position 19. GC returns false, since we are still above limit.
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(15, 0));
  CheckExpectedRanges("{ [15,19) }");
  CheckExpectedBuffers(15, 19, &kDataA);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteSeveralRanges) {
  // Append 5 buffers at positions 0 through 4.
  NewCodedFrameGroupAppend(0, 5);

  // Append 5 buffers at positions 10 through 14.
  NewCodedFrameGroupAppend(10, 5);

  // Append 5 buffers at positions 20 through 24.
  NewCodedFrameGroupAppend(20, 5);

  // Append 5 buffers at positions 40 through 44.
  NewCodedFrameGroupAppend(40, 5);

  CheckExpectedRanges("{ [0,4) [10,14) [20,24) [40,44) }");

  // Seek to position 20.
  Seek(20);
  CheckExpectedBuffers(20, 20);

  // Set memory limit to 1 buffer.
  SetMemoryLimit(1);

  // Append 5 buffers at positions 30 through 34.
  NewCodedFrameGroupAppend(30, 5);

  // We will have more than 1 buffer left, GC will fail
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(20, 0));

  // Should have deleted all buffer ranges before the current buffer and after
  // last GOP
  CheckExpectedRanges("{ [20,24) [30,34) }");
  CheckExpectedBuffers(21, 24);
  CheckNoNextBuffer();

  // Continue appending into the last range to make sure it didn't break.
  AppendBuffers(35, 10);
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(20, 0));
  // Should save everything between read head and last appended
  CheckExpectedRanges("{ [20,24) [30,44) }");

  // Make sure appending before and after the ranges didn't somehow break.
  SetMemoryLimit(100);
  NewCodedFrameGroupAppend(0, 10);
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(20, 0));
  CheckExpectedRanges("{ [0,9) [20,24) [30,44) }");
  Seek(0);
  CheckExpectedBuffers(0, 9);

  NewCodedFrameGroupAppend(90, 10);
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(0, 0));
  CheckExpectedRanges("{ [0,9) [20,24) [30,44) [90,99) }");
  Seek(30);
  CheckExpectedBuffers(30, 44);
  CheckNoNextBuffer();
  Seek(90);
  CheckExpectedBuffers(90, 99);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteAfterLastAppend) {
  // Set memory limit to 10 buffers.
  SetMemoryLimit(10);

  // Append 1 GOP starting at 310ms, 30ms apart.
  NewCodedFrameGroupAppend("310K 340 370");

  // Append 2 GOPs starting at 490ms, 30ms apart.
  NewCodedFrameGroupAppend("490K 520 550 580K 610 640");

  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(0, 0));

  CheckExpectedRangesByTimestamp("{ [310,400) [490,670) }");

  // Seek to the GOP at 580ms.
  SeekToTimestampMs(580);

  // Append 2 GOPs before the existing ranges.
  // So the ranges before GC are "{ [100,280) [310,400) [490,670) }".
  NewCodedFrameGroupAppend("100K 130 160 190K 220 250K");

  EXPECT_TRUE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(580), 0));

  // Should save the newly appended GOPs.
  CheckExpectedRangesByTimestamp("{ [100,280) [580,670) }");
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteAfterLastAppendMerged) {
  // Set memory limit to 10 buffers.
  SetMemoryLimit(10);

  // Append 3 GOPs starting at 400ms, 30ms apart.
  NewCodedFrameGroupAppend("400K 430 460 490K 520 550 580K 610 640");

  // Seek to the GOP at 580ms.
  SeekToTimestampMs(580);

  // Append 2 GOPs starting at 220ms, and they will be merged with the existing
  // range.  So the range before GC is "{ [220,670) }".
  NewCodedFrameGroupAppend("220K 250 280 310K 340 370");

  EXPECT_TRUE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(580), 0));

  // Should save the newly appended GOPs.
  CheckExpectedRangesByTimestamp("{ [220,400) [580,670) }");
}

TEST_F(SourceBufferStreamTest, GarbageCollection_NoSeek) {
  // Set memory limit to 20 buffers.
  SetMemoryLimit(20);

  // Append 25 buffers at positions 0 through 24.
  NewCodedFrameGroupAppend(0, 25, &kDataA);

  // If playback is still in the first GOP (starting at 0), GC should fail.
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(2, 0));
  CheckExpectedRanges("{ [0,24) }");

  // As soon as playback position moves past the first GOP, it should be removed
  // and after removing the first GOP we are under memory limit.
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(5, 0));
  CheckExpectedRanges("{ [5,24) }");
  CheckNoNextBuffer();
  Seek(5);
  CheckExpectedBuffers(5, 24, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_PendingSeek) {
  // Append 10 buffers at positions 0 through 9.
  NewCodedFrameGroupAppend(0, 10, &kDataA);

  // Append 5 buffers at positions 25 through 29.
  NewCodedFrameGroupAppend(25, 5, &kDataA);

  // Seek to position 15.
  Seek(15);
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [0,9) [25,29) }");

  // Set memory limit to 5 buffers.
  SetMemoryLimit(5);

  // Append 5 buffers as positions 30 to 34 to trigger GC.
  AppendBuffers(30, 5, &kDataA);

  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(30, 0));

  // The current algorithm will delete from the beginning until the memory is
  // under cap.
  CheckExpectedRanges("{ [30,34) }");

  // Expand memory limit again so that GC won't be triggered.
  SetMemoryLimit(100);

  // Append data to fulfill seek.
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(30, 5));
  NewCodedFrameGroupAppend(15, 5, &kDataA);

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
  NewCodedFrameGroupAppend(0, 10, &kDataA);

  // Advance next buffer position to 10.
  Seek(0);
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(0, 0));
  CheckExpectedBuffers(0, 9, &kDataA);
  CheckNoNextBuffer();

  // Append 20 buffers at positions 15 through 34.
  NewCodedFrameGroupAppend(15, 20, &kDataA);
  CheckExpectedRanges("{ [0,9) [15,34) }");

  // GC should save the keyframe before the next buffer position and the data
  // closest to the next buffer position. It will also save all buffers from
  // next buffer to the last GOP appended, which overflows limit and leads to
  // failure.
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(5, 0));
  CheckExpectedRanges("{ [5,9) [15,34) }");

  // Now fulfill the seek at position 10. This will make GC delete the data
  // before position 10 to keep it within cap.
  NewCodedFrameGroupAppend(10, 5, &kDataA);
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(10, 0));
  CheckExpectedRanges("{ [10,24) }");
  CheckExpectedBuffers(10, 24, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_TrackBuffer) {
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(99));

  // Set memory limit to 3 buffers.
  SetMemoryLimit(3);

  // Seek to position 15.
  Seek(15);

  // Append 18 buffers at positions 0 through 17.
  NewCodedFrameGroupAppend(0, 18, &kDataA);

  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(15, 0));

  // Should leave GOP containing seek position.
  CheckExpectedRanges("{ [15,17) }");

  // Move next buffer position to 16.
  CheckExpectedBuffers(15, 15, &kDataA);

  // Completely overlap the existing buffers.
  NewCodedFrameGroupAppend(0, 20, &kDataB);

  // Final GOP [15,19) contains 5 buffers, which is more than memory limit of
  // 3 buffers set at the beginning of the test, so GC will fail.
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(15, 0));

  // Because buffers 16 and 17 are not keyframes, they are moved to the track
  // buffer upon overlap. The source buffer (i.e. not the track buffer) is now
  // waiting for the next keyframe.
  CheckExpectedRanges("{ [15,19) }");
  CheckExpectedBuffers(16, 17, &kDataA);
  CheckNoNextBuffer();

  // Now add a keyframe at position 20.
  AppendBuffers(20, 5, &kDataB);

  // 5 buffers in final GOP, GC will fail
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(20, 0));

  // Should garbage collect such that there are 5 frames remaining, starting at
  // the keyframe.
  CheckExpectedRanges("{ [20,24) }");
  CheckExpectedBuffers(20, 24, &kDataB);
  CheckNoNextBuffer();
}

// Test GC preserves data starting at first GOP containing playback position.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveDataAtPlaybackPosition) {
  // Set memory limit to 30 buffers = 1 second of data.
  SetMemoryLimit(30);
  // And append 300 buffers = 10 seconds of data.
  NewCodedFrameGroupAppend(0, 300, &kDataA);
  CheckExpectedRanges("{ [0,299) }");

  // Playback position at 0, all data must be preserved.
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(0), 0));
  CheckExpectedRanges("{ [0,299) }");

  // Playback position at 1 sec, the first second of data [0,29) should be
  // collected, since we are way over memory limit.
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(1000), 0));
  CheckExpectedRanges("{ [30,299) }");

  // Playback position at 1.1 sec, no new data can be collected, since the
  // playback position is still in the first GOP of buffered data.
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(1100), 0));
  CheckExpectedRanges("{ [30,299) }");

  // Playback position at 5.166 sec, just at the very end of GOP corresponding
  // to buffer range 150-155, which should be preserved.
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(5166), 0));
  CheckExpectedRanges("{ [150,299) }");

  // Playback position at 5.167 sec, just past the end of GOP corresponding to
  // buffer range 150-155, it should be garbage collected now.
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(5167), 0));
  CheckExpectedRanges("{ [155,299) }");

  // Playback at 9.0 sec, we can now successfully collect all data except the
  // last second and we are back under memory limit of 30 buffers, so GCIfNeeded
  // should return true.
  EXPECT_TRUE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(9000), 0));
  CheckExpectedRanges("{ [270,299) }");

  // Playback at 9.999 sec, GC succeeds, since we are under memory limit even
  // without removing any data.
  EXPECT_TRUE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(9999), 0));
  CheckExpectedRanges("{ [270,299) }");

  // Playback at 15 sec, this should never happen during regular playback in
  // browser, since this position has no data buffered, but it should still
  // cause no problems to GC algorithm, so test it just in case.
  EXPECT_TRUE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(15000), 0));
  CheckExpectedRanges("{ [270,299) }");
}

// Test saving the last GOP appended when this GOP is the only GOP in its range.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP) {
  // Set memory limit to 3 and make sure the 4-byte GOP is not garbage
  // collected.
  SetMemoryLimit(3);
  NewCodedFrameGroupAppend("0K 30 60 90");
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(0, 0));
  CheckExpectedRangesByTimestamp("{ [0,120) }");

  // Make sure you can continue appending data to this GOP; again, GC should not
  // wipe out anything.
  AppendBuffers("120D30");
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(0, 0));
  CheckExpectedRangesByTimestamp("{ [0,150) }");

  // Append a 2nd range after this without triggering GC.
  NewCodedFrameGroupAppend("200K 230 260 290K 320 350");
  CheckExpectedRangesByTimestamp("{ [0,150) [200,380) }");

  // Seek to 290ms.
  SeekToTimestampMs(290);

  // Now append a GOP in a separate range after the selected range and trigger
  // GC. Because it is after 290ms, this tests that the GOP is saved when
  // deleting from the back.
  NewCodedFrameGroupAppend("500K 530 560 590");
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(290), 0));

  // Should save GOPs between 290ms and the last GOP appended.
  CheckExpectedRangesByTimestamp("{ [290,380) [500,620) }");

  // Continue appending to this GOP after GC.
  AppendBuffers("620D30");
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(290), 0));
  CheckExpectedRangesByTimestamp("{ [290,380) [500,650) }");
}

// Test saving the last GOP appended when this GOP is in the middle of a
// non-selected range.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Middle) {
  // Append 3 GOPs starting at 0ms, 30ms apart.
  NewCodedFrameGroupAppend("0K 30 60 90K 120 150 180K 210 240");
  CheckExpectedRangesByTimestamp("{ [0,270) }");

  // Now set the memory limit to 1 and overlap the middle of the range with a
  // new GOP.
  SetMemoryLimit(1);
  NewCodedFrameGroupAppend("80K 110 140");

  // This whole GOP should be saved after GC, which will fail due to GOP being
  // larger than 1 buffer
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(80), 0));
  CheckExpectedRangesByTimestamp("{ [80,170) }");
  // We should still be able to continue appending data to GOP
  AppendBuffers("170D30");
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(80), 0));
  CheckExpectedRangesByTimestamp("{ [80,200) }");

  // Append a 2nd range after this range, without triggering GC.
  NewCodedFrameGroupAppend("400K 430 460 490K 520 550 580K 610 640");
  CheckExpectedRangesByTimestamp("{ [80,200) [400,670) }");

  // Seek to 80ms to make the first range the selected range.
  SeekToTimestampMs(80);

  // Now append a GOP in the middle of the second range and trigger GC. Because
  // it is after the selected range, this tests that the GOP is saved when
  // deleting from the back.
  NewCodedFrameGroupAppend("500K 530 560 590");
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(80), 0));

  // Should save the GOPs between the seek point and GOP that was last appended
  CheckExpectedRangesByTimestamp("{ [80,200) [400,620) }");

  // Continue appending to this GOP after GC.
  AppendBuffers("620D30");
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(80), 0));
  CheckExpectedRangesByTimestamp("{ [80,200) [400,650) }");
}

// Test saving the last GOP appended when the GOP containing the next buffer is
// adjacent to the last GOP appended.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Selected1) {
  // Append 3 GOPs at 0ms, 90ms, and 180ms.
  NewCodedFrameGroupAppend("0K 30 60 90K 120 150 180K 210 240");
  CheckExpectedRangesByTimestamp("{ [0,270) }");

  // Seek to the GOP at 90ms.
  SeekToTimestampMs(90);

  // Set the memory limit to 1, then overlap the GOP at 0.
  SetMemoryLimit(1);
  NewCodedFrameGroupAppend("0K 30 60");

  // GC should save the GOP at 0ms and 90ms, and will fail since GOP larger
  // than 1 buffer
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(90), 0));
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Seek to 0 and check all buffers.
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 30 60 90K 120 150");
  CheckNoNextBuffer();

  // Now seek back to 90ms and append a GOP at 180ms.
  SeekToTimestampMs(90);
  NewCodedFrameGroupAppend("180K 210 240");

  // Should save the GOP at 90ms and the GOP at 180ms.
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(90), 0));
  CheckExpectedRangesByTimestamp("{ [90,270) }");
  CheckExpectedBuffers("90K 120 150 180K 210 240");
  CheckNoNextBuffer();
}

// Test saving the last GOP appended when it is at the beginning or end of the
// selected range. This tests when the last GOP appended is before or after the
// GOP containing the next buffer, but not directly adjacent to this GOP.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Selected2) {
  // Append 4 GOPs starting at positions 0ms, 90ms, 180ms, 270ms.
  NewCodedFrameGroupAppend("0K 30 60 90K 120 150 180K 210 240 270K 300 330");
  CheckExpectedRangesByTimestamp("{ [0,360) }");

  // Seek to the last GOP at 270ms.
  SeekToTimestampMs(270);

  // Set the memory limit to 1, then overlap the GOP at 90ms.
  SetMemoryLimit(1);
  NewCodedFrameGroupAppend("90K 120 150");

  // GC will save data in the range where the most recent append has happened
  // [0; 180) and the range where the next read position is [270;360)
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(270), 0));
  CheckExpectedRangesByTimestamp("{ [0,180) [270,360) }");

  // Add 3 GOPs to the end of the selected range at 360ms, 450ms, and 540ms.
  NewCodedFrameGroupAppend("360K 390 420 450K 480 510 540K 570 600");
  CheckExpectedRangesByTimestamp("{ [0,180) [270,630) }");

  // Overlap the GOP at 450ms and garbage collect to test deleting from the
  // back.
  NewCodedFrameGroupAppend("450K 480 510");
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(270), 0));

  // Should save GOPs from GOP at 270ms to GOP at 450ms.
  CheckExpectedRangesByTimestamp("{ [270,540) }");
}

// Test saving the last GOP appended when it is the same as the GOP containing
// the next buffer.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Selected3) {
  // Seek to start of stream.
  SeekToTimestampMs(0);

  // Append 3 GOPs starting at 0ms, 90ms, 180ms.
  NewCodedFrameGroupAppend("0K 30 60 90K 120 150 180K 210 240");
  CheckExpectedRangesByTimestamp("{ [0,270) }");

  // Set the memory limit to 1 then begin appending the start of a GOP starting
  // at 0ms.
  SetMemoryLimit(1);
  NewCodedFrameGroupAppend("0K 30");

  // GC should save the newly appended GOP, which is also the next GOP that
  // will be returned from the seek request.
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(0), 0));
  CheckExpectedRangesByTimestamp("{ [0,60) }");

  // Check the buffers in the range.
  CheckExpectedBuffers("0K 30");
  CheckNoNextBuffer();

  // Continue appending to this buffer.
  AppendBuffers("60 90");

  // GC should still save the rest of this GOP and should be able to fulfill
  // the read.
  EXPECT_FALSE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(0), 0));
  CheckExpectedRangesByTimestamp("{ [0,120) }");
  CheckExpectedBuffers("60 90");
  CheckNoNextBuffer();
}

// Test the performance of garbage collection.
TEST_F(SourceBufferStreamTest, GarbageCollection_Performance) {
  // Force |keyframes_per_second_| to be equal to kDefaultFramesPerSecond.
  SetStreamInfo(kDefaultFramesPerSecond, kDefaultFramesPerSecond);

  const int kBuffersToKeep = 1000;
  SetMemoryLimit(kBuffersToKeep);

  int buffers_appended = 0;

  NewCodedFrameGroupAppend(0, kBuffersToKeep);
  buffers_appended += kBuffersToKeep;

  const int kBuffersToAppend = 1000;
  const int kGarbageCollections = 3;
  for (int i = 0; i < kGarbageCollections; ++i) {
    AppendBuffers(buffers_appended, kBuffersToAppend);
    buffers_appended += kBuffersToAppend;
  }
}

TEST_F(SourceBufferStreamTest, GarbageCollection_MediaTimeAfterLastAppendTime) {
  // Set memory limit to 10 buffers.
  SetMemoryLimit(10);

  // Append 12 buffers. The duration of the last buffer is 30
  NewCodedFrameGroupAppend("0K 30 60 90 120K 150 180 210K 240 270 300K 330D30");
  CheckExpectedRangesByTimestamp("{ [0,360) }");

  // Do a garbage collection with the media time higher than the timestamp of
  // the last appended buffer (330), but still within buffered ranges, taking
  // into account the duration of the last frame (timestamp of the last frame is
  // 330, duration is 30, so the latest valid buffered position is 330+30=360).
  EXPECT_TRUE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(360), 0));

  // GC should collect one GOP from the front to bring us back under memory
  // limit of 10 buffers.
  CheckExpectedRangesByTimestamp("{ [120,360) }");
}

TEST_F(SourceBufferStreamTest,
       GarbageCollection_MediaTimeOutsideOfStreamBufferedRange) {
  // Set memory limit to 10 buffers.
  SetMemoryLimit(10);

  // Append 12 buffers.
  NewCodedFrameGroupAppend("0K 30 60 90 120K 150 180 210K 240 270 300K 330");
  CheckExpectedRangesByTimestamp("{ [0,360) }");

  // Seek in order to set the stream read position to 330 an ensure that the
  // stream selects the buffered range.
  SeekToTimestampMs(330);

  // Do a garbage collection with the media time outside the buffered ranges
  // (this might happen when there's both audio and video streams, audio stream
  // buffered range is longer than the video stream buffered range, since
  // media::Pipeline uses audio stream as a time source in that case, it might
  // return a media_time that is slightly outside of video buffered range). In
  // those cases the GC algorithm should clamp the media_time value to the
  // buffered ranges to work correctly (see crbug.com/563292).
  EXPECT_TRUE(stream_->GarbageCollectIfNeeded(
      DecodeTimestamp::FromMilliseconds(361), 0));

  // GC should collect one GOP from the front to bring us back under memory
  // limit of 10 buffers.
  CheckExpectedRangesByTimestamp("{ [120,360) }");
}

TEST_F(SourceBufferStreamTest, GetRemovalRange_BytesToFree) {
  // Append 2 GOPs starting at 300ms, 30ms apart.
  NewCodedFrameGroupAppend("300K 330 360 390K 420 450");

  // Append 2 GOPs starting at 600ms, 30ms apart.
  NewCodedFrameGroupAppend("600K 630 660 690K 720 750");

  // Append 2 GOPs starting at 900ms, 30ms apart.
  NewCodedFrameGroupAppend("900K 930 960 990K 1020 1050");

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
  NewCodedFrameGroupAppend("300K 330 360 390K 420 450");

  // Append 2 GOPs starting at 600ms, 30ms apart.
  NewCodedFrameGroupAppend("600K 630 660 690K 720 750");

  // Append 2 GOPs starting at 900ms, 30ms apart.
  NewCodedFrameGroupAppend("900K 930 960 990K 1020 1050");

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
  NewCodedFrameGroupAppend(0, 5, &kDataA);

  CheckVideoConfig(video_config_);

  // Signal a config change.
  stream_->UpdateVideoConfig(new_config);

  // Make sure updating the config doesn't change anything since new_config
  // should not be associated with the buffer GetNextBuffer() will return.
  CheckVideoConfig(video_config_);

  // Append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5, &kDataB);

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
  NewCodedFrameGroupAppend(0, 5, &kDataA);
  stream_->UpdateVideoConfig(new_config);
  NewCodedFrameGroupAppend(5, 5, &kDataB);

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
  NewCodedFrameGroupAppend(5, 2);

  // Append 2 buffers at positions 10 through 11.
  NewCodedFrameGroupAppend(10, 2);

  // Append 2 buffers at positions 15 through 16.
  NewCodedFrameGroupAppend(15, 2);

  // Check expected ranges.
  CheckExpectedRanges("{ [5,6) [10,11) [15,16) }");

  // Set duration to be between buffers 6 and 10.
  stream_->OnSetDuration(frame_duration() * 8);

  // Should truncate the data after 6.
  CheckExpectedRanges("{ [5,6) }");

  // Adding data past the previous duration should still work.
  NewCodedFrameGroupAppend(0, 20);
  CheckExpectedRanges("{ [0,19) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_EdgeCase) {
  // Append 10 buffers at positions 10 through 19.
  NewCodedFrameGroupAppend(10, 10);

  // Append 5 buffers at positions 25 through 29.
  NewCodedFrameGroupAppend(25, 5);

  // Check expected ranges.
  CheckExpectedRanges("{ [10,19) [25,29) }");

  // Set duration to be right before buffer 25.
  stream_->OnSetDuration(frame_duration() * 25);

  // Should truncate the last range.
  CheckExpectedRanges("{ [10,19) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_DeletePartialRange) {
  // Append 5 buffers at positions 0 through 4.
  NewCodedFrameGroupAppend(0, 5);

  // Append 10 buffers at positions 10 through 19.
  NewCodedFrameGroupAppend(10, 10);

  // Append 5 buffers at positions 25 through 29.
  NewCodedFrameGroupAppend(25, 5);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,4) [10,19) [25,29) }");

  // Set duration to be between buffers 13 and 14.
  stream_->OnSetDuration(frame_duration() * 14);

  // Should truncate the data after 13.
  CheckExpectedRanges("{ [0,4) [10,13) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_DeleteSelectedRange) {
  // Append 2 buffers at positions 5 through 6.
  NewCodedFrameGroupAppend(5, 2);

  // Append 2 buffers at positions 10 through 11.
  NewCodedFrameGroupAppend(10, 2);

  // Append 2 buffers at positions 15 through 16.
  NewCodedFrameGroupAppend(15, 2);

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
  NewCodedFrameGroupAppend(0, 15);
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [0,14) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_DeletePartialSelectedRange) {
  // Append 5 buffers at positions 0 through 4.
  NewCodedFrameGroupAppend(0, 5);

  // Append 20 buffers at positions 10 through 29.
  NewCodedFrameGroupAppend(10, 20);

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
  SeekToTimestampMs(0);

  NewCodedFrameGroupAppend("0K 30 60 90");

  // Read out the first few buffers.
  CheckExpectedBuffers("0K 30");

  // Set duration to be right before buffer 1.
  stream_->OnSetDuration(base::TimeDelta::FromMilliseconds(60));

  // Verify that there is no next buffer.
  CheckNoNextBuffer();

  // We should be able to append new buffers at this point.
  NewCodedFrameGroupAppend("120K 150");

  CheckExpectedRangesByTimestamp("{ [0,60) [120,180) }");
}

TEST_F(SourceBufferStreamTest,
       SetExplicitDuration_AfterGroupTimestampAndBeforeFirstBufferTimestamp) {
  NewCodedFrameGroupAppend("0K 30K 60K");

  // Append a coded frame group with a start timestamp of 200, but the first
  // buffer starts at 230ms. This can happen in muxed content where the
  // audio starts before the first frame.
  NewCodedFrameGroupAppend(base::TimeDelta::FromMilliseconds(200),
                           "230K 260K 290K 320K");

  NewCodedFrameGroupAppend("400K 430K 460K");

  CheckExpectedRangesByTimestamp("{ [0,90) [200,350) [400,490) }");

  stream_->OnSetDuration(base::TimeDelta::FromMilliseconds(120));

  // Verify that the buffered ranges are updated properly and we don't crash.
  CheckExpectedRangesByTimestamp("{ [0,90) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_MarkEOS) {
  // Append 1 buffer at positions 0 through 8.
  NewCodedFrameGroupAppend(0, 9);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,8) }");

  // Seek to 5.
  Seek(5);

  // Set duration to be before the seeked to position.
  // This will result in truncation of the selected range and a reset
  // of NextBufferPosition.
  stream_->OnSetDuration(frame_duration() * 4);

  // Check the expected ranges.
  CheckExpectedRanges("{ [0,3) }");

  // Mark EOS reached.
  stream_->MarkEndOfStream();

  // Expect EOS to be reached.
  CheckEOSReached();
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_MarkEOS_IsSeekPending) {
  // Append 1 buffer at positions 0 through 8.
  NewCodedFrameGroupAppend(0, 9);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,8) }");

  // Seek to 9 which will result in a pending seek.
  Seek(9);

  // Set duration to be before the seeked to position.
  // This will result in truncation of the selected range and a reset
  // of NextBufferPosition.
  stream_->OnSetDuration(frame_duration() * 4);

  // Check the expected ranges.
  CheckExpectedRanges("{ [0,3) }");

  EXPECT_TRUE(stream_->IsSeekPending());
  // Mark EOS reached.
  stream_->MarkEndOfStream();
  EXPECT_FALSE(stream_->IsSeekPending());
}

// Test the case were the current playback position is at the end of the
// buffered data and several overlaps occur that causes the selected
// range to get split and then merged back into a single range.
TEST_F(SourceBufferStreamTest, OverlapSplitAndMergeWhileWaitingForMoreData) {
  // Seek to start of stream.
  SeekToTimestampMs(0);

  NewCodedFrameGroupAppend("0K 30 60 90 120K 150");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Read all the buffered data.
  CheckExpectedBuffers("0K 30 60 90 120K 150");
  CheckNoNextBuffer();

  // Append data over the current GOP so that a keyframe is needed before
  // playback can continue from the current position.
  NewCodedFrameGroupAppend("120K 150");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Append buffers that cause the range to get split.
  NewCodedFrameGroupAppend("0K 30");
  CheckExpectedRangesByTimestamp("{ [0,60) [120,180) }");

  // Append buffers that cause the ranges to get merged.
  AppendBuffers("60 90");

  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Verify that we still don't have a next buffer.
  CheckNoNextBuffer();

  // Add more data to the end and verify that this new data is read correctly.
  NewCodedFrameGroupAppend("180K 210");
  CheckExpectedRangesByTimestamp("{ [0,240) }");
  CheckExpectedBuffers("180K 210");
}

// Verify that a single coded frame at the current read position unblocks the
// read even if the frame is buffered after the previously read position is
// removed.
TEST_F(SourceBufferStreamTest, AfterRemove_SingleFrameRange_Unblocks_Read) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90D30");
  CheckExpectedRangesByTimestamp("{ [0,120) }");
  CheckExpectedBuffers("0K 30 60 90");
  CheckNoNextBuffer();

  RemoveInMs(0, 120, 120);
  CheckExpectedRangesByTimestamp("{ }");
  NewCodedFrameGroupAppend("120D30K");
  CheckExpectedRangesByTimestamp("{ [120,150) }");
  CheckExpectedBuffers("120K");
  CheckNoNextBuffer();
}

// Verify that multiple short (relative to max-inter-buffer-distance * 2) coded
// frames at the current read position unblock the read even if the frames are
// buffered after the previously read position is removed.
TEST_F(SourceBufferStreamTest, AfterRemove_TinyFrames_Unblock_Read_1) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90D30");
  CheckExpectedRangesByTimestamp("{ [0,120) }");
  CheckExpectedBuffers("0K 30 60 90");
  CheckNoNextBuffer();

  RemoveInMs(0, 120, 120);
  CheckExpectedRangesByTimestamp("{ }");
  NewCodedFrameGroupAppend("120D1K 121D1");
  CheckExpectedRangesByTimestamp("{ [120,122) }");
  CheckExpectedBuffers("120K 121");
  CheckNoNextBuffer();
}

// Verify that multiple short (relative to max-inter-buffer-distance * 2) coded
// frames starting at the fudge room boundary unblock the read even if the
// frames are buffered after the previously read position is removed.
TEST_F(SourceBufferStreamTest, AfterRemove_TinyFrames_Unblock_Read_2) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90D30");
  CheckExpectedRangesByTimestamp("{ [0,120) }");
  CheckExpectedBuffers("0K 30 60 90");
  CheckNoNextBuffer();

  RemoveInMs(0, 120, 120);
  CheckExpectedRangesByTimestamp("{ }");
  NewCodedFrameGroupAppend("150D1K 151D1");
  CheckExpectedRangesByTimestamp("{ [150,152) }");
  CheckExpectedBuffers("150K 151");
  CheckNoNextBuffer();
}

// Verify that coded frames starting after the fudge room boundary do not
// unblock the read when buffered after the previously read position is removed.
TEST_F(SourceBufferStreamTest, AfterRemove_BeyondFudge_Stalled) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90D30");
  CheckExpectedRangesByTimestamp("{ [0,120) }");
  CheckExpectedBuffers("0K 30 60 90");
  CheckNoNextBuffer();

  RemoveInMs(0, 120, 120);
  CheckExpectedRangesByTimestamp("{ }");
  NewCodedFrameGroupAppend("151D1K 152D1");
  CheckExpectedRangesByTimestamp("{ [151,153) }");
  CheckNoNextBuffer();
}

// Verify that non-keyframes with the same timestamp in the same
// append are handled correctly.
TEST_F(SourceBufferStreamTest, SameTimestamp_Video_SingleAppend) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 30 60 90 120K 150");
  CheckExpectedBuffers("0K 30 30 60 90 120K 150");
}

// Verify that non-keyframes with the same timestamp can occur
// in different appends.
TEST_F(SourceBufferStreamTest, SameTimestamp_Video_TwoAppends) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30");
  AppendBuffers("30 60 90 120K 150");
  CheckExpectedBuffers("0K 30 30 60 90 120K 150");
}

// Verify that a non-keyframe followed by a keyframe with the same timestamp
// is allowed, but also results in a MediaLog.
TEST_F(SourceBufferStreamTest, SameTimestamp_Video_SingleAppend_Warning) {
  EXPECT_MEDIA_LOG(ContainsSameTimestampAt30MillisecondsLog());

  Seek(0);
  NewCodedFrameGroupAppend("0K 30 30K 60");
  CheckExpectedBuffers("0K 30 30K 60");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_Video_TwoAppends_Warning) {
  EXPECT_MEDIA_LOG(ContainsSameTimestampAt30MillisecondsLog());

  Seek(0);
  NewCodedFrameGroupAppend("0K 30");
  AppendBuffers("30K 60");
  CheckExpectedBuffers("0K 30 30K 60");
}

// Verify that a keyframe followed by a non-keyframe with the same timestamp
// is allowed.
TEST_F(SourceBufferStreamTest, SameTimestamp_VideoKeyFrame_TwoAppends) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30K");
  AppendBuffers("30 60");
  CheckExpectedBuffers("0K 30K 30 60");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_VideoKeyFrame_SingleAppend) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30K 30 60");
  CheckExpectedBuffers("0K 30K 30 60");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_Video_Overlap_1) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 60 90 120K 150");

  NewCodedFrameGroupAppend("60K 91 121K 151");
  CheckExpectedBuffers("0K 30 60K 91 121K 151");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_Video_Overlap_2) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 60 90 120K 150");
  NewCodedFrameGroupAppend("0K 30 61");
  CheckExpectedBuffers("0K 30 61 120K 150");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_Video_Overlap_3) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 20 40 60 80 100K 101 102 103K");
  NewCodedFrameGroupAppend("0K 20 40 60 80 90");
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
                            44100, EmptyExtraData(), Unencrypted());
  stream_.reset(new SourceBufferStream(config, media_log_, true));
  Seek(0);
  NewCodedFrameGroupAppend("0K 0K 30K 30 60 60");
  CheckExpectedBuffers("0K 0K 30K 30 60 60");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_Audio_SingleAppend_Warning) {
  EXPECT_MEDIA_LOG(ContainsSameTimestampAt30MillisecondsLog());

  AudioDecoderConfig config(kCodecMP3, kSampleFormatF32, CHANNEL_LAYOUT_STEREO,
                            44100, EmptyExtraData(), Unencrypted());
  stream_.reset(new SourceBufferStream(config, media_log_, true));
  Seek(0);

  // Note, in reality, a non-keyframe audio frame is rare or perhaps not
  // possible.
  NewCodedFrameGroupAppend("0K 30 30K 60");
  CheckExpectedBuffers("0K 30 30K 60");
}

// If seeking past any existing range and the seek is pending
// because no data has been provided for that position,
// the stream position can be considered as the end of stream.
TEST_F(SourceBufferStreamTest, EndSelected_During_PendingSeek) {
  // Append 15 buffers at positions 0 through 14.
  NewCodedFrameGroupAppend(0, 15);

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
  NewCodedFrameGroupAppend(0, 10);
  NewCodedFrameGroupAppend(30, 10);

  Seek(20);
  EXPECT_TRUE(stream_->IsSeekPending());
  stream_->MarkEndOfStream();
  EXPECT_TRUE(stream_->IsSeekPending());
}


// Removing exact start & end of a range.
TEST_F(SourceBufferStreamTest, Remove_WholeRange1) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");
  RemoveInMs(10, 160, 160);
  CheckExpectedRangesByTimestamp("{ }");
}

// Removal range starts before range and ends exactly at end.
TEST_F(SourceBufferStreamTest, Remove_WholeRange2) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");
  RemoveInMs(0, 160, 160);
  CheckExpectedRangesByTimestamp("{ }");
}

// Removal range starts at the start of a range and ends beyond the
// range end.
TEST_F(SourceBufferStreamTest, Remove_WholeRange3) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");
  RemoveInMs(10, 200, 200);
  CheckExpectedRangesByTimestamp("{ }");
}

// Removal range starts before range start and ends after the range end.
TEST_F(SourceBufferStreamTest, Remove_WholeRange4) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");
  RemoveInMs(0, 200, 200);
  CheckExpectedRangesByTimestamp("{ }");
}

// Removes multiple ranges.
TEST_F(SourceBufferStreamTest, Remove_WholeRange5) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  NewCodedFrameGroupAppend("1000K 1030 1060K 1090 1120K");
  NewCodedFrameGroupAppend("2000K 2030 2060K 2090 2120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) [2000,2150) }");
  RemoveInMs(10, 3000, 3000);
  CheckExpectedRangesByTimestamp("{ }");
}

// Verifies a [0-infinity) range removes everything.
TEST_F(SourceBufferStreamTest, Remove_ZeroToInfinity) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  NewCodedFrameGroupAppend("1000K 1030 1060K 1090 1120K");
  NewCodedFrameGroupAppend("2000K 2030 2060K 2090 2120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) [2000,2150) }");
  Remove(base::TimeDelta(), kInfiniteDuration(), kInfiniteDuration());
  CheckExpectedRangesByTimestamp("{ }");
}

// Removal range starts at the beginning of the range and ends in the
// middle of the range. This test verifies that full GOPs are removed.
TEST_F(SourceBufferStreamTest, Remove_Partial1) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  NewCodedFrameGroupAppend("1000K 1030 1060K 1090 1120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) }");
  RemoveInMs(0, 80, 2200);
  CheckExpectedRangesByTimestamp("{ [130,160) [1000,1150) }");
}

// Removal range starts in the middle of a range and ends at the exact
// end of the range.
TEST_F(SourceBufferStreamTest, Remove_Partial2) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  NewCodedFrameGroupAppend("1000K 1030 1060K 1090 1120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) }");
  RemoveInMs(40, 160, 2200);
  CheckExpectedRangesByTimestamp("{ [10,40) [1000,1150) }");
}

// Removal range starts and ends within a range.
TEST_F(SourceBufferStreamTest, Remove_Partial3) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  NewCodedFrameGroupAppend("1000K 1030 1060K 1090 1120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) }");
  RemoveInMs(40, 120, 2200);
  CheckExpectedRangesByTimestamp("{ [10,40) [130,160) [1000,1150) }");
}

// Removal range starts in the middle of one range and ends in the
// middle of another range.
TEST_F(SourceBufferStreamTest, Remove_Partial4) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  NewCodedFrameGroupAppend("1000K 1030 1060K 1090 1120K");
  NewCodedFrameGroupAppend("2000K 2030 2060K 2090 2120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) [2000,2150) }");
  RemoveInMs(40, 2030, 2200);
  CheckExpectedRangesByTimestamp("{ [10,40) [2060,2150) }");
}

// Test behavior when the current position is removed and new buffers
// are appended over the removal range.
TEST_F(SourceBufferStreamTest, Remove_CurrentPosition) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90K 120 150 180K 210 240 270K 300 330");
  CheckExpectedRangesByTimestamp("{ [0,360) }");
  CheckExpectedBuffers("0K 30 60 90K 120");

  // Remove a range that includes the next buffer (i.e., 150).
  RemoveInMs(150, 210, 360);
  CheckExpectedRangesByTimestamp("{ [0,150) [270,360) }");

  // Verify that no next buffer is returned.
  CheckNoNextBuffer();

  // Append some buffers to fill the gap that was created.
  NewCodedFrameGroupAppend("120K 150 180 210K 240");
  CheckExpectedRangesByTimestamp("{ [0,360) }");

  // Verify that buffers resume at the next keyframe after the
  // current position.
  CheckExpectedBuffers("210K 240 270K 300 330");
}

// Test behavior when buffers in the selected range before the current position
// are removed.
TEST_F(SourceBufferStreamTest, Remove_BeforeCurrentPosition) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90K 120 150 180K 210 240 270K 300 330");
  CheckExpectedRangesByTimestamp("{ [0,360) }");
  CheckExpectedBuffers("0K 30 60 90K 120");

  // Remove a range that is before the current playback position.
  RemoveInMs(0, 90, 360);
  CheckExpectedRangesByTimestamp("{ [90,360) }");

  CheckExpectedBuffers("150 180K 210 240 270K 300 330");
}

// Test removing the entire range for the current coded frame group
// being appended.
TEST_F(SourceBufferStreamTest, Remove_MidGroup) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90 120K 150 180 210");
  CheckExpectedRangesByTimestamp("{ [0,240) }");

  NewCodedFrameGroupAppend("0K 30");

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

  SeekToTimestampMs(150);
  CheckExpectedBuffers("150K 180");
  CheckNoNextBuffer();
}

// Test removing the current GOP being appended, while not removing
// the entire range the GOP belongs to.
TEST_F(SourceBufferStreamTest, Remove_GOPBeingAppended) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90 120K 150 180");
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
  SeekToTimestampMs(240);
  CheckExpectedBuffers("240K 270 300");
}

TEST_F(SourceBufferStreamTest, Remove_WholeGOPBeingAppended) {
  SeekToTimestampMs(1000);
  NewCodedFrameGroupAppend("1000K 1030 1060 1090");
  CheckExpectedRangesByTimestamp("{ [1000,1120) }");

  // Remove the keyframe of the current GOP being appended.
  RemoveInMs(1000, 1030, 1120);
  CheckExpectedRangesByTimestamp("{ }");

  // Continue appending the current GOP.
  AppendBuffers("1210 1240");

  CheckExpectedRangesByTimestamp("{ }");

  // Append the beginning of the next GOP.
  AppendBuffers("1270K 1300");

  // Verify that the new range is started at the
  // beginning of the next GOP.
  CheckExpectedRangesByTimestamp("{ [1270,1330) }");

  // Verify the buffers in the ranges.
  CheckNoNextBuffer();
  SeekToTimestampMs(1270);
  CheckExpectedBuffers("1270K 1300");
}

TEST_F(SourceBufferStreamTest,
       Remove_PreviousAppendDestroyedAndOverwriteExistingRange) {
  SeekToTimestampMs(90);

  NewCodedFrameGroupAppend("90K 120 150");
  CheckExpectedRangesByTimestamp("{ [90,180) }");

  // Append a coded frame group before the previously appended data.
  NewCodedFrameGroupAppend("0K 30 60");

  // Verify that the ranges get merged.
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Remove the data from the last append.
  RemoveInMs(0, 90, 360);
  CheckExpectedRangesByTimestamp("{ [90,180) }");

  // Append a new coded frame group that follows the removed group and
  // starts at the beginning of the range left over from the
  // remove.
  NewCodedFrameGroupAppend("90K 121 151");
  CheckExpectedBuffers("90K 121 151");
}

TEST_F(SourceBufferStreamTest, Remove_GapAtBeginningOfGroup) {
  Seek(0);

  // Append a coded frame group that has a gap at the beginning of it.
  NewCodedFrameGroupAppend(base::TimeDelta::FromMilliseconds(0),
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
  NewCodedFrameGroupAppend("0K 500K 1000K");
  CheckExpectedRangesByTimestamp("{ [0,1500) }");

  Seek(0);
  CheckExpectedBuffers("0K 500K 1000K");
}

TEST_F(SourceBufferStreamTest, Text_Append_DisjointAfter) {
  SetTextStream();
  NewCodedFrameGroupAppend("0K 500K 1000K");
  CheckExpectedRangesByTimestamp("{ [0,1500) }");
  NewCodedFrameGroupAppend("3000K 3500K 4000K");
  CheckExpectedRangesByTimestamp("{ [0,4500) }");

  Seek(0);
  CheckExpectedBuffers("0K 500K 1000K 3000K 3500K 4000K");
}

TEST_F(SourceBufferStreamTest, Text_Append_DisjointBefore) {
  SetTextStream();
  NewCodedFrameGroupAppend("3000K 3500K 4000K");
  CheckExpectedRangesByTimestamp("{ [3000,4500) }");
  NewCodedFrameGroupAppend("0K 500K 1000K");
  CheckExpectedRangesByTimestamp("{ [0,4500) }");

  Seek(0);
  CheckExpectedBuffers("0K 500K 1000K 3000K 3500K 4000K");
}

TEST_F(SourceBufferStreamTest, Text_CompleteOverlap) {
  SetTextStream();
  NewCodedFrameGroupAppend("3000K 3500K 4000K");
  CheckExpectedRangesByTimestamp("{ [3000,4500) }");
  NewCodedFrameGroupAppend(
      "0K 501K 1001K 1501K 2001K 2501K "
      "3001K 3501K 4001K 4501K 5001K");
  CheckExpectedRangesByTimestamp("{ [0,5501) }");

  Seek(0);
  CheckExpectedBuffers("0K 501K 1001K 1501K 2001K 2501K "
                       "3001K 3501K 4001K 4501K 5001K");
}

TEST_F(SourceBufferStreamTest, Text_OverlapAfter) {
  SetTextStream();
  NewCodedFrameGroupAppend("0K 500K 1000K 1500K 2000K");
  CheckExpectedRangesByTimestamp("{ [0,2500) }");
  NewCodedFrameGroupAppend("1499K 2001K 2501K 3001K");
  CheckExpectedRangesByTimestamp("{ [0,3501) }");

  Seek(0);
  CheckExpectedBuffers("0K 500K 1000K 1499K 2001K 2501K 3001K");
}

TEST_F(SourceBufferStreamTest, Text_OverlapBefore) {
  SetTextStream();
  NewCodedFrameGroupAppend("1500K 2000K 2500K 3000K 3500K");
  CheckExpectedRangesByTimestamp("{ [1500,4000) }");
  NewCodedFrameGroupAppend("0K 501K 1001K 1501K 2001K");
  CheckExpectedRangesByTimestamp("{ [0,4000) }");

  Seek(0);
  CheckExpectedBuffers("0K 501K 1001K 1501K 2001K 3000K 3500K");
}

TEST_F(SourceBufferStreamTest, SpliceFrame_Basic) {
  Seek(0);
  NewCodedFrameGroupAppend("0K S(3K 6 9D3 10D5) 15 20 S(25K 30D5 35D5) 40");
  CheckExpectedBuffers("0K 3K 6 9 C 10 15 20 25K 30 C 35 40");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, SpliceFrame_SeekClearsSplice) {
  Seek(0);
  NewCodedFrameGroupAppend("0K S(3K 6 9D3 10D5) 15K 20");
  CheckExpectedBuffers("0K 3K 6");

  SeekToTimestampMs(15);
  CheckExpectedBuffers("15K 20");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, SpliceFrame_SeekClearsSpliceFromTrackBuffer) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 2K S(3K 6 9D3 10D5) 15K 20");
  CheckExpectedBuffers("0K 2K");

  // Overlap the existing coded frame group.
  NewCodedFrameGroupAppend("5K 15K 20");
  CheckExpectedBuffers("3K 6");

  SeekToTimestampMs(15);
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
  NewCodedFrameGroupAppend("0K S(3K 6C 9D3 10D5) 15");

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
  NewCodedFrameGroupAppend("0K 5K S(8K 9D1 10D10) 20");
  CheckExpectedBuffers("0K 5K");

  // Overlap the existing coded frame group.
  NewCodedFrameGroupAppend("5K 20");
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
  NewCodedFrameGroupAppend("0K 5K S(7K 8C 9D1 10D10) 20");
  CheckExpectedBuffers("0K 5K");

  // Overlap the existing coded frame group.
  NewCodedFrameGroupAppend("5K 20");
  CheckExpectedBuffers("7K C");
  CheckVideoConfig(new_config);
  CheckExpectedBuffers("8 9 C");
  CheckExpectedBuffers("10 C");
  CheckVideoConfig(video_config_);
  CheckExpectedBuffers("20");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_Basic) {
  EXPECT_MEDIA_LOG(ContainsGeneratedSpliceLog(3000, 11000));

  SetAudioStream();
  Seek(0);
  NewCodedFrameGroupAppend("0K 2K 4K 6K 8K 10K 12K");
  NewCodedFrameGroupAppend("11K 13K 15K 17K");
  CheckExpectedBuffers("0K 2K 4K 6K 8K 10K 12K C 11K 13K 15K 17K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_NoExactSplices) {
  EXPECT_MEDIA_LOG(
      HasSubstr("Skipping splice frame generation: first new buffer at 10000us "
                "begins at or before existing buffer at 10000us."));

  SetAudioStream();
  Seek(0);
  NewCodedFrameGroupAppend("0K 2K 4K 6K 8K 10K 12K");
  NewCodedFrameGroupAppend("10K 14K");
  CheckExpectedBuffers("0K 2K 4K 6K 8K 10K 14K");
  CheckNoNextBuffer();
}

// Do not allow splices on top of splices.
TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_NoDoubleSplice) {
  InSequence s;
  EXPECT_MEDIA_LOG(ContainsGeneratedSpliceLog(3000, 11000));
  EXPECT_MEDIA_LOG(
      HasSubstr("Skipping splice frame generation: overlapped buffers at "
                "10000us are in a previously buffered splice."));

  SetAudioStream();
  Seek(0);
  NewCodedFrameGroupAppend("0K 2K 4K 6K 8K 10K 12K");
  NewCodedFrameGroupAppend("11K 13K 15K 17K");

  // Verify the splice was created.
  CheckExpectedBuffers("0K 2K 4K 6K 8K 10K 12K C 11K 13K 15K 17K");
  CheckNoNextBuffer();
  Seek(0);

  // Create a splice before the first splice which would include it.
  NewCodedFrameGroupAppend("9D2K");

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
  NewCodedFrameGroupAppend("0K 2K 4K 6K 8K 10K");
  NewCodedFrameGroupAppend("12K 14K 16K 18K");
  CheckExpectedBuffers("0K 2K 4K 6K 8K 10K 12K 14K 16K 18K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_CorrectGroupStartTime) {
  EXPECT_MEDIA_LOG(ContainsGeneratedSpliceLog(5000, 1000));

  SetAudioStream();
  Seek(0);
  NewCodedFrameGroupAppend("0K 2K 4K");
  CheckExpectedRangesByTimestamp("{ [0,6) }");
  NewCodedFrameGroupAppend("6K 8K 10K");
  CheckExpectedRangesByTimestamp("{ [0,12) }");
  NewCodedFrameGroupAppend("1K 4D2K");
  CheckExpectedRangesByTimestamp("{ [0,12) }");
  CheckExpectedBuffers("0K 2K 4K C 1K 4K 6K 8K 10K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_ConfigChange) {
  EXPECT_MEDIA_LOG(ContainsGeneratedSpliceLog(3000, 5000));

  SetAudioStream();

  AudioDecoderConfig new_config(kCodecVorbis, kSampleFormatPlanarF32,
                                CHANNEL_LAYOUT_MONO, 1000, EmptyExtraData(),
                                Unencrypted());
  ASSERT_NE(new_config.channel_layout(), audio_config_.channel_layout());

  Seek(0);
  CheckAudioConfig(audio_config_);
  NewCodedFrameGroupAppend("0K 2K 4K 6K");
  stream_->UpdateAudioConfig(new_config);
  NewCodedFrameGroupAppend("5K 8K 12K");
  CheckExpectedBuffers("0K 2K 4K 6K C 5K 8K 12K");
  CheckAudioConfig(new_config);
  CheckNoNextBuffer();
}

// Ensure splices are not created if there are not enough frames to crossfade.
TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_NoTinySplices) {
  EXPECT_MEDIA_LOG(HasSubstr(
      "Skipping splice frame generation: not enough samples for splicing new "
      "buffer at 1000us. Have 1000us, but need 2000us."));

  SetAudioStream();
  Seek(0);

  // Overlap the range [0, 2) with [1, 3).  Since each frame has a duration of
  // 2ms this results in an overlap of 1ms between the ranges.  A splice frame
  // should not be generated since it requires at least 2 frames, or 2ms in this
  // case, of data to crossfade.
  NewCodedFrameGroupAppend("0D2K");
  CheckExpectedRangesByTimestamp("{ [0,2) }");
  NewCodedFrameGroupAppend("1D2K");
  CheckExpectedRangesByTimestamp("{ [0,3) }");
  CheckExpectedBuffers("0K 1K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_NoMillisecondSplices) {
  EXPECT_MEDIA_LOG(
      HasSubstr("Skipping splice frame generation: not enough samples for "
                "splicing new buffer at 1250us. Have 750us, but need 1000us."));

  video_config_ = TestVideoConfig::Invalid();
  audio_config_.Initialize(kCodecVorbis, kSampleFormatPlanarF32,
                           CHANNEL_LAYOUT_STEREO, 4000, EmptyExtraData(),
                           Unencrypted(), base::TimeDelta(), 0);
  stream_.reset(new SourceBufferStream(audio_config_, media_log_, true));
  // Equivalent to 0.5ms per frame.
  SetStreamInfo(2000, 2000);
  Seek(0);

  // Append four buffers with a 0.5ms duration each.
  NewCodedFrameGroupAppend(0, 4);
  CheckExpectedRangesByTimestamp("{ [0,2) }");

  // Overlap the range [0, 2) with [1.25, 2); this results in an overlap of
  // 0.75ms between the ranges.
  NewCodedFrameGroupAppend_OffsetFirstBuffer(
      2, 2, base::TimeDelta::FromMillisecondsD(0.25));
  CheckExpectedRangesByTimestamp("{ [0,2) }");

  // A splice frame should not be generated (indicated by the lack of a config
  // change in the expected buffer string) since it requires at least 1ms of
  // data to crossfade.
  CheckExpectedBuffers("0K 0K 1K 1K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_Preroll) {
  EXPECT_MEDIA_LOG(ContainsGeneratedSpliceLog(3000, 11000));

  SetAudioStream();
  Seek(0);
  NewCodedFrameGroupAppend("0K 2K 4K 6K 8K 10K 12K");
  NewCodedFrameGroupAppend("11P 13K 15K 17K");
  CheckExpectedBuffers("0K 2K 4K 6K 8K 10K 12K C 11P 13K 15K 17K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_PrerollFrame) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 3P 6K");
  CheckExpectedBuffers("0K 3P 6K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, BFrames) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 120|30 30|60 60|90 90|120");
  CheckExpectedRangesByTimestamp("{ [0,150) }");

  CheckExpectedBuffers("0K 120|30 30|60 60|90 90|120");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, RemoveShouldAlwaysExcludeEnd) {
  NewCodedFrameGroupAppend("10D2K 12D2 14D2");
  CheckExpectedRangesByTimestamp("{ [10,16) }");

  // Start new coded frame group, appending KF to abut the start of previous
  // group.
  NewCodedFrameGroupAppend("0D10K");
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,16) }");
  CheckExpectedBuffers("0K 10K 12 14");
  CheckNoNextBuffer();

  // Append another buffer with the same timestamp as the last KF. This triggers
  // special logic that allows two buffers to have the same timestamp. When
  // preparing for this new append, there is no reason to remove the later GOP
  // starting at timestamp 10. This verifies the fix for http://crbug.com/469325
  // where the decision *not* to remove the start of the overlapped range was
  // erroneously triggering buffers with a timestamp matching the end
  // of the append (and any later dependent frames) to be removed.
  AppendBuffers("0D10");
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,16) }");
  CheckExpectedBuffers("0K 0 10K 12 14");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, RefinedDurationEstimates_BackOverlap) {
  // Append a few buffers, the last one having estimated duration.
  NewCodedFrameGroupAppend("0K 5 10 20D10E");
  CheckExpectedRangesByTimestamp("{ [0,30) }");
  Seek(0);
  CheckExpectedBuffers("0K 5 10 20D10E");
  CheckNoNextBuffer();

  // Append a buffer to the end that overlaps the *back* of the existing range.
  // This should trigger the estimated duration to be recomputed as a timestamp
  // delta.
  AppendBuffers("25D10");
  CheckExpectedRangesByTimestamp("{ [0,35) }");
  Seek(0);
  // The duration of the buffer at time 20 has changed from 10ms to 5ms.
  CheckExpectedBuffers("0K 5 10 20D5E 25");
  CheckNoNextBuffer();

  // If the last buffer is removed, the adjusted duration should remain at 5ms.
  RemoveInMs(25, 35, 35);
  CheckExpectedRangesByTimestamp("{ [0,25) }");
  Seek(0);
  CheckExpectedBuffers("0K 5 10 20D5E");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, RefinedDurationEstimates_FrontOverlap) {
  // Append a few buffers.
  NewCodedFrameGroupAppend("10K 15 20D5");
  CheckExpectedRangesByTimestamp("{ [10,25) }");
  SeekToTimestampMs(10);
  CheckExpectedBuffers("10K 15 20");
  CheckNoNextBuffer();

  // Append new buffers, where the last has estimated duration that overlaps the
  // *front* of the existing range. The overlap should trigger refinement of the
  // estimated duration from 7ms to 5ms.
  NewCodedFrameGroupAppend("0K 5D7E");
  CheckExpectedRangesByTimestamp("{ [0,25) }");
  Seek(0);
  CheckExpectedBuffers("0K 5D5E 10K 15 20");
  CheckNoNextBuffer();

  // If the overlapped buffer at timestamp 10 is removed, the adjusted duration
  // should remain adjusted.
  RemoveInMs(10, 20, 25);
  CheckExpectedRangesByTimestamp("{ [0,10) }");
  Seek(0);
  CheckExpectedBuffers("0K 5D5E");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, SeekToStartSatisfiedUpToThreshold) {
  NewCodedFrameGroupAppend("999K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [999,1030) }");

  SeekToTimestampMs(0);
  CheckExpectedBuffers("999K 1010 1020D10");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, SeekToStartUnsatisfiedBeyondThreshold) {
  NewCodedFrameGroupAppend("1000K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [1000,1030) }");

  SeekToTimestampMs(0);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       ReSeekToStartSatisfiedUpToThreshold_SameTimestamps) {
  // Append a few buffers.
  NewCodedFrameGroupAppend("999K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [999,1030) }");

  // Don't read any buffers between Seek and Remove.
  SeekToTimestampMs(0);
  RemoveInMs(999, 1030, 1030);
  CheckExpectedRangesByTimestamp("{ }");
  CheckNoNextBuffer();

  // Append buffers at the original timestamps and verify no stall.
  NewCodedFrameGroupAppend("999K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [999,1030) }");
  CheckExpectedBuffers("999K 1010 1020D10");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       ReSeekToStartSatisfiedUpToThreshold_EarlierTimestamps) {
  // Append a few buffers.
  NewCodedFrameGroupAppend("999K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [999,1030) }");

  // Don't read any buffers between Seek and Remove.
  SeekToTimestampMs(0);
  RemoveInMs(999, 1030, 1030);
  CheckExpectedRangesByTimestamp("{ }");
  CheckNoNextBuffer();

  // Append buffers before the original timestamps and verify no stall (the
  // re-seek to time 0 should still be satisfied with the new buffers).
  NewCodedFrameGroupAppend("500K 510 520D10");
  CheckExpectedRangesByTimestamp("{ [500,530) }");
  CheckExpectedBuffers("500K 510 520D10");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       ReSeekToStartSatisfiedUpToThreshold_LaterTimestamps) {
  // Append a few buffers.
  NewCodedFrameGroupAppend("500K 510 520D10");
  CheckExpectedRangesByTimestamp("{ [500,530) }");

  // Don't read any buffers between Seek and Remove.
  SeekToTimestampMs(0);
  RemoveInMs(500, 530, 530);
  CheckExpectedRangesByTimestamp("{ }");
  CheckNoNextBuffer();

  // Append buffers beginning after original timestamps, but still below the
  // start threshold, and verify no stall (the re-seek to time 0 should still be
  // satisfied with the new buffers).
  NewCodedFrameGroupAppend("999K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [999,1030) }");
  CheckExpectedBuffers("999K 1010 1020D10");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, ReSeekBeyondStartThreshold_SameTimestamps) {
  // Append a few buffers.
  NewCodedFrameGroupAppend("1000K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [1000,1030) }");

  // Don't read any buffers between Seek and Remove.
  SeekToTimestampMs(1000);
  RemoveInMs(1000, 1030, 1030);
  CheckExpectedRangesByTimestamp("{ }");
  CheckNoNextBuffer();

  // Append buffers at the original timestamps and verify no stall.
  NewCodedFrameGroupAppend("1000K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [1000,1030) }");
  CheckExpectedBuffers("1000K 1010 1020D10");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, ReSeekBeyondThreshold_EarlierTimestamps) {
  // Append a few buffers.
  NewCodedFrameGroupAppend("2000K 2010 2020D10");
  CheckExpectedRangesByTimestamp("{ [2000,2030) }");

  // Don't read any buffers between Seek and Remove.
  SeekToTimestampMs(2000);
  RemoveInMs(2000, 2030, 2030);
  CheckExpectedRangesByTimestamp("{ }");
  CheckNoNextBuffer();

  // Append buffers before the original timestamps and verify no stall (the
  // re-seek to time 2 seconds should still be satisfied with the new buffers
  // and should emit preroll from last keyframe).
  NewCodedFrameGroupAppend("1080K 1090 2000D10");
  CheckExpectedRangesByTimestamp("{ [1080,2010) }");
  CheckExpectedBuffers("1080K 1090 2000D10");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, ConfigChange_ReSeek) {
  // Append a few buffers, with a config change in the middle.
  VideoDecoderConfig new_config = TestVideoConfig::Large();
  NewCodedFrameGroupAppend("2000K 2010 2020D10");
  stream_->UpdateVideoConfig(new_config);
  NewCodedFrameGroupAppend("2030K 2040 2050D10");
  CheckExpectedRangesByTimestamp("{ [2000,2060) }");

  // Read the config change, but don't read any non-config-change buffer between
  // Seek and Remove.
  scoped_refptr<StreamParserBuffer> buffer;
  CheckVideoConfig(video_config_);
  SeekToTimestampMs(2030);
  CheckVideoConfig(video_config_);
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kConfigChange);
  CheckVideoConfig(new_config);

  // Trigger the re-seek.
  RemoveInMs(2030, 2060, 2060);
  CheckExpectedRangesByTimestamp("{ [2000,2030) }");
  CheckNoNextBuffer();

  // Append buffers at the original timestamps and verify no stall or redundant
  // signalling of config change.
  NewCodedFrameGroupAppend("2030K 2040 2050D10");
  CheckVideoConfig(new_config);
  CheckExpectedRangesByTimestamp("{ [2000,2060) }");
  CheckExpectedBuffers("2030K 2040 2050D10");
  CheckNoNextBuffer();
  CheckVideoConfig(new_config);

  // Seek to the start of buffered and verify config changes and buffers.
  SeekToTimestampMs(2000);
  CheckVideoConfig(new_config);
  ASSERT_FALSE(new_config.Matches(video_config_));
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kConfigChange);
  CheckVideoConfig(video_config_);
  CheckExpectedBuffers("2000K 2010 2020D10");
  CheckVideoConfig(video_config_);
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kConfigChange);
  CheckVideoConfig(new_config);
  CheckExpectedBuffers("2030K 2040 2050D10");
  CheckNoNextBuffer();
  CheckVideoConfig(new_config);
}

TEST_F(SourceBufferStreamTest, TrackBuffer_ExhaustionWithSkipForward) {
  NewCodedFrameGroupAppend("0K 10 20 30 40");

  // Read the first 4 buffers, so next buffer is at time 40.
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,50) }");
  CheckExpectedBuffers("0K 10 20 30");

  // Overlap-append, populating track buffer with timestamp 40 from original
  // append. Confirm there could be a large jump in time until the next key
  // frame after exhausting the track buffer.
  NewCodedFrameGroupAppend(
      "31K 41 51 61 71 81 91 101 111 121 "
      "131K 141");
  CheckExpectedRangesByTimestamp("{ [0,151) }");

  // Confirm the large jump occurs and warning log is generated.
  // If this test is changed, update
  // TrackBufferExhaustion_ImmediateNewTrackBuffer accordingly.
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(91));

  CheckExpectedBuffers("40 131K 141");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       TrackBuffer_ExhaustionAndImmediateNewTrackBuffer) {
  NewCodedFrameGroupAppend("0K 10 20 30 40");

  // Read the first 4 buffers, so next buffer is at time 40.
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,50) }");
  CheckExpectedBuffers("0K 10 20 30");

  // Overlap-append
  NewCodedFrameGroupAppend(
      "31K 41 51 61 71 81 91 101 111 121 "
      "131K 141");
  CheckExpectedRangesByTimestamp("{ [0,151) }");

  // Exhaust the track buffer, but don't read any of the overlapping append yet.
  CheckExpectedBuffers("40");

  // Selected range's next buffer is now the 131K buffer from the overlapping
  // append. (See TrackBuffer_ExhaustionWithSkipForward for that verification.)
  // Do another overlap-append to immediately create another track buffer and
  // verify both track buffer exhaustions skip forward and emit log warnings.
  NewCodedFrameGroupAppend(
      "22K 32 42 52 62 72 82 92 102 112 122K 132 142 152K 162");
  CheckExpectedRangesByTimestamp("{ [0,172) }");

  InSequence s;
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(91));
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(11));

  CheckExpectedBuffers("131K 141 152K 162");
  CheckNoNextBuffer();
}

TEST_F(
    SourceBufferStreamTest,
    AdjacentCodedFrameGroupContinuation_NoGapCreatedByTinyGapInGroupContinuation) {
  NewCodedFrameGroupAppend("0K 10 20K 30 40K 50D10");
  CheckExpectedRangesByTimestamp("{ [0,60) }");

  // Continue appending to the previously started coded frame group, albeit with
  // a tiny (1ms) gap. This gap should *NOT* produce a buffered range gap.
  AppendBuffers("61K 71D10");
  CheckExpectedRangesByTimestamp("{ [0,81) }");
}

TEST_F(SourceBufferStreamTest,
       AdjacentCodedFrameGroupContinuation_NoGapCreatedPrefixRemoved) {
  NewCodedFrameGroupAppend("0K 10 20K 30 40K 50D10");
  CheckExpectedRangesByTimestamp("{ [0,60) }");

  RemoveInMs(0, 35, 60);
  CheckExpectedRangesByTimestamp("{ [40,60) }");

  // Continue appending to the previously started coded frame group, albeit with
  // a tiny (1ms) gap. This gap should *NOT* produce a buffered range gap.
  AppendBuffers("61K 71D10");
  CheckExpectedRangesByTimestamp("{ [40,81) }");
}

TEST_F(SourceBufferStreamTest,
       AdjacentNewCodedFrameGroupContinuation_NoGapCreatedPrefixRemoved) {
  NewCodedFrameGroupAppend("0K 10 20K 30 40K 50D10");
  CheckExpectedRangesByTimestamp("{ [0,60) }");

  RemoveInMs(0, 35, 60);
  CheckExpectedRangesByTimestamp("{ [40,60) }");

  // Continue appending, with a new coded frame group, albeit with
  // a tiny (1ms) gap. This gap should *NOT* produce a buffered range gap.
  // This test demonstrates the "pre-relaxation" behavior, where a new "media
  // segment" (now a new "coded frame group") was signaled at every media
  // segment boundary.
  NewCodedFrameGroupAppend("61K 71D10");
  CheckExpectedRangesByTimestamp("{ [40,81) }");
}

TEST_F(SourceBufferStreamTest,
       StartCodedFrameGroup_RemoveThenAppendMoreMuchLater) {
  NewCodedFrameGroupAppend("1000K 1010 1020 1030K 1040 1050 1060K 1070 1080");
  NewCodedFrameGroupAppend("0K 10 20");
  CheckExpectedRangesByTimestamp("{ [0,30) [1000,1090) }");

  SignalStartOfCodedFrameGroup(base::TimeDelta::FromMilliseconds(1070));
  CheckExpectedRangesByTimestamp("{ [0,30) [1000,1090) }");

  RemoveInMs(1030, 1050, 1090);
  CheckExpectedRangesByTimestamp("{ [0,30) [1000,1030) [1060,1090) }");

  // We've signalled that we're about to do some appends to a coded frame group
  // which starts at time 1070ms. Note that the first frame, if any ever,
  // appended to this SourceBufferStream for that coded frame group must have a
  // decode timestamp >= 1070ms (it can be significantly in the future).
  // Regardless, that appended frame must be buffered into the same existing
  // range as current [1060,1090), since the new coded frame group's start of
  // 1070ms is within that range.
  AppendBuffers("2000K 2010");
  CheckExpectedRangesByTimestamp("{ [0,30) [1000,1030) [1060,2020) }");
  SeekToTimestampMs(1060);
  CheckExpectedBuffers("1060K 2000K 2010");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       StartCodedFrameGroup_InExisting_AppendMuchLater) {
  NewCodedFrameGroupAppend("0K 10 20 30K 40 50");
  SignalStartOfCodedFrameGroup(base::TimeDelta::FromMilliseconds(45));
  CheckExpectedRangesByTimestamp("{ [0,60) }");

  AppendBuffers("2000K 2010");
  CheckExpectedRangesByTimestamp("{ [0,2020) }");
  Seek(0);
  CheckExpectedBuffers("0K 10 20 30K 40 2000K 2010");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       StartCodedFrameGroup_InExisting_RemoveGOP_ThenAppend_1) {
  NewCodedFrameGroupAppend("0K 10 20 30K 40 50");
  SignalStartOfCodedFrameGroup(base::TimeDelta::FromMilliseconds(30));
  RemoveInMs(30, 60, 60);
  CheckExpectedRangesByTimestamp("{ [0,30) }");

  AppendBuffers("2000K 2010");
  CheckExpectedRangesByTimestamp("{ [0,2020) }");
  Seek(0);
  CheckExpectedBuffers("0K 10 20 2000K 2010");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       StartCodedFrameGroup_InExisting_RemoveGOP_ThenAppend_2) {
  NewCodedFrameGroupAppend("0K 10 20 30K 40 50");
  SignalStartOfCodedFrameGroup(base::TimeDelta::FromMilliseconds(45));
  RemoveInMs(30, 60, 60);
  CheckExpectedRangesByTimestamp("{ [0,30) }");

  AppendBuffers("2000K 2010");
  CheckExpectedRangesByTimestamp("{ [0,30) [45,2020) }");
  Seek(0);
  CheckExpectedBuffers("0K 10 20");
  CheckNoNextBuffer();
  SeekToTimestampMs(45);
  CheckExpectedBuffers("2000K 2010");
  CheckNoNextBuffer();
  SeekToTimestampMs(1000);
  CheckExpectedBuffers("2000K 2010");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       StartCodedFrameGroup_InExisting_RemoveMostRecentAppend_ThenAppend_1) {
  NewCodedFrameGroupAppend("0K 10 20 30K 40 50");
  SignalStartOfCodedFrameGroup(base::TimeDelta::FromMilliseconds(45));
  RemoveInMs(50, 60, 60);
  CheckExpectedRangesByTimestamp("{ [0,50) }");

  AppendBuffers("2000K 2010");
  CheckExpectedRangesByTimestamp("{ [0,2020) }");
  Seek(0);
  CheckExpectedBuffers("0K 10 20 30K 40 2000K 2010");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       StartCodedFrameGroup_InExisting_RemoveMostRecentAppend_ThenAppend_2) {
  NewCodedFrameGroupAppend("0K 10 20 30K 40 50");
  SignalStartOfCodedFrameGroup(base::TimeDelta::FromMilliseconds(50));
  RemoveInMs(50, 60, 60);
  CheckExpectedRangesByTimestamp("{ [0,50) }");

  AppendBuffers("2000K 2010");
  CheckExpectedRangesByTimestamp("{ [0,2020) }");
  Seek(0);
  CheckExpectedBuffers("0K 10 20 30K 40 2000K 2010");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GetHighestPresentationTimestamp) {
  // TODO(wolenetz): Add coverage for when DTS != PTS once
  // https://crbug.com/398130 is fixed.

  EXPECT_EQ(base::TimeDelta(), stream_->GetHighestPresentationTimestamp());

  NewCodedFrameGroupAppend("0K 10K");
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(10),
            stream_->GetHighestPresentationTimestamp());

  RemoveInMs(0, 10, 20);
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(10),
            stream_->GetHighestPresentationTimestamp());

  RemoveInMs(10, 20, 20);
  EXPECT_EQ(base::TimeDelta(), stream_->GetHighestPresentationTimestamp());

  NewCodedFrameGroupAppend("0K 10K");
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(10),
            stream_->GetHighestPresentationTimestamp());

  RemoveInMs(10, 20, 20);
  EXPECT_EQ(base::TimeDelta(), stream_->GetHighestPresentationTimestamp());
}

// TODO(vrk): Add unit tests where keyframes are unaligned between streams.
// (crbug.com/133557)

// TODO(vrk): Add unit tests with end of stream being called at interesting
// times.

}  // namespace media
