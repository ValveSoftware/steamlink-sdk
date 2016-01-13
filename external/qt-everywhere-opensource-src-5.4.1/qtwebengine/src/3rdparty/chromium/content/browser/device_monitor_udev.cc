// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// libudev is used for monitoring device changes.

#include "content/browser/device_monitor_udev.h"

#include <string>

#include "base/system_monitor/system_monitor.h"
#include "content/browser/udev_linux.h"
#include "content/public/browser/browser_thread.h"
#include "device/udev_linux/udev.h"

namespace {

struct SubsystemMap {
  base::SystemMonitor::DeviceType device_type;
  const char* subsystem;
  const char* devtype;
};

const char kAudioSubsystem[] = "sound";
const char kVideoSubsystem[] = "video4linux";

// Add more subsystems here for monitoring.
const SubsystemMap kSubsystemMap[] = {
  { base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE, kAudioSubsystem, NULL },
  { base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE, kVideoSubsystem, NULL },
};

}  // namespace

namespace content {

DeviceMonitorLinux::DeviceMonitorLinux() {
  DCHECK(BrowserThread::IsMessageLoopValid(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&DeviceMonitorLinux::Initialize, base::Unretained(this)));
}

DeviceMonitorLinux::~DeviceMonitorLinux() {
}

void DeviceMonitorLinux::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // We want to be notified of IO message loop destruction to delete |udev_|.
  base::MessageLoop::current()->AddDestructionObserver(this);

  std::vector<UdevLinux::UdevMonitorFilter> filters;
  for (size_t i = 0; i < arraysize(kSubsystemMap); ++i) {
    filters.push_back(UdevLinux::UdevMonitorFilter(
        kSubsystemMap[i].subsystem, kSubsystemMap[i].devtype));
  }
  udev_.reset(new UdevLinux(filters,
      base::Bind(&DeviceMonitorLinux::OnDevicesChanged,
                 base::Unretained(this))));
}

void DeviceMonitorLinux::WillDestroyCurrentMessageLoop() {
  // Called on IO thread.
  udev_.reset();
}

void DeviceMonitorLinux::OnDevicesChanged(udev_device* device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(device);

  base::SystemMonitor::DeviceType device_type =
      base::SystemMonitor::DEVTYPE_UNKNOWN;
  std::string subsystem(libudev.udev_device_get_subsystem(device));
  for (size_t i = 0; i < arraysize(kSubsystemMap); ++i) {
    if (subsystem == kSubsystemMap[i].subsystem) {
      device_type = kSubsystemMap[i].device_type;
      break;
    }
  }
  DCHECK_NE(device_type, base::SystemMonitor::DEVTYPE_UNKNOWN);

  base::SystemMonitor::Get()->ProcessDevicesChanged(device_type);
}

}  // namespace content
