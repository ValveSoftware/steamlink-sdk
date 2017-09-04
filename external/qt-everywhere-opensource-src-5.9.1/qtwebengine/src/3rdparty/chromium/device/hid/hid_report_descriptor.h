// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_REPORT_DESCRIPTOR_H_
#define DEVICE_HID_HID_REPORT_DESCRIPTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/memory/linked_ptr.h"
#include "device/hid/hid_collection_info.h"
#include "device/hid/hid_report_descriptor_item.h"

namespace device {

// HID report descriptor.
// See section 6.2.2 of HID specifications (v1.11).
class HidReportDescriptor {

 public:
  HidReportDescriptor(const std::vector<uint8_t>& bytes);
  ~HidReportDescriptor();

  const std::vector<linked_ptr<HidReportDescriptorItem> >& items() const {
    return items_;
  }

  // Returns top-level collections present in the descriptor,
  // together with max report sizes
  void GetDetails(std::vector<HidCollectionInfo>* top_level_collections,
                  bool* has_report_id,
                  size_t* max_input_report_size,
                  size_t* max_output_report_size,
                  size_t* max_feature_report_size);

 private:
  std::vector<linked_ptr<HidReportDescriptorItem> > items_;
};

}  // namespace device

#endif  // DEVICE_HID_HID_REPORT_DESCRIPTOR_H_
