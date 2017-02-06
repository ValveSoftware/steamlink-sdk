// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_pref_migration.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "url/gurl.h"

namespace {

// Loads the user-selected DSE (if there is one) from legacy preferences.
std::unique_ptr<TemplateURLData> LoadDefaultSearchProviderFromLegacyPrefs(
    PrefService* prefs) {
  if (!prefs->HasPrefPath(prefs::kDefaultSearchProviderSearchURL) ||
      !prefs->HasPrefPath(prefs::kDefaultSearchProviderKeyword))
    return std::unique_ptr<TemplateURLData>();

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kDefaultSearchProviderSearchURL);
  DCHECK(pref);
  if (pref->IsManaged())
    return std::unique_ptr<TemplateURLData>();

  base::string16 keyword =
      base::UTF8ToUTF16(prefs->GetString(prefs::kDefaultSearchProviderKeyword));
  std::string search_url =
      prefs->GetString(prefs::kDefaultSearchProviderSearchURL);
  if (keyword.empty() || search_url.empty())
    return std::unique_ptr<TemplateURLData>();

  std::unique_ptr<TemplateURLData> default_provider_data(new TemplateURLData);
  default_provider_data->SetShortName(
      base::UTF8ToUTF16(prefs->GetString(prefs::kDefaultSearchProviderName)));
  default_provider_data->SetKeyword(keyword);
  default_provider_data->SetURL(search_url);
  default_provider_data->suggestions_url =
      prefs->GetString(prefs::kDefaultSearchProviderSuggestURL);
  default_provider_data->instant_url =
      prefs->GetString(prefs::kDefaultSearchProviderInstantURL);
  default_provider_data->image_url =
      prefs->GetString(prefs::kDefaultSearchProviderImageURL);
  default_provider_data->new_tab_url =
      prefs->GetString(prefs::kDefaultSearchProviderNewTabURL);
  default_provider_data->search_url_post_params =
      prefs->GetString(prefs::kDefaultSearchProviderSearchURLPostParams);
  default_provider_data->suggestions_url_post_params =
      prefs->GetString(prefs::kDefaultSearchProviderSuggestURLPostParams);
  default_provider_data->instant_url_post_params =
      prefs->GetString(prefs::kDefaultSearchProviderInstantURLPostParams);
  default_provider_data->image_url_post_params =
      prefs->GetString(prefs::kDefaultSearchProviderImageURLPostParams);
  default_provider_data->favicon_url =
      GURL(prefs->GetString(prefs::kDefaultSearchProviderIconURL));
  default_provider_data->show_in_default_list = true;
  default_provider_data->search_terms_replacement_key =
      prefs->GetString(prefs::kDefaultSearchProviderSearchTermsReplacementKey);
  default_provider_data->input_encodings = base::SplitString(
      prefs->GetString(prefs::kDefaultSearchProviderEncodings),
      ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  default_provider_data->alternate_urls.clear();
  const base::ListValue* alternate_urls =
      prefs->GetList(prefs::kDefaultSearchProviderAlternateURLs);
  for (size_t i = 0; i < alternate_urls->GetSize(); ++i) {
    std::string alternate_url;
    if (alternate_urls->GetString(i, &alternate_url))
      default_provider_data->alternate_urls.push_back(alternate_url);
  }

  std::string id_string = prefs->GetString(prefs::kDefaultSearchProviderID);
  if (!id_string.empty()) {
    int64_t value;
    base::StringToInt64(id_string, &value);
    default_provider_data->id = value;
  }

  std::string prepopulate_id =
      prefs->GetString(prefs::kDefaultSearchProviderPrepopulateID);
  if (!prepopulate_id.empty()) {
    int value;
    base::StringToInt(prepopulate_id, &value);
    default_provider_data->prepopulate_id = value;
  }

  return default_provider_data;
}

void ClearDefaultSearchProviderFromLegacyPrefs(PrefService* prefs) {
  prefs->ClearPref(prefs::kDefaultSearchProviderName);
  prefs->ClearPref(prefs::kDefaultSearchProviderKeyword);
  prefs->ClearPref(prefs::kDefaultSearchProviderSearchURL);
  prefs->ClearPref(prefs::kDefaultSearchProviderSuggestURL);
  prefs->ClearPref(prefs::kDefaultSearchProviderInstantURL);
  prefs->ClearPref(prefs::kDefaultSearchProviderImageURL);
  prefs->ClearPref(prefs::kDefaultSearchProviderNewTabURL);
  prefs->ClearPref(prefs::kDefaultSearchProviderSearchURLPostParams);
  prefs->ClearPref(prefs::kDefaultSearchProviderSuggestURLPostParams);
  prefs->ClearPref(prefs::kDefaultSearchProviderInstantURLPostParams);
  prefs->ClearPref(prefs::kDefaultSearchProviderImageURLPostParams);
  prefs->ClearPref(prefs::kDefaultSearchProviderIconURL);
  prefs->ClearPref(prefs::kDefaultSearchProviderEncodings);
  prefs->ClearPref(prefs::kDefaultSearchProviderPrepopulateID);
  prefs->ClearPref(prefs::kDefaultSearchProviderAlternateURLs);
  prefs->ClearPref(prefs::kDefaultSearchProviderSearchTermsReplacementKey);
}

void MigrateDefaultSearchPref(PrefService* pref_service) {
  DCHECK(pref_service);

  std::unique_ptr<TemplateURLData> legacy_dse_from_prefs =
      LoadDefaultSearchProviderFromLegacyPrefs(pref_service);
  if (!legacy_dse_from_prefs)
    return;

  DefaultSearchManager default_search_manager(
      pref_service, DefaultSearchManager::ObserverCallback());
  DefaultSearchManager::Source modern_source;
  TemplateURLData* modern_value =
      default_search_manager.GetDefaultSearchEngine(&modern_source);
  if (modern_source == DefaultSearchManager::FROM_FALLBACK) {
    // |modern_value| is the prepopulated default. If it matches the legacy DSE
    // we assume it is not a user-selected value.
    if (!modern_value ||
        legacy_dse_from_prefs->prepopulate_id != modern_value->prepopulate_id) {
      // This looks like a user-selected value, so let's migrate it.
      // TODO(erikwright): Remove this migration logic when this stat approaches
      // zero.
      UMA_HISTOGRAM_BOOLEAN("Search.MigratedPrefToDictionaryValue", true);
      default_search_manager.SetUserSelectedDefaultSearchEngine(
          *legacy_dse_from_prefs);
    }
  }

  ClearDefaultSearchProviderFromLegacyPrefs(pref_service);
}

void OnPrefsInitialized(PrefService* pref_service,
                        bool pref_service_initialization_success) {
  MigrateDefaultSearchPref(pref_service);
}

}  // namespace

void ConfigureDefaultSearchPrefMigrationToDictionaryValue(
    PrefService* pref_service) {
  if (pref_service->GetInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_WAITING) {
    pref_service->AddPrefInitObserver(
        base::Bind(&OnPrefsInitialized, base::Unretained(pref_service)));
  } else {
    MigrateDefaultSearchPref(pref_service);
  }
}
