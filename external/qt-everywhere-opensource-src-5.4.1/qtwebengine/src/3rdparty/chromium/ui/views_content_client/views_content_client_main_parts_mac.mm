// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "content/public/common/content_paths.h"
#include "content/shell/browser/shell_browser_context.h"
#include "ui/views_content_client/views_content_client.h"
#include "ui/views_content_client/views_content_client_main_parts.h"

namespace ui {

namespace {

class ViewsContentClientMainPartsMac : public ViewsContentClientMainParts {
 public:
  ViewsContentClientMainPartsMac(
      const content::MainFunctionParams& content_params,
      ViewsContentClient* views_content_client);
  virtual ~ViewsContentClientMainPartsMac() {}

  // content::BrowserMainParts:
  virtual void PreMainMessageLoopRun() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewsContentClientMainPartsMac);
};

ViewsContentClientMainPartsMac::ViewsContentClientMainPartsMac(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client)
    : ViewsContentClientMainParts(content_params, views_content_client) {
  // Cache the child process path to avoid triggering an AssertIOAllowed.
  base::FilePath child_process_exe;
  PathService::Get(content::CHILD_PROCESS_EXE, &child_process_exe);
}

void ViewsContentClientMainPartsMac::PreMainMessageLoopRun() {
  ViewsContentClientMainParts::PreMainMessageLoopRun();

  views_content_client()->task().Run(browser_context(), NULL);
}

}  // namespace

// static
ViewsContentClientMainParts* ViewsContentClientMainParts::Create(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client) {
  return
      new ViewsContentClientMainPartsMac(content_params, views_content_client);
}

}  // namespace ui
