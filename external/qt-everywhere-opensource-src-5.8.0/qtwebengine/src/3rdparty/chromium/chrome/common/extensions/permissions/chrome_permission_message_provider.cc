// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"

#include <vector>

#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_rules.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/permissions/permission_message_util.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// Copyable wrapper to make PermissionMessages comparable.
class ComparablePermission {
 public:
  explicit ComparablePermission(const PermissionMessage& msg) : msg_(&msg) {}

  bool operator<(const ComparablePermission& rhs) const {
    if (msg_->message() < rhs.msg_->message())
      return true;
    if (msg_->message() > rhs.msg_->message())
      return false;
    return msg_->submessages() < rhs.msg_->submessages();
  }

 private:
  const PermissionMessage* msg_;
};
using ComparablePermissions = std::vector<ComparablePermission>;

void DropPermissionParameter(APIPermission::ID id,
                             PermissionIDSet* permissions) {
  if (permissions->ContainsID(id)) {
    // Erase the permission, and insert it again without a parameter.
    permissions->erase(id);
    permissions->insert(id);
  }
}

}  // namespace

typedef std::set<PermissionMessage> PermissionMsgSet;

ChromePermissionMessageProvider::ChromePermissionMessageProvider() {
}

ChromePermissionMessageProvider::~ChromePermissionMessageProvider() {
}

PermissionMessages ChromePermissionMessageProvider::GetPermissionMessages(
    const PermissionIDSet& permissions) const {
  std::vector<ChromePermissionMessageRule> rules =
      ChromePermissionMessageRule::GetAllRules();

  // Apply each of the rules, in order, to generate the messages for the given
  // permissions. Once a permission is used in a rule, remove it from the set
  // of available permissions so it cannot be applied to subsequent rules.
  PermissionIDSet remaining_permissions = permissions;
  PermissionMessages messages;
  for (const auto& rule : rules) {
    // Only apply the rule if we have all the required permission IDs.
    if (remaining_permissions.ContainsAllIDs(rule.required_permissions())) {
      // We can apply the rule. Add all the required permissions, and as many
      // optional permissions as we can, to the new message.
      PermissionIDSet used_permissions =
          remaining_permissions.GetAllPermissionsWithIDs(
              rule.all_permissions());
      messages.push_back(rule.GetPermissionMessage(used_permissions));

      remaining_permissions =
          PermissionIDSet::Difference(remaining_permissions, used_permissions);
    }
  }

  return messages;
}

bool ChromePermissionMessageProvider::IsPrivilegeIncrease(
    const PermissionSet& old_permissions,
    const PermissionSet& new_permissions,
    Manifest::Type extension_type) const {
  // Things can't get worse than native code access.
  if (old_permissions.HasEffectiveFullAccess())
    return false;

  // Otherwise, it's a privilege increase if the new one has full access.
  if (new_permissions.HasEffectiveFullAccess())
    return true;

  if (IsHostPrivilegeIncrease(old_permissions, new_permissions, extension_type))
    return true;

  if (IsAPIOrManifestPrivilegeIncrease(old_permissions, new_permissions))
    return true;

  return false;
}

PermissionIDSet ChromePermissionMessageProvider::GetAllPermissionIDs(
    const PermissionSet& permissions,
    Manifest::Type extension_type) const {
  PermissionIDSet permission_ids;
  AddAPIPermissions(permissions, &permission_ids);
  AddManifestPermissions(permissions, &permission_ids);
  AddHostPermissions(permissions, &permission_ids, extension_type);
  return permission_ids;
}

void ChromePermissionMessageProvider::AddAPIPermissions(
    const PermissionSet& permissions,
    PermissionIDSet* permission_ids) const {
  for (const APIPermission* permission : permissions.apis())
    permission_ids->InsertAll(permission->GetPermissions());

  // A special hack: The warning message for declarativeWebRequest
  // permissions speaks about blocking parts of pages, which is a
  // subset of what the "<all_urls>" access allows. Therefore we
  // display only the "<all_urls>" warning message if both permissions
  // are required.
  // TODO(treib): The same should apply to other permissions that are implied by
  // "<all_urls>" (aka APIPermission::kHostsAll), such as kTab. This would
  // happen automatically if we didn't differentiate between API/Manifest/Host
  // permissions here.
  if (permissions.ShouldWarnAllHosts())
    permission_ids->erase(APIPermission::kDeclarativeWebRequest);
}

void ChromePermissionMessageProvider::AddManifestPermissions(
    const PermissionSet& permissions,
    PermissionIDSet* permission_ids) const {
  for (const ManifestPermission* p : permissions.manifest_permissions())
    permission_ids->InsertAll(p->GetPermissions());
}

void ChromePermissionMessageProvider::AddHostPermissions(
    const PermissionSet& permissions,
    PermissionIDSet* permission_ids,
    Manifest::Type extension_type) const {
  // Since platform apps always use isolated storage, they can't (silently)
  // access user data on other domains, so there's no need to prompt.
  // Note: this must remain consistent with IsHostPrivilegeIncrease.
  // See crbug.com/255229.
  if (extension_type == Manifest::TYPE_PLATFORM_APP)
    return;

  if (permissions.ShouldWarnAllHosts()) {
    permission_ids->insert(APIPermission::kHostsAll);
  } else {
    URLPatternSet regular_hosts;
    ExtensionsClient::Get()->FilterHostPermissions(
        permissions.effective_hosts(), &regular_hosts, permission_ids);

    std::set<std::string> hosts =
        permission_message_util::GetDistinctHosts(regular_hosts, true, true);
    for (const auto& host : hosts) {
      permission_ids->insert(APIPermission::kHostReadWrite,
                             base::UTF8ToUTF16(host));
    }
  }
}

bool ChromePermissionMessageProvider::IsAPIOrManifestPrivilegeIncrease(
    const PermissionSet& old_permissions,
    const PermissionSet& new_permissions) const {
  PermissionIDSet old_ids;
  AddAPIPermissions(old_permissions, &old_ids);
  AddManifestPermissions(old_permissions, &old_ids);
  PermissionIDSet new_ids;
  AddAPIPermissions(new_permissions, &new_ids);
  AddManifestPermissions(new_permissions, &new_ids);

  // Ugly hack: Before M46 beta, we didn't store the parameter for settings
  // override permissions in prefs (which is where |old_permissions| is coming
  // from). To avoid a spurious permission increase warning, drop the parameter.
  // See crbug.com/533086 and crbug.com/619759.
  // TODO(treib,devlin): Remove this for M56, when hopefully all users will have
  // updated prefs.
  const APIPermission::ID kSettingsOverrideIDs[] = {
      APIPermission::kHomepage, APIPermission::kSearchProvider,
      APIPermission::kStartupPages};
  for (auto id : kSettingsOverrideIDs) {
    DropPermissionParameter(id, &old_ids);
    DropPermissionParameter(id, &new_ids);
  }

  // If all the IDs were already there, it's not a privilege increase.
  if (old_ids.Includes(new_ids))
    return false;

  // Otherwise, check the actual messages - not all IDs result in a message,
  // and some messages can suppress others.
  PermissionMessages old_messages = GetPermissionMessages(old_ids);
  PermissionMessages new_messages = GetPermissionMessages(new_ids);

  ComparablePermissions old_strings(old_messages.begin(), old_messages.end());
  ComparablePermissions new_strings(new_messages.begin(), new_messages.end());

  std::sort(old_strings.begin(), old_strings.end());
  std::sort(new_strings.begin(), new_strings.end());

  return !base::STLIncludes(old_strings, new_strings);
}

bool ChromePermissionMessageProvider::IsHostPrivilegeIncrease(
    const PermissionSet& old_permissions,
    const PermissionSet& new_permissions,
    Manifest::Type extension_type) const {
  // Platform apps host permission changes do not count as privilege increases.
  // Note: this must remain consistent with AddHostPermissions.
  if (extension_type == Manifest::TYPE_PLATFORM_APP)
    return false;

  // If the old permission set can access any host, then it can't be elevated.
  if (old_permissions.HasEffectiveAccessToAllHosts())
    return false;

  // Likewise, if the new permission set has full host access, then it must be
  // a privilege increase.
  if (new_permissions.HasEffectiveAccessToAllHosts())
    return true;

  const URLPatternSet& old_list = old_permissions.effective_hosts();
  const URLPatternSet& new_list = new_permissions.effective_hosts();

  // TODO(jstritar): This is overly conservative with respect to subdomains.
  // For example, going from *.google.com to www.google.com will be
  // considered an elevation, even though it is not (http://crbug.com/65337).
  std::set<std::string> new_hosts_set(
      permission_message_util::GetDistinctHosts(new_list, false, false));
  std::set<std::string> old_hosts_set(
      permission_message_util::GetDistinctHosts(old_list, false, false));
  std::set<std::string> new_hosts_only =
      base::STLSetDifference<std::set<std::string> >(new_hosts_set,
                                                     old_hosts_set);

  return !new_hosts_only.empty();
}

}  // namespace extensions
