// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NAVIGATION_CONTENT_CLIENT_BROWSER_MAIN_PARTS_H_
#define SERVICES_NAVIGATION_CONTENT_CLIENT_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/browser_main_parts.h"

namespace content {
class ShellBrowserContext;
struct MainFunctionParams;
}

namespace net {
class NetLog;
}

namespace views {
class ViewsDelegate;
class WindowManagerConnection;
}

namespace navigation {

class Navigation;

class BrowserMainParts : public content::BrowserMainParts {
 public:
  BrowserMainParts(const content::MainFunctionParams& parameters);
  ~BrowserMainParts() override;

  // Overridden from content::BrowserMainParts:
  void ToolkitInitialized() override;
  void PreMainMessageLoopRun() override;
  bool MainMessageLoopRun(int* result_code) override;
  void PostMainMessageLoopRun() override;

  content::ShellBrowserContext* browser_context() {
    return browser_context_.get();
  }

 private:
  std::unique_ptr<net::NetLog> net_log_;
  std::unique_ptr<content::ShellBrowserContext> browser_context_;
  std::unique_ptr<views::ViewsDelegate> views_delegate_;
  std::unique_ptr<views::WindowManagerConnection> window_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(BrowserMainParts);
};

}  // namespace navigation

#endif  // SERVICES_NAVIGATION_CONTENT_CLIENT_BROWSER_MAIN_PARTS_H_
