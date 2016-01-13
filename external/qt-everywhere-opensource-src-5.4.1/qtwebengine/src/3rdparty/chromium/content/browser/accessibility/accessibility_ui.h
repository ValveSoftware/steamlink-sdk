// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace base {
  class ListValue;
}  // namespace base

namespace content {

class AccessibilityUI : public WebUIController {
 public:
  explicit AccessibilityUI(WebUI* web_ui);
  virtual ~AccessibilityUI();

 private:
  void ToggleAccessibility(const base::ListValue* args);
  void ToggleGlobalAccessibility(const base::ListValue* args);
  void RequestAccessibilityTree(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(AccessibilityUI);
};

}  // namespace content

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_UI_H_
