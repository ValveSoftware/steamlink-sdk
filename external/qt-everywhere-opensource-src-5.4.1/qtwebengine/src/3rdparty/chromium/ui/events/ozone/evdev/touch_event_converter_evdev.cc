// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_event_converter_evdev.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

#include <cmath>
#include <limits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/screen.h"
#include "ui/ozone/public/event_factory_ozone.h"

namespace {

// Number is determined empirically.
// TODO(rjkroege): Configure this per device.
const float kFingerWidth = 25.f;

struct TouchCalibration {
  int bezel_left;
  int bezel_right;
  int bezel_top;
  int bezel_bottom;
};

void GetTouchCalibration(TouchCalibration* cal) {
  std::vector<std::string> parts;
  if (Tokenize(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                   switches::kTouchCalibration),
               ",",
               &parts) >= 4) {
    if (!base::StringToInt(parts[0], &cal->bezel_left))
      DLOG(ERROR) << "Incorrect left border calibration value passed.";
    if (!base::StringToInt(parts[1], &cal->bezel_right))
      DLOG(ERROR) << "Incorrect right border calibration value passed.";
    if (!base::StringToInt(parts[2], &cal->bezel_top))
      DLOG(ERROR) << "Incorrect top border calibration value passed.";
    if (!base::StringToInt(parts[3], &cal->bezel_bottom))
      DLOG(ERROR) << "Incorrect bottom border calibration value passed.";
  }
}

float TuxelsToPixels(float val,
                     float min_tuxels,
                     float num_tuxels,
                     float min_pixels,
                     float num_pixels) {
  // Map [min_tuxels, min_tuxels + num_tuxels) to
  //     [min_pixels, min_pixels + num_pixels).
  return min_pixels + (val - min_tuxels) * num_pixels / num_tuxels;
}

}  // namespace

namespace ui {

TouchEventConverterEvdev::TouchEventConverterEvdev(
    int fd,
    base::FilePath path,
    const EventDeviceInfo& info,
    const EventDispatchCallback& callback)
    : EventConverterEvdev(callback),
      syn_dropped_(false),
      is_type_a_(false),
      current_slot_(0),
      fd_(fd),
      path_(path) {
  Init(info);
}

TouchEventConverterEvdev::~TouchEventConverterEvdev() {
  Stop();
  close(fd_);
}

void TouchEventConverterEvdev::Init(const EventDeviceInfo& info) {
  gfx::Screen *screen = gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE);
  if (!screen)
    return;  // No scaling.
  gfx::Display display = screen->GetPrimaryDisplay();
  gfx::Size size = display.GetSizeInPixel();

  pressure_min_ = info.GetAbsMinimum(ABS_MT_PRESSURE),
  pressure_max_ = info.GetAbsMaximum(ABS_MT_PRESSURE),
  x_min_tuxels_ = info.GetAbsMinimum(ABS_MT_POSITION_X),
  x_num_tuxels_ = info.GetAbsMaximum(ABS_MT_POSITION_X) - x_min_tuxels_ + 1,
  y_min_tuxels_ = info.GetAbsMinimum(ABS_MT_POSITION_Y),
  y_num_tuxels_ = info.GetAbsMaximum(ABS_MT_POSITION_Y) - y_min_tuxels_ + 1,
  x_min_pixels_ = x_min_tuxels_,
  x_num_pixels_ = x_num_tuxels_,
  y_min_pixels_ = y_min_tuxels_,
  y_num_pixels_ = y_num_tuxels_,

  // Map coordinates onto screen.
  x_min_pixels_ = 0;
  y_min_pixels_ = 0;
  x_num_pixels_ = size.width();
  y_num_pixels_ = size.height();

  VLOG(1) << "mapping touch coordinates to screen coordinates: "
          << base::StringPrintf("%dx%d", size.width(), size.height());

  // Apply --touch-calibration.
  TouchCalibration cal = {};
  GetTouchCalibration(&cal);
  x_min_tuxels_ += cal.bezel_left;
  x_num_tuxels_ -= cal.bezel_left + cal.bezel_right;
  y_min_tuxels_ += cal.bezel_top;
  y_num_tuxels_ -= cal.bezel_top + cal.bezel_bottom;

  VLOG(1) << "applying touch calibration: "
          << base::StringPrintf("[%d, %d, %d, %d]",
                                cal.bezel_left,
                                cal.bezel_right,
                                cal.bezel_top,
                                cal.bezel_bottom);
}

void TouchEventConverterEvdev::Start() {
  base::MessageLoopForUI::current()->WatchFileDescriptor(
      fd_, true, base::MessagePumpLibevent::WATCH_READ, &controller_, this);
}

void TouchEventConverterEvdev::Stop() {
  controller_.StopWatchingFileDescriptor();
}

bool TouchEventConverterEvdev::Reinitialize() {
  EventDeviceInfo info;
  if (info.Initialize(fd_)) {
    Init(info);
    return true;
  }
  return false;
}

void TouchEventConverterEvdev::OnFileCanWriteWithoutBlocking(int /* fd */) {
  // Read-only file-descriptors.
  NOTREACHED();
}

void TouchEventConverterEvdev::OnFileCanReadWithoutBlocking(int fd) {
  input_event inputs[MAX_FINGERS * 6 + 1];
  ssize_t read_size = read(fd, inputs, sizeof(inputs));
  if (read_size < 0) {
    if (errno == EINTR || errno == EAGAIN)
      return;
    if (errno != ENODEV)
      PLOG(ERROR) << "error reading device " << path_.value();
    Stop();
    return;
  }

  for (unsigned i = 0; i < read_size / sizeof(*inputs); i++) {
    ProcessInputEvent(inputs[i]);
  }
}

void TouchEventConverterEvdev::ProcessInputEvent(const input_event& input) {
  if (input.type == EV_SYN) {
    ProcessSyn(input);
  } else if(syn_dropped_) {
    // Do nothing. This branch indicates we have lost sync with the driver.
  } else if (input.type == EV_ABS) {
    if (current_slot_ >= MAX_FINGERS) {
      LOG(ERROR) << "too many touch events: " << current_slot_;
      return;
    }
    ProcessAbs(input);
  } else if (input.type == EV_KEY) {
    switch (input.code) {
      case BTN_TOUCH:
        break;
      default:
        NOTIMPLEMENTED() << "invalid code for EV_KEY: " << input.code;
    }
  } else {
    NOTIMPLEMENTED() << "invalid type: " << input.type;
  }
}

void TouchEventConverterEvdev::ProcessAbs(const input_event& input) {
  switch (input.code) {
    case ABS_MT_TOUCH_MAJOR:
      altered_slots_.set(current_slot_);
      events_[current_slot_].major_ = input.value;
      break;
    case ABS_X:
    case ABS_MT_POSITION_X:
      altered_slots_.set(current_slot_);
      events_[current_slot_].x_ = TuxelsToPixels(input.value,
                                                 x_min_tuxels_,
                                                 x_num_tuxels_,
                                                 x_min_pixels_,
                                                 x_num_pixels_);
      break;
    case ABS_Y:
    case ABS_MT_POSITION_Y:
      altered_slots_.set(current_slot_);
      events_[current_slot_].y_ = TuxelsToPixels(input.value,
                                                 y_min_tuxels_,
                                                 y_num_tuxels_,
                                                 y_min_pixels_,
                                                 y_num_pixels_);
      break;
    case ABS_MT_TRACKING_ID:
      altered_slots_.set(current_slot_);
      if (input.value < 0) {
        events_[current_slot_].type_ = ET_TOUCH_RELEASED;
      } else {
        events_[current_slot_].finger_ = input.value;
        events_[current_slot_].type_ = ET_TOUCH_PRESSED;
      }
      break;
    case ABS_MT_PRESSURE:
    case ABS_PRESSURE:
      altered_slots_.set(current_slot_);
      events_[current_slot_].pressure_ = input.value - pressure_min_;
      events_[current_slot_].pressure_ /= pressure_max_ - pressure_min_;
      break;
    case ABS_MT_SLOT:
      current_slot_ = input.value;
      altered_slots_.set(current_slot_);
      break;
    default:
      NOTIMPLEMENTED() << "invalid code for EV_ABS: " << input.code;
  }
}

void TouchEventConverterEvdev::ProcessSyn(const input_event& input) {
  switch (input.code) {
    case SYN_REPORT:
      if (syn_dropped_) {
        // Have to re-initialize.
        if (Reinitialize()) {
          syn_dropped_ = false;
          altered_slots_.reset();
        } else {
          LOG(ERROR) << "failed to re-initialize device info";
        }
      } else {
        ReportEvents(base::TimeDelta::FromMicroseconds(
            input.time.tv_sec * 1000000 + input.time.tv_usec));
      }
      if (is_type_a_)
        current_slot_ = 0;
      break;
    case SYN_MT_REPORT:
      // For type A devices, we just get a stream of all current contacts,
      // in some arbitrary order.
      events_[current_slot_++].type_ = ET_TOUCH_PRESSED;
      is_type_a_ = true;
      break;
    case SYN_DROPPED:
      // Some buffer has overrun. We ignore all events up to and
      // including the next SYN_REPORT.
      syn_dropped_ = true;
      break;
    default:
      NOTIMPLEMENTED() << "invalid code for EV_SYN: " << input.code;
  }
}

void TouchEventConverterEvdev::ReportEvents(base::TimeDelta delta) {
  for (int i = 0; i < MAX_FINGERS; i++) {
    if (altered_slots_[i]) {
      // TODO(rikroege): Support elliptical finger regions.
      TouchEvent evt(
          events_[i].type_,
          gfx::PointF(events_[i].x_, events_[i].y_),
          /* flags */ 0,
          /* touch_id */ i,
          delta,
          events_[i].pressure_ * kFingerWidth,
          events_[i].pressure_ * kFingerWidth,
          /* angle */ 0.,
          events_[i].pressure_);
      DispatchEventToCallback(&evt);

      // Subsequent events for this finger will be touch-move until it
      // is released.
      events_[i].type_ = ET_TOUCH_MOVED;
    }
  }
  altered_slots_.reset();
}

}  // namespace ui
