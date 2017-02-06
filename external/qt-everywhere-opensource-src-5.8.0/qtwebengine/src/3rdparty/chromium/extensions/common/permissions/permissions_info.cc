// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permissions_info.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"

namespace extensions {

static base::LazyInstance<PermissionsInfo> g_permissions_info =
    LAZY_INSTANCE_INITIALIZER;

// static
PermissionsInfo* PermissionsInfo::GetInstance() {
  return g_permissions_info.Pointer();
}

void PermissionsInfo::AddProvider(const PermissionsProvider& provider) {
  std::vector<APIPermissionInfo*> permissions = provider.GetAllPermissions();
  std::vector<PermissionsProvider::AliasInfo> aliases =
      provider.GetAllAliases();

  for (size_t i = 0; i < permissions.size(); ++i)
    RegisterPermission(permissions[i]);
  for (size_t i = 0; i < aliases.size(); ++i)
    RegisterAlias(aliases[i].name, aliases[i].alias);
}

const APIPermissionInfo* PermissionsInfo::GetByID(
    APIPermission::ID id) const {
  IDMap::const_iterator i = id_map_.find(id);
  return (i == id_map_.end()) ? NULL : i->second;
}

const APIPermissionInfo* PermissionsInfo::GetByName(
    const std::string& name) const {
  NameMap::const_iterator i = name_map_.find(name);
  return (i == name_map_.end()) ? NULL : i->second;
}

APIPermissionSet PermissionsInfo::GetAll() const {
  APIPermissionSet permissions;
  for (IDMap::const_iterator i = id_map_.begin(); i != id_map_.end(); ++i)
    permissions.insert(i->second->id());
  return permissions;
}

APIPermissionSet PermissionsInfo::GetAllByName(
    const std::set<std::string>& permission_names) const {
  APIPermissionSet permissions;
  for (std::set<std::string>::const_iterator i = permission_names.begin();
       i != permission_names.end(); ++i) {
    const APIPermissionInfo* permission_info = GetByName(*i);
    if (permission_info)
      permissions.insert(permission_info->id());
  }
  return permissions;
}

bool PermissionsInfo::HasChildPermissions(const std::string& name) const {
  NameMap::const_iterator i = name_map_.lower_bound(name + '.');
  if (i == name_map_.end()) return false;
  return base::StartsWith(i->first, name + '.', base::CompareCase::SENSITIVE);
}

PermissionsInfo::PermissionsInfo()
    : permission_count_(0) {
}

PermissionsInfo::~PermissionsInfo() {
  STLDeleteContainerPairSecondPointers(id_map_.begin(), id_map_.end());
}

void PermissionsInfo::RegisterAlias(
    const char* name,
    const char* alias) {
  DCHECK(ContainsKey(name_map_, name));
  DCHECK(!ContainsKey(name_map_, alias));
  name_map_[alias] = name_map_[name];
}

void PermissionsInfo::RegisterPermission(APIPermissionInfo* permission) {
  DCHECK(!ContainsKey(id_map_, permission->id()));
  DCHECK(!ContainsKey(name_map_, permission->name()));

  id_map_[permission->id()] = permission;
  name_map_[permission->name()] = permission;

  permission_count_++;
}

}  // namespace extensions
