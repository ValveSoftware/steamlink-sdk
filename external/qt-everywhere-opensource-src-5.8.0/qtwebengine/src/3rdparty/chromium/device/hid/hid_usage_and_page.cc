// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_usage_and_page.h"

namespace device {

bool HidUsageAndPage::IsProtected() const {
  if (usage_page == HidUsageAndPage::kPageKeyboard)
    return true;

  if (usage_page != HidUsageAndPage::kPageGenericDesktop)
    return false;

  if (usage == HidUsageAndPage::kGenericDesktopPointer ||
      usage == HidUsageAndPage::kGenericDesktopMouse ||
      usage == HidUsageAndPage::kGenericDesktopKeyboard ||
      usage == HidUsageAndPage::kGenericDesktopKeypad) {
    return true;
  }

  if (usage >= HidUsageAndPage::kGenericDesktopSystemControl &&
      usage <= HidUsageAndPage::kGenericDesktopSystemWarmRestart) {
    return true;
  }

  if (usage >= HidUsageAndPage::kGenericDesktopSystemDock &&
      usage <= HidUsageAndPage::kGenericDesktopSystemDisplaySwap) {
    return true;
  }

  return false;
}

}  // namespace device
