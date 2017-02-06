// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions BrowsingData API functions, which entail
// clearing browsing data, and clearing the browser's cache (which, let's be
// honest, are the same thing), as specified in the extension API JSON.

#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"

#include <string>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/plugins/plugin_data_remover_helper.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"

using content::BrowserThread;

namespace extension_browsing_data_api_constants {

// Parameter name keys.
const char kDataRemovalPermittedKey[] = "dataRemovalPermitted";
const char kDataToRemoveKey[] = "dataToRemove";
const char kOptionsKey[] = "options";

// Type keys.
const char kAppCacheKey[] = "appcache";
const char kCacheKey[] = "cache";
const char kChannelIDsKey[] = "serverBoundCertificates";
const char kCookiesKey[] = "cookies";
const char kDownloadsKey[] = "downloads";
const char kFileSystemsKey[] = "fileSystems";
const char kFormDataKey[] = "formData";
const char kHistoryKey[] = "history";
const char kIndexedDBKey[] = "indexedDB";
const char kLocalStorageKey[] = "localStorage";
const char kPasswordsKey[] = "passwords";
const char kPluginDataKey[] = "pluginData";
const char kServiceWorkersKey[] = "serviceWorkers";
const char kCacheStorageKey[] = "cacheStorage";
const char kWebSQLKey[] = "webSQL";

// Option keys.
const char kExtensionsKey[] = "extension";
const char kOriginTypesKey[] = "originTypes";
const char kProtectedWebKey[] = "protectedWeb";
const char kSinceKey[] = "since";
const char kUnprotectedWebKey[] = "unprotectedWeb";

// Errors!
// The placeholder will be filled by the name of the affected data type (e.g.,
// "history").
const char kBadDataTypeDetails[] = "Invalid value for data type '%s'.";
const char kDeleteProhibitedError[] = "Browsing history and downloads are not "
                                      "permitted to be removed.";
const char kOneAtATimeError[] = "Only one 'browsingData' API call can run at "
                                "a time.";

}  // namespace extension_browsing_data_api_constants

namespace {
int MaskForKey(const char* key) {
  if (strcmp(key, extension_browsing_data_api_constants::kAppCacheKey) == 0)
    return BrowsingDataRemover::REMOVE_APPCACHE;
  if (strcmp(key, extension_browsing_data_api_constants::kCacheKey) == 0)
    return BrowsingDataRemover::REMOVE_CACHE;
  if (strcmp(key, extension_browsing_data_api_constants::kCookiesKey) == 0) {
    return BrowsingDataRemover::REMOVE_COOKIES |
        BrowsingDataRemover::REMOVE_WEBRTC_IDENTITY;
  }
  if (strcmp(key, extension_browsing_data_api_constants::kDownloadsKey) == 0)
    return BrowsingDataRemover::REMOVE_DOWNLOADS;
  if (strcmp(key, extension_browsing_data_api_constants::kFileSystemsKey) == 0)
    return BrowsingDataRemover::REMOVE_FILE_SYSTEMS;
  if (strcmp(key, extension_browsing_data_api_constants::kFormDataKey) == 0)
    return BrowsingDataRemover::REMOVE_FORM_DATA;
  if (strcmp(key, extension_browsing_data_api_constants::kHistoryKey) == 0)
    return BrowsingDataRemover::REMOVE_HISTORY;
  if (strcmp(key, extension_browsing_data_api_constants::kIndexedDBKey) == 0)
    return BrowsingDataRemover::REMOVE_INDEXEDDB;
  if (strcmp(key, extension_browsing_data_api_constants::kLocalStorageKey) == 0)
    return BrowsingDataRemover::REMOVE_LOCAL_STORAGE;
  if (strcmp(key,
             extension_browsing_data_api_constants::kChannelIDsKey) == 0)
    return BrowsingDataRemover::REMOVE_CHANNEL_IDS;
  if (strcmp(key, extension_browsing_data_api_constants::kPasswordsKey) == 0)
    return BrowsingDataRemover::REMOVE_PASSWORDS;
  if (strcmp(key, extension_browsing_data_api_constants::kPluginDataKey) == 0)
    return BrowsingDataRemover::REMOVE_PLUGIN_DATA;
  if (strcmp(key, extension_browsing_data_api_constants::kServiceWorkersKey) ==
      0)
    return BrowsingDataRemover::REMOVE_SERVICE_WORKERS;
  if (strcmp(key, extension_browsing_data_api_constants::kCacheStorageKey) == 0)
    return BrowsingDataRemover::REMOVE_CACHE_STORAGE;
  if (strcmp(key, extension_browsing_data_api_constants::kWebSQLKey) == 0)
    return BrowsingDataRemover::REMOVE_WEBSQL;

  return 0;
}

// Returns false if any of the selected data types are not allowed to be
// deleted.
bool IsRemovalPermitted(int removal_mask, PrefService* prefs) {
  // Enterprise policy or user preference might prohibit deleting browser or
  // download history.
  if ((removal_mask & BrowsingDataRemover::REMOVE_HISTORY) ||
      (removal_mask & BrowsingDataRemover::REMOVE_DOWNLOADS)) {
    return prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory);
  }
  return true;
}

}  // namespace

bool BrowsingDataSettingsFunction::RunSync() {
  PrefService* prefs = GetProfile()->GetPrefs();

  // Fill origin types.
  // The "cookies" and "hosted apps" UI checkboxes both map to
  // REMOVE_SITE_DATA in browsing_data_remover.h, the former for the unprotected
  // web, the latter for  protected web data. There is no UI control for
  // extension data.
  std::unique_ptr<base::DictionaryValue> origin_types(
      new base::DictionaryValue);
  origin_types->SetBoolean(
      extension_browsing_data_api_constants::kUnprotectedWebKey,
      prefs->GetBoolean(prefs::kDeleteCookies));
  origin_types->SetBoolean(
      extension_browsing_data_api_constants::kProtectedWebKey,
      prefs->GetBoolean(prefs::kDeleteHostedAppsData));
  origin_types->SetBoolean(
      extension_browsing_data_api_constants::kExtensionsKey, false);

  // Fill deletion time period.
  int period_pref = prefs->GetInteger(prefs::kDeleteTimePeriod);
  BrowsingDataRemover::TimePeriod period =
      static_cast<BrowsingDataRemover::TimePeriod>(period_pref);
  double since = 0;
  if (period != BrowsingDataRemover::EVERYTHING) {
    base::Time time = BrowsingDataRemover::CalculateBeginDeleteTime(period);
    since = time.ToJsTime();
  }

  std::unique_ptr<base::DictionaryValue> options(new base::DictionaryValue);
  options->Set(extension_browsing_data_api_constants::kOriginTypesKey,
               origin_types.release());
  options->SetDouble(extension_browsing_data_api_constants::kSinceKey, since);

  // Fill dataToRemove and dataRemovalPermitted.
  std::unique_ptr<base::DictionaryValue> selected(new base::DictionaryValue);
  std::unique_ptr<base::DictionaryValue> permitted(new base::DictionaryValue);

  bool delete_site_data = prefs->GetBoolean(prefs::kDeleteCookies) ||
                          prefs->GetBoolean(prefs::kDeleteHostedAppsData);

  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kAppCacheKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kCookiesKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kFileSystemsKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kIndexedDBKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
      extension_browsing_data_api_constants::kLocalStorageKey,
      delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kWebSQLKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
      extension_browsing_data_api_constants::kChannelIDsKey,
      delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kServiceWorkersKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kCacheStorageKey,
             delete_site_data);

  SetDetails(selected.get(), permitted.get(),
      extension_browsing_data_api_constants::kPluginDataKey,
      delete_site_data && prefs->GetBoolean(prefs::kClearPluginLSODataEnabled));

  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kHistoryKey,
             prefs->GetBoolean(prefs::kDeleteBrowsingHistory));
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kDownloadsKey,
             prefs->GetBoolean(prefs::kDeleteDownloadHistory));
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kCacheKey,
             prefs->GetBoolean(prefs::kDeleteCache));
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kFormDataKey,
             prefs->GetBoolean(prefs::kDeleteFormData));
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kPasswordsKey,
             prefs->GetBoolean(prefs::kDeletePasswords));

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  result->Set(extension_browsing_data_api_constants::kOptionsKey,
              options.release());
  result->Set(extension_browsing_data_api_constants::kDataToRemoveKey,
              selected.release());
  result->Set(extension_browsing_data_api_constants::kDataRemovalPermittedKey,
              permitted.release());
  SetResult(std::move(result));
  return true;
}

void BrowsingDataSettingsFunction::SetDetails(
    base::DictionaryValue* selected_dict,
    base::DictionaryValue* permitted_dict,
    const char* data_type,
    bool is_selected) {
  bool is_permitted =
      IsRemovalPermitted(MaskForKey(data_type), GetProfile()->GetPrefs());
  selected_dict->SetBoolean(data_type, is_selected && is_permitted);
  permitted_dict->SetBoolean(data_type, is_permitted);
}

BrowsingDataRemoverFunction::BrowsingDataRemoverFunction() : observer_(this) {}

void BrowsingDataRemoverFunction::OnBrowsingDataRemoverDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  observer_.RemoveAll();

  this->SendResponse(true);

  Release();  // Balanced in RunAsync.
}

bool BrowsingDataRemoverFunction::RunAsync() {
  // If we don't have a profile, something's pretty wrong.
  DCHECK(GetProfile());

  // Grab the initial |options| parameter, and parse out the arguments.
  base::DictionaryValue* options;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &options));
  DCHECK(options);

  origin_type_mask_ = ParseOriginTypeMask(*options);

  // If |ms_since_epoch| isn't set, default it to 0.
  double ms_since_epoch;
  if (!options->GetDouble(extension_browsing_data_api_constants::kSinceKey,
                          &ms_since_epoch))
    ms_since_epoch = 0;

  // base::Time takes a double that represents seconds since epoch. JavaScript
  // gives developers milliseconds, so do a quick conversion before populating
  // the object. Also, Time::FromDoubleT converts double time 0 to empty Time
  // object. So we need to do special handling here.
  remove_since_ = (ms_since_epoch == 0) ?
      base::Time::UnixEpoch() :
      base::Time::FromDoubleT(ms_since_epoch / 1000.0);

  removal_mask_ = GetRemovalMask();
  if (bad_message_)
    return false;

  // Check for prohibited data types.
  if (!IsRemovalPermitted(removal_mask_, GetProfile()->GetPrefs())) {
    error_ = extension_browsing_data_api_constants::kDeleteProhibitedError;
    return false;
  }

  if (removal_mask_ & BrowsingDataRemover::REMOVE_PLUGIN_DATA) {
    // If we're being asked to remove plugin data, check whether it's actually
    // supported.
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(
            &BrowsingDataRemoverFunction::CheckRemovingPluginDataSupported,
            this,
            PluginPrefs::GetForProfile(GetProfile())));
  } else {
    StartRemoving();
  }

  // Will finish asynchronously.
  return true;
}

BrowsingDataRemoverFunction::~BrowsingDataRemoverFunction() {}

void BrowsingDataRemoverFunction::CheckRemovingPluginDataSupported(
    scoped_refptr<PluginPrefs> plugin_prefs) {
  if (!PluginDataRemoverHelper::IsSupported(plugin_prefs.get()))
    removal_mask_ &= ~BrowsingDataRemover::REMOVE_PLUGIN_DATA;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataRemoverFunction::StartRemoving, this));
}

void BrowsingDataRemoverFunction::StartRemoving() {
  BrowsingDataRemover* remover =
      BrowsingDataRemoverFactory::GetForBrowserContext(GetProfile());
  if (remover->is_removing()) {
    error_ = extension_browsing_data_api_constants::kOneAtATimeError;
    SendResponse(false);
    return;
  }

  // If we're good to go, add a ref (Balanced in OnBrowsingDataRemoverDone)
  AddRef();

  // Create a BrowsingDataRemover, set the current object as an observer (so
  // that we're notified after removal) and call remove() with the arguments
  // we've generated above. We can use a raw pointer here, as the browsing data
  // remover is responsible for deleting itself once data removal is complete.
  observer_.Add(remover);
  remover->Remove(
      BrowsingDataRemover::TimeRange(remove_since_, base::Time::Max()),
      removal_mask_, origin_type_mask_);
}

int BrowsingDataRemoverFunction::ParseOriginTypeMask(
    const base::DictionaryValue& options) {
  // Parse the |options| dictionary to generate the origin set mask. Default to
  // UNPROTECTED_WEB if the developer doesn't specify anything.
  int mask = BrowsingDataHelper::UNPROTECTED_WEB;

  const base::DictionaryValue* d = NULL;
  if (options.HasKey(extension_browsing_data_api_constants::kOriginTypesKey)) {
    EXTENSION_FUNCTION_VALIDATE(options.GetDictionary(
        extension_browsing_data_api_constants::kOriginTypesKey, &d));
    bool value;

    // The developer specified something! Reset to 0 and parse the dictionary.
    mask = 0;

    // Unprotected web.
    if (d->HasKey(extension_browsing_data_api_constants::kUnprotectedWebKey)) {
      EXTENSION_FUNCTION_VALIDATE(d->GetBoolean(
          extension_browsing_data_api_constants::kUnprotectedWebKey, &value));
      mask |= value ? BrowsingDataHelper::UNPROTECTED_WEB : 0;
    }

    // Protected web.
    if (d->HasKey(extension_browsing_data_api_constants::kProtectedWebKey)) {
      EXTENSION_FUNCTION_VALIDATE(d->GetBoolean(
          extension_browsing_data_api_constants::kProtectedWebKey, &value));
      mask |= value ? BrowsingDataHelper::PROTECTED_WEB : 0;
    }

    // Extensions.
    if (d->HasKey(extension_browsing_data_api_constants::kExtensionsKey)) {
      EXTENSION_FUNCTION_VALIDATE(d->GetBoolean(
          extension_browsing_data_api_constants::kExtensionsKey, &value));
      mask |= value ? BrowsingDataHelper::EXTENSION : 0;
    }
  }

  return mask;
}

// Parses the |dataToRemove| argument to generate the removal mask. Sets
// |bad_message_| (like EXTENSION_FUNCTION_VALIDATE would if this were a bool
// method) if 'dataToRemove' is not present or any data-type keys don't have
// supported (boolean) values.
int BrowsingDataRemoveFunction::GetRemovalMask() {
  base::DictionaryValue* data_to_remove;
  if (!args_->GetDictionary(1, &data_to_remove)) {
    bad_message_ = true;
    return 0;
  }

  int removal_mask = 0;

  for (base::DictionaryValue::Iterator i(*data_to_remove);
       !i.IsAtEnd();
       i.Advance()) {
    bool selected = false;
    if (!i.value().GetAsBoolean(&selected)) {
      bad_message_ = true;
      return 0;
    }
    if (selected)
      removal_mask |= MaskForKey(i.key().c_str());
  }

  return removal_mask;
}

int BrowsingDataRemoveAppcacheFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_APPCACHE;
}

int BrowsingDataRemoveCacheFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_CACHE;
}

int BrowsingDataRemoveCookiesFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_COOKIES |
         BrowsingDataRemover::REMOVE_CHANNEL_IDS |
         BrowsingDataRemover::REMOVE_WEBRTC_IDENTITY;
}

int BrowsingDataRemoveDownloadsFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_DOWNLOADS;
}

int BrowsingDataRemoveFileSystemsFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_FILE_SYSTEMS;
}

int BrowsingDataRemoveFormDataFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_FORM_DATA;
}

int BrowsingDataRemoveHistoryFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_HISTORY;
}

int BrowsingDataRemoveIndexedDBFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_INDEXEDDB;
}

int BrowsingDataRemoveLocalStorageFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_LOCAL_STORAGE;
}

int BrowsingDataRemovePluginDataFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_PLUGIN_DATA;
}

int BrowsingDataRemovePasswordsFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_PASSWORDS;
}

int BrowsingDataRemoveServiceWorkersFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_SERVICE_WORKERS;
}

int BrowsingDataRemoveCacheStorageFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_CACHE_STORAGE;
}

int BrowsingDataRemoveWebSQLFunction::GetRemovalMask() {
  return BrowsingDataRemover::REMOVE_WEBSQL;
}
