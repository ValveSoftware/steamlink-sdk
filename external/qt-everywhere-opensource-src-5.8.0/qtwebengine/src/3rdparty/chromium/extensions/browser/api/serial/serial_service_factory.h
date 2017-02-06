// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SERIAL_SERIAL_SERVICE_FACTORY_H_
#define EXTENSIONS_BROWSER_API_SERIAL_SERIAL_SERVICE_FACTORY_H_

#include "base/callback.h"
#include "device/serial/serial.mojom.h"
#include "extensions/browser/mojo/service_registration.h"

namespace extensions {

void BindToSerialServiceRequest(
    mojo::InterfaceRequest<device::serial::SerialService> request);

void SetSerialServiceFactoryForTest(const base::Callback<void(
    mojo::InterfaceRequest<device::serial::SerialService> request)>* callback);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SERIAL_SERIAL_SERVICE_FACTORY_H_
