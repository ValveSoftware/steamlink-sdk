// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/device/udev/device_manager_udev.h"

#include <libudev.h>

#include "base/debug/trace_event.h"
#include "base/strings/stringprintf.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_event_observer.h"

namespace ui {

namespace {

const char* kSubsystems[] = {
  "input",
  "drm",
};

// Severity levels from syslog.h. We can't include it directly as it
// conflicts with base/logging.h
enum {
  SYS_LOG_EMERG = 0,
  SYS_LOG_ALERT = 1,
  SYS_LOG_CRIT = 2,
  SYS_LOG_ERR = 3,
  SYS_LOG_WARNING = 4,
  SYS_LOG_NOTICE = 5,
  SYS_LOG_INFO = 6,
  SYS_LOG_DEBUG = 7,
};

// Log handler for messages generated from libudev.
void UdevLog(struct udev* udev,
             int priority,
             const char* file,
             int line,
             const char* fn,
             const char* format,
             va_list args) {
  if (priority <= SYS_LOG_ERR)
    LOG(ERROR) << "libudev: " << fn << ": " << base::StringPrintV(format, args);
  else if (priority <= SYS_LOG_INFO)
    VLOG(1) << "libudev: " << fn << ": " << base::StringPrintV(format, args);
  else  // SYS_LOG_DEBUG
    VLOG(2) << "libudev: " << fn << ": " << base::StringPrintV(format, args);
}

// Create libudev context.
device::ScopedUdevPtr UdevCreate() {
  struct udev* udev = udev_new();
  if (udev) {
    udev_set_log_fn(udev, UdevLog);
    udev_set_log_priority(udev, SYS_LOG_DEBUG);
  }
  return device::ScopedUdevPtr(udev);
}

// Start monitoring input device changes.
device::ScopedUdevMonitorPtr UdevCreateMonitor(struct udev* udev) {
  struct udev_monitor* monitor = udev_monitor_new_from_netlink(udev, "udev");
  if (monitor) {
    for (size_t i = 0; i < arraysize(kSubsystems); ++i)
      udev_monitor_filter_add_match_subsystem_devtype(
          monitor, kSubsystems[i], NULL);

    if (udev_monitor_enable_receiving(monitor))
      LOG(ERROR) << "Failed to start receiving events from udev";
  } else {
    LOG(ERROR) << "Failed to create udev monitor";
  }

  return device::ScopedUdevMonitorPtr(monitor);
}

}  // namespace

DeviceManagerUdev::DeviceManagerUdev() : udev_(UdevCreate()) {
}

DeviceManagerUdev::~DeviceManagerUdev() {
}

void DeviceManagerUdev::CreateMonitor() {
  if (monitor_)
    return;
  monitor_ = UdevCreateMonitor(udev_.get());
  if (monitor_) {
    int fd = udev_monitor_get_fd(monitor_.get());
    CHECK_GT(fd, 0);
    base::MessageLoopForUI::current()->WatchFileDescriptor(
        fd, true, base::MessagePumpLibevent::WATCH_READ, &controller_, this);
  }
}

void DeviceManagerUdev::ScanDevices(DeviceEventObserver* observer) {
  CreateMonitor();

  device::ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev_.get()));
  if (!enumerate)
    return;

  for (size_t i = 0; i < arraysize(kSubsystems); ++i)
    udev_enumerate_add_match_subsystem(enumerate.get(), kSubsystems[i]);
  udev_enumerate_scan_devices(enumerate.get());

  struct udev_list_entry* devices =
      udev_enumerate_get_list_entry(enumerate.get());
  struct udev_list_entry* entry;

  udev_list_entry_foreach(entry, devices) {
    device::ScopedUdevDevicePtr device(udev_device_new_from_syspath(
        udev_.get(), udev_list_entry_get_name(entry)));
    if (!device)
      continue;

    scoped_ptr<DeviceEvent> event = ProcessMessage(device.get());
    if (event)
      observer->OnDeviceEvent(*event.get());
  }
}

void DeviceManagerUdev::AddObserver(DeviceEventObserver* observer) {
  observers_.AddObserver(observer);
}

void DeviceManagerUdev::RemoveObserver(DeviceEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceManagerUdev::OnFileCanReadWithoutBlocking(int fd) {
  // The netlink socket should never become disconnected. There's no need
  // to handle broken connections here.
  TRACE_EVENT1("ozone", "UdevDeviceChange", "socket", fd);

  device::ScopedUdevDevicePtr device(
      udev_monitor_receive_device(monitor_.get()));
  if (!device)
    return;

  scoped_ptr<DeviceEvent> event = ProcessMessage(device.get());
  if (event)
    FOR_EACH_OBSERVER(
        DeviceEventObserver, observers_, OnDeviceEvent(*event.get()));
}

void DeviceManagerUdev::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

scoped_ptr<DeviceEvent> DeviceManagerUdev::ProcessMessage(udev_device* device) {
  const char* path = udev_device_get_devnode(device);
  const char* action = udev_device_get_action(device);
  const char* hotplug = udev_device_get_property_value(device, "HOTPLUG");
  const char* subsystem = udev_device_get_property_value(device, "SUBSYSTEM");

  if (!path || !subsystem)
    return scoped_ptr<DeviceEvent>();

  DeviceEvent::DeviceType device_type;
  if (!strcmp(subsystem, "input") &&
      StartsWithASCII(path, "/dev/input/event", true))
    device_type = DeviceEvent::INPUT;
  else if (!strcmp(subsystem, "drm") && hotplug && !strcmp(hotplug, "1"))
    device_type = DeviceEvent::DISPLAY;
  else
    return scoped_ptr<DeviceEvent>();

  DeviceEvent::ActionType action_type;
  if (!action || !strcmp(action, "add"))
    action_type = DeviceEvent::ADD;
  else if (!strcmp(action, "remove"))
    action_type = DeviceEvent::REMOVE;
  else if (!strcmp(action, "change"))
    action_type = DeviceEvent::CHANGE;
  else
    return scoped_ptr<DeviceEvent>();

  return scoped_ptr<DeviceEvent>(
      new DeviceEvent(device_type, action_type, base::FilePath(path)));
}

}  // namespace ui
