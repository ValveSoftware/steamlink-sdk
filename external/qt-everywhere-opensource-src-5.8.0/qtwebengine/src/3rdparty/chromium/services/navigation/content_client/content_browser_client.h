// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NAVIGATION_CONTENT_CLIENT_CONTENT_BROWSER_CLIENT_H_
#define SERVICES_NAVIGATION_CONTENT_CLIENT_CONTENT_BROWSER_CLIENT_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/content_browser_client.h"

namespace navigation {

class BrowserMainParts;

class ContentBrowserClient : public content::ContentBrowserClient {
 public:
  ContentBrowserClient();
  ~ContentBrowserClient() override;

  // Overridden from content::ContentBrowserClient:
  content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  std::string GetShellUserIdForBrowserContext(
      content::BrowserContext* browser_context) override;
  void RegisterInProcessMojoApplications(
      StaticMojoApplicationMap* apps) override;

 private:
  BrowserMainParts* browser_main_parts_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ContentBrowserClient);
};

}  // namespace navigation

#endif  // SERVICES_NAVIGATION_CONTENT_CLIENT_CONTENT_BROWSER_CLIENT_H_
