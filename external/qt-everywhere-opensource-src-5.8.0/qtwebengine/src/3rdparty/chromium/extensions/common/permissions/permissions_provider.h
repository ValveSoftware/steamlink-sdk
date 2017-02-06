// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_PROVIDER_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_PROVIDER_H_

#include <vector>

namespace extensions {

class APIPermissionInfo;

// The PermissionsProvider creates APIPermissions instances. It is only
// needed at startup time. Typically, ExtensionsClient will register
// its PermissionsProviders with the global PermissionsInfo at startup.
// TODO(sashab): Remove all permission messages from this class, moving the
// permission message rules into ChromePermissionMessageProvider.
class PermissionsProvider {
 public:
  // An alias for a given permission |name|.
  struct AliasInfo {
    const char* name;
    const char* alias;

    AliasInfo(const char* name, const char* alias)
        : name(name), alias(alias) {
    }
  };
  // Returns all the known permissions. The caller, PermissionsInfo,
  // takes ownership of the APIPermissionInfos.
  virtual std::vector<APIPermissionInfo*> GetAllPermissions() const = 0;

  // Returns all the known permission aliases.
  virtual std::vector<AliasInfo> GetAllAliases() const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_PROVIDER_H_
