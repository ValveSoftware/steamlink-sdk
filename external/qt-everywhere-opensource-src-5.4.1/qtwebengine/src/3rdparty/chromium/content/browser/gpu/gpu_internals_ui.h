// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_GPU_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_GPU_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace content {

class GpuInternalsUI : public WebUIController {
 public:
  explicit GpuInternalsUI(WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuInternalsUI);
};

}  // namespace content

#endif  // CHROME_BROWSER_UI_WEBUI_GPU_INTERNALS_UI_H_

