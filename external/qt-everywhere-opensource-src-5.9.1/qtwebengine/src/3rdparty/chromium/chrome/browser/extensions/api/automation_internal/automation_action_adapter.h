// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_ACTION_ADAPTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_ACTION_ADAPTER_H_

#include <stdint.h>

#include "ui/accessibility/ax_enums.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
struct AXActionData;
}

namespace extensions {

// Adapts an object to receive actions from the Automation extension API.
class AutomationActionAdapter {
 public:
  // Performs an action on the target node.
  virtual void PerformAction(const ui::AXActionData& data) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_ACTION_ADAPTER_H_
