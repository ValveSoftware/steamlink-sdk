// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_COLLECTION_INFO_H_
#define DEVICE_HID_HID_COLLECTION_INFO_H_

#include <set>

#include "device/hid/hid_usage_and_page.h"

namespace device {

struct HidCollectionInfo {
  HidCollectionInfo();
  HidCollectionInfo(const HidCollectionInfo& other);
  ~HidCollectionInfo();

  // Collection's usage ID.
  HidUsageAndPage usage;

  // HID report IDs which belong
  // to this collection or to its
  // embedded collections.
  std::set<int> report_ids;
};

}  // namespace device"

#endif  // DEVICE_HID_HID_COLLECTION_INFO_H_
