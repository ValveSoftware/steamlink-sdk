// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_BLIMP_CONTENT_BROWSER_CLIENT_H_
#define BLIMP_ENGINE_APP_BLIMP_CONTENT_BROWSER_CLIENT_H_

#include "base/macros.h"
#include "content/public/browser/content_browser_client.h"

namespace content {
class GeolocationDelegate;
}  // namespace content

namespace blimp {
namespace engine {

class BlimpBrowserMainParts;
class BlimpBrowserContext;

class BlimpContentBrowserClient : public content::ContentBrowserClient {
 public:
  BlimpContentBrowserClient();
  ~BlimpContentBrowserClient() override;

  BlimpBrowserMainParts* blimp_browser_main_parts() {
    return blimp_browser_main_parts_;
  }

  // content::ContentBrowserClient implementation.
  content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  void OverrideWebkitPrefs(content::RenderViewHost* render_view_host,
                           content::WebPreferences* prefs) override;
  void ExposeInterfacesToRenderer(
      shell::InterfaceRegistry* registry,
      content::RenderProcessHost* render_process_host) override;

  content::GeolocationDelegate* CreateGeolocationDelegate() override;

  BlimpBrowserContext* GetBrowserContext();

 private:
  // Owned by BrowserMainLoop
  BlimpBrowserMainParts* blimp_browser_main_parts_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentBrowserClient);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_BLIMP_CONTENT_BROWSER_CLIENT_H_
