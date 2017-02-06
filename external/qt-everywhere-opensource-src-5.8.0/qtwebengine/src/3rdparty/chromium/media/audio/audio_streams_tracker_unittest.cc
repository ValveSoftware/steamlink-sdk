// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_streams_tracker.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(AudioStreamsTrackerTest, IncreaseAndDecreaseOnce) {
  AudioStreamsTracker audio_streams_tracker;
  const size_t kNumberOfOperations = 5;

  // Increase count.
  for (size_t i = 1; i <= kNumberOfOperations; ++i) {
    audio_streams_tracker.IncreaseStreamCount();
    EXPECT_EQ(i, audio_streams_tracker.current_stream_count_);
    EXPECT_EQ(i, audio_streams_tracker.max_stream_count());
  }

  // Reset and check max count.
  audio_streams_tracker.ResetMaxStreamCount();
  EXPECT_EQ(kNumberOfOperations, audio_streams_tracker.max_stream_count());

  // Decrease count.
  for (size_t i = 1; i <= kNumberOfOperations; ++i) {
    audio_streams_tracker.DecreaseStreamCount();
    EXPECT_EQ(kNumberOfOperations - i,
              audio_streams_tracker.current_stream_count_);
    EXPECT_EQ(kNumberOfOperations, audio_streams_tracker.max_stream_count());
  }

  // Reset and check max count.
  audio_streams_tracker.ResetMaxStreamCount();
  EXPECT_EQ(0u, audio_streams_tracker.max_stream_count());
}

TEST(AudioStreamsTrackerTest, IncreaseAndDecreaseMultiple) {
  AudioStreamsTracker audio_streams_tracker;
  const size_t kNumberOfOperations = 5;

  // Increase 2N (N = kNumberOfOperations).
  for (size_t i = 1; i <= 2 * kNumberOfOperations; ++i) {
    audio_streams_tracker.IncreaseStreamCount();
    EXPECT_EQ(i, audio_streams_tracker.current_stream_count_);
    EXPECT_EQ(i, audio_streams_tracker.max_stream_count());
  }

  // Decrease N.
  for (size_t i = 1; i <= kNumberOfOperations; ++i) {
    audio_streams_tracker.DecreaseStreamCount();
    EXPECT_EQ(2 * kNumberOfOperations - i,
              audio_streams_tracker.current_stream_count_);
    EXPECT_EQ(2 * kNumberOfOperations,
              audio_streams_tracker.max_stream_count());
  }

  // Reset and check max count.
  audio_streams_tracker.ResetMaxStreamCount();
  EXPECT_EQ(kNumberOfOperations, audio_streams_tracker.max_stream_count());

  // Increase 2N.
  for (size_t i = 1; i <= 2 * kNumberOfOperations; ++i) {
    audio_streams_tracker.IncreaseStreamCount();
    EXPECT_EQ(kNumberOfOperations + i,
              audio_streams_tracker.current_stream_count_);
    EXPECT_EQ(kNumberOfOperations + i,
              audio_streams_tracker.max_stream_count());
  }

  // Decrease N.
  for (size_t i = 1; i <= kNumberOfOperations; ++i) {
    audio_streams_tracker.DecreaseStreamCount();
    EXPECT_EQ(3 * kNumberOfOperations - i,
              audio_streams_tracker.current_stream_count_);
    EXPECT_EQ(3 * kNumberOfOperations,
              audio_streams_tracker.max_stream_count());
  }

  // Reset and check max count.
  audio_streams_tracker.ResetMaxStreamCount();
  EXPECT_EQ(2 * kNumberOfOperations, audio_streams_tracker.max_stream_count());

  // Decrease 2N.
  for (size_t i = 1; i <= 2 * kNumberOfOperations; ++i) {
    audio_streams_tracker.DecreaseStreamCount();
    EXPECT_EQ(2 * kNumberOfOperations - i,
              audio_streams_tracker.current_stream_count_);
    EXPECT_EQ(2 * kNumberOfOperations,
              audio_streams_tracker.max_stream_count());
  }

  // Reset and check max count.
  audio_streams_tracker.ResetMaxStreamCount();
  EXPECT_EQ(0u, audio_streams_tracker.max_stream_count());
}

}  // namespace media
