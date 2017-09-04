// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_HELPER_H_
#define EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_HELPER_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/renderer/render_view_observer.h"

namespace extensions {

// Renderer-side implementation for chrome.automation API (for the few pieces
// which aren't built in to the existing accessibility system).
class AutomationApiHelper : public content::RenderViewObserver {
 public:
  explicit AutomationApiHelper(content::RenderView* render_view);
  ~AutomationApiHelper() override;

 private:
  // RenderViewObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;

  void OnQuerySelector(int acc_obj_id,
                       int request_id,
                       const base::string16& selector);

  DISALLOW_COPY_AND_ASSIGN(AutomationApiHelper);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_HELPER_H_
