// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/device_monitor_linux.h"

#include <libudev.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"

namespace device {

namespace {

const char kUdevName[] = "udev";
const char kUdevActionAdd[] = "add";
const char kUdevActionRemove[] = "remove";

// The instance will be reset when message loop destroys.
base::LazyInstance<scoped_ptr<DeviceMonitorLinux> >::Leaky
    g_device_monitor_linux_ptr = LAZY_INSTANCE_INITIALIZER;

}  // namespace

DeviceMonitorLinux::DeviceMonitorLinux() : monitor_fd_(-1) {
  base::ThreadRestrictions::AssertIOAllowed();
  base::MessageLoop::current()->AddDestructionObserver(this);

  udev_.reset(udev_new());
  if (!udev_) {
    LOG(ERROR) << "Failed to create udev.";
    return;
  }
  monitor_.reset(udev_monitor_new_from_netlink(udev_.get(), kUdevName));
  if (!monitor_) {
    LOG(ERROR) << "Failed to create udev monitor.";
    return;
  }

  int ret = udev_monitor_enable_receiving(monitor_.get());
  if (ret != 0) {
    LOG(ERROR) << "Failed to start udev monitoring.";
    return;
  }

  monitor_fd_ = udev_monitor_get_fd(monitor_.get());
  if (monitor_fd_ <= 0) {
    LOG(ERROR) << "Failed to start udev monitoring.";
    return;
  }

  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          monitor_fd_,
          true,
          base::MessageLoopForIO::WATCH_READ,
          &monitor_watcher_,
          this)) {
    return;
  }
}

// static
DeviceMonitorLinux* DeviceMonitorLinux::GetInstance() {
  if (!HasInstance())
    g_device_monitor_linux_ptr.Get().reset(new DeviceMonitorLinux());
  return g_device_monitor_linux_ptr.Get().get();
}

// static
bool DeviceMonitorLinux::HasInstance() {
  return g_device_monitor_linux_ptr.Get().get();
}

void DeviceMonitorLinux::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (observer)
    observers_.AddObserver(observer);
}

void DeviceMonitorLinux::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (observer)
    observers_.RemoveObserver(observer);
}

ScopedUdevDevicePtr DeviceMonitorLinux::GetDeviceFromPath(
    const std::string& path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScopedUdevDevicePtr device(
      udev_device_new_from_syspath(udev_.get(), path.c_str()));
  return device.Pass();
}

void DeviceMonitorLinux::Enumerate(const EnumerateCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev_.get()));

  if (!enumerate) {
    LOG(ERROR) << "Failed to enumerate devices.";
    return;
  }

  if (udev_enumerate_scan_devices(enumerate.get()) != 0) {
    LOG(ERROR) << "Failed to enumerate devices.";
    return;
  }

  // This list is managed by |enumerate|.
  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate.get());
  for (udev_list_entry* i = devices; i != NULL;
       i = udev_list_entry_get_next(i)) {
    ScopedUdevDevicePtr device(
        udev_device_new_from_syspath(udev_.get(), udev_list_entry_get_name(i)));
    if (device)
      callback.Run(device.get());
  }
}

void DeviceMonitorLinux::WillDestroyCurrentMessageLoop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  g_device_monitor_linux_ptr.Get().reset(NULL);
}

void DeviceMonitorLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(monitor_fd_, fd);

  ScopedUdevDevicePtr device(udev_monitor_receive_device(monitor_.get()));
  if (!device)
    return;

  std::string action(udev_device_get_action(device.get()));
  if (action == kUdevActionAdd)
    FOR_EACH_OBSERVER(Observer, observers_, OnDeviceAdded(device.get()));
  else if (action == kUdevActionRemove)
    FOR_EACH_OBSERVER(Observer, observers_, OnDeviceRemoved(device.get()));
}

void DeviceMonitorLinux::OnFileCanWriteWithoutBlocking(int fd) {}

DeviceMonitorLinux::~DeviceMonitorLinux() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::MessageLoop::current()->RemoveDestructionObserver(this);
  monitor_watcher_.StopWatchingFileDescriptor();
  close(monitor_fd_);
}

}  // namespace device
