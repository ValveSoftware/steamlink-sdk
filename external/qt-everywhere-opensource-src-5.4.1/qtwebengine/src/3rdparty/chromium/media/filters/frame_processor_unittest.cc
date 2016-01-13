// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/frame_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::StrictMock;
using ::testing::Values;

namespace media {

typedef StreamParser::BufferQueue BufferQueue;
typedef StreamParser::TextBufferQueueMap TextBufferQueueMap;
typedef StreamParser::TrackId TrackId;

static void LogFunc(const std::string& str) { DVLOG(1) << str; }

// Used for setting expectations on callbacks. Using a StrictMock also lets us
// test for missing or extra callbacks.
class FrameProcessorTestCallbackHelper {
 public:
  FrameProcessorTestCallbackHelper() {}
  virtual ~FrameProcessorTestCallbackHelper() {}

  MOCK_METHOD1(PossibleDurationIncrease, void(base::TimeDelta new_duration));

  // Helper that calls the mock method as well as does basic sanity checks on
  // |new_duration|.
  void OnPossibleDurationIncrease(base::TimeDelta new_duration) {
    PossibleDurationIncrease(new_duration);
    ASSERT_NE(kNoTimestamp(), new_duration);
    ASSERT_NE(kInfiniteDuration(), new_duration);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameProcessorTestCallbackHelper);
};

// Test parameter determines indicates if the TEST_P instance is targeted for
// sequence mode (if true), or segments mode (if false).
class FrameProcessorTest : public testing::TestWithParam<bool> {
 protected:
  FrameProcessorTest()
      : frame_processor_(new FrameProcessor(base::Bind(
            &FrameProcessorTestCallbackHelper::OnPossibleDurationIncrease,
            base::Unretained(&callbacks_)))),
        append_window_end_(kInfiniteDuration()),
        new_media_segment_(false),
        audio_id_(FrameProcessor::kAudioTrackId),
        video_id_(FrameProcessor::kVideoTrackId),
        frame_duration_(base::TimeDelta::FromMilliseconds(10)) {
  }

  enum StreamFlags {
    HAS_AUDIO = 1 << 0,
    HAS_VIDEO = 1 << 1
  };

  void AddTestTracks(int stream_flags) {
    const bool has_audio = (stream_flags & HAS_AUDIO) != 0;
    const bool has_video = (stream_flags & HAS_VIDEO) != 0;
    ASSERT_TRUE(has_audio || has_video);

    if (has_audio) {
      CreateAndConfigureStream(DemuxerStream::AUDIO);
      ASSERT_TRUE(audio_);
      EXPECT_TRUE(frame_processor_->AddTrack(audio_id_, audio_.get()));
      audio_->Seek(base::TimeDelta());
      audio_->StartReturningData();
    }
    if (has_video) {
      CreateAndConfigureStream(DemuxerStream::VIDEO);
      ASSERT_TRUE(video_);
      EXPECT_TRUE(frame_processor_->AddTrack(video_id_, video_.get()));
      video_->Seek(base::TimeDelta());
      video_->StartReturningData();
    }
  }

  void SetTimestampOffset(base::TimeDelta new_offset) {
    timestamp_offset_ = new_offset;
    frame_processor_->SetGroupStartTimestampIfInSequenceMode(timestamp_offset_);
  }

  BufferQueue StringToBufferQueue(const std::string& buffers_to_append,
                                  const TrackId track_id,
                                  const DemuxerStream::Type type) {
    std::vector<std::string> timestamps;
    base::SplitString(buffers_to_append, ' ', &timestamps);

    BufferQueue buffers;
    for (size_t i = 0; i < timestamps.size(); i++) {
      bool is_keyframe = false;
      if (EndsWith(timestamps[i], "K", true)) {
        is_keyframe = true;
        // Remove the "K" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }

      double time_in_ms;
      CHECK(base::StringToDouble(timestamps[i], &time_in_ms));

      // Create buffer. Encode the original time_in_ms as the buffer's data to
      // enable later verification of possible buffer relocation in presentation
      // timeline due to coded frame processing.
      const uint8* timestamp_as_data = reinterpret_cast<uint8*>(&time_in_ms);
      scoped_refptr<StreamParserBuffer> buffer =
          StreamParserBuffer::CopyFrom(timestamp_as_data, sizeof(time_in_ms),
                                       is_keyframe, type, track_id);
      base::TimeDelta timestamp = base::TimeDelta::FromSecondsD(
          time_in_ms / base::Time::kMillisecondsPerSecond);
      buffer->set_timestamp(timestamp);
      buffer->SetDecodeTimestamp(timestamp);
      buffer->set_duration(frame_duration_);
      buffers.push_back(buffer);
    }
    return buffers;
  }

  void ProcessFrames(const std::string& audio_timestamps,
                     const std::string& video_timestamps) {
    ASSERT_TRUE(frame_processor_->ProcessFrames(
        StringToBufferQueue(audio_timestamps, audio_id_, DemuxerStream::AUDIO),
        StringToBufferQueue(video_timestamps, video_id_, DemuxerStream::VIDEO),
        empty_text_buffers_,
        append_window_start_, append_window_end_,
        &new_media_segment_, &timestamp_offset_));
  }

  void CheckExpectedRangesByTimestamp(ChunkDemuxerStream* stream,
                                      const std::string& expected) {
    // Note, DemuxerStream::TEXT streams return [0,duration (==infinity here))
    Ranges<base::TimeDelta> r = stream->GetBufferedRanges(kInfiniteDuration());

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

  void CheckReadStalls(ChunkDemuxerStream* stream) {
    int loop_count = 0;

    do {
      read_callback_called_ = false;
      stream->Read(base::Bind(&FrameProcessorTest::StoreStatusAndBuffer,
                              base::Unretained(this)));
      message_loop_.RunUntilIdle();
    } while (++loop_count < 2 && read_callback_called_ &&
             last_read_status_ == DemuxerStream::kAborted);

    ASSERT_FALSE(read_callback_called_ &&
                 last_read_status_ == DemuxerStream::kAborted)
        << "2 kAborted reads in a row. Giving up.";
    EXPECT_FALSE(read_callback_called_);
  }

  // Format of |expected| is a space-delimited sequence of
  // timestamp_in_ms:original_timestamp_in_ms
  // original_timestamp_in_ms (and the colon) must be omitted if it is the same
  // as timestamp_in_ms.
  void CheckReadsThenReadStalls(ChunkDemuxerStream* stream,
                                const std::string& expected) {
    std::vector<std::string> timestamps;
    base::SplitString(expected, ' ', &timestamps);
    std::stringstream ss;
    for (size_t i = 0; i < timestamps.size(); ++i) {
      int loop_count = 0;

      do {
        read_callback_called_ = false;
        stream->Read(base::Bind(&FrameProcessorTest::StoreStatusAndBuffer,
                                base::Unretained(this)));
        message_loop_.RunUntilIdle();
        EXPECT_TRUE(read_callback_called_);
      } while (++loop_count < 2 &&
               last_read_status_ == DemuxerStream::kAborted);

      ASSERT_FALSE(last_read_status_ == DemuxerStream::kAborted)
          << "2 kAborted reads in a row. Giving up.";
      EXPECT_EQ(DemuxerStream::kOk, last_read_status_);
      EXPECT_FALSE(last_read_buffer_->end_of_stream());

      if (i > 0)
        ss << " ";

      int time_in_ms = last_read_buffer_->timestamp().InMilliseconds();
      ss << time_in_ms;

      // Decode the original_time_in_ms from the buffer's data.
      double original_time_in_ms;
      ASSERT_EQ(static_cast<int>(sizeof(original_time_in_ms)),
                last_read_buffer_->data_size());
      original_time_in_ms = *(reinterpret_cast<const double*>(
          last_read_buffer_->data()));
      if (original_time_in_ms != time_in_ms)
        ss << ":" << original_time_in_ms;

      // Detect full-discard preroll buffer.
      if (last_read_buffer_->discard_padding().first == kInfiniteDuration() &&
          last_read_buffer_->discard_padding().second == base::TimeDelta()) {
        ss << "P";
      }
    }

    EXPECT_EQ(expected, ss.str());
    CheckReadStalls(stream);
  }

  base::MessageLoop message_loop_;
  StrictMock<FrameProcessorTestCallbackHelper> callbacks_;

  scoped_ptr<FrameProcessor> frame_processor_;
  base::TimeDelta append_window_start_;
  base::TimeDelta append_window_end_;
  bool new_media_segment_;
  base::TimeDelta timestamp_offset_;
  scoped_ptr<ChunkDemuxerStream> audio_;
  scoped_ptr<ChunkDemuxerStream> video_;
  const TrackId audio_id_;
  const TrackId video_id_;
  const base::TimeDelta frame_duration_;  // Currently the same for all streams.
  const BufferQueue empty_queue_;
  const TextBufferQueueMap empty_text_buffers_;

  // StoreStatusAndBuffer's most recent result.
  DemuxerStream::Status last_read_status_;
  scoped_refptr<DecoderBuffer> last_read_buffer_;
  bool read_callback_called_;

 private:
  void StoreStatusAndBuffer(DemuxerStream::Status status,
                            const scoped_refptr<DecoderBuffer>& buffer) {
    if (status == DemuxerStream::kOk && buffer) {
      DVLOG(3) << __FUNCTION__ << "status: " << status << " ts: "
               << buffer->timestamp().InSecondsF();
    } else {
      DVLOG(3) << __FUNCTION__ << "status: " << status << " ts: n/a";
    }

    read_callback_called_ = true;
    last_read_status_ = status;
    last_read_buffer_ = buffer;
  }

  void CreateAndConfigureStream(DemuxerStream::Type type) {
    // TODO(wolenetz/dalecurtis): Also test with splicing disabled?
    switch (type) {
      case DemuxerStream::AUDIO: {
        ASSERT_FALSE(audio_);
        audio_.reset(new ChunkDemuxerStream(DemuxerStream::AUDIO, true));
        AudioDecoderConfig decoder_config(kCodecVorbis,
                                          kSampleFormatPlanarF32,
                                          CHANNEL_LAYOUT_STEREO,
                                          1000,
                                          NULL,
                                          0,
                                          false);
        frame_processor_->OnPossibleAudioConfigUpdate(decoder_config);
        ASSERT_TRUE(
            audio_->UpdateAudioConfig(decoder_config, base::Bind(&LogFunc)));
        break;
      }
      case DemuxerStream::VIDEO: {
        ASSERT_FALSE(video_);
        video_.reset(new ChunkDemuxerStream(DemuxerStream::VIDEO, true));
        ASSERT_TRUE(video_->UpdateVideoConfig(TestVideoConfig::Normal(),
                                              base::Bind(&LogFunc)));
        break;
      }
      // TODO(wolenetz): Test text coded frame processing.
      case DemuxerStream::TEXT:
      case DemuxerStream::UNKNOWN:
      case DemuxerStream::NUM_TYPES: {
        ASSERT_FALSE(true);
      }
    }
  }

  DISALLOW_COPY_AND_ASSIGN(FrameProcessorTest);
};

TEST_F(FrameProcessorTest, WrongTypeInAppendedBuffer) {
  AddTestTracks(HAS_AUDIO);
  new_media_segment_ = true;

  ASSERT_FALSE(frame_processor_->ProcessFrames(
      StringToBufferQueue("0K", audio_id_, DemuxerStream::VIDEO),
      empty_queue_,
      empty_text_buffers_,
      append_window_start_, append_window_end_,
      &new_media_segment_, &timestamp_offset_));
  EXPECT_TRUE(new_media_segment_);
  EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ }");
  CheckReadStalls(audio_.get());
}

TEST_F(FrameProcessorTest, NonMonotonicallyIncreasingTimestampInOneCall) {
  AddTestTracks(HAS_AUDIO);
  new_media_segment_ = true;

  ASSERT_FALSE(frame_processor_->ProcessFrames(
      StringToBufferQueue("10K 0K", audio_id_, DemuxerStream::AUDIO),
      empty_queue_,
      empty_text_buffers_,
      append_window_start_, append_window_end_,
      &new_media_segment_, &timestamp_offset_));
  EXPECT_TRUE(new_media_segment_);
  EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ }");
  CheckReadStalls(audio_.get());
}

TEST_P(FrameProcessorTest, AudioOnly_SingleFrame) {
  // Tests A: P(A) -> (a)
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  new_media_segment_ = true;
  if (GetParam())
    frame_processor_->SetSequenceMode(true);

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_));
  ProcessFrames("0K", "");
  EXPECT_FALSE(new_media_segment_);
  EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,10) }");
  CheckReadsThenReadStalls(audio_.get(), "0");
}

TEST_P(FrameProcessorTest, VideoOnly_SingleFrame) {
  // Tests V: P(V) -> (v)
  InSequence s;
  AddTestTracks(HAS_VIDEO);
  new_media_segment_ = true;
  if (GetParam())
    frame_processor_->SetSequenceMode(true);

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_));
  ProcessFrames("", "0K");
  EXPECT_FALSE(new_media_segment_);
  EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
  CheckExpectedRangesByTimestamp(video_.get(), "{ [0,10) }");
  CheckReadsThenReadStalls(video_.get(), "0");
}

TEST_P(FrameProcessorTest, AudioOnly_TwoFrames) {
  // Tests A: P(A0, A10) -> (a0, a10)
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  new_media_segment_ = true;
  if (GetParam())
    frame_processor_->SetSequenceMode(true);

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ * 2));
  ProcessFrames("0K 10K", "");
  EXPECT_FALSE(new_media_segment_);
  EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,20) }");
  CheckReadsThenReadStalls(audio_.get(), "0 10");
}

TEST_P(FrameProcessorTest, AudioOnly_SetOffsetThenSingleFrame) {
  // Tests A: STSO(50)+P(A0) -> TSO==50,(a0@50)
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  new_media_segment_ = true;
  if (GetParam())
    frame_processor_->SetSequenceMode(true);

  const base::TimeDelta fifty_ms = base::TimeDelta::FromMilliseconds(50);
  SetTimestampOffset(fifty_ms);
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ + fifty_ms));
  ProcessFrames("0K", "");
  EXPECT_FALSE(new_media_segment_);
  EXPECT_EQ(fifty_ms, timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [50,60) }");

  // We do not stall on reading without seeking to 50ms due to
  // SourceBufferStream::kSeekToStartFudgeRoom().
  CheckReadsThenReadStalls(audio_.get(), "50:0");
}

TEST_P(FrameProcessorTest, AudioOnly_SetOffsetThenFrameTimestampBelowOffset) {
  // Tests A: STSO(50)+P(A20) ->
  //   if sequence mode: TSO==30,(a20@50)
  //   if segments mode: TSO==50,(a20@70)
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  new_media_segment_ = true;
  bool using_sequence_mode = GetParam();
  if (using_sequence_mode)
    frame_processor_->SetSequenceMode(true);

  const base::TimeDelta fifty_ms = base::TimeDelta::FromMilliseconds(50);
  const base::TimeDelta twenty_ms = base::TimeDelta::FromMilliseconds(20);
  SetTimestampOffset(fifty_ms);

  if (using_sequence_mode) {
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(
        fifty_ms + frame_duration_));
  } else {
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(
        fifty_ms + twenty_ms + frame_duration_));
  }

  ProcessFrames("20K", "");
  EXPECT_FALSE(new_media_segment_);

  // We do not stall on reading without seeking to 50ms / 70ms due to
  // SourceBufferStream::kSeekToStartFudgeRoom().
  if (using_sequence_mode) {
    EXPECT_EQ(fifty_ms - twenty_ms, timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [50,60) }");
    CheckReadsThenReadStalls(audio_.get(), "50:20");
  } else {
    EXPECT_EQ(fifty_ms, timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [70,80) }");
    CheckReadsThenReadStalls(audio_.get(), "70:20");
  }
}

TEST_P(FrameProcessorTest, AudioOnly_SequentialProcessFrames) {
  // Tests A: P(A0,A10)+P(A20,A30) -> (a0,a10,a20,a30)
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  new_media_segment_ = true;
  if (GetParam())
    frame_processor_->SetSequenceMode(true);

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ * 2));
  ProcessFrames("0K 10K", "");
  EXPECT_FALSE(new_media_segment_);
  EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,20) }");

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ * 4));
  ProcessFrames("20K 30K", "");
  EXPECT_FALSE(new_media_segment_);
  EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,40) }");

  CheckReadsThenReadStalls(audio_.get(), "0 10 20 30");
}

TEST_P(FrameProcessorTest, AudioOnly_NonSequentialProcessFrames) {
  // Tests A: P(A20,A30)+P(A0,A10) ->
  //   if sequence mode: TSO==-20 after first P(), 20 after second P(), and
  //                     a(20@0,a30@10,a0@20,a10@30)
  //   if segments mode: TSO==0,(a0,a10,a20,a30)
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  new_media_segment_ = true;
  bool using_sequence_mode = GetParam();
  if (using_sequence_mode) {
    frame_processor_->SetSequenceMode(true);
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ * 2));
  } else {
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ * 4));
  }

  ProcessFrames("20K 30K", "");
  EXPECT_FALSE(new_media_segment_);

  if (using_sequence_mode) {
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,20) }");
    EXPECT_EQ(frame_duration_ * -2, timestamp_offset_);
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ * 4));
  } else {
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [20,40) }");
    EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ * 2));
  }

  ProcessFrames("0K 10K", "");
  EXPECT_FALSE(new_media_segment_);

  if (using_sequence_mode) {
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,40) }");
    EXPECT_EQ(frame_duration_ * 2, timestamp_offset_);
    CheckReadsThenReadStalls(audio_.get(), "0:20 10:30 20:0 30:10");
  } else {
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,40) }");
    EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
    // TODO(wolenetz): Fix this need to seek to 0ms, possibly by having
    // SourceBufferStream defer initial seek until next read. See
    // http://crbug.com/371493.
    audio_->AbortReads();
    audio_->Seek(base::TimeDelta());
    audio_->StartReturningData();
    CheckReadsThenReadStalls(audio_.get(), "0 10 20 30");
  }
}

TEST_P(FrameProcessorTest, AudioVideo_SequentialProcessFrames) {
  // Tests AV: P(A0,A10;V0k,V10,V20)+P(A20,A30,A40,V30) ->
  //   (a0,a10,a20,a30,a40);(v0,v10,v20,v30)
  InSequence s;
  AddTestTracks(HAS_AUDIO | HAS_VIDEO);
  new_media_segment_ = true;
  if (GetParam())
    frame_processor_->SetSequenceMode(true);

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ * 3));
  ProcessFrames("0K 10K", "0K 10 20");
  EXPECT_FALSE(new_media_segment_);
  EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,20) }");
  CheckExpectedRangesByTimestamp(video_.get(), "{ [0,30) }");

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ * 5));
  ProcessFrames("20K 30K 40K", "30");
  EXPECT_FALSE(new_media_segment_);
  EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,50) }");
  CheckExpectedRangesByTimestamp(video_.get(), "{ [0,40) }");

  CheckReadsThenReadStalls(audio_.get(), "0 10 20 30 40");
  CheckReadsThenReadStalls(video_.get(), "0 10 20 30");
}

TEST_P(FrameProcessorTest, AudioVideo_Discontinuity) {
  // Tests AV: P(A0,A10,A30,A40,A50;V0k,V10,V40,V50key) ->
  //   if sequence mode: TSO==10,(a0,a10,a30,a40,a50@60);(v0,v10,v50@60)
  //   if segments mode: TSO==0,(a0,a10,a30,a40,a50);(v0,v10,v50)
  // This assumes A40K is processed before V40, which depends currently on
  // MergeBufferQueues() behavior.
  InSequence s;
  AddTestTracks(HAS_AUDIO | HAS_VIDEO);
  new_media_segment_ = true;
  bool using_sequence_mode = GetParam();
  if (using_sequence_mode) {
    frame_processor_->SetSequenceMode(true);
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ * 7));
  } else {
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ * 6));
  }

  ProcessFrames("0K 10K 30K 40K 50K", "0K 10 40 50K");
  EXPECT_FALSE(new_media_segment_);

  if (using_sequence_mode) {
    EXPECT_EQ(frame_duration_, timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,70) }");
    CheckExpectedRangesByTimestamp(video_.get(), "{ [0,70) }");
    CheckReadsThenReadStalls(audio_.get(), "0 10 30 40 60:50");
    CheckReadsThenReadStalls(video_.get(), "0 10 60:50");
  } else {
    EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,60) }");
    CheckExpectedRangesByTimestamp(video_.get(), "{ [0,20) [50,60) }");
    CheckReadsThenReadStalls(audio_.get(), "0 10 30 40 50");
    CheckReadsThenReadStalls(video_.get(), "0 10");
    video_->AbortReads();
    video_->Seek(frame_duration_ * 5);
    video_->StartReturningData();
    CheckReadsThenReadStalls(video_.get(), "50");
  }
}

TEST_P(FrameProcessorTest,
       AppendWindowFilterOfNegativeBufferTimestampsWithPrerollDiscard) {
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  new_media_segment_ = true;
  if (GetParam())
    frame_processor_->SetSequenceMode(true);

  SetTimestampOffset(frame_duration_ * -2);
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_));
  ProcessFrames("0K 10K 20K", "");
  EXPECT_FALSE(new_media_segment_);
  EXPECT_EQ(frame_duration_ * -2, timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,10) }");
  CheckReadsThenReadStalls(audio_.get(), "0:10P 0:20");
}

TEST_P(FrameProcessorTest, AppendWindowFilterWithInexactPreroll) {
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  new_media_segment_ = true;
  if (GetParam())
    frame_processor_->SetSequenceMode(true);
  SetTimestampOffset(-frame_duration_);
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ * 2));
  ProcessFrames("0K 9.75K 20K", "");
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,20) }");
  CheckReadsThenReadStalls(audio_.get(), "0P 0:9.75 10:20");
}

TEST_P(FrameProcessorTest, AllowNegativeFramePTSAndDTSBeforeOffsetAdjustment) {
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  new_media_segment_ = true;
  bool using_sequence_mode = GetParam();
  if (using_sequence_mode) {
    frame_processor_->SetSequenceMode(true);
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(frame_duration_ * 3));
  } else {
    EXPECT_CALL(callbacks_,
                PossibleDurationIncrease((frame_duration_ * 5) / 2));
  }

  ProcessFrames("-5K 5K 15K", "");

  if (using_sequence_mode) {
    EXPECT_EQ(frame_duration_ / 2, timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,30) }");
    CheckReadsThenReadStalls(audio_.get(), "0:-5 10:5 20:15");
  } else {
    EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,25) }");
    CheckReadsThenReadStalls(audio_.get(), "0:-5 5 15");
  }
}

TEST_P(FrameProcessorTest, PartialAppendWindowFilterNoDiscontinuity) {
  // Tests that spurious discontinuity is not introduced by a partially
  // trimmed frame.
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  new_media_segment_ = true;
  if (GetParam())
    frame_processor_->SetSequenceMode(true);
  EXPECT_CALL(callbacks_,
              PossibleDurationIncrease(base::TimeDelta::FromMilliseconds(29)));

  append_window_start_ = base::TimeDelta::FromMilliseconds(7);
  ProcessFrames("0K 19K", "");

  EXPECT_EQ(base::TimeDelta(), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [7,29) }");
  CheckReadsThenReadStalls(audio_.get(), "7:0 19");
}

INSTANTIATE_TEST_CASE_P(SequenceMode, FrameProcessorTest, Values(true));
INSTANTIATE_TEST_CASE_P(SegmentsMode, FrameProcessorTest, Values(false));

}  // namespace media
