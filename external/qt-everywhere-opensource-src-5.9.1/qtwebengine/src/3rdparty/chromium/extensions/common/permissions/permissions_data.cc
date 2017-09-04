// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permissions_data.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace extensions {

namespace {

PermissionsData::PolicyDelegate* g_policy_delegate = nullptr;

class AutoLockOnValidThread {
 public:
  AutoLockOnValidThread(base::Lock& lock, base::ThreadChecker* thread_checker)
      : auto_lock_(lock) {
    DCHECK(!thread_checker || thread_checker->CalledOnValidThread());
  }

 private:
  base::AutoLock auto_lock_;

  DISALLOW_COPY_AND_ASSIGN(AutoLockOnValidThread);
};

}  // namespace

PermissionsData::PermissionsData(const Extension* extension)
    : extension_id_(extension->id()), manifest_type_(extension->GetType()) {
  const PermissionSet& required_permissions =
      PermissionsParser::GetRequiredPermissions(extension);
  active_permissions_unsafe_.reset(new PermissionSet(
      required_permissions.apis(), required_permissions.manifest_permissions(),
      required_permissions.explicit_hosts(),
      required_permissions.scriptable_hosts()));
  withheld_permissions_unsafe_.reset(new PermissionSet());
}

PermissionsData::~PermissionsData() {
}

// static
void PermissionsData::SetPolicyDelegate(PolicyDelegate* delegate) {
  g_policy_delegate = delegate;
}

// static
bool PermissionsData::CanExecuteScriptEverywhere(const Extension* extension) {
  if (extension->location() == Manifest::COMPONENT)
    return true;

  const ExtensionsClient::ScriptingWhitelist& whitelist =
      ExtensionsClient::Get()->GetScriptingWhitelist();

  return std::find(whitelist.begin(), whitelist.end(), extension->id()) !=
         whitelist.end();
}

// static
bool PermissionsData::ShouldSkipPermissionWarnings(
    const std::string& extension_id) {
  // See http://b/4946060 for more details.
  return extension_id == extension_misc::kProdHangoutsExtensionId;
}

// static
bool PermissionsData::IsRestrictedUrl(const GURL& document_url,
                                      const Extension* extension,
                                      std::string* error) {
  if (extension && CanExecuteScriptEverywhere(extension))
    return false;

  // Check if the scheme is valid for extensions. If not, return.
  if (!URLPattern::IsValidSchemeForExtensions(document_url.scheme()) &&
      document_url.spec() != url::kAboutBlankURL) {
    if (error) {
      if (extension->permissions_data()->active_permissions().HasAPIPermission(
              APIPermission::kTab)) {
        *error = ErrorUtils::FormatErrorMessage(
            manifest_errors::kCannotAccessPageWithUrl, document_url.spec());
      } else {
        *error = manifest_errors::kCannotAccessPage;
      }
    }
    return true;
  }

  if (!ExtensionsClient::Get()->IsScriptableURL(document_url, error))
    return true;

  bool allow_on_chrome_urls = base::CommandLine::ForCurrentProcess()->HasSwitch(
                                  switches::kExtensionsOnChromeURLs);
  if (document_url.SchemeIs(content::kChromeUIScheme) &&
      !allow_on_chrome_urls) {
    if (error)
      *error = manifest_errors::kCannotAccessChromeUrl;
    return true;
  }

  if (extension && document_url.SchemeIs(kExtensionScheme) &&
      document_url.host() != extension->id() && !allow_on_chrome_urls) {
    if (error)
      *error = manifest_errors::kCannotAccessExtensionUrl;
    return true;
  }

  return false;
}

void PermissionsData::BindToCurrentThread() const {
  DCHECK(!thread_checker_);
  thread_checker_.reset(new base::ThreadChecker());
}

void PermissionsData::SetPermissions(
    std::unique_ptr<const PermissionSet> active,
    std::unique_ptr<const PermissionSet> withheld) const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  active_permissions_unsafe_ = std::move(active);
  withheld_permissions_unsafe_ = std::move(withheld);
}

void PermissionsData::SetActivePermissions(
    std::unique_ptr<const PermissionSet> active) const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  active_permissions_unsafe_ = std::move(active);
}

void PermissionsData::UpdateTabSpecificPermissions(
    int tab_id,
    const PermissionSet& permissions) const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  CHECK_GE(tab_id, 0);
  TabPermissionsMap::const_iterator iter =
      tab_specific_permissions_.find(tab_id);
  std::unique_ptr<const PermissionSet> new_permissions =
      PermissionSet::CreateUnion(
          iter == tab_specific_permissions_.end()
              ? static_cast<const PermissionSet&>(PermissionSet())
              : *iter->second,
          permissions);
  tab_specific_permissions_[tab_id] = std::move(new_permissions);
}

void PermissionsData::ClearTabSpecificPermissions(int tab_id) const {
  AutoLockOnValidThread lock(runtime_lock_, thread_checker_.get());
  CHECK_GE(tab_id, 0);
  tab_specific_permissions_.erase(tab_id);
}

bool PermissionsData::HasAPIPermission(APIPermission::ID permission) const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->HasAPIPermission(permission);
}

bool PermissionsData::HasAPIPermission(
    const std::string& permission_name) const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->HasAPIPermission(permission_name);
}

bool PermissionsData::HasAPIPermissionForTab(
    int tab_id,
    APIPermission::ID permission) const {
  base::AutoLock auto_lock(runtime_lock_);
  if (active_permissions_unsafe_->HasAPIPermission(permission))
    return true;

  const PermissionSet* tab_permissions = GetTabSpecificPermissions(tab_id);
  return tab_permissions && tab_permissions->HasAPIPermission(permission);
}

bool PermissionsData::CheckAPIPermissionWithParam(
    APIPermission::ID permission,
    const APIPermission::CheckParam* param) const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->CheckAPIPermissionWithParam(permission,
                                                                 param);
}

URLPatternSet PermissionsData::GetEffectiveHostPermissions() const {
  base::AutoLock auto_lock(runtime_lock_);
  URLPatternSet effective_hosts = active_permissions_unsafe_->effective_hosts();
  for (const auto& val : tab_specific_permissions_)
    effective_hosts.AddPatterns(val.second->effective_hosts());
  return effective_hosts;
}

bool PermissionsData::HasHostPermission(const GURL& url) const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->HasExplicitAccessToOrigin(url);
}

bool PermissionsData::HasEffectiveAccessToAllHosts() const {
  base::AutoLock auto_lock(runtime_lock_);
  return active_permissions_unsafe_->HasEffectiveAccessToAllHosts();
}

PermissionMessages PermissionsData::GetPermissionMessages() const {
  base::AutoLock auto_lock(runtime_lock_);
  return PermissionMessageProvider::Get()->GetPermissionMessages(
      PermissionMessageProvider::Get()->GetAllPermissionIDs(
          *active_permissions_unsafe_, manifest_type_));
}

bool PermissionsData::HasWithheldImpliedAllHosts() const {
  base::AutoLock auto_lock(runtime_lock_);
  // Since we currently only withhold all_hosts, it's sufficient to check
  // that either set is not empty.
  return !withheld_permissions_unsafe_->explicit_hosts().is_empty() ||
         !withheld_permissions_unsafe_->scriptable_hosts().is_empty();
}

bool PermissionsData::CanAccessPage(const Extension* extension,
                                    const GURL& document_url,
                                    int tab_id,
                                    std::string* error) const {
  base::AutoLock auto_lock(runtime_lock_);
  AccessType result =
      CanRunOnPage(extension, document_url, tab_id,
                   active_permissions_unsafe_->explicit_hosts(),
                   withheld_permissions_unsafe_->explicit_hosts(), error);
  // TODO(rdevlin.cronin) Update callers so that they only need ACCESS_ALLOWED.
  return result == ACCESS_ALLOWED || result == ACCESS_WITHHELD;
}

PermissionsData::AccessType PermissionsData::GetPageAccess(
    const Extension* extension,
    const GURL& document_url,
    int tab_id,
    std::string* error) const {
  base::AutoLock auto_lock(runtime_lock_);
  return CanRunOnPage(extension, document_url, tab_id,
                      active_permissions_unsafe_->explicit_hosts(),
                      withheld_permissions_unsafe_->explicit_hosts(), error);
}

bool PermissionsData::CanRunContentScriptOnPage(const Extension* extension,
                                                const GURL& document_url,
                                                int tab_id,
                                                std::string* error) const {
  base::AutoLock auto_lock(runtime_lock_);
  AccessType result =
      CanRunOnPage(extension, document_url, tab_id,
                   active_permissions_unsafe_->scriptable_hosts(),
                   withheld_permissions_unsafe_->scriptable_hosts(), error);
  // TODO(rdevlin.cronin) Update callers so that they only need ACCESS_ALLOWED.
  return result == ACCESS_ALLOWED || result == ACCESS_WITHHELD;
}

PermissionsData::AccessType PermissionsData::GetContentScriptAccess(
    const Extension* extension,
    const GURL& document_url,
    int tab_id,
    std::string* error) const {
  base::AutoLock auto_lock(runtime_lock_);
  return CanRunOnPage(extension, document_url, tab_id,
                      active_permissions_unsafe_->scriptable_hosts(),
                      withheld_permissions_unsafe_->scriptable_hosts(), error);
}

bool PermissionsData::CanCaptureVisiblePage(int tab_id,
                                            std::string* error) const {
  const URLPattern all_urls(URLPattern::SCHEME_ALL,
                            URLPattern::kAllUrlsPattern);

  base::AutoLock auto_lock(runtime_lock_);
  if (active_permissions_unsafe_->explicit_hosts().ContainsPattern(all_urls))
    return true;

  if (tab_id >= 0) {
    const PermissionSet* tab_permissions = GetTabSpecificPermissions(tab_id);
    if (tab_permissions &&
        tab_permissions->HasAPIPermission(APIPermission::kTab)) {
      return true;
    }
    if (error)
      *error = manifest_errors::kActiveTabPermissionNotGranted;
    return false;
  }

  if (error)
    *error = manifest_errors::kAllURLOrActiveTabNeeded;
  return false;
}

const PermissionSet* PermissionsData::GetTabSpecificPermissions(
    int tab_id) const {
  CHECK_GE(tab_id, 0);
  runtime_lock_.AssertAcquired();
  TabPermissionsMap::const_iterator iter =
      tab_specific_permissions_.find(tab_id);
  return iter != tab_specific_permissions_.end() ? iter->second.get() : nullptr;
}

bool PermissionsData::HasTabSpecificPermissionToExecuteScript(
    int tab_id,
    const GURL& url) const {
  runtime_lock_.AssertAcquired();
  if (tab_id >= 0) {
    const PermissionSet* tab_permissions = GetTabSpecificPermissions(tab_id);
    if (tab_permissions &&
        tab_permissions->explicit_hosts().MatchesSecurityOrigin(url)) {
      return true;
    }
  }
  return false;
}

PermissionsData::AccessType PermissionsData::CanRunOnPage(
    const Extension* extension,
    const GURL& document_url,
    int tab_id,
    const URLPatternSet& permitted_url_patterns,
    const URLPatternSet& withheld_url_patterns,
    std::string* error) const {
  runtime_lock_.AssertAcquired();
  if (g_policy_delegate &&
      !g_policy_delegate->CanExecuteScriptOnPage(extension, document_url,
                                                 tab_id, error)) {
    return ACCESS_DENIED;
  }

  if (IsRestrictedUrl(document_url, extension, error))
    return ACCESS_DENIED;

  if (HasTabSpecificPermissionToExecuteScript(tab_id, document_url))
    return ACCESS_ALLOWED;

  if (permitted_url_patterns.MatchesURL(document_url))
    return ACCESS_ALLOWED;

  if (withheld_url_patterns.MatchesURL(document_url))
    return ACCESS_WITHHELD;

  if (error) {
    if (extension->permissions_data()->active_permissions().HasAPIPermission(
            APIPermission::kTab)) {
      *error = ErrorUtils::FormatErrorMessage(
          manifest_errors::kCannotAccessPageWithUrl, document_url.spec());
    } else {
      *error = manifest_errors::kCannotAccessPage;
    }
  }

  return ACCESS_DENIED;
}

}  // namespace extensions
