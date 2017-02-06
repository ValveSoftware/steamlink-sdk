// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/serial/serial_service_factory.h"

#include <utility>

#include "content/public/browser/browser_thread.h"
#include "device/serial/serial_service_impl.h"

namespace extensions {
namespace {
const base::Callback<void(
    mojo::InterfaceRequest<device::serial::SerialService>)>*
    g_serial_service_test_factory = nullptr;
}

void BindToSerialServiceRequest(
    mojo::InterfaceRequest<device::serial::SerialService> request) {
  if (g_serial_service_test_factory) {
    g_serial_service_test_factory->Run(std::move(request));
    return;
  }
  device::SerialServiceImpl::CreateOnMessageLoop(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI),
      std::move(request));
}

void SetSerialServiceFactoryForTest(const base::Callback<
    void(mojo::InterfaceRequest<device::serial::SerialService>)>* callback) {
  g_serial_service_test_factory = callback;
}

}  // namespace extensions
