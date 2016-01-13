// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/device/device_manager_manual.h"

#include "base/files/file_enumerator.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_event_observer.h"

namespace ui {

DeviceManagerManual::DeviceManagerManual() {}

DeviceManagerManual::~DeviceManagerManual() {}

void DeviceManagerManual::ScanDevices(DeviceEventObserver* observer) {
  base::FileEnumerator file_enum(base::FilePath("/dev/input"),
                                 false,
                                 base::FileEnumerator::FILES,
                                 "event*[0-9]");
  for (base::FilePath path = file_enum.Next(); !path.empty();
       path = file_enum.Next()) {
    DeviceEvent event(DeviceEvent::INPUT, DeviceEvent::ADD, path);
    observer->OnDeviceEvent(event);
  }
}

void DeviceManagerManual::AddObserver(DeviceEventObserver* observer) {}

void DeviceManagerManual::RemoveObserver(DeviceEventObserver* observer) {}

}  // namespace ui
