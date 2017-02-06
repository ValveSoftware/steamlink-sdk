// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_service.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/updater/update_data_provider.h"
#include "extensions/browser/updater/update_service_factory.h"

namespace {

void UpdateCheckCompleteCallback(int error) {}

void InstallUpdateCallback(content::BrowserContext* context,
                           const std::string& extension_id,
                           const base::FilePath& temp_dir) {
  extensions::ExtensionSystem::Get(context)
      ->InstallUpdate(extension_id, temp_dir);
}

}  // namespace

namespace extensions {

// static
UpdateService* UpdateService::Get(content::BrowserContext* context) {
  return UpdateServiceFactory::GetForBrowserContext(context);
}

void UpdateService::Shutdown() {
  if (update_data_provider_) {
    update_data_provider_->Shutdown();
    update_data_provider_ = nullptr;
  }
  update_client_ = nullptr;
  context_ = nullptr;
}

void UpdateService::SendUninstallPing(const std::string& id,
                                      const Version& version,
                                      int reason) {
  update_client_->SendUninstallPing(id, version, reason);
}

void UpdateService::StartUpdateCheck(std::vector<std::string> extension_ids) {
  if (!update_client_)
    return;
  update_client_->Update(extension_ids, base::Bind(&UpdateDataProvider::GetData,
                                                   update_data_provider_),
                         base::Bind(&UpdateCheckCompleteCallback));
}

UpdateService::UpdateService(
    content::BrowserContext* context,
    scoped_refptr<update_client::UpdateClient> update_client)
    : context_(context), update_client_(update_client) {
  CHECK(update_client_);
  update_data_provider_ =
      new UpdateDataProvider(context_, base::Bind(&InstallUpdateCallback));
}

UpdateService::~UpdateService() {}

}  // namespace extensions
