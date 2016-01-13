// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_factory_evdev.h"

#include <fcntl.h>
#include <linux/input.h>

#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "base/threading/worker_pool.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/key_event_converter_evdev.h"
#include "ui/events/ozone/evdev/touch_event_converter_evdev.h"

#if defined(USE_EVDEV_GESTURES)
#include "ui/events/ozone/evdev/libgestures_glue/event_reader_libevdev_cros.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_interpreter_libevdev_cros.h"
#endif

#ifndef EVIOCSCLOCKID
#define EVIOCSCLOCKID  _IOW('E', 0xa0, int)
#endif

namespace ui {

namespace {

#if defined(USE_EVDEV_GESTURES)
bool UseGesturesLibraryForDevice(const EventDeviceInfo& devinfo) {
  if (devinfo.HasAbsXY() && !devinfo.IsMappedToScreen())
    return true;  // touchpad

  if (devinfo.HasRelXY())
    return true;  // mouse

  return false;
}
#endif

scoped_ptr<EventConverterEvdev> CreateConverter(
    int fd,
    const base::FilePath& path,
    const EventDeviceInfo& devinfo,
    const EventDispatchCallback& dispatch,
    EventModifiersEvdev* modifiers,
    CursorDelegateEvdev* cursor) {
#if defined(USE_EVDEV_GESTURES)
  // Touchpad or mouse: use gestures library.
  // EventReaderLibevdevCros -> GestureInterpreterLibevdevCros -> DispatchEvent
  if (UseGesturesLibraryForDevice(devinfo)) {
    scoped_ptr<GestureInterpreterLibevdevCros> gesture_interp = make_scoped_ptr(
        new GestureInterpreterLibevdevCros(modifiers, cursor, dispatch));
    scoped_ptr<EventReaderLibevdevCros> libevdev_reader =
        make_scoped_ptr(new EventReaderLibevdevCros(
            fd,
            path,
            gesture_interp.PassAs<EventReaderLibevdevCros::Delegate>()));
    return libevdev_reader.PassAs<EventConverterEvdev>();
  }
#endif

  // Touchscreen: use TouchEventConverterEvdev.
  scoped_ptr<EventConverterEvdev> converter;
  if (devinfo.HasAbsXY())
    return make_scoped_ptr<EventConverterEvdev>(
        new TouchEventConverterEvdev(fd, path, devinfo, dispatch));

  // Everything else: use KeyEventConverterEvdev.
  return make_scoped_ptr<EventConverterEvdev>(
      new KeyEventConverterEvdev(fd, path, modifiers, dispatch));
}

// Open an input device. Opening may put the calling thread to sleep, and
// therefore should be run on a thread where latency is not critical. We
// run it on a thread from the worker pool.
//
// This takes a TaskRunner and runs the reply on that thread, so that we
// can hop threads if necessary (back to the UI thread).
void OpenInputDevice(
    const base::FilePath& path,
    EventModifiersEvdev* modifiers,
    CursorDelegateEvdev* cursor,
    scoped_refptr<base::TaskRunner> reply_runner,
    const EventDispatchCallback& dispatch,
    base::Callback<void(scoped_ptr<EventConverterEvdev>)> reply_callback) {
  TRACE_EVENT1("ozone", "OpenInputDevice", "path", path.value());

  int fd = open(path.value().c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    PLOG(ERROR) << "Cannot open '" << path.value();
    return;
  }

  // Use monotonic timestamps for events. The touch code in particular
  // expects event timestamps to correlate to the monotonic clock
  // (base::TimeTicks).
  unsigned int clk = CLOCK_MONOTONIC;
  if (ioctl(fd, EVIOCSCLOCKID, &clk))
    PLOG(ERROR) << "failed to set CLOCK_MONOTONIC";

  EventDeviceInfo devinfo;
  if (!devinfo.Initialize(fd)) {
    LOG(ERROR) << "failed to get device information for " << path.value();
    close(fd);
    return;
  }

  scoped_ptr<EventConverterEvdev> converter =
      CreateConverter(fd, path, devinfo, dispatch, modifiers, cursor);

  // Reply with the constructed converter.
  reply_runner->PostTask(FROM_HERE,
                         base::Bind(reply_callback, base::Passed(&converter)));
}

// Close an input device. Closing may put the calling thread to sleep, and
// therefore should be run on a thread where latency is not critical. We
// run it on the FILE thread.
void CloseInputDevice(const base::FilePath& path,
                      scoped_ptr<EventConverterEvdev> converter) {
  TRACE_EVENT1("ozone", "CloseInputDevice", "path", path.value());
  converter.reset();
}

}  // namespace

EventFactoryEvdev::EventFactoryEvdev(
    CursorDelegateEvdev* cursor,
    DeviceManager* device_manager)
    : device_manager_(device_manager),
      cursor_(cursor),
      dispatch_callback_(
          base::Bind(base::IgnoreResult(&EventFactoryEvdev::DispatchUiEvent),
                     base::Unretained(this))),
      weak_ptr_factory_(this) {
  CHECK(device_manager_);
}

EventFactoryEvdev::~EventFactoryEvdev() { STLDeleteValues(&converters_); }

void EventFactoryEvdev::DispatchUiEvent(Event* event) {
  DispatchEvent(event);
}

void EventFactoryEvdev::AttachInputDevice(
    const base::FilePath& path,
    scoped_ptr<EventConverterEvdev> converter) {
  TRACE_EVENT1("ozone", "AttachInputDevice", "path", path.value());
  CHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  // If we have an existing device, detach it. We don't want two
  // devices with the same name open at the same time.
  if (converters_[path])
    DetachInputDevice(path);

  // Add initialized device to map.
  converters_[path] = converter.release();
  converters_[path]->Start();
}

void EventFactoryEvdev::OnDeviceEvent(const DeviceEvent& event) {
  if (event.device_type() != DeviceEvent::INPUT)
    return;

  switch (event.action_type()) {
    case DeviceEvent::ADD:
    case DeviceEvent::CHANGE: {
      TRACE_EVENT1("ozone", "OnDeviceAdded", "path", event.path().value());

      // Dispatch task to open from the worker pool, since open may block.
      base::WorkerPool::PostTask(
          FROM_HERE,
          base::Bind(&OpenInputDevice,
                     event.path(),
                     &modifiers_,
                     cursor_,
                     ui_task_runner_,
                     dispatch_callback_,
                     base::Bind(&EventFactoryEvdev::AttachInputDevice,
                                weak_ptr_factory_.GetWeakPtr(),
                                event.path())),
          true);
    }
      break;
    case DeviceEvent::REMOVE: {
      TRACE_EVENT1("ozone", "OnDeviceRemoved", "path", event.path().value());
      DetachInputDevice(event.path());
    }
      break;
  }
}

void EventFactoryEvdev::OnDispatcherListChanged() {
  if (!ui_task_runner_) {
    ui_task_runner_ = base::MessageLoopProxy::current();
    // Scan & monitor devices.
    device_manager_->AddObserver(this);
    device_manager_->ScanDevices(this);
  }
}

void EventFactoryEvdev::DetachInputDevice(const base::FilePath& path) {
  TRACE_EVENT1("ozone", "DetachInputDevice", "path", path.value());
  CHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  // Remove device from map.
  scoped_ptr<EventConverterEvdev> converter(converters_[path]);
  converters_.erase(path);

  if (converter) {
    // Cancel libevent notifications from this converter. This part must be
    // on UI since the polling happens on UI.
    converter->Stop();

    // Dispatch task to close from the worker pool, since close may block.
    base::WorkerPool::PostTask(
        FROM_HERE,
        base::Bind(&CloseInputDevice, path, base::Passed(&converter)),
        true);
  }
}

void EventFactoryEvdev::WarpCursorTo(gfx::AcceleratedWidget widget,
                                     const gfx::PointF& location) {
  if (cursor_) {
    cursor_->MoveCursorTo(widget, location);
    MouseEvent mouse_event(ET_MOUSE_MOVED,
                           cursor_->location(),
                           cursor_->location(),
                           modifiers_.GetModifierFlags(),
                           /* changed_button_flags */ 0);
    DispatchEvent(&mouse_event);
  }
}

}  // namespace ui
