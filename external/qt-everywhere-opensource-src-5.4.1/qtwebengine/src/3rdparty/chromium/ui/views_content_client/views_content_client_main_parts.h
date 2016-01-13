// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_MAIN_PARTS_H_
#define UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_MAIN_PARTS_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_main_parts.h"

namespace content {
class ShellBrowserContext;
struct MainFunctionParams;
}

namespace views {
class ViewsDelegate;
}

namespace ui {

class ViewsContentClient;

class ViewsContentClientMainParts : public content::BrowserMainParts {
 public:
  // Platform-specific create function.
  static ViewsContentClientMainParts* Create(
      const content::MainFunctionParams& content_params,
      ViewsContentClient* views_content_client);

  virtual ~ViewsContentClientMainParts();

  // content::BrowserMainParts:
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual bool MainMessageLoopRun(int* result_code) OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;

  content::ShellBrowserContext* browser_context() {
    return browser_context_.get();
  }

  ViewsContentClient* views_content_client() {
    return views_content_client_;
  }

 protected:
  ViewsContentClientMainParts(
      const content::MainFunctionParams& content_params,
      ViewsContentClient* views_content_client);

 private:
  scoped_ptr<content::ShellBrowserContext> browser_context_;

  scoped_ptr<views::ViewsDelegate> views_delegate_;

  ViewsContentClient* views_content_client_;

  DISALLOW_COPY_AND_ASSIGN(ViewsContentClientMainParts);
};

}  // namespace ui

#endif  // UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_MAIN_PARTS_H_
