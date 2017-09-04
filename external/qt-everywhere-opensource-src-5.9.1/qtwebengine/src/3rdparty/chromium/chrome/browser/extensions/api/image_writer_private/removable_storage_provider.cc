
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

// A device list to be returned when testing.
static base::LazyInstance<scoped_refptr<StorageDeviceList> > g_test_device_list;

// TODO(haven): Udev code may be duplicated in the Chrome codebase.
// https://code.google.com/p/chromium/issues/detail?id=284898

void RemovableStorageProvider::GetAllDevices(DeviceListReadyCallback callback) {
  if (g_test_device_list.Get().get() != NULL) {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE,
        FROM_HERE,
        base::Bind(callback, g_test_device_list.Get(), true));
    return;
  }

  scoped_refptr<StorageDeviceList> device_list(new StorageDeviceList);

  // We need to do some file i/o to get the device block size
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(PopulateDeviceList, device_list),
      base::Bind(callback, device_list));
}

void RemovableStorageProvider::SetDeviceListForTesting(
    scoped_refptr<StorageDeviceList> device_list) {
  g_test_device_list.Get() = device_list;
}

void RemovableStorageProvider::ClearDeviceListForTesting() {
  g_test_device_list.Get() = NULL;
}

}  // namespace extensions
