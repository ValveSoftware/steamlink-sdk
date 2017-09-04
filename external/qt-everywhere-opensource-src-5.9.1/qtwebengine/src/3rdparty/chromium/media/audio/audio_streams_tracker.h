// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_STREAMS_TRACKER_H_
#define MEDIA_AUDIO_AUDIO_STREAMS_TRACKER_H_

#include <stddef.h>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"

namespace media {

// Tracks the maximum number of simultaneous streams that was ever registered.
// All functions must be called on the same thread.
class MEDIA_EXPORT AudioStreamsTracker {
 public:
  AudioStreamsTracker();
  ~AudioStreamsTracker();

  // Increases/decreases current stream count. Updates max stream count if
  // current count is larger.
  void IncreaseStreamCount();
  void DecreaseStreamCount();

  // Resets the max stream count, i.e. sets it to the current stream count.
  void ResetMaxStreamCount();

  size_t max_stream_count() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioStreamsTrackerTest, IncreaseAndDecreaseOnce);
  FRIEND_TEST_ALL_PREFIXES(AudioStreamsTrackerTest,
                           IncreaseAndDecreaseMultiple);

  // Confirms single-threaded access.
  base::ThreadChecker thread_checker_;

  // Stores the current and maximum number of streams.
  size_t current_stream_count_;
  size_t max_stream_count_;

  DISALLOW_COPY_AND_ASSIGN(AudioStreamsTracker);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_STREAMS_TRACKER_H_
