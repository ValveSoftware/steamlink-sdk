// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_splicer.h"

#include <memory>

#include "base/macros.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Do not change this format.  AddInput() and GetValue() only work with float.
static const SampleFormat kSampleFormat = kSampleFormatF32;
static_assert(kSampleFormat == kSampleFormatF32, "invalid splice format");

static const int kChannels = 1;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_MONO;
static const int kDefaultSampleRate = 44100;
static const int kDefaultBufferSize = 100;

class AudioSplicerTest : public ::testing::Test {
 public:
  AudioSplicerTest()
      : splicer_(kDefaultSampleRate, new MediaLog()),
        input_timestamp_helper_(kDefaultSampleRate) {
    input_timestamp_helper_.SetBaseTimestamp(base::TimeDelta());
  }

  scoped_refptr<AudioBuffer> GetNextInputBuffer(float value) {
    return GetNextInputBuffer(value, kDefaultBufferSize);
  }

  scoped_refptr<AudioBuffer> GetNextInputBuffer(float value, int frame_size) {
    scoped_refptr<AudioBuffer> buffer =
        MakeAudioBuffer<float>(kSampleFormat,
                               kChannelLayout,
                               kChannels,
                               kDefaultSampleRate,
                               value,
                               0.0f,
                               frame_size,
                               input_timestamp_helper_.GetTimestamp());
    input_timestamp_helper_.AddFrames(frame_size);
    return buffer;
  }

  float GetValue(const scoped_refptr<AudioBuffer>& buffer) {
    return reinterpret_cast<const float*>(buffer->channel_data()[0])[0];
  }

  bool VerifyData(const scoped_refptr<AudioBuffer>& buffer, float value) {
    int frames = buffer->frame_count();
    std::unique_ptr<AudioBus> bus = AudioBus::Create(kChannels, frames);
    buffer->ReadFrames(frames, 0, 0, bus.get());
    for (int ch = 0; ch < buffer->channel_count(); ++ch) {
      for (int i = 0; i < frames; ++i) {
        if (bus->channel(ch)[i] != value)
          return false;
      }
    }
    return true;
  }

  void VerifyNextBuffer(const scoped_refptr<AudioBuffer>& input) {
    ASSERT_TRUE(splicer_.HasNextBuffer());
    scoped_refptr<AudioBuffer> output = splicer_.GetNextBuffer();
    EXPECT_EQ(input->timestamp(), output->timestamp());
    EXPECT_EQ(input->duration(), output->duration());
    EXPECT_EQ(input->frame_count(), output->frame_count());
    EXPECT_TRUE(VerifyData(output, GetValue(input)));
  }

  void VerifyPreSpliceOutput(
      const scoped_refptr<AudioBuffer>& overlapped_buffer,
      const scoped_refptr<AudioBuffer>& overlapping_buffer,
      int expected_pre_splice_size,
      base::TimeDelta expected_pre_splice_duration) {
    ASSERT_TRUE(splicer_.HasNextBuffer());
    scoped_refptr<AudioBuffer> pre_splice_output = splicer_.GetNextBuffer();
    EXPECT_EQ(overlapped_buffer->timestamp(), pre_splice_output->timestamp());
    EXPECT_EQ(expected_pre_splice_size, pre_splice_output->frame_count());
    EXPECT_EQ(expected_pre_splice_duration, pre_splice_output->duration());
    EXPECT_TRUE(VerifyData(pre_splice_output, GetValue(overlapped_buffer)));
  }

  void VerifyCrossfadeOutput(
      const scoped_refptr<AudioBuffer>& overlapped_buffer_1,
      const scoped_refptr<AudioBuffer>& overlapped_buffer_2,
      const scoped_refptr<AudioBuffer>& overlapping_buffer,
      int second_overlap_index,
      int expected_crossfade_size,
      base::TimeDelta expected_crossfade_duration) {
    ASSERT_TRUE(splicer_.HasNextBuffer());

    scoped_refptr<AudioBuffer> crossfade_output = splicer_.GetNextBuffer();
    EXPECT_EQ(expected_crossfade_size, crossfade_output->frame_count());
    EXPECT_EQ(expected_crossfade_duration, crossfade_output->duration());

    // The splice timestamp may be adjusted by a microsecond.
    EXPECT_NEAR(overlapping_buffer->timestamp().InMicroseconds(),
                crossfade_output->timestamp().InMicroseconds(),
                1);

    // Verify the actual crossfade.
    const int frames = crossfade_output->frame_count();
    float overlapped_value = GetValue(overlapped_buffer_1);
    const float overlapping_value = GetValue(overlapping_buffer);
    std::unique_ptr<AudioBus> bus = AudioBus::Create(kChannels, frames);
    crossfade_output->ReadFrames(frames, 0, 0, bus.get());
    for (int ch = 0; ch < crossfade_output->channel_count(); ++ch) {
      float cf_ratio = 0;
      const float cf_increment = 1.0f / frames;
      for (int i = 0; i < frames; ++i, cf_ratio += cf_increment) {
        if (overlapped_buffer_2.get() && i >= second_overlap_index)
          overlapped_value = GetValue(overlapped_buffer_2);
        const float actual = bus->channel(ch)[i];
        const float expected =
            (1.0f - cf_ratio) * overlapped_value + cf_ratio * overlapping_value;
        ASSERT_FLOAT_EQ(expected, actual) << "i=" << i;
      }
    }
  }

  bool AddInput(const scoped_refptr<AudioBuffer>& input) {
    // Since the splicer doesn't make copies it's working directly on the input
    // buffers.  We must make a copy before adding to ensure the original buffer
    // is not modified in unexpected ways.
    scoped_refptr<AudioBuffer> buffer_copy =
        input->end_of_stream()
            ? AudioBuffer::CreateEOSBuffer()
            : AudioBuffer::CopyFrom(kSampleFormat,
                                    input->channel_layout(),
                                    input->channel_count(),
                                    input->sample_rate(),
                                    input->frame_count(),
                                    &input->channel_data()[0],
                                    input->timestamp());
    return splicer_.AddInput(buffer_copy);
  }

  base::TimeDelta max_crossfade_duration() {
    return splicer_.max_crossfade_duration_;
  }

 protected:
  AudioSplicer splicer_;
  AudioTimestampHelper input_timestamp_helper_;

  DISALLOW_COPY_AND_ASSIGN(AudioSplicerTest);
};

TEST_F(AudioSplicerTest, PassThru) {
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Test single buffer pass-thru behavior.
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);
  EXPECT_TRUE(AddInput(input_1));
  VerifyNextBuffer(input_1);
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Test that multiple buffers can be queued in the splicer.
  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.2f);
  scoped_refptr<AudioBuffer> input_3 = GetNextInputBuffer(0.3f);
  EXPECT_TRUE(AddInput(input_2));
  EXPECT_TRUE(AddInput(input_3));
  VerifyNextBuffer(input_2);
  VerifyNextBuffer(input_3);
  EXPECT_FALSE(splicer_.HasNextBuffer());
}

TEST_F(AudioSplicerTest, Reset) {
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);
  EXPECT_TRUE(AddInput(input_1));
  ASSERT_TRUE(splicer_.HasNextBuffer());

  splicer_.Reset();
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Add some bytes to the timestamp helper so that the
  // next buffer starts many frames beyond the end of
  // |input_1|. This is to make sure that Reset() actually
  // clears its state and doesn't try to insert a gap.
  input_timestamp_helper_.AddFrames(100);

  // Verify that a new input buffer passes through as expected.
  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.2f);
  EXPECT_TRUE(AddInput(input_2));
  VerifyNextBuffer(input_2);
  EXPECT_FALSE(splicer_.HasNextBuffer());
}

TEST_F(AudioSplicerTest, EndOfStream) {
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);
  scoped_refptr<AudioBuffer> input_2 = AudioBuffer::CreateEOSBuffer();
  scoped_refptr<AudioBuffer> input_3 = GetNextInputBuffer(0.2f);
  EXPECT_TRUE(input_2->end_of_stream());

  EXPECT_TRUE(AddInput(input_1));
  EXPECT_TRUE(AddInput(input_2));

  VerifyNextBuffer(input_1);

  scoped_refptr<AudioBuffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());
  EXPECT_TRUE(output_2->end_of_stream());

  // Verify that buffers can be added again after Reset().
  splicer_.Reset();
  EXPECT_TRUE(AddInput(input_3));
  VerifyNextBuffer(input_3);
  EXPECT_FALSE(splicer_.HasNextBuffer());
}

// Test the gap insertion code.
// +--------------+    +--------------+
// |11111111111111|    |22222222222222|
// +--------------+    +--------------+
// Results in:
// +--------------+----+--------------+
// |11111111111111|0000|22222222222222|
// +--------------+----+--------------+
TEST_F(AudioSplicerTest, GapInsertion) {
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);

  // Add bytes to the timestamp helper so that the next buffer
  // will have a starting timestamp that indicates a gap is
  // present.
  const int kGapSize = 7;
  input_timestamp_helper_.AddFrames(kGapSize);
  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.2f);

  EXPECT_TRUE(AddInput(input_1));
  EXPECT_TRUE(AddInput(input_2));

  // Verify that the first input buffer passed through unmodified.
  VerifyNextBuffer(input_1);

  // Verify the contents of the gap buffer.
  scoped_refptr<AudioBuffer> output_2 = splicer_.GetNextBuffer();
  base::TimeDelta gap_timestamp =
      input_1->timestamp() + input_1->duration();
  base::TimeDelta gap_duration = input_2->timestamp() - gap_timestamp;
  EXPECT_GT(gap_duration, base::TimeDelta());
  EXPECT_EQ(gap_timestamp, output_2->timestamp());
  EXPECT_NEAR(
      gap_duration.InMicroseconds(), output_2->duration().InMicroseconds(), 1);
  EXPECT_EQ(kGapSize, output_2->frame_count());
  EXPECT_TRUE(VerifyData(output_2, 0.0f));

  // Verify that the second input buffer passed through unmodified.
  VerifyNextBuffer(input_2);
  EXPECT_FALSE(splicer_.HasNextBuffer());
}

// Test that an error is signalled when the gap between input buffers is
// too large.
TEST_F(AudioSplicerTest, GapTooLarge) {
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);

  // Add a seconds worth of bytes so that an unacceptably large
  // gap exists between |input_1| and |input_2|.
  const int kGapSize = kDefaultSampleRate;
  input_timestamp_helper_.AddFrames(kGapSize);
  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.2f);

  EXPECT_TRUE(AddInput(input_1));
  EXPECT_FALSE(AddInput(input_2));

  VerifyNextBuffer(input_1);

  // Verify that the second buffer is not available.
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Reset the timestamp helper so it can generate a buffer that is
  // right after |input_1|.
  input_timestamp_helper_.SetBaseTimestamp(
      input_1->timestamp() + input_1->duration());

  // Verify that valid buffers are still accepted.
  scoped_refptr<AudioBuffer> input_3 = GetNextInputBuffer(0.3f);
  EXPECT_TRUE(AddInput(input_3));
  VerifyNextBuffer(input_3);
  EXPECT_FALSE(splicer_.HasNextBuffer());
}

// Verifies that an error is signalled if AddInput() is called
// with a timestamp that is earlier than the first buffer added.
TEST_F(AudioSplicerTest, BufferAddedBeforeBase) {
  input_timestamp_helper_.SetBaseTimestamp(
      base::TimeDelta::FromMicroseconds(10));
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);

  // Reset the timestamp helper so the next buffer will have a timestamp earlier
  // than |input_1|.
  input_timestamp_helper_.SetBaseTimestamp(base::TimeDelta::FromSeconds(0));
  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.1f);

  EXPECT_GT(input_1->timestamp(), input_2->timestamp());
  EXPECT_TRUE(AddInput(input_1));
  EXPECT_FALSE(AddInput(input_2));
}

// Test when one buffer partially overlaps another.
// +--------------+
// |11111111111111|
// +--------------+
//            +--------------+
//            |22222222222222|
//            +--------------+
// Results in:
// +--------------+----------+
// |11111111111111|2222222222|
// +--------------+----------+
TEST_F(AudioSplicerTest, PartialOverlap) {
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);

  // Reset timestamp helper so that the next buffer will have a
  // timestamp that starts in the middle of |input_1|.
  const int kOverlapSize = input_1->frame_count() / 4;
  input_timestamp_helper_.SetBaseTimestamp(input_1->timestamp());
  input_timestamp_helper_.AddFrames(input_1->frame_count() - kOverlapSize);

  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.2f);

  EXPECT_TRUE(AddInput(input_1));
  EXPECT_TRUE(AddInput(input_2));

  // Verify that the first input buffer passed through unmodified.
  VerifyNextBuffer(input_1);

  ASSERT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<AudioBuffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Verify that the second input buffer was truncated to only contain
  // the samples that are after the end of |input_1|.
  base::TimeDelta expected_timestamp =
      input_1->timestamp() + input_1->duration();
  base::TimeDelta expected_duration =
      (input_2->timestamp() + input_2->duration()) - expected_timestamp;
  EXPECT_EQ(expected_timestamp, output_2->timestamp());
  EXPECT_EQ(expected_duration, output_2->duration());
  EXPECT_TRUE(VerifyData(output_2, GetValue(input_2)));
}

// Test that an input buffer that is completely overlapped by a buffer
// that was already added is dropped.
// +--------------+
// |11111111111111|
// +--------------+
//       +-----+
//       |22222|
//       +-----+
//                +-------------+
//                |3333333333333|
//                +-------------+
// Results in:
// +--------------+-------------+
// |11111111111111|3333333333333|
// +--------------+-------------+
TEST_F(AudioSplicerTest, DropBuffer) {
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);

  // Reset timestamp helper so that the next buffer will have a
  // timestamp that starts in the middle of |input_1|.
  const int kOverlapOffset = input_1->frame_count() / 2;
  const int kOverlapSize = input_1->frame_count() / 4;
  input_timestamp_helper_.SetBaseTimestamp(input_1->timestamp());
  input_timestamp_helper_.AddFrames(kOverlapOffset);

  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.2f, kOverlapSize);

  // Reset the timestamp helper so the next buffer will be right after
  // |input_1|.
  input_timestamp_helper_.SetBaseTimestamp(input_1->timestamp());
  input_timestamp_helper_.AddFrames(input_1->frame_count());
  scoped_refptr<AudioBuffer> input_3 = GetNextInputBuffer(0.3f);

  EXPECT_TRUE(AddInput(input_1));
  EXPECT_TRUE(AddInput(input_2));
  EXPECT_TRUE(AddInput(input_3));

  VerifyNextBuffer(input_1);
  VerifyNextBuffer(input_3);
  EXPECT_FALSE(splicer_.HasNextBuffer());
}

// Test crossfade when one buffer partially overlaps another.
// +--------------+
// |11111111111111|
// +--------------+
//            +--------------+
//            |22222222222222|
//            +--------------+
// Results in:
// +----------+----+----------+
// |1111111111|xxxx|2222222222|
// +----------+----+----------+
// Where "xxxx" represents the crossfaded portion of the signal.
TEST_F(AudioSplicerTest, PartialOverlapCrossfade) {
  const int kCrossfadeSize =
      input_timestamp_helper_.GetFramesToTarget(max_crossfade_duration());
  const int kBufferSize = kCrossfadeSize * 2;

  scoped_refptr<AudioBuffer> extra_pre_splice_buffer =
      GetNextInputBuffer(0.2f, kBufferSize);
  scoped_refptr<AudioBuffer> overlapped_buffer =
      GetNextInputBuffer(1.0f, kBufferSize);

  // Reset timestamp helper so that the next buffer will have a timestamp that
  // starts in the middle of |overlapped_buffer|.
  input_timestamp_helper_.SetBaseTimestamp(overlapped_buffer->timestamp());
  input_timestamp_helper_.AddFrames(overlapped_buffer->frame_count() -
                                    kCrossfadeSize);
  splicer_.SetSpliceTimestamp(input_timestamp_helper_.GetTimestamp());
  scoped_refptr<AudioBuffer> overlapping_buffer =
      GetNextInputBuffer(0.0f, kBufferSize);

  // |extra_pre_splice_buffer| is entirely before the splice and should be ready
  // for output.
  EXPECT_TRUE(AddInput(extra_pre_splice_buffer));
  VerifyNextBuffer(extra_pre_splice_buffer);

  // The splicer should be internally queuing input since |overlapped_buffer| is
  // part of the splice.
  EXPECT_TRUE(AddInput(overlapped_buffer));
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // |overlapping_buffer| completes the splice.
  splicer_.SetSpliceTimestamp(kNoTimestamp());
  EXPECT_TRUE(AddInput(overlapping_buffer));
  ASSERT_TRUE(splicer_.HasNextBuffer());

  // Add one more buffer to make sure it's passed through untouched.
  scoped_refptr<AudioBuffer> extra_post_splice_buffer =
      GetNextInputBuffer(0.5f, kBufferSize);
  EXPECT_TRUE(AddInput(extra_post_splice_buffer));

  VerifyPreSpliceOutput(overlapped_buffer,
                        overlapping_buffer,
                        221,
                        base::TimeDelta::FromMicroseconds(5011));

  // Due to rounding the crossfade size may vary by up to a frame.
  const int kExpectedCrossfadeSize = 220;
  EXPECT_NEAR(kExpectedCrossfadeSize, kCrossfadeSize, 1);

  VerifyCrossfadeOutput(overlapped_buffer,
                        NULL,
                        overlapping_buffer,
                        0,
                        kExpectedCrossfadeSize,
                        base::TimeDelta::FromMicroseconds(4988));

  // Retrieve the remaining portion after crossfade.
  ASSERT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<AudioBuffer> post_splice_output = splicer_.GetNextBuffer();
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(20022),
            post_splice_output->timestamp());
  EXPECT_EQ(overlapping_buffer->frame_count() - kExpectedCrossfadeSize,
            post_splice_output->frame_count());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(5034),
            post_splice_output->duration());

  EXPECT_TRUE(VerifyData(post_splice_output, GetValue(overlapping_buffer)));

  VerifyNextBuffer(extra_post_splice_buffer);
  EXPECT_FALSE(splicer_.HasNextBuffer());
}

// Test crossfade when one buffer partially overlaps another, but an end of
// stream buffer is received before the crossfade duration is reached.
// +--------------+
// |11111111111111|
// +--------------+
//            +---------++---+
//            |222222222||EOS|
//            +---------++---+
// Results in:
// +----------+----+----++---+
// |1111111111|xxxx|2222||EOS|
// +----------+----+----++---+
// Where "x" represents the crossfaded portion of the signal.
TEST_F(AudioSplicerTest, PartialOverlapCrossfadeEndOfStream) {
  const int kCrossfadeSize =
      input_timestamp_helper_.GetFramesToTarget(max_crossfade_duration());

  scoped_refptr<AudioBuffer> overlapped_buffer =
      GetNextInputBuffer(1.0f, kCrossfadeSize * 2);

  // Reset timestamp helper so that the next buffer will have a timestamp that
  // starts 3/4 of the way into |overlapped_buffer|.
  input_timestamp_helper_.SetBaseTimestamp(overlapped_buffer->timestamp());
  input_timestamp_helper_.AddFrames(3 * overlapped_buffer->frame_count() / 4);
  splicer_.SetSpliceTimestamp(input_timestamp_helper_.GetTimestamp());
  scoped_refptr<AudioBuffer> overlapping_buffer =
      GetNextInputBuffer(0.0f, kCrossfadeSize / 3);

  // The splicer should be internally queuing input since |overlapped_buffer| is
  // part of the splice.
  EXPECT_TRUE(AddInput(overlapped_buffer));
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // |overlapping_buffer| should not have enough data to complete the splice, so
  // ensure output is not available.
  splicer_.SetSpliceTimestamp(kNoTimestamp());
  EXPECT_TRUE(AddInput(overlapping_buffer));
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Now add an EOS buffer which should complete the splice.
  EXPECT_TRUE(AddInput(AudioBuffer::CreateEOSBuffer()));

  VerifyPreSpliceOutput(overlapped_buffer,
                        overlapping_buffer,
                        331,
                        base::TimeDelta::FromMicroseconds(7505));
  VerifyCrossfadeOutput(overlapped_buffer,
                        NULL,
                        overlapping_buffer,
                        0,
                        overlapping_buffer->frame_count(),
                        overlapping_buffer->duration());

  // Ensure the last buffer is an EOS buffer.
  ASSERT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<AudioBuffer> post_splice_output = splicer_.GetNextBuffer();
  EXPECT_TRUE(post_splice_output->end_of_stream());

  EXPECT_FALSE(splicer_.HasNextBuffer());
}

// Test crossfade when one buffer partially overlaps another, but the amount of
// overlapped data is less than the crossfade duration.
// +------------+
// |111111111111|
// +------------+
//            +--------------+
//            |22222222222222|
//            +--------------+
// Results in:
// +----------+-+------------+
// |1111111111|x|222222222222|
// +----------+-+------------+
// Where "x" represents the crossfaded portion of the signal.
TEST_F(AudioSplicerTest, PartialOverlapCrossfadeShortPreSplice) {
  const int kCrossfadeSize =
      input_timestamp_helper_.GetFramesToTarget(max_crossfade_duration());

  scoped_refptr<AudioBuffer> overlapped_buffer =
      GetNextInputBuffer(1.0f, kCrossfadeSize / 2);

  // Reset timestamp helper so that the next buffer will have a timestamp that
  // starts in the middle of |overlapped_buffer|.
  input_timestamp_helper_.SetBaseTimestamp(overlapped_buffer->timestamp());
  input_timestamp_helper_.AddFrames(overlapped_buffer->frame_count() / 2);
  splicer_.SetSpliceTimestamp(input_timestamp_helper_.GetTimestamp());
  scoped_refptr<AudioBuffer> overlapping_buffer =
      GetNextInputBuffer(0.0f, kCrossfadeSize * 2);

  // The splicer should be internally queuing input since |overlapped_buffer| is
  // part of the splice.
  EXPECT_TRUE(AddInput(overlapped_buffer));
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // |overlapping_buffer| completes the splice.
  splicer_.SetSpliceTimestamp(kNoTimestamp());
  EXPECT_TRUE(AddInput(overlapping_buffer));

  const int kExpectedPreSpliceSize = 55;
  const base::TimeDelta kExpectedPreSpliceDuration =
      base::TimeDelta::FromMicroseconds(1247);
  VerifyPreSpliceOutput(overlapped_buffer,
                        overlapping_buffer,
                        kExpectedPreSpliceSize,
                        kExpectedPreSpliceDuration);
  VerifyCrossfadeOutput(overlapped_buffer,
                        NULL,
                        overlapping_buffer,
                        0,
                        kExpectedPreSpliceSize,
                        kExpectedPreSpliceDuration);

  // Retrieve the remaining portion after crossfade.
  ASSERT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<AudioBuffer> post_splice_output = splicer_.GetNextBuffer();
  EXPECT_EQ(overlapping_buffer->timestamp() + kExpectedPreSpliceDuration,
            post_splice_output->timestamp());
  EXPECT_EQ(overlapping_buffer->frame_count() - kExpectedPreSpliceSize,
            post_splice_output->frame_count());
  EXPECT_EQ(overlapping_buffer->duration() - kExpectedPreSpliceDuration,
            post_splice_output->duration());

  EXPECT_TRUE(VerifyData(post_splice_output, GetValue(overlapping_buffer)));
  EXPECT_FALSE(splicer_.HasNextBuffer());
}

// Test behavior when a splice frame is incorrectly marked and does not actually
// overlap.
// +----------+
// |1111111111|
// +----------+
//            +--------------+
//            |22222222222222|
//            +--------------+
// Results in:
// +----------+--------------+
// |1111111111|22222222222222|
// +----------+--------------+
TEST_F(AudioSplicerTest, IncorrectlyMarkedSplice) {
  const int kBufferSize =
      input_timestamp_helper_.GetFramesToTarget(max_crossfade_duration()) * 2;

  scoped_refptr<AudioBuffer> first_buffer =
      GetNextInputBuffer(1.0f, kBufferSize);
  // Fuzz the duration slightly so that the buffer overlaps the splice timestamp
  // by a microsecond, which is not enough to crossfade.
  const base::TimeDelta kSpliceTimestamp =
      input_timestamp_helper_.GetTimestamp() -
      base::TimeDelta::FromMicroseconds(1);
  splicer_.SetSpliceTimestamp(kSpliceTimestamp);
  scoped_refptr<AudioBuffer> second_buffer =
      GetNextInputBuffer(0.0f, kBufferSize);
  second_buffer->set_timestamp(kSpliceTimestamp);

  // The splicer should be internally queuing input since |first_buffer| is part
  // of the supposed splice.
  EXPECT_TRUE(AddInput(first_buffer));
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // |second_buffer| should complete the supposed splice, so ensure output is
  // now available.
  splicer_.SetSpliceTimestamp(kNoTimestamp());
  EXPECT_TRUE(AddInput(second_buffer));

  VerifyNextBuffer(first_buffer);
  VerifyNextBuffer(second_buffer);
  EXPECT_FALSE(splicer_.HasNextBuffer());
}

// Test behavior when a splice frame is incorrectly marked and there is a gap
// between whats in the pre splice and post splice.
// +--------+
// |11111111|
// +--------+
//            +--------------+
//            |22222222222222|
//            +--------------+
// Results in:
// +--------+-+--------------+
// |11111111|0|22222222222222|
// +--------+-+--------------+
TEST_F(AudioSplicerTest, IncorrectlyMarkedSpliceWithGap) {
  const int kBufferSize =
      input_timestamp_helper_.GetFramesToTarget(max_crossfade_duration()) * 2;
  const int kGapSize = 2;

  scoped_refptr<AudioBuffer> first_buffer =
      GetNextInputBuffer(1.0f, kBufferSize - kGapSize);
  scoped_refptr<AudioBuffer> gap_buffer =
      GetNextInputBuffer(0.0f, kGapSize);
  splicer_.SetSpliceTimestamp(input_timestamp_helper_.GetTimestamp());
  scoped_refptr<AudioBuffer> second_buffer =
      GetNextInputBuffer(0.0f, kBufferSize);

  // The splicer should pass through the first buffer since it's not part of the
  // splice.
  EXPECT_TRUE(AddInput(first_buffer));
  VerifyNextBuffer(first_buffer);

  // Do not add |gap_buffer|.

  // |second_buffer| will complete the supposed splice.
  splicer_.SetSpliceTimestamp(kNoTimestamp());
  EXPECT_TRUE(AddInput(second_buffer));

  VerifyNextBuffer(gap_buffer);
  VerifyNextBuffer(second_buffer);
  EXPECT_FALSE(splicer_.HasNextBuffer());
}

// Test behavior when a splice frame is incorrectly marked and there is a gap
// between what's in the pre splice and post splice that is too large to recover
// from.
// +--------+
// |11111111|
// +--------+
//                    +------+
//                    |222222|
//                    +------+
// Results in an error and not a crash.
TEST_F(AudioSplicerTest, IncorrectlyMarkedSpliceWithBadGap) {
  const int kBufferSize =
      input_timestamp_helper_.GetFramesToTarget(max_crossfade_duration()) * 2;
  const int kGapSize = kBufferSize +
                       input_timestamp_helper_.GetFramesToTarget(
                           base::TimeDelta::FromMilliseconds(
                               AudioSplicer::kMaxTimeDeltaInMilliseconds + 1));

  scoped_refptr<AudioBuffer> first_buffer =
      GetNextInputBuffer(1.0f, kBufferSize);
  scoped_refptr<AudioBuffer> gap_buffer =
      GetNextInputBuffer(0.0f, kGapSize);
  splicer_.SetSpliceTimestamp(input_timestamp_helper_.GetTimestamp());
  scoped_refptr<AudioBuffer> second_buffer =
      GetNextInputBuffer(0.0f, kBufferSize);

  // The splicer should pass through the first buffer since it's not part of the
  // splice.
  EXPECT_TRUE(AddInput(first_buffer));
  VerifyNextBuffer(first_buffer);

  // Do not add |gap_buffer|.

  // |second_buffer| will complete the supposed splice.
  splicer_.SetSpliceTimestamp(kNoTimestamp());
  EXPECT_FALSE(AddInput(second_buffer));
}

// Ensure we don't crash when a splice frame is incorrectly marked such that the
// splice timestamp has already passed when SetSpliceTimestamp() is called.
// This can happen if the encoded timestamps are too far behind the decoded
// timestamps.
TEST_F(AudioSplicerTest, IncorrectlyMarkedPastSplice) {
  const int kBufferSize = 200;

  scoped_refptr<AudioBuffer> first_buffer =
      GetNextInputBuffer(1.0f, kBufferSize);
  EXPECT_TRUE(AddInput(first_buffer));
  VerifyNextBuffer(first_buffer);

  // Start the splice at a timestamp which has already occurred.
  splicer_.SetSpliceTimestamp(base::TimeDelta());

  scoped_refptr<AudioBuffer> second_buffer =
      GetNextInputBuffer(0.5f, kBufferSize);
  EXPECT_TRUE(AddInput(second_buffer));
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // |third_buffer| will complete the supposed splice.  The buffer size is set
  // such that unchecked the splicer would try to trim off a negative number of
  // frames.
  splicer_.SetSpliceTimestamp(kNoTimestamp());
  scoped_refptr<AudioBuffer> third_buffer =
      GetNextInputBuffer(0.0f, kBufferSize * 10);
  third_buffer->set_timestamp(base::TimeDelta());
  EXPECT_TRUE(AddInput(third_buffer));

  // The second buffer should come through unmodified.
  VerifyNextBuffer(second_buffer);

  // The third buffer should be partially dropped since it overlaps the second.
  ASSERT_TRUE(splicer_.HasNextBuffer());
  const base::TimeDelta second_buffer_end_ts =
      second_buffer->timestamp() + second_buffer->duration();
  scoped_refptr<AudioBuffer> output = splicer_.GetNextBuffer();
  EXPECT_EQ(second_buffer_end_ts, output->timestamp());
  EXPECT_EQ(third_buffer->duration() -
                (second_buffer_end_ts - third_buffer->timestamp()),
            output->duration());
  EXPECT_TRUE(VerifyData(output, GetValue(third_buffer)));
}

}  // namespace media
