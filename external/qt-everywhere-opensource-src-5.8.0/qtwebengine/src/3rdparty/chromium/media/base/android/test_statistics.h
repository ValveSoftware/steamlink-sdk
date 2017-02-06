// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_TEST_STATISTICS_H_
#define MEDIA_BASE_ANDROID_TEST_STATISTICS_H_

namespace media {

// Class that computes statistics: number of calls, minimum and maximum values.
// It is used for in tests PTS statistics to verify that playback did actually
// happen.

template <typename T>
class Minimax {
 public:
  Minimax() : num_values_(0) {}
  ~Minimax() {}

  void AddValue(const T& value) {
    if (num_values_ == 0)
      min_ = max_ = value;
    else if (value < min_)
      min_ = value;
    else if (max_ < value)
      max_ = value;

    ++num_values_;
  }

  void Clear() {
    min_ = T();
    max_ = T();
    num_values_ = 0;
  }

  const T& min() const { return min_; }
  const T& max() const { return max_; }
  int num_values() const { return num_values_; }

 private:
  T min_;
  T max_;
  int num_values_;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_TEST_STATISTICS_H_
