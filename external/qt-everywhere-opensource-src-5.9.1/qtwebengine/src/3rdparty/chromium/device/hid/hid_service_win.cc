// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service_win.h"

#include <memory>

#define INITGUID

#include <dbt.h>
#include <setupapi.h>
#include <stddef.h>
#include <winioctl.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/memory/free_deleter.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/hid/hid_connection_win.h"
#include "device/hid/hid_device_info.h"
#include "net/base/io_buffer.h"

namespace device {

HidServiceWin::HidServiceWin(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : file_task_runner_(file_task_runner),
      device_observer_(this),
      weak_factory_(this) {
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  DCHECK(task_runner_.get());
  DeviceMonitorWin* device_monitor =
      DeviceMonitorWin::GetForDeviceInterface(GUID_DEVINTERFACE_HID);
  if (device_monitor) {
    device_observer_.Add(device_monitor);
  }
  file_task_runner_->PostTask(
      FROM_HERE, base::Bind(&HidServiceWin::EnumerateOnFileThread,
                            weak_factory_.GetWeakPtr(), task_runner_));
}

HidServiceWin::~HidServiceWin() {}

void HidServiceWin::Connect(const HidDeviceId& device_id,
                            const ConnectCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const auto& map_entry = devices().find(device_id);
  if (map_entry == devices().end()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
    return;
  }
  scoped_refptr<HidDeviceInfo> device_info = map_entry->second;

  base::win::ScopedHandle file(OpenDevice(device_info->device_id()));
  if (!file.IsValid()) {
    HID_PLOG(EVENT) << "Failed to open device";
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(callback, make_scoped_refptr(
          new HidConnectionWin(device_info, std::move(file)))));
}

// static
void HidServiceWin::EnumerateOnFileThread(
    base::WeakPtr<HidServiceWin> service,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  HDEVINFO device_info_set =
      SetupDiGetClassDevs(&GUID_DEVINTERFACE_HID, NULL, NULL,
                          DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

  if (device_info_set != INVALID_HANDLE_VALUE) {
    SP_DEVICE_INTERFACE_DATA device_interface_data;
    device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (int device_index = 0;
         SetupDiEnumDeviceInterfaces(device_info_set,
                                     NULL,
                                     &GUID_DEVINTERFACE_HID,
                                     device_index,
                                     &device_interface_data);
         ++device_index) {
      DWORD required_size = 0;

      // Determime the required size of detail struct.
      SetupDiGetDeviceInterfaceDetail(device_info_set, &device_interface_data,
                                      NULL, 0, &required_size, NULL);

      std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA, base::FreeDeleter>
      device_interface_detail_data(
          static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(malloc(required_size)));
      device_interface_detail_data->cbSize =
          sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

      // Get the detailed data for this device.
      BOOL res = SetupDiGetDeviceInterfaceDetail(
          device_info_set, &device_interface_data,
          device_interface_detail_data.get(), required_size, NULL, NULL);
      if (!res) {
        continue;
      }

      std::string device_path(
          base::SysWideToUTF8(device_interface_detail_data->DevicePath));
      DCHECK(base::IsStringASCII(device_path));
      AddDeviceOnFileThread(service, task_runner,
                            base::ToLowerASCII(device_path));
    }
  }

  task_runner->PostTask(
      FROM_HERE, base::Bind(&HidServiceWin::FirstEnumerationComplete, service));
}

// static
void HidServiceWin::CollectInfoFromButtonCaps(
    PHIDP_PREPARSED_DATA preparsed_data,
    HIDP_REPORT_TYPE report_type,
    USHORT button_caps_length,
    HidCollectionInfo* collection_info) {
  if (button_caps_length > 0) {
    std::unique_ptr<HIDP_BUTTON_CAPS[]> button_caps(
        new HIDP_BUTTON_CAPS[button_caps_length]);
    if (HidP_GetButtonCaps(report_type,
                           &button_caps[0],
                           &button_caps_length,
                           preparsed_data) == HIDP_STATUS_SUCCESS) {
      for (size_t i = 0; i < button_caps_length; i++) {
        int report_id = button_caps[i].ReportID;
        if (report_id != 0) {
          collection_info->report_ids.insert(report_id);
        }
      }
    }
  }
}

// static
void HidServiceWin::CollectInfoFromValueCaps(
    PHIDP_PREPARSED_DATA preparsed_data,
    HIDP_REPORT_TYPE report_type,
    USHORT value_caps_length,
    HidCollectionInfo* collection_info) {
  if (value_caps_length > 0) {
    std::unique_ptr<HIDP_VALUE_CAPS[]> value_caps(
        new HIDP_VALUE_CAPS[value_caps_length]);
    if (HidP_GetValueCaps(
            report_type, &value_caps[0], &value_caps_length, preparsed_data) ==
        HIDP_STATUS_SUCCESS) {
      for (size_t i = 0; i < value_caps_length; i++) {
        int report_id = value_caps[i].ReportID;
        if (report_id != 0) {
          collection_info->report_ids.insert(report_id);
        }
      }
    }
  }
}

// static
void HidServiceWin::AddDeviceOnFileThread(
    base::WeakPtr<HidServiceWin> service,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const std::string& device_path) {
  base::win::ScopedHandle device_handle(OpenDevice(device_path));
  if (!device_handle.IsValid()) {
    return;
  }

  HIDD_ATTRIBUTES attrib = {0};
  attrib.Size = sizeof(HIDD_ATTRIBUTES);
  if (!HidD_GetAttributes(device_handle.Get(), &attrib)) {
    HID_LOG(EVENT) << "Failed to get device attributes.";
    return;
  }

  PHIDP_PREPARSED_DATA preparsed_data = nullptr;
  if (!HidD_GetPreparsedData(device_handle.Get(), &preparsed_data) ||
      !preparsed_data) {
    HID_LOG(EVENT) << "Failed to get device data.";
    return;
  }

  HIDP_CAPS capabilities = {0};
  if (HidP_GetCaps(preparsed_data, &capabilities) != HIDP_STATUS_SUCCESS) {
    HID_LOG(EVENT) << "Failed to get device capabilities.";
    HidD_FreePreparsedData(preparsed_data);
    return;
  }

  // Whether or not the device includes report IDs in its reports the size
  // of the report ID is included in the value provided by Windows. This
  // appears contrary to the MSDN documentation.
  size_t max_input_report_size = 0;
  size_t max_output_report_size = 0;
  size_t max_feature_report_size = 0;
  if (capabilities.InputReportByteLength > 0) {
    max_input_report_size = capabilities.InputReportByteLength - 1;
  }
  if (capabilities.OutputReportByteLength > 0) {
    max_output_report_size = capabilities.OutputReportByteLength - 1;
  }
  if (capabilities.FeatureReportByteLength > 0) {
    max_feature_report_size = capabilities.FeatureReportByteLength - 1;
  }

  HidCollectionInfo collection_info;
  collection_info.usage = HidUsageAndPage(
      capabilities.Usage,
      static_cast<HidUsageAndPage::Page>(capabilities.UsagePage));
  CollectInfoFromButtonCaps(preparsed_data, HidP_Input,
                            capabilities.NumberInputButtonCaps,
                            &collection_info);
  CollectInfoFromButtonCaps(preparsed_data, HidP_Output,
                            capabilities.NumberOutputButtonCaps,
                            &collection_info);
  CollectInfoFromButtonCaps(preparsed_data, HidP_Feature,
                            capabilities.NumberFeatureButtonCaps,
                            &collection_info);
  CollectInfoFromValueCaps(preparsed_data, HidP_Input,
                           capabilities.NumberInputValueCaps, &collection_info);
  CollectInfoFromValueCaps(preparsed_data, HidP_Output,
                           capabilities.NumberOutputValueCaps,
                           &collection_info);
  CollectInfoFromValueCaps(preparsed_data, HidP_Feature,
                           capabilities.NumberFeatureValueCaps,
                           &collection_info);

  // 1023 characters plus NULL terminator is more than enough for a USB string
  // descriptor which is limited to 126 characters.
  wchar_t buffer[1024];
  std::string product_name;
  if (HidD_GetProductString(device_handle.Get(), &buffer[0], sizeof(buffer))) {
    // NULL termination guaranteed by the API.
    product_name = base::SysWideToUTF8(buffer);
  }
  std::string serial_number;
  if (HidD_GetSerialNumberString(device_handle.Get(), &buffer[0],
                                 sizeof(buffer))) {
    // NULL termination guaranteed by the API.
    serial_number = base::SysWideToUTF8(buffer);
  }

  // This populates the HidDeviceInfo instance without a raw report descriptor.
  // The descriptor is unavailable on Windows because HID devices are exposed to
  // user-space as individual top-level collections.
  scoped_refptr<HidDeviceInfo> device_info(new HidDeviceInfo(
      device_path, attrib.VendorID, attrib.ProductID, product_name,
      serial_number,
      kHIDBusTypeUSB,  // TODO(reillyg): Detect Bluetooth. crbug.com/443335
      collection_info, max_input_report_size, max_output_report_size,
      max_feature_report_size));

  HidD_FreePreparsedData(preparsed_data);
  task_runner->PostTask(
      FROM_HERE, base::Bind(&HidServiceWin::AddDevice, service, device_info));
}

void HidServiceWin::OnDeviceAdded(const GUID& class_guid,
                                  const std::string& device_path) {
  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&HidServiceWin::AddDeviceOnFileThread,
                 weak_factory_.GetWeakPtr(), task_runner_, device_path));
}

void HidServiceWin::OnDeviceRemoved(const GUID& class_guid,
                                    const std::string& device_path) {
  // Execute a no-op closure on the file task runner to synchronize with any
  // devices that are still being enumerated.
  file_task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&base::DoNothing),
      base::Bind(&HidServiceWin::RemoveDevice, weak_factory_.GetWeakPtr(),
                 device_path));
}

// static
base::win::ScopedHandle HidServiceWin::OpenDevice(
    const std::string& device_path) {
  base::win::ScopedHandle file(
      CreateFileA(device_path.c_str(), GENERIC_WRITE | GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                  FILE_FLAG_OVERLAPPED, NULL));
  if (!file.IsValid() && GetLastError() == ERROR_ACCESS_DENIED) {
    file.Set(CreateFileA(device_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                         NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL));
  }
  return file;
}

}  // namespace device
