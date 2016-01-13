// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_VELOCITY_CALCULATOR_H_
#define UI_EVENTS_GESTURES_VELOCITY_CALCULATOR_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/events_export.h"

namespace ui {

class EVENTS_EXPORT VelocityCalculator {
 public:
  explicit VelocityCalculator(int bufferSize);
  ~VelocityCalculator();
  void PointSeen(float x, float y, int64 time);
  float XVelocity();
  float YVelocity();
  float VelocitySquared();
  void ClearHistory();

 private:
  struct Point {
    float x;
    float y;
    int64 time;
  };

  void UpdateVelocity();

  typedef scoped_ptr<Point[]> HistoryBuffer;
  HistoryBuffer buffer_;

  // index_ points directly after the last point added.
  int index_;
  size_t num_valid_entries_;
  const size_t buffer_size_;
  float x_velocity_;
  float y_velocity_;
  bool velocities_stale_;
  DISALLOW_COPY_AND_ASSIGN(VelocityCalculator);
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_VELOCITY_CALCULATOR_H_
