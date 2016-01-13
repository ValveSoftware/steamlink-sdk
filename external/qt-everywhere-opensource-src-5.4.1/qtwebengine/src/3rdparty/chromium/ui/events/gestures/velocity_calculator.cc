// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/velocity_calculator.h"

namespace ui {

VelocityCalculator::VelocityCalculator(int buffer_size)
    : buffer_(new Point[buffer_size]) ,
      index_(0),
      num_valid_entries_(0),
      buffer_size_(buffer_size),
      x_velocity_(0),
      y_velocity_(0),
      velocities_stale_(false) {
}

VelocityCalculator::~VelocityCalculator() {}

float VelocityCalculator::XVelocity() {
  if (velocities_stale_)
    UpdateVelocity();
  return x_velocity_;
}

float VelocityCalculator::YVelocity() {
  if (velocities_stale_)
    UpdateVelocity();
  return y_velocity_;
}

void VelocityCalculator::PointSeen(float x, float y, int64 time) {
  buffer_[index_].x = x;
  buffer_[index_].y = y;
  buffer_[index_].time = time;

  index_ = (index_ + 1) % buffer_size_;
  if (num_valid_entries_ < buffer_size_)
    ++num_valid_entries_;

  velocities_stale_ = true;
}

float VelocityCalculator::VelocitySquared() {
  if (velocities_stale_)
    UpdateVelocity();
  return x_velocity_ * x_velocity_ + y_velocity_ * y_velocity_;
}

void VelocityCalculator::UpdateVelocity() {
  // We don't have enough data to make a good estimate of the velocity.
  if (num_valid_entries_ < 2)
    return;

  // Where A_i = A[i] - mean(A)
  // x velocity = sum_i(x_i * t_i) / sum_i(t_i * t_i)
  // This is an Ordinary Least Squares Regression.

  float mean_x = 0;
  float mean_y = 0;
  int64 mean_time = 0;

  for (size_t i = 0; i < num_valid_entries_; ++i) {
    mean_x += buffer_[i].x;
    mean_y += buffer_[i].y;
    mean_time += buffer_[i].time;
  }

  // Minimize number of divides.
  const float num_valid_entries_i = 1.0f / num_valid_entries_;

  mean_x *= num_valid_entries_i;
  mean_y *= num_valid_entries_i;

  // The loss in accuracy due to rounding is insignificant compared to
  // the error due to the resolution of the timer.
  // Use integer division to avoid the cast to double, which would cause
  // VelocityCalculatorTest.IsAccurateWithLargeTimes to fail.
  mean_time /= num_valid_entries_;

  float xt = 0;  // sum_i(x_i * t_i)
  float yt = 0;  // sum_i(y_i * t_i)
  int64 tt = 0;  // sum_i(t_i * t_i)

  int64 t_i;

  for (size_t i = 0; i < num_valid_entries_; ++i) {
    t_i = (buffer_[i].time - mean_time);
    xt += (buffer_[i].x - mean_x) * t_i;
    yt += (buffer_[i].y - mean_y) * t_i;
    tt += t_i * t_i;
  }

  if (tt > 0) {
    // Convert time from microseconds to seconds.
    x_velocity_ = xt / (tt / 1000000.0f);
    y_velocity_ = yt / (tt / 1000000.0f);
  } else {
    x_velocity_ = 0.0f;
    y_velocity_ = 0.0f;
  }
  velocities_stale_ = false;
}

void VelocityCalculator::ClearHistory() {
  index_ = 0;
  num_valid_entries_ = 0;
  x_velocity_ = 0;
  y_velocity_ = 0;
  velocities_stale_ = false;
}

}  // namespace ui
