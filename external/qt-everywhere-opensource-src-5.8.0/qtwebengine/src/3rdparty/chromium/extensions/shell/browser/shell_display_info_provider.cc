// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_display_info_provider.h"

#include "base/logging.h"

namespace extensions {

ShellDisplayInfoProvider::ShellDisplayInfoProvider() {
}

ShellDisplayInfoProvider::~ShellDisplayInfoProvider() {
}

bool ShellDisplayInfoProvider::SetInfo(
    const std::string& display_id,
    const api::system_display::DisplayProperties& info,
    std::string* error) {
  *error = "Not implemented";
  return false;
}

void ShellDisplayInfoProvider::UpdateDisplayUnitInfoForPlatform(
    const display::Display& display,
    extensions::api::system_display::DisplayUnitInfo* unit) {
  static bool logged_once = false;
  if (!logged_once) {
    NOTIMPLEMENTED();
    logged_once = true;
  }
}

// static
DisplayInfoProvider* DisplayInfoProvider::Create() {
  return new ShellDisplayInfoProvider();
}

}  // namespace extensions
