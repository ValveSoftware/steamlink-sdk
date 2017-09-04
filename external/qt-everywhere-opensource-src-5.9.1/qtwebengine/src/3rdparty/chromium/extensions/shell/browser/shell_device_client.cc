// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_device_client.h"

#include "content/public/browser/browser_thread.h"
#include "device/hid/hid_service.h"
#include "device/usb/usb_service.h"

using content::BrowserThread;

namespace extensions {

ShellDeviceClient::ShellDeviceClient() {}

ShellDeviceClient::~ShellDeviceClient() {
#if DCHECK_IS_ON()
  DCHECK(did_shutdown_);
#endif
}

void ShellDeviceClient::Shutdown() {
  if (usb_service_)
    usb_service_->Shutdown();
  if (hid_service_)
    hid_service_->Shutdown();
#if DCHECK_IS_ON()
  did_shutdown_ = true;
#endif
}

device::UsbService* ShellDeviceClient::GetUsbService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!usb_service_) {
    usb_service_ = device::UsbService::Create(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE));
  }
  return usb_service_.get();
}

device::HidService* ShellDeviceClient::GetHidService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!hid_service_) {
    hid_service_ = device::HidService::Create(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE));
  }
  return hid_service_.get();
}

}
