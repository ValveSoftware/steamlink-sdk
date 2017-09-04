// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_data_provider.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/updater/update_install_shim.h"
#include "extensions/common/extension.h"

namespace extensions {

UpdateDataProvider::UpdateDataProvider(content::BrowserContext* context,
                                       const InstallCallback& callback)
    : context_(context), callback_(callback) {}

UpdateDataProvider::~UpdateDataProvider() {}

void UpdateDataProvider::Shutdown() {
  context_ = nullptr;
}

void UpdateDataProvider::GetData(
    const std::vector<std::string>& ids,
    std::vector<update_client::CrxComponent>* data) {
  if (!context_)
    return;
  const ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  for (const auto& id : ids) {
    const Extension* extension = registry->GetInstalledExtension(id);
    if (!extension)
      continue;
    data->push_back(update_client::CrxComponent());
    update_client::CrxComponent* info = &data->back();
    std::string pubkey_bytes;
    base::Base64Decode(extension->public_key(), &pubkey_bytes);
    info->pk_hash.resize(crypto::kSHA256Length, 0);
    crypto::SHA256HashString(pubkey_bytes, info->pk_hash.data(),
                             info->pk_hash.size());
    info->version = *extension->version();
    info->allows_background_download = false;
    info->requires_network_encryption = true;
    info->installer = new UpdateInstallShim(
        id, extension->path(),
        base::Bind(&UpdateDataProvider::RunInstallCallback, this));
  }
}

void UpdateDataProvider::RunInstallCallback(const std::string& extension_id,
                                            const base::FilePath& temp_dir) {
  if (!context_) {
    content::BrowserThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&base::DeleteFile), temp_dir, false));
    return;
  } else {
    callback_.Run(context_, extension_id, temp_dir);
  }
}

}  // namespace extensions
