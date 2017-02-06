// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_storage/system_storage_api.h"

#include "base/memory/ptr_util.h"

using storage_monitor::StorageMonitor;

namespace extensions {

using api::system_storage::StorageUnitInfo;
namespace EjectDevice = api::system_storage::EjectDevice;
namespace GetAvailableCapacity = api::system_storage::GetAvailableCapacity;

SystemStorageGetInfoFunction::SystemStorageGetInfoFunction() {
}

SystemStorageGetInfoFunction::~SystemStorageGetInfoFunction() {
}

bool SystemStorageGetInfoFunction::RunAsync() {
  StorageInfoProvider::Get()->StartQueryInfo(base::Bind(
      &SystemStorageGetInfoFunction::OnGetStorageInfoCompleted, this));
  return true;
}

void SystemStorageGetInfoFunction::OnGetStorageInfoCompleted(bool success) {
  if (success) {
    results_ = api::system_storage::GetInfo::Results::Create(
        StorageInfoProvider::Get()->storage_unit_info_list());
  } else {
    SetError("Error occurred when querying storage information.");
  }

  SendResponse(success);
}

SystemStorageEjectDeviceFunction::~SystemStorageEjectDeviceFunction() {
}

bool SystemStorageEjectDeviceFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<EjectDevice::Params> params(
      EjectDevice::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  StorageMonitor::GetInstance()->EnsureInitialized(
      base::Bind(&SystemStorageEjectDeviceFunction::OnStorageMonitorInit,
                 this,
                 params->id));
  return true;
}

void SystemStorageEjectDeviceFunction::OnStorageMonitorInit(
    const std::string& transient_device_id) {
  DCHECK(StorageMonitor::GetInstance()->IsInitialized());
  StorageMonitor* monitor = StorageMonitor::GetInstance();
  std::string device_id_str =
      StorageMonitor::GetInstance()->GetDeviceIdForTransientId(
          transient_device_id);

  if (device_id_str.empty()) {
    HandleResponse(StorageMonitor::EJECT_NO_SUCH_DEVICE);
    return;
  }

  monitor->EjectDevice(
      device_id_str,
      base::Bind(&SystemStorageEjectDeviceFunction::HandleResponse, this));
}

void SystemStorageEjectDeviceFunction::HandleResponse(
    StorageMonitor::EjectStatus status) {
  api::system_storage::EjectDeviceResultCode result =
      api::system_storage::EJECT_DEVICE_RESULT_CODE_FAILURE;
  switch (status) {
    case StorageMonitor::EJECT_OK:
      result = api::system_storage::EJECT_DEVICE_RESULT_CODE_SUCCESS;
      break;
    case StorageMonitor::EJECT_IN_USE:
      result = api::system_storage::EJECT_DEVICE_RESULT_CODE_IN_USE;
      break;
    case StorageMonitor::EJECT_NO_SUCH_DEVICE:
      result = api::system_storage::EJECT_DEVICE_RESULT_CODE_NO_SUCH_DEVICE;
      break;
    case StorageMonitor::EJECT_FAILURE:
      result = api::system_storage::EJECT_DEVICE_RESULT_CODE_FAILURE;
  }

  SetResult(base::MakeUnique<base::StringValue>(
      api::system_storage::ToString(result)));
  SendResponse(true);
}

SystemStorageGetAvailableCapacityFunction::
    SystemStorageGetAvailableCapacityFunction() {
}

SystemStorageGetAvailableCapacityFunction::
    ~SystemStorageGetAvailableCapacityFunction() {
}

bool SystemStorageGetAvailableCapacityFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<GetAvailableCapacity::Params> params(
      GetAvailableCapacity::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  StorageMonitor::GetInstance()->EnsureInitialized(base::Bind(
      &SystemStorageGetAvailableCapacityFunction::OnStorageMonitorInit,
      this,
      params->id));
  return true;
}

void SystemStorageGetAvailableCapacityFunction::OnStorageMonitorInit(
    const std::string& transient_id) {
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &StorageInfoProvider::GetStorageFreeSpaceFromTransientIdOnFileThread,
          StorageInfoProvider::Get(),
          transient_id),
      base::Bind(&SystemStorageGetAvailableCapacityFunction::OnQueryCompleted,
                 this,
                 transient_id));
}

void SystemStorageGetAvailableCapacityFunction::OnQueryCompleted(
    const std::string& transient_id,
    double available_capacity) {
  bool success = available_capacity >= 0;
  if (success) {
    api::system_storage::StorageAvailableCapacityInfo result;
    result.id = transient_id;
    result.available_capacity = available_capacity;
    SetResult(result.ToValue());
  } else {
    SetError("Error occurred when querying available capacity.");
  }
  SendResponse(success);
}

}  // namespace extensions
