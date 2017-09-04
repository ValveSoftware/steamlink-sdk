// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_UI_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace base {
  class ListValue;
}  // namespace base

namespace content {

class AccessibilityUI : public WebUIController {
 public:
  explicit AccessibilityUI(WebUI* web_ui);
  ~AccessibilityUI() override;

 private:
  void ToggleAccessibility(const base::ListValue* args);
  void ToggleGlobalAccessibility(const base::ListValue* args);
  void ToggleInternalTree(const base::ListValue* args);
  void RequestAccessibilityTree(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(AccessibilityUI);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_UI_H_
