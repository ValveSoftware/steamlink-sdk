// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NAVIGATION_CONTENT_CLIENT_MAIN_DELEGATE_H_
#define SERVICES_NAVIGATION_CONTENT_CLIENT_MAIN_DELEGATE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/app/content_main_delegate.h"
#include "content/shell/common/shell_content_client.h"

namespace content {
class ShellContentRendererClient;
class ShellContentUtilityClient;
}

namespace navigation {

class ContentBrowserClient;

class MainDelegate : public content::ContentMainDelegate {
 public:
  MainDelegate();
  ~MainDelegate() override;

  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;

 private:
  std::unique_ptr<ContentBrowserClient> browser_client_;
  content::ShellContentClient content_client_;

  DISALLOW_COPY_AND_ASSIGN(MainDelegate);
};

}  // namespace navigation

#endif  // SERVICES_NAVIGATION_CONTENT_CLIENT_MAIN_DELEGATE_H_
