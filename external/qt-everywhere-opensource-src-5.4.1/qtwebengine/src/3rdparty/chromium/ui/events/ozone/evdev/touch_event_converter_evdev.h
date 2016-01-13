// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_

#include <bitset>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_pump_libevent.h"
#include "ui/events/event_constants.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"

namespace ui {

class TouchEvent;

class EVENTS_OZONE_EVDEV_EXPORT TouchEventConverterEvdev
    : public EventConverterEvdev,
      public base::MessagePumpLibevent::Watcher {
 public:
  enum {
    MAX_FINGERS = 11
  };
  TouchEventConverterEvdev(int fd,
                           base::FilePath path,
                           const EventDeviceInfo& info,
                           const EventDispatchCallback& dispatch);
  virtual ~TouchEventConverterEvdev();

  // Start & stop watching for events.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  friend class MockTouchEventConverterEvdev;

  // Unsafe part of initialization.
  void Init(const EventDeviceInfo& info);

  // Overidden from base::MessagePumpLibevent::Watcher.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

  virtual bool Reinitialize();

  void ProcessInputEvent(const input_event& input);
  void ProcessAbs(const input_event& input);
  void ProcessSyn(const input_event& input);

  void ReportEvents(base::TimeDelta delta);

  // Set if we have seen a SYN_DROPPED and not yet re-synced with the device.
  bool syn_dropped_;

  // Set if this is a type A device (uses SYN_MT_REPORT).
  bool is_type_a_;

  // Pressure values.
  int pressure_min_;
  int pressure_max_;  // Used to normalize pressure values.

  // Input range for x-axis.
  float x_min_tuxels_;
  float x_num_tuxels_;

  // Input range for y-axis.
  float y_min_tuxels_;
  float y_num_tuxels_;

  // Output range for x-axis.
  float x_min_pixels_;
  float x_num_pixels_;

  // Output range for y-axis.
  float y_min_pixels_;
  float y_num_pixels_;

  // Touch point currently being updated from the /dev/input/event* stream.
  int current_slot_;

  // File descriptor for the /dev/input/event* instance.
  int fd_;

  // Path to input device.
  base::FilePath path_;

  // Bit field tracking which in-progress touch points have been modified
  // without a syn event.
  std::bitset<MAX_FINGERS> altered_slots_;

  struct InProgressEvents {
    float x_;
    float y_;
    int id_;  // Device reported "unique" touch point id; -1 means not active
    int finger_;  // "Finger" id starting from 0; -1 means not active

    EventType type_;
    int major_;
    float pressure_;
  };

  // In-progress touch points.
  InProgressEvents events_[MAX_FINGERS];

  // Controller for watching the input fd.
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventConverterEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_
