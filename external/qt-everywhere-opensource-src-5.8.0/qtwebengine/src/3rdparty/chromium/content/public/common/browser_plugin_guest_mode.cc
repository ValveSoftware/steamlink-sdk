// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/browser_plugin_guest_mode.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"

namespace content {

// static
bool BrowserPluginGuestMode::UseCrossProcessFramesForGuests() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseCrossProcessFramesForGuests);
}

}  // namespace content
