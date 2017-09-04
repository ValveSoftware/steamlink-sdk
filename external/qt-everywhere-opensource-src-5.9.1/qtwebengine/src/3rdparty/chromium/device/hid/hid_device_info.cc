// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_report_descriptor.h"

namespace device {

#if !defined(OS_MACOSX)
const char kInvalidHidDeviceId[] = "";
#endif

HidDeviceInfo::HidDeviceInfo(const HidDeviceId& device_id,
                             uint16_t vendor_id,
                             uint16_t product_id,
                             const std::string& product_name,
                             const std::string& serial_number,
                             HidBusType bus_type,
                             const std::vector<uint8_t> report_descriptor)
    : device_id_(device_id),
      vendor_id_(vendor_id),
      product_id_(product_id),
      product_name_(product_name),
      serial_number_(serial_number),
      bus_type_(bus_type),
      report_descriptor_(report_descriptor) {
  HidReportDescriptor descriptor_parser(report_descriptor_);
  descriptor_parser.GetDetails(
      &collections_, &has_report_id_, &max_input_report_size_,
      &max_output_report_size_, &max_feature_report_size_);
}

HidDeviceInfo::HidDeviceInfo(const HidDeviceId& device_id,
                             uint16_t vendor_id,
                             uint16_t product_id,
                             const std::string& product_name,
                             const std::string& serial_number,
                             HidBusType bus_type,
                             const HidCollectionInfo& collection,
                             size_t max_input_report_size,
                             size_t max_output_report_size,
                             size_t max_feature_report_size)
    : device_id_(device_id),
      vendor_id_(vendor_id),
      product_id_(product_id),
      product_name_(product_name),
      serial_number_(serial_number),
      bus_type_(bus_type),
      max_input_report_size_(max_input_report_size),
      max_output_report_size_(max_output_report_size),
      max_feature_report_size_(max_feature_report_size) {
  collections_.push_back(collection);
  has_report_id_ = !collection.report_ids.empty();
}

HidDeviceInfo::~HidDeviceInfo() {}

}  // namespace device
