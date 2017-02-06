// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CONNECTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CONNECTION_H_

#include <memory>

#include "base/macros.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"

namespace proximity_auth {
class Connection;
}  // namespace proximity_auth

namespace extensions {
// An ApiResource wrapper for a proximity_auth::Connection.
class EasyUnlockPrivateConnection : public ApiResource {
 public:
  EasyUnlockPrivateConnection(
      bool persistent,
      const std::string& owner_extension_id,
      std::unique_ptr<proximity_auth::Connection> connection);
  ~EasyUnlockPrivateConnection() override;

  // Returns a pointer to the underlying connection object.
  proximity_auth::Connection* GetConnection() const;

  // ApiResource override.
  bool IsPersistent() const override;

  // This resource should be managed on the UI thread.
  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

 private:
  friend class ApiResourceManager<EasyUnlockPrivateConnection>;
  static const char* service_name() {
    return "EasyUnlockPrivateConnectionManager";
  }

  // True, if this resource should be persistent.
  bool persistent_;

  // The connection is owned by this instance and will automatically disconnect
  // when deleted.
  std::unique_ptr<proximity_auth::Connection> connection_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateConnection);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CONNECTION_H_
