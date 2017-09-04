// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_service.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "services/service_manager/public/cpp/connection.h"

namespace device {

std::unique_ptr<service_manager::Service> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) {
  return base::MakeUnique<DeviceService>(std::move(file_task_runner));
}

DeviceService::DeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : file_task_runner_(std::move(file_task_runner)) {}

DeviceService::~DeviceService() {}

void DeviceService::OnStart() {}

bool DeviceService::OnConnect(const service_manager::ServiceInfo& remote_info,
                              service_manager::InterfaceRegistry* registry) {
  return true;
}

}  // namespace device
