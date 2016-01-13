// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_KEY_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_KEY_EVENT_CONVERTER_EVDEV_H_

#include "base/files/file_path.h"
#include "base/message_loop/message_pump_libevent.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_modifiers_evdev.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"

struct input_event;

namespace ui {

class EVENTS_OZONE_EVDEV_EXPORT KeyEventConverterEvdev
    : public EventConverterEvdev,
      public base::MessagePumpLibevent::Watcher {
 public:
  KeyEventConverterEvdev(int fd,
                         base::FilePath path,
                         EventModifiersEvdev* modifiers,
                         const EventDispatchCallback& dispatch);
  virtual ~KeyEventConverterEvdev();

  // Start & stop watching for events.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;

  // Overidden from base::MessagePumpLibevent::Watcher.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

  void ProcessEvents(const struct input_event* inputs, int count);

 private:
  // File descriptor for the /dev/input/event* instance.
  int fd_;

  // Path to input device.
  base::FilePath path_;

  // Shared modifier state.
  EventModifiersEvdev* modifiers_;

  // Controller for watching the input fd.
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  void ConvertKeyEvent(int key, int value);

  DISALLOW_COPY_AND_ASSIGN(KeyEventConverterEvdev);
};

}  // namspace ui

#endif  // UI_EVENTS_OZONE_EVDEV_KEY_EVENT_CONVERTER_EVDEV_H_

