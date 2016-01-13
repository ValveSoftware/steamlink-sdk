// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service_win.h"

#include <cstdlib>

#include "base/files/file.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "device/hid/hid_connection_win.h"
#include "device/hid/hid_device_info.h"
#include "net/base/io_buffer.h"

#if defined(OS_WIN)

#define INITGUID

#include <windows.h>
#include <hidclass.h>

extern "C" {

#include <hidsdi.h>
#include <hidpi.h>

}

#include <setupapi.h>
#include <winioctl.h>
#include "base/win/scoped_handle.h"

#endif  // defined(OS_WIN)

// Setup API is required to enumerate HID devices.
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace device {
namespace {

const char kHIDClass[] = "HIDClass";

}  // namespace

HidServiceWin::HidServiceWin() {
  Enumerate();
}

HidServiceWin::~HidServiceWin() {}

void HidServiceWin::Enumerate() {
  BOOL res;
  HDEVINFO device_info_set;
  SP_DEVINFO_DATA devinfo_data;
  SP_DEVICE_INTERFACE_DATA device_interface_data;

  memset(&devinfo_data, 0, sizeof(SP_DEVINFO_DATA));
  devinfo_data.cbSize = sizeof(SP_DEVINFO_DATA);
  device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

  device_info_set = SetupDiGetClassDevs(
      &GUID_DEVINTERFACE_HID,
      NULL,
      NULL,
      DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

  if (device_info_set == INVALID_HANDLE_VALUE)
    return;

  for (int device_index = 0;
       SetupDiEnumDeviceInterfaces(device_info_set,
                                   NULL,
                                   &GUID_DEVINTERFACE_HID,
                                   device_index,
                                   &device_interface_data);
       ++device_index) {
    DWORD required_size = 0;

    // Determime the required size of detail struct.
    SetupDiGetDeviceInterfaceDetailA(device_info_set,
                                     &device_interface_data,
                                     NULL,
                                     0,
                                     &required_size,
                                     NULL);

    scoped_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA_A, base::FreeDeleter>
        device_interface_detail_data(
            static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(
                malloc(required_size)));
    device_interface_detail_data->cbSize =
        sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

    // Get the detailed data for this device.
    res = SetupDiGetDeviceInterfaceDetailA(device_info_set,
                                           &device_interface_data,
                                           device_interface_detail_data.get(),
                                           required_size,
                                           NULL,
                                           NULL);
    if (!res)
      continue;

    // Enumerate device info. Looking for Setup Class "HIDClass".
    for (DWORD i = 0;
        SetupDiEnumDeviceInfo(device_info_set, i, &devinfo_data);
        i++) {
      char class_name[256] = {0};
      res = SetupDiGetDeviceRegistryPropertyA(device_info_set,
                                              &devinfo_data,
                                              SPDRP_CLASS,
                                              NULL,
                                              (PBYTE) class_name,
                                              sizeof(class_name) - 1,
                                              NULL);
      if (!res)
        break;
      if (memcmp(class_name, kHIDClass, sizeof(kHIDClass)) == 0) {
        char driver_name[256] = {0};
        // Get bounded driver.
        res = SetupDiGetDeviceRegistryPropertyA(device_info_set,
                                                &devinfo_data,
                                                SPDRP_DRIVER,
                                                NULL,
                                                (PBYTE) driver_name,
                                                sizeof(driver_name) - 1,
                                                NULL);
        if (res) {
          // Found the driver.
          break;
        }
      }
    }

    if (!res)
      continue;

    PlatformAddDevice(device_interface_detail_data->DevicePath);
  }
}

void HidServiceWin::PlatformAddDevice(const std::string& device_path) {
  HidDeviceInfo device_info;
  device_info.device_id = device_path;

  // Try to open the device.
  base::win::ScopedHandle device_handle(
      CreateFileA(device_path.c_str(),
                  GENERIC_WRITE | GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                  NULL,
                  OPEN_EXISTING,
                  FILE_FLAG_OVERLAPPED,
                  0));

  if (!device_handle.IsValid() &&
      GetLastError() == base::File::FILE_ERROR_ACCESS_DENIED) {
    base::win::ScopedHandle device_handle(
      CreateFileA(device_path.c_str(),
      GENERIC_READ,
      FILE_SHARE_READ,
      NULL,
      OPEN_EXISTING,
      FILE_FLAG_OVERLAPPED,
      0));

    if (!device_handle.IsValid())
      return;
  }

  // Get VID/PID pair.
  HIDD_ATTRIBUTES attrib = {0};
  attrib.Size = sizeof(HIDD_ATTRIBUTES);
  if (!HidD_GetAttributes(device_handle.Get(), &attrib))
    return;

  device_info.vendor_id = attrib.VendorID;
  device_info.product_id = attrib.ProductID;

  for (ULONG i = 32;
      HidD_SetNumInputBuffers(device_handle.Get(), i);
      i <<= 1);

  // Get usage and usage page (optional).
  PHIDP_PREPARSED_DATA preparsed_data;
  if (HidD_GetPreparsedData(device_handle.Get(), &preparsed_data) &&
      preparsed_data) {
    HIDP_CAPS capabilities;
    if (HidP_GetCaps(preparsed_data, &capabilities) == HIDP_STATUS_SUCCESS) {
      device_info.input_report_size = capabilities.InputReportByteLength;
      device_info.output_report_size = capabilities.OutputReportByteLength;
      device_info.feature_report_size = capabilities.FeatureReportByteLength;
      device_info.usages.push_back(HidUsageAndPage(
        capabilities.Usage,
        static_cast<HidUsageAndPage::Page>(capabilities.UsagePage)));
    }
    // Detect if the device supports report ids.
    if (capabilities.NumberInputValueCaps > 0) {
      scoped_ptr<HIDP_VALUE_CAPS[]> value_caps(
          new HIDP_VALUE_CAPS[capabilities.NumberInputValueCaps]);
      USHORT value_caps_length = capabilities.NumberInputValueCaps;
      if (HidP_GetValueCaps(HidP_Input, &value_caps[0], &value_caps_length,
                            preparsed_data) == HIDP_STATUS_SUCCESS) {
        device_info.has_report_id = (value_caps[0].ReportID != 0);
      }
    }
    if (!device_info.has_report_id && capabilities.NumberInputButtonCaps > 0)
    {
      scoped_ptr<HIDP_BUTTON_CAPS[]> button_caps(
        new HIDP_BUTTON_CAPS[capabilities.NumberInputButtonCaps]);
      USHORT button_caps_length = capabilities.NumberInputButtonCaps;
      if (HidP_GetButtonCaps(HidP_Input,
                             &button_caps[0],
                             &button_caps_length,
                             preparsed_data) == HIDP_STATUS_SUCCESS) {
        device_info.has_report_id = (button_caps[0].ReportID != 0);
      }
    }

    HidD_FreePreparsedData(preparsed_data);
  }

  AddDevice(device_info);
}

void HidServiceWin::PlatformRemoveDevice(const std::string& device_path) {
  RemoveDevice(device_path);
}

void HidServiceWin::GetDevices(std::vector<HidDeviceInfo>* devices) {
  Enumerate();
  HidService::GetDevices(devices);
}

scoped_refptr<HidConnection> HidServiceWin::Connect(
    const HidDeviceId& device_id) {
  HidDeviceInfo device_info;
  if (!GetDeviceInfo(device_id, &device_info))
    return NULL;
  scoped_refptr<HidConnectionWin> connection(new HidConnectionWin(device_info));
  if (!connection->available()) {
    PLOG(ERROR) << "Failed to open device.";
    return NULL;
  }
  return connection;
}

}  // namespace device
