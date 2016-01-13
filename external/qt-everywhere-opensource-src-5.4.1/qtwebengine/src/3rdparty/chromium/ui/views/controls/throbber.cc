// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/throbber.h"

#include "base/time/time.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

using base::Time;
using base::TimeDelta;

namespace views {

Throbber::Throbber(int frame_time_ms,
                   bool paint_while_stopped)
    : running_(false),
      paint_while_stopped_(paint_while_stopped),
      frames_(NULL),
      frame_time_(TimeDelta::FromMilliseconds(frame_time_ms)) {
  SetFrames(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_THROBBER).ToImageSkia());
}

Throbber::~Throbber() {
  Stop();
}

void Throbber::Start() {
  if (running_)
    return;

  start_time_ = Time::Now();

  timer_.Start(FROM_HERE, frame_time_ - TimeDelta::FromMilliseconds(10),
               this, &Throbber::Run);

  running_ = true;

  SchedulePaint();  // paint right away
}

void Throbber::Stop() {
  if (!running_)
    return;

  timer_.Stop();

  running_ = false;
  SchedulePaint();  // Important if we're not painting while stopped
}

void Throbber::SetFrames(const gfx::ImageSkia* frames) {
  frames_ = frames;
  DCHECK(frames_->width() > 0 && frames_->height() > 0);
  DCHECK(frames_->width() % frames_->height() == 0);
  frame_count_ = frames_->width() / frames_->height();
  PreferredSizeChanged();
}

void Throbber::Run() {
  DCHECK(running_);

  SchedulePaint();
}

gfx::Size Throbber::GetPreferredSize() const {
  return gfx::Size(frames_->height(), frames_->height());
}

void Throbber::OnPaint(gfx::Canvas* canvas) {
  if (!running_ && !paint_while_stopped_)
    return;

  const TimeDelta elapsed_time = Time::Now() - start_time_;
  const int current_frame =
      static_cast<int>(elapsed_time / frame_time_) % frame_count_;

  int image_size = frames_->height();
  int image_offset = current_frame * image_size;
  canvas->DrawImageInt(*frames_,
                       image_offset, 0, image_size, image_size,
                       0, 0, image_size, image_size,
                       false);
}



// Smoothed throbber ---------------------------------------------------------


// Delay after work starts before starting throbber, in milliseconds.
static const int kStartDelay = 200;

// Delay after work stops before stopping, in milliseconds.
static const int kStopDelay = 50;


SmoothedThrobber::SmoothedThrobber(int frame_time_ms)
    : Throbber(frame_time_ms, /* paint_while_stopped= */ false),
      start_delay_ms_(kStartDelay),
      stop_delay_ms_(kStopDelay) {
}

SmoothedThrobber::~SmoothedThrobber() {}

void SmoothedThrobber::Start() {
  stop_timer_.Stop();

  if (!running_ && !start_timer_.IsRunning()) {
    start_timer_.Start(FROM_HERE, TimeDelta::FromMilliseconds(start_delay_ms_),
                       this, &SmoothedThrobber::StartDelayOver);
  }
}

void SmoothedThrobber::StartDelayOver() {
  Throbber::Start();
}

void SmoothedThrobber::Stop() {
  if (!running_)
    start_timer_.Stop();

  stop_timer_.Stop();
  stop_timer_.Start(FROM_HERE, TimeDelta::FromMilliseconds(stop_delay_ms_),
                    this, &SmoothedThrobber::StopDelayOver);
}

void SmoothedThrobber::StopDelayOver() {
  Throbber::Stop();
}

// Checkmark throbber ---------------------------------------------------------

CheckmarkThrobber::CheckmarkThrobber()
    : Throbber(kFrameTimeMs, false),
      checked_(false),
      checkmark_(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_CHECKMARK).ToImageSkia()) {
}

void CheckmarkThrobber::SetChecked(bool checked) {
  bool changed = checked != checked_;
  if (changed) {
    checked_ = checked;
    SchedulePaint();
  }
}

void CheckmarkThrobber::OnPaint(gfx::Canvas* canvas) {
  if (running_) {
    // Let the throbber throb...
    Throbber::OnPaint(canvas);
    return;
  }
  // Otherwise we paint our tick mark or nothing depending on our state.
  if (checked_) {
    int checkmark_x = (width() - checkmark_->width()) / 2;
    int checkmark_y = (height() - checkmark_->height()) / 2;
    canvas->DrawImageInt(*checkmark_, checkmark_x, checkmark_y);
  }
}

}  // namespace views
