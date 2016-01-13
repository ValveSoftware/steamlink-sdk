// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/nfc/nfc_tag_technology.h"

#include "device/nfc/nfc_tag.h"

namespace device {

NfcTagTechnology::NfcTagTechnology(NfcTag* tag) : tag_(tag) {
}

NfcTagTechnology::NfcTagTechnology() : tag_(NULL) {
}

NfcTagTechnology::~NfcTagTechnology() {
}

bool NfcNdefTagTechnology::IsSupportedByTag() const {
  return tag() && (tag()->GetSupportedTechnologies() &
      NfcTagTechnology::kTechnologyTypeNdef);
}

NfcNdefTagTechnology::NfcNdefTagTechnology(NfcTag* tag)
    : NfcTagTechnology(tag) {
}

NfcNdefTagTechnology::~NfcNdefTagTechnology() {
}

}  // namespace device
