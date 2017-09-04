// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_connection.h"

#include "base/lazy_instance.h"
#include "components/proximity_auth/connection.h"

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<EasyUnlockPrivateConnection>>> g_factory =
    LAZY_INSTANCE_INITIALIZER;

template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<EasyUnlockPrivateConnection>>*
ApiResourceManager<EasyUnlockPrivateConnection>::GetFactoryInstance() {
  return g_factory.Pointer();
}

EasyUnlockPrivateConnection::EasyUnlockPrivateConnection(
    bool persistent,
    const std::string& owner_extension_id,
    std::unique_ptr<proximity_auth::Connection> connection)
    : ApiResource(owner_extension_id),
      persistent_(persistent),
      connection_(connection.release()) {}

EasyUnlockPrivateConnection::~EasyUnlockPrivateConnection() {}

proximity_auth::Connection* EasyUnlockPrivateConnection::GetConnection() const {
  return connection_.get();
}

bool EasyUnlockPrivateConnection::IsPersistent() const {
  return persistent_;
}

}  // namespace extensions
