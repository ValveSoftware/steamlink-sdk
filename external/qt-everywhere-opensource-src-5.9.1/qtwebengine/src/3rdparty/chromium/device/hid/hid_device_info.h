// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_DEVICE_INFO_H_
#define DEVICE_HID_HID_DEVICE_INFO_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "device/hid/hid_collection_info.h"

namespace device {

enum HidBusType {
  kHIDBusTypeUSB = 0,
  kHIDBusTypeBluetooth = 1,
};

#if defined(OS_MACOSX)
typedef uint64_t HidDeviceId;
const uint64_t kInvalidHidDeviceId = -1;
#else
typedef std::string HidDeviceId;
extern const char kInvalidHidDeviceId[];
#endif

class HidDeviceInfo : public base::RefCountedThreadSafe<HidDeviceInfo> {
 public:
  HidDeviceInfo(const HidDeviceId& device_id,
                uint16_t vendor_id,
                uint16_t product_id,
                const std::string& product_name,
                const std::string& serial_number,
                HidBusType bus_type,
                const std::vector<uint8_t> report_descriptor);

  HidDeviceInfo(const HidDeviceId& device_id,
                uint16_t vendor_id,
                uint16_t product_id,
                const std::string& product_name,
                const std::string& serial_number,
                HidBusType bus_type,
                const HidCollectionInfo& collection,
                size_t max_input_report_size,
                size_t max_output_report_size,
                size_t max_feature_report_size);

  // Device identification.
  const HidDeviceId& device_id() const { return device_id_; }
  uint16_t vendor_id() const { return vendor_id_; }
  uint16_t product_id() const { return product_id_; }
  const std::string& product_name() const { return product_name_; }
  const std::string& serial_number() const { return serial_number_; }
  HidBusType bus_type() const { return bus_type_; }

  // Top-Level Collections information.
  const std::vector<HidCollectionInfo>& collections() const {
    return collections_;
  }
  bool has_report_id() const { return has_report_id_; };
  size_t max_input_report_size() const { return max_input_report_size_; }
  size_t max_output_report_size() const { return max_output_report_size_; }
  size_t max_feature_report_size() const { return max_feature_report_size_; }

  // The raw HID report descriptor is not available on Windows.
  const std::vector<uint8_t>& report_descriptor() const {
    return report_descriptor_;
  }

 protected:
  virtual ~HidDeviceInfo();

 private:
  friend class base::RefCountedThreadSafe<HidDeviceInfo>;

  // Device identification.
  HidDeviceId device_id_;
  uint16_t vendor_id_;
  uint16_t product_id_;
  std::string product_name_;
  std::string serial_number_;
  HidBusType bus_type_;
  std::vector<uint8_t> report_descriptor_;

  // Top-Level Collections information.
  std::vector<HidCollectionInfo> collections_;
  bool has_report_id_;
  size_t max_input_report_size_;
  size_t max_output_report_size_;
  size_t max_feature_report_size_;

  DISALLOW_COPY_AND_ASSIGN(HidDeviceInfo);
};

}  // namespace device

#endif  // DEVICE_HID_HID_DEVICE_INFO_H_
