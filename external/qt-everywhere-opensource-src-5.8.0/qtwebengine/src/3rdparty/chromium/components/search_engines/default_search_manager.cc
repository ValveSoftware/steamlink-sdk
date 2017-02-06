// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"

namespace {

bool g_fallback_search_engines_disabled = false;

}  // namespace

// A dictionary to hold all data related to the Default Search Engine.
// Eventually, this should replace all the data stored in the
// default_search_provider.* prefs.
const char DefaultSearchManager::kDefaultSearchProviderDataPrefName[] =
    "default_search_provider_data.template_url_data";

const char DefaultSearchManager::kID[] = "id";
const char DefaultSearchManager::kShortName[] = "short_name";
const char DefaultSearchManager::kKeyword[] = "keyword";
const char DefaultSearchManager::kPrepopulateID[] = "prepopulate_id";
const char DefaultSearchManager::kSyncGUID[] = "synced_guid";

const char DefaultSearchManager::kURL[] = "url";
const char DefaultSearchManager::kSuggestionsURL[] = "suggestions_url";
const char DefaultSearchManager::kInstantURL[] = "instant_url";
const char DefaultSearchManager::kImageURL[] = "image_url";
const char DefaultSearchManager::kNewTabURL[] = "new_tab_url";
const char DefaultSearchManager::kFaviconURL[] = "favicon_url";
const char DefaultSearchManager::kOriginatingURL[] = "originating_url";

const char DefaultSearchManager::kSearchURLPostParams[] =
    "search_url_post_params";
const char DefaultSearchManager::kSuggestionsURLPostParams[] =
    "suggestions_url_post_params";
const char DefaultSearchManager::kInstantURLPostParams[] =
    "instant_url_post_params";
const char DefaultSearchManager::kImageURLPostParams[] =
    "image_url_post_params";

const char DefaultSearchManager::kSafeForAutoReplace[] = "safe_for_autoreplace";
const char DefaultSearchManager::kInputEncodings[] = "input_encodings";

const char DefaultSearchManager::kDateCreated[] = "date_created";
const char DefaultSearchManager::kLastModified[] = "last_modified";

const char DefaultSearchManager::kUsageCount[] = "usage_count";
const char DefaultSearchManager::kAlternateURLs[] = "alternate_urls";
const char DefaultSearchManager::kSearchTermsReplacementKey[] =
    "search_terms_replacement_key";
const char DefaultSearchManager::kCreatedByPolicy[] = "created_by_policy";
const char DefaultSearchManager::kDisabledByPolicy[] = "disabled_by_policy";

DefaultSearchManager::DefaultSearchManager(
    PrefService* pref_service,
    const ObserverCallback& change_observer)
    : pref_service_(pref_service),
      change_observer_(change_observer),
      default_search_controlled_by_policy_(false) {
  if (pref_service_) {
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        kDefaultSearchProviderDataPrefName,
        base::Bind(&DefaultSearchManager::OnDefaultSearchPrefChanged,
                   base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSearchProviderOverrides,
        base::Bind(&DefaultSearchManager::OnOverridesPrefChanged,
                   base::Unretained(this)));
  }
  LoadPrepopulatedDefaultSearch();
  LoadDefaultSearchEngineFromPrefs();
}

DefaultSearchManager::~DefaultSearchManager() {
}

// static
void DefaultSearchManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kDefaultSearchProviderDataPrefName);
}

// static
void DefaultSearchManager::AddPrefValueToMap(
    std::unique_ptr<base::DictionaryValue> value,
    PrefValueMap* pref_value_map) {
  pref_value_map->SetValue(kDefaultSearchProviderDataPrefName,
                           std::move(value));
}

// static
void DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(
    bool disabled) {
  g_fallback_search_engines_disabled = disabled;
}

TemplateURLData* DefaultSearchManager::GetDefaultSearchEngine(
    Source* source) const {
  if (default_search_controlled_by_policy_) {
    if (source)
      *source = FROM_POLICY;
    return prefs_default_search_.get();
  }
  if (extension_default_search_) {
    if (source)
      *source = FROM_EXTENSION;
    return extension_default_search_.get();
  }
  if (prefs_default_search_) {
    if (source)
      *source = FROM_USER;
    return prefs_default_search_.get();
  }

  if (source)
    *source = FROM_FALLBACK;
  return g_fallback_search_engines_disabled ?
      NULL : fallback_default_search_.get();
}

DefaultSearchManager::Source
DefaultSearchManager::GetDefaultSearchEngineSource() const {
  Source source;
  GetDefaultSearchEngine(&source);
  return source;
}

void DefaultSearchManager::SetUserSelectedDefaultSearchEngine(
    const TemplateURLData& data) {
  if (!pref_service_) {
    prefs_default_search_.reset(new TemplateURLData(data));
    MergePrefsDataWithPrepopulated();
    NotifyObserver();
    return;
  }

  base::DictionaryValue url_dict;
  url_dict.SetString(kID, base::Int64ToString(data.id));
  url_dict.SetString(kShortName, data.short_name());
  url_dict.SetString(kKeyword, data.keyword());
  url_dict.SetInteger(kPrepopulateID, data.prepopulate_id);
  url_dict.SetString(kSyncGUID, data.sync_guid);

  url_dict.SetString(kURL, data.url());
  url_dict.SetString(kSuggestionsURL, data.suggestions_url);
  url_dict.SetString(kInstantURL, data.instant_url);
  url_dict.SetString(kImageURL, data.image_url);
  url_dict.SetString(kNewTabURL, data.new_tab_url);
  url_dict.SetString(kFaviconURL, data.favicon_url.spec());
  url_dict.SetString(kOriginatingURL, data.originating_url.spec());

  url_dict.SetString(kSearchURLPostParams, data.search_url_post_params);
  url_dict.SetString(kSuggestionsURLPostParams,
                     data.suggestions_url_post_params);
  url_dict.SetString(kInstantURLPostParams, data.instant_url_post_params);
  url_dict.SetString(kImageURLPostParams, data.image_url_post_params);

  url_dict.SetBoolean(kSafeForAutoReplace, data.safe_for_autoreplace);

  url_dict.SetString(kDateCreated,
                     base::Int64ToString(data.date_created.ToInternalValue()));
  url_dict.SetString(kLastModified,
                     base::Int64ToString(data.last_modified.ToInternalValue()));
  url_dict.SetInteger(kUsageCount, data.usage_count);

  std::unique_ptr<base::ListValue> alternate_urls(new base::ListValue);
  for (std::vector<std::string>::const_iterator it =
           data.alternate_urls.begin();
       it != data.alternate_urls.end(); ++it) {
    alternate_urls->AppendString(*it);
  }
  url_dict.Set(kAlternateURLs, alternate_urls.release());

  std::unique_ptr<base::ListValue> encodings(new base::ListValue);
  for (std::vector<std::string>::const_iterator it =
           data.input_encodings.begin();
       it != data.input_encodings.end(); ++it) {
    encodings->AppendString(*it);
  }
  url_dict.Set(kInputEncodings, encodings.release());

  url_dict.SetString(kSearchTermsReplacementKey,
                     data.search_terms_replacement_key);

  url_dict.SetBoolean(kCreatedByPolicy, data.created_by_policy);

  pref_service_->Set(kDefaultSearchProviderDataPrefName, url_dict);
}

void DefaultSearchManager::SetExtensionControlledDefaultSearchEngine(
    const TemplateURLData& data) {
  extension_default_search_.reset(new TemplateURLData(data));
  if (GetDefaultSearchEngineSource() == FROM_EXTENSION)
    NotifyObserver();
}

void DefaultSearchManager::ClearExtensionControlledDefaultSearchEngine() {
  Source old_source = GetDefaultSearchEngineSource();
  extension_default_search_.reset();
  if (old_source == FROM_EXTENSION)
    NotifyObserver();
}

void DefaultSearchManager::ClearUserSelectedDefaultSearchEngine() {
  if (pref_service_) {
    pref_service_->ClearPref(kDefaultSearchProviderDataPrefName);
  } else {
    prefs_default_search_.reset();
    NotifyObserver();
  }
}

void DefaultSearchManager::OnDefaultSearchPrefChanged() {
  Source source = GetDefaultSearchEngineSource();
  LoadDefaultSearchEngineFromPrefs();

  // If we were/are FROM_USER or FROM_POLICY the effective DSE may have changed.
  if (source != FROM_USER && source != FROM_POLICY)
    source = GetDefaultSearchEngineSource();
  if (source == FROM_USER || source == FROM_POLICY)
    NotifyObserver();
}

void DefaultSearchManager::OnOverridesPrefChanged() {
  LoadPrepopulatedDefaultSearch();

  TemplateURLData* effective_data = GetDefaultSearchEngine(NULL);
  if (effective_data && effective_data->prepopulate_id) {
    // A user-selected, policy-selected or fallback pre-populated engine is
    // active and may have changed with this event.
    NotifyObserver();
  }
}

void DefaultSearchManager::MergePrefsDataWithPrepopulated() {
  if (!prefs_default_search_ || !prefs_default_search_->prepopulate_id)
    return;

  size_t default_search_index;
  ScopedVector<TemplateURLData> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(pref_service_,
                                                         &default_search_index);

  for (size_t i = 0; i < prepopulated_urls.size(); ++i) {
    if (prepopulated_urls[i]->prepopulate_id ==
        prefs_default_search_->prepopulate_id) {
      if (!prefs_default_search_->safe_for_autoreplace) {
        prepopulated_urls[i]->safe_for_autoreplace = false;
        prepopulated_urls[i]->SetKeyword(prefs_default_search_->keyword());
        prepopulated_urls[i]->SetShortName(prefs_default_search_->short_name());
      }
      prepopulated_urls[i]->id = prefs_default_search_->id;
      prepopulated_urls[i]->sync_guid = prefs_default_search_->sync_guid;
      prepopulated_urls[i]->date_created = prefs_default_search_->date_created;
      prepopulated_urls[i]->last_modified =
          prefs_default_search_->last_modified;
      prefs_default_search_.reset(prepopulated_urls[i]);
      prepopulated_urls.weak_erase(prepopulated_urls.begin() + i);
      return;
    }
  }
}

void DefaultSearchManager::LoadDefaultSearchEngineFromPrefs() {
  if (!pref_service_)
    return;

  prefs_default_search_.reset();
  const PrefService::Preference* pref =
      pref_service_->FindPreference(kDefaultSearchProviderDataPrefName);
  DCHECK(pref);
  default_search_controlled_by_policy_ = pref->IsManaged();

  const base::DictionaryValue* url_dict =
      pref_service_->GetDictionary(kDefaultSearchProviderDataPrefName);
  if (url_dict->empty())
    return;

  if (default_search_controlled_by_policy_) {
    bool disabled_by_policy = false;
    if (url_dict->GetBoolean(kDisabledByPolicy, &disabled_by_policy) &&
        disabled_by_policy)
      return;
  }

  std::string search_url;
  base::string16 keyword;
  url_dict->GetString(kURL, &search_url);
  url_dict->GetString(kKeyword, &keyword);
  if (search_url.empty() || keyword.empty())
    return;

  prefs_default_search_.reset(new TemplateURLData);
  prefs_default_search_->SetKeyword(keyword);
  prefs_default_search_->SetURL(search_url);

  std::string id;
  url_dict->GetString(kID, &id);
  base::StringToInt64(id, &prefs_default_search_->id);
  base::string16 short_name;
  url_dict->GetString(kShortName, &short_name);
  prefs_default_search_->SetShortName(short_name);
  url_dict->GetInteger(kPrepopulateID, &prefs_default_search_->prepopulate_id);
  url_dict->GetString(kSyncGUID, &prefs_default_search_->sync_guid);

  url_dict->GetString(kSuggestionsURL, &prefs_default_search_->suggestions_url);
  url_dict->GetString(kInstantURL, &prefs_default_search_->instant_url);
  url_dict->GetString(kImageURL, &prefs_default_search_->image_url);
  url_dict->GetString(kNewTabURL, &prefs_default_search_->new_tab_url);

  std::string favicon_url;
  std::string originating_url;
  url_dict->GetString(kFaviconURL, &favicon_url);
  url_dict->GetString(kOriginatingURL, &originating_url);
  prefs_default_search_->favicon_url = GURL(favicon_url);
  prefs_default_search_->originating_url = GURL(originating_url);

  url_dict->GetString(kSearchURLPostParams,
                      &prefs_default_search_->search_url_post_params);
  url_dict->GetString(kSuggestionsURLPostParams,
                      &prefs_default_search_->suggestions_url_post_params);
  url_dict->GetString(kInstantURLPostParams,
                      &prefs_default_search_->instant_url_post_params);
  url_dict->GetString(kImageURLPostParams,
                      &prefs_default_search_->image_url_post_params);

  url_dict->GetBoolean(kSafeForAutoReplace,
                       &prefs_default_search_->safe_for_autoreplace);

  std::string date_created_str;
  std::string last_modified_str;
  url_dict->GetString(kDateCreated, &date_created_str);
  url_dict->GetString(kLastModified, &last_modified_str);

  int64_t date_created = 0;
  if (base::StringToInt64(date_created_str, &date_created)) {
    prefs_default_search_->date_created =
        base::Time::FromInternalValue(date_created);
  }

  int64_t last_modified = 0;
  if (base::StringToInt64(date_created_str, &last_modified)) {
    prefs_default_search_->last_modified =
        base::Time::FromInternalValue(last_modified);
  }

  url_dict->GetInteger(kUsageCount, &prefs_default_search_->usage_count);

  const base::ListValue* alternate_urls = NULL;
  if (url_dict->GetList(kAlternateURLs, &alternate_urls)) {
    for (base::ListValue::const_iterator it = alternate_urls->begin();
         it != alternate_urls->end();
         ++it) {
      std::string alternate_url;
      if ((*it)->GetAsString(&alternate_url))
        prefs_default_search_->alternate_urls.push_back(alternate_url);
    }
  }

  const base::ListValue* encodings = NULL;
  if (url_dict->GetList(kInputEncodings, &encodings)) {
    for (base::ListValue::const_iterator it = encodings->begin();
         it != encodings->end();
         ++it) {
      std::string encoding;
      if ((*it)->GetAsString(&encoding))
        prefs_default_search_->input_encodings.push_back(encoding);
    }
  }

  url_dict->GetString(kSearchTermsReplacementKey,
                      &prefs_default_search_->search_terms_replacement_key);

  url_dict->GetBoolean(kCreatedByPolicy,
                       &prefs_default_search_->created_by_policy);

  prefs_default_search_->show_in_default_list = true;
  MergePrefsDataWithPrepopulated();
}

void DefaultSearchManager::LoadPrepopulatedDefaultSearch() {
  std::unique_ptr<TemplateURLData> data =
      TemplateURLPrepopulateData::GetPrepopulatedDefaultSearch(pref_service_);
  fallback_default_search_ = std::move(data);
  MergePrefsDataWithPrepopulated();
}

void DefaultSearchManager::NotifyObserver() {
  if (!change_observer_.is_null()) {
    Source source = FROM_FALLBACK;
    TemplateURLData* data = GetDefaultSearchEngine(&source);
    change_observer_.Run(data, source);
  }
}
