// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_content_browser_client.h"
#include "blimp/engine/app/blimp_browser_main_parts.h"
#include "blimp/engine/app/settings_manager.h"
#include "blimp/engine/feature/geolocation/blimp_location_provider.h"
#include "blimp/engine/mojo/blob_channel_service.h"
#include "content/public/browser/geolocation_delegate.h"
#include "services/shell/public/cpp/interface_registry.h"

namespace blimp {
namespace engine {

namespace {
// A provider of services needed by Geolocation.
class BlimpGeolocationDelegate : public content::GeolocationDelegate {
 public:
  BlimpGeolocationDelegate() = default;

  bool UseNetworkLocationProviders() final { return false; }

  content::LocationProvider* OverrideSystemLocationProvider() final {
    if (!location_provider_)
      location_provider_ = base::WrapUnique(new BlimpLocationProvider());
    return location_provider_.get();
  }

 private:
  std::unique_ptr<BlimpLocationProvider> location_provider_;

  DISALLOW_COPY_AND_ASSIGN(BlimpGeolocationDelegate);
};

}  // anonymous namespace

BlimpContentBrowserClient::BlimpContentBrowserClient() {}

BlimpContentBrowserClient::~BlimpContentBrowserClient() {}

content::BrowserMainParts* BlimpContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  blimp_browser_main_parts_ = new BlimpBrowserMainParts(parameters);
  // BrowserMainLoop takes ownership of the returned BrowserMainParts.
  return blimp_browser_main_parts_;
}

void BlimpContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* render_view_host,
    content::WebPreferences* prefs) {
  if (!blimp_browser_main_parts_)
    return;

  if (!blimp_browser_main_parts_->GetSettingsManager())
    return;

  blimp_browser_main_parts_->GetSettingsManager()->UpdateWebkitPreferences(
      prefs);
}

BlimpBrowserContext* BlimpContentBrowserClient::GetBrowserContext() {
  return blimp_browser_main_parts_->GetBrowserContext();
}

content::GeolocationDelegate*
BlimpContentBrowserClient::CreateGeolocationDelegate() {
  return new BlimpGeolocationDelegate();
}

void BlimpContentBrowserClient::ExposeInterfacesToRenderer(
    shell::InterfaceRegistry* registry,
    content::RenderProcessHost* render_process_host) {
  registry->AddInterface<mojom::BlobChannel>(
      base::Bind(&BlobChannelService::Create,
                 blimp_browser_main_parts_->GetBlobChannelSender()));
}

}  // namespace engine
}  // namespace blimp
