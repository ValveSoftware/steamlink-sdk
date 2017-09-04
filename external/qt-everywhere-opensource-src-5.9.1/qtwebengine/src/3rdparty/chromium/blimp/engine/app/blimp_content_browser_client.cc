// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "blimp/engine/app/blimp_browser_main_parts.h"
#include "blimp/engine/app/blimp_content_browser_client.h"
#include "blimp/engine/app/settings_manager.h"
#include "blimp/engine/grit/blimp_browser_resources.h"
#include "blimp/engine/mojo/blob_channel_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "ui/base/resource/resource_bundle.h"

namespace blimp {
namespace engine {

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

void BlimpContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::InterfaceRegistry* registry,
    content::RenderProcessHost* render_process_host) {
  registry->AddInterface<mojom::BlobChannel>(base::Bind(
      &BlobChannelService::BindRequest,
      base::Unretained(blimp_browser_main_parts_->GetBlobChannelService())));
}

std::unique_ptr<base::Value>
BlimpContentBrowserClient::GetServiceManifestOverlay(
    const std::string& name) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  int id = -1;
  if (name == content::mojom::kBrowserServiceName) {
    id = IDR_BLIMP_CONTENT_BROWSER_MANIFEST_OVERLAY;
  }
  if (id == -1) {
    return nullptr;
  }

  base::StringPiece manifest_contents =
      rb.GetRawDataResourceForScale(id, ui::ScaleFactor::SCALE_FACTOR_NONE);
  return base::JSONReader::Read(manifest_contents);
}

}  // namespace engine
}  // namespace blimp
