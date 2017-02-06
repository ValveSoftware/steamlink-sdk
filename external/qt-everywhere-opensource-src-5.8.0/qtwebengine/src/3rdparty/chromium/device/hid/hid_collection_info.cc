// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_collection_info.h"

namespace device {

HidCollectionInfo::HidCollectionInfo()
    : usage(HidUsageAndPage::kGenericDesktopUndefined,
            HidUsageAndPage::kPageUndefined) {
}

HidCollectionInfo::HidCollectionInfo(const HidCollectionInfo& other) = default;

HidCollectionInfo::~HidCollectionInfo() {
}

}  // namespace device
