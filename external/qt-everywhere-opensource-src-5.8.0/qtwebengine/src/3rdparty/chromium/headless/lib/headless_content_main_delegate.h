// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_HEADLESS_CONTENT_MAIN_DELEGATE_H_
#define HEADLESS_LIB_HEADLESS_CONTENT_MAIN_DELEGATE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/app/content_main_delegate.h"
#include "headless/lib/headless_content_client.h"

namespace content {
class BrowserContext;
}

namespace headless {

class HeadlessBrowserImpl;
class HeadlessContentBrowserClient;
class HeadlessContentUtilityClient;
class HeadlessContentRendererClient;

class HeadlessContentMainDelegate : public content::ContentMainDelegate {
 public:
  explicit HeadlessContentMainDelegate(
      std::unique_ptr<HeadlessBrowserImpl> browser);
  ~HeadlessContentMainDelegate() override;

  // content::ContentMainDelegate implementation:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  void ZygoteForked() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;

  HeadlessBrowserImpl* browser() const { return browser_.get(); }

 private:
  friend class HeadlessBrowserTest;

  static void InitializeResourceBundle();

  static HeadlessContentMainDelegate* GetInstance();

  std::unique_ptr<HeadlessContentBrowserClient> browser_client_;
  std::unique_ptr<HeadlessContentRendererClient> renderer_client_;
  std::unique_ptr<HeadlessContentUtilityClient> utility_client_;
  HeadlessContentClient content_client_;

  std::unique_ptr<HeadlessBrowserImpl> browser_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessContentMainDelegate);
};

}  // namespace headless

#endif  // HEADLESS_LIB_HEADLESS_CONTENT_MAIN_DELEGATE_H_
