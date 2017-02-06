// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_
#define EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class Version;
}

namespace content {
class BrowserContext;
}

namespace update_client {
class UpdateClient;
}

namespace extensions {

class UpdateDataProvider;
class UpdateService;
class UpdateServiceFactory;

// This service manages the autoupdate of extensions.  It should eventually
// replace ExtensionUpdater in Chrome.
// TODO(rockot): Replace ExtensionUpdater with this service.
class UpdateService : public KeyedService {
 public:
  static UpdateService* Get(content::BrowserContext* context);

  void Shutdown() override;

  void SendUninstallPing(const std::string& id,
                         const base::Version& version,
                         int reason);

  // Starts an update check for each of |extension_ids|. If there are any
  // updates available, they will be downloaded, checked for integrity,
  // unpacked, and then passed off to the ExtensionSystem::InstallUpdate method
  // for install completion.
  void StartUpdateCheck(std::vector<std::string> extension_ids);

 private:
  friend class UpdateServiceFactory;

  UpdateService(content::BrowserContext* context,
                scoped_refptr<update_client::UpdateClient> update_client);
  ~UpdateService() override;

  content::BrowserContext* context_;

  scoped_refptr<update_client::UpdateClient> update_client_;
  scoped_refptr<UpdateDataProvider> update_data_provider_;

  DISALLOW_COPY_AND_ASSIGN(UpdateService);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_
