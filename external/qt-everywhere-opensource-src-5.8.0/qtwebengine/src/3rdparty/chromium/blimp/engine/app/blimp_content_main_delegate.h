// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_BLIMP_CONTENT_MAIN_DELEGATE_H_
#define BLIMP_ENGINE_APP_BLIMP_CONTENT_MAIN_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "blimp/engine/common/blimp_content_client.h"
#include "content/public/app/content_main_delegate.h"

namespace blimp {
namespace engine {

class BlimpContentBrowserClient;
class BlimpContentRendererClient;

class BlimpContentMainDelegate : public content::ContentMainDelegate {
 public:
  BlimpContentMainDelegate();
  ~BlimpContentMainDelegate() override;

  BlimpContentBrowserClient* browser_client() { return browser_client_.get(); }

  // content::ContentMainDelegate implementation.
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

 private:
  void InitializeResourceBundle();

  std::unique_ptr<BlimpContentBrowserClient> browser_client_;
  std::unique_ptr<BlimpContentRendererClient> renderer_client_;
  BlimpContentClient content_client_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentMainDelegate);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_BLIMP_CONTENT_MAIN_DELEGATE_H_
