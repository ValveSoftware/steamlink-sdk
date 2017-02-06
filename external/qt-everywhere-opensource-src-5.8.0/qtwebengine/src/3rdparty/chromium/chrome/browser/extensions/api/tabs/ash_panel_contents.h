// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_ASH_PANEL_CONTENTS_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_ASH_PANEL_CONTENTS_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/launcher_favicon_loader.h"
#include "extensions/browser/app_window/app_window.h"

class GURL;

namespace content {
class RenderViewHost;
}

namespace extensions {
struct DraggableRegion;
}

// extensions::AppWindowContents class specific to panel windows created by v1
// extenstions. This class maintains a WebContents instance and observes it for
// the purpose of passing messages to the extensions system. It also creates
// an extensions::WindowController instance for interfacing with the v1
// extensions API.
class AshPanelContents
    : public extensions::AppWindowContents,
      public LauncherFaviconLoader::Delegate {
 public:
  explicit AshPanelContents(extensions::AppWindow* host);
  ~AshPanelContents() override;

  // extensions::AppWindowContents
  void Initialize(content::BrowserContext* context,
                  content::RenderFrameHost* creator_frame,
                  const GURL& url) override;
  void LoadContents(int32_t creator_process_id) override;
  void NativeWindowChanged(
      extensions::NativeAppWindow* native_app_window) override;
  void NativeWindowClosed() override;
  void DispatchWindowShownForTests() const override;
  void OnWindowReady() override;
  content::WebContents* GetWebContents() const override;
  extensions::WindowController* GetWindowController() const override;

  // LauncherFaviconLoader::Delegate overrides:
  void FaviconUpdated() override;

  LauncherFaviconLoader* launcher_favicon_loader_for_test() {
    return launcher_favicon_loader_.get();
  }

 private:
  extensions::AppWindow* host_;
  GURL url_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<LauncherFaviconLoader> launcher_favicon_loader_;

  DISALLOW_COPY_AND_ASSIGN(AshPanelContents);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_ASH_PANEL_CONTENTS_H_
