// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_browser_context.h"
#include "ui/aura/env.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views_content_client/views_content_client.h"
#include "ui/views_content_client/views_content_client_main_parts_aura.h"

namespace ui {

namespace {

class ViewsContentClientMainPartsDesktopAura
    : public ViewsContentClientMainPartsAura {
 public:
  ViewsContentClientMainPartsDesktopAura(
      const content::MainFunctionParams& content_params,
      ViewsContentClient* views_content_client);
  virtual ~ViewsContentClientMainPartsDesktopAura() {}

  // content::BrowserMainParts:
  virtual void PreMainMessageLoopRun() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewsContentClientMainPartsDesktopAura);
};

ViewsContentClientMainPartsDesktopAura::ViewsContentClientMainPartsDesktopAura(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client)
    : ViewsContentClientMainPartsAura(content_params, views_content_client) {
}

void ViewsContentClientMainPartsDesktopAura::PreMainMessageLoopRun() {
  ViewsContentClientMainPartsAura::PreMainMessageLoopRun();

  aura::Env::CreateInstance(true);
  gfx::Screen::SetScreenInstance(
      gfx::SCREEN_TYPE_NATIVE, views::CreateDesktopScreen());

  views_content_client()->task().Run(browser_context(), NULL);
}

}  // namespace

// static
ViewsContentClientMainParts* ViewsContentClientMainParts::Create(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client) {
  return new ViewsContentClientMainPartsDesktopAura(
      content_params, views_content_client);
}

}  // namespace ui
