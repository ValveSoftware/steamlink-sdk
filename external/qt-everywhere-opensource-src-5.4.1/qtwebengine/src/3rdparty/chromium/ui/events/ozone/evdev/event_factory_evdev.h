// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_FACTORY_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_FACTORY_EVDEV_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_modifiers_evdev.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/public/event_factory_ozone.h"

namespace ui {

class CursorDelegateEvdev;
class DeviceManager;

// Ozone events implementation for the Linux input subsystem ("evdev").
class EVENTS_OZONE_EVDEV_EXPORT EventFactoryEvdev : public EventFactoryOzone,
                                                    public DeviceEventObserver,
                                                    public PlatformEventSource {
 public:
  EventFactoryEvdev(CursorDelegateEvdev* cursor,
                    DeviceManager* device_manager);
  virtual ~EventFactoryEvdev();

  void DispatchUiEvent(Event* event);

  // EventFactoryOzone:
  virtual void WarpCursorTo(gfx::AcceleratedWidget widget,
                            const gfx::PointF& location) OVERRIDE;

 private:
  // Open device at path & starting processing events (on UI thread).
  void AttachInputDevice(const base::FilePath& file_path,
                         scoped_ptr<EventConverterEvdev> converter);

  // Close device at path (on UI thread).
  void DetachInputDevice(const base::FilePath& file_path);

  // DeviceEventObserver overrides:
  //
  // Callback for device add (on UI thread).
  virtual void OnDeviceEvent(const DeviceEvent& event) OVERRIDE;

  // PlatformEventSource:
  virtual void OnDispatcherListChanged() OVERRIDE;

  // Owned per-device event converters (by path).
  std::map<base::FilePath, EventConverterEvdev*> converters_;

  // Interface for scanning & monitoring input devices.
  DeviceManager* device_manager_;  // Not owned.

  // Task runner for event dispatch.
  scoped_refptr<base::TaskRunner> ui_task_runner_;

  // Modifier key state (shift, ctrl, etc).
  EventModifiersEvdev modifiers_;

  // Cursor movement.
  CursorDelegateEvdev* cursor_;

  // Dispatch callback for events.
  EventDispatchCallback dispatch_callback_;

  // Support weak pointers for attach & detach callbacks.
  base::WeakPtrFactory<EventFactoryEvdev> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EventFactoryEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_FACTORY_EVDEV_H_
