// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_policy_handler.h"

#include <stddef.h>

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "grit/components_strings.h"
#include "policy/policy_constants.h"

namespace policy {

namespace {
// Extracts a list from a policy value and adds it to a pref dictionary.
void SetListInPref(const PolicyMap& policies,
                   const char* policy_name,
                   const char* key,
                   base::DictionaryValue* dict) {
  DCHECK(dict);
  const base::Value* policy_value = policies.GetValue(policy_name);
  const base::ListValue* policy_list = NULL;
  if (policy_value) {
    bool is_list = policy_value->GetAsList(&policy_list);
    DCHECK(is_list);
  }
  dict->Set(key, policy_list ? policy_list->DeepCopy() : new base::ListValue());
}

// Extracts a string from a policy value and adds it to a pref dictionary.
void SetStringInPref(const PolicyMap& policies,
                     const char* policy_name,
                     const char* key,
                     base::DictionaryValue* dict) {
  DCHECK(dict);
  const base::Value* policy_value = policies.GetValue(policy_name);
  std::string str;
  if (policy_value) {
    bool is_string = policy_value->GetAsString(&str);
    DCHECK(is_string);
  }
  dict->SetString(key, str);
}

}  // namespace

// List of policy types to preference names, for policies affecting the default
// search provider.
const PolicyToPreferenceMapEntry kDefaultSearchPolicyMap[] = {
  { key::kDefaultSearchProviderEnabled,
    prefs::kDefaultSearchProviderEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kDefaultSearchProviderName,
    prefs::kDefaultSearchProviderName,
    base::Value::TYPE_STRING },
  { key::kDefaultSearchProviderKeyword,
    prefs::kDefaultSearchProviderKeyword,
    base::Value::TYPE_STRING },
  { key::kDefaultSearchProviderSearchURL,
    prefs::kDefaultSearchProviderSearchURL,
    base::Value::TYPE_STRING },
  { key::kDefaultSearchProviderSuggestURL,
    prefs::kDefaultSearchProviderSuggestURL,
    base::Value::TYPE_STRING },
  { key::kDefaultSearchProviderInstantURL,
    prefs::kDefaultSearchProviderInstantURL,
    base::Value::TYPE_STRING },
  { key::kDefaultSearchProviderIconURL,
    prefs::kDefaultSearchProviderIconURL,
    base::Value::TYPE_STRING },
  { key::kDefaultSearchProviderEncodings,
    prefs::kDefaultSearchProviderEncodings,
    base::Value::TYPE_LIST },
  { key::kDefaultSearchProviderAlternateURLs,
    prefs::kDefaultSearchProviderAlternateURLs,
    base::Value::TYPE_LIST },
  { key::kDefaultSearchProviderSearchTermsReplacementKey,
    prefs::kDefaultSearchProviderSearchTermsReplacementKey,
    base::Value::TYPE_STRING },
  { key::kDefaultSearchProviderImageURL,
    prefs::kDefaultSearchProviderImageURL,
    base::Value::TYPE_STRING },
  { key::kDefaultSearchProviderNewTabURL,
    prefs::kDefaultSearchProviderNewTabURL,
    base::Value::TYPE_STRING },
  { key::kDefaultSearchProviderSearchURLPostParams,
    prefs::kDefaultSearchProviderSearchURLPostParams,
    base::Value::TYPE_STRING },
  { key::kDefaultSearchProviderSuggestURLPostParams,
    prefs::kDefaultSearchProviderSuggestURLPostParams,
    base::Value::TYPE_STRING },
  { key::kDefaultSearchProviderInstantURLPostParams,
    prefs::kDefaultSearchProviderInstantURLPostParams,
    base::Value::TYPE_STRING },
  { key::kDefaultSearchProviderImageURLPostParams,
    prefs::kDefaultSearchProviderImageURLPostParams,
    base::Value::TYPE_STRING },
};

// List of policy types to preference names, for policies affecting the default
// search provider.
const PolicyToPreferenceMapEntry kDefaultSearchPolicyDataMap[] = {
    {key::kDefaultSearchProviderName, DefaultSearchManager::kShortName,
     base::Value::TYPE_STRING},
    {key::kDefaultSearchProviderKeyword, DefaultSearchManager::kKeyword,
     base::Value::TYPE_STRING},
    {key::kDefaultSearchProviderSearchURL, DefaultSearchManager::kURL,
     base::Value::TYPE_STRING},
    {key::kDefaultSearchProviderSuggestURL,
     DefaultSearchManager::kSuggestionsURL, base::Value::TYPE_STRING},
    {key::kDefaultSearchProviderInstantURL, DefaultSearchManager::kInstantURL,
     base::Value::TYPE_STRING},
    {key::kDefaultSearchProviderIconURL, DefaultSearchManager::kFaviconURL,
     base::Value::TYPE_STRING},
    {key::kDefaultSearchProviderEncodings,
     DefaultSearchManager::kInputEncodings, base::Value::TYPE_LIST},
    {key::kDefaultSearchProviderAlternateURLs,
     DefaultSearchManager::kAlternateURLs, base::Value::TYPE_LIST},
    {key::kDefaultSearchProviderSearchTermsReplacementKey,
     DefaultSearchManager::kSearchTermsReplacementKey,
     base::Value::TYPE_STRING},
    {key::kDefaultSearchProviderImageURL, DefaultSearchManager::kImageURL,
     base::Value::TYPE_STRING},
    {key::kDefaultSearchProviderNewTabURL, DefaultSearchManager::kNewTabURL,
     base::Value::TYPE_STRING},
    {key::kDefaultSearchProviderSearchURLPostParams,
     DefaultSearchManager::kSearchURLPostParams, base::Value::TYPE_STRING},
    {key::kDefaultSearchProviderSuggestURLPostParams,
     DefaultSearchManager::kSuggestionsURLPostParams, base::Value::TYPE_STRING},
    {key::kDefaultSearchProviderInstantURLPostParams,
     DefaultSearchManager::kInstantURLPostParams, base::Value::TYPE_STRING},
    {key::kDefaultSearchProviderImageURLPostParams,
     DefaultSearchManager::kImageURLPostParams, base::Value::TYPE_STRING},
};

// DefaultSearchEncodingsPolicyHandler implementation --------------------------

DefaultSearchEncodingsPolicyHandler::DefaultSearchEncodingsPolicyHandler()
    : TypeCheckingPolicyHandler(key::kDefaultSearchProviderEncodings,
                                base::Value::TYPE_LIST) {}

DefaultSearchEncodingsPolicyHandler::~DefaultSearchEncodingsPolicyHandler() {
}

void DefaultSearchEncodingsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies, PrefValueMap* prefs) {
  // The DefaultSearchProviderEncodings policy has type list, but the related
  // preference has type string. Convert one into the other here, using
  // ';' as a separator.
  const base::Value* value = policies.GetValue(policy_name());
  const base::ListValue* list;
  if (!value || !value->GetAsList(&list))
    return;

  base::ListValue::const_iterator iter(list->begin());
  base::ListValue::const_iterator end(list->end());
  std::vector<std::string> string_parts;
  for (; iter != end; ++iter) {
    std::string s;
    if ((*iter)->GetAsString(&s)) {
      string_parts.push_back(s);
    }
  }
  std::string encodings = base::JoinString(string_parts, ";");
  prefs->SetString(prefs::kDefaultSearchProviderEncodings, encodings);
}


// DefaultSearchPolicyHandler implementation -----------------------------------

DefaultSearchPolicyHandler::DefaultSearchPolicyHandler() {
  for (size_t i = 0; i < arraysize(kDefaultSearchPolicyMap); ++i) {
    const char* policy_name = kDefaultSearchPolicyMap[i].policy_name;
    if (policy_name == key::kDefaultSearchProviderEncodings) {
      handlers_.push_back(new DefaultSearchEncodingsPolicyHandler());
    } else {
      handlers_.push_back(new SimplePolicyHandler(
          policy_name,
          kDefaultSearchPolicyMap[i].preference_path,
          kDefaultSearchPolicyMap[i].value_type));
    }
  }
}

DefaultSearchPolicyHandler::~DefaultSearchPolicyHandler() {
  STLDeleteElements(&handlers_);
}

bool DefaultSearchPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                     PolicyErrorMap* errors) {
  if (!CheckIndividualPolicies(policies, errors))
    return false;

  if (DefaultSearchProviderIsDisabled(policies)) {
    // Add an error for all specified default search policies except
    // DefaultSearchProviderEnabled.

    for (std::vector<TypeCheckingPolicyHandler*>::const_iterator handler =
             handlers_.begin();
         handler != handlers_.end(); ++handler) {
      const char* policy_name = (*handler)->policy_name();
      if (policy_name != key::kDefaultSearchProviderEnabled &&
          HasDefaultSearchPolicy(policies, policy_name)) {
        errors->AddError(policy_name, IDS_POLICY_DEFAULT_SEARCH_DISABLED);
      }
    }
    return true;
  }

  const base::Value* url;
  std::string dummy;
  if (DefaultSearchURLIsValid(policies, &url, &dummy) ||
      !AnyDefaultSearchPoliciesSpecified(policies))
    return true;
  errors->AddError(key::kDefaultSearchProviderSearchURL, url ?
      IDS_POLICY_INVALID_SEARCH_URL_ERROR : IDS_POLICY_NOT_SPECIFIED_ERROR);
  return false;
}

void DefaultSearchPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  if (DefaultSearchProviderIsDisabled(policies)) {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    dict->SetBoolean(DefaultSearchManager::kDisabledByPolicy, true);
    DefaultSearchManager::AddPrefValueToMap(std::move(dict), prefs);
    return;
  }

  // The search URL is required.  The other entries are optional.  Just make
  // sure that they are all specified via policy, so that the regular prefs
  // aren't used.
  const base::Value* dummy;
  std::string url;
  if (!DefaultSearchURLIsValid(policies, &dummy, &url))
    return;

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  for (size_t i = 0; i < arraysize(kDefaultSearchPolicyDataMap); ++i) {
    const char* policy_name = kDefaultSearchPolicyDataMap[i].policy_name;
    switch (kDefaultSearchPolicyDataMap[i].value_type) {
      case base::Value::TYPE_STRING:
        SetStringInPref(policies,
                        policy_name,
                        kDefaultSearchPolicyDataMap[i].preference_path,
                        dict.get());
        break;
      case base::Value::TYPE_LIST:
        SetListInPref(policies,
                      policy_name,
                      kDefaultSearchPolicyDataMap[i].preference_path,
                      dict.get());
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  // Set the fields which are not specified by the policy to default values.
  dict->SetString(DefaultSearchManager::kID,
                  base::Int64ToString(kInvalidTemplateURLID));
  dict->SetInteger(DefaultSearchManager::kPrepopulateID, 0);
  dict->SetString(DefaultSearchManager::kSyncGUID, std::string());
  dict->SetString(DefaultSearchManager::kOriginatingURL, std::string());
  dict->SetBoolean(DefaultSearchManager::kSafeForAutoReplace, true);
  dict->SetDouble(DefaultSearchManager::kDateCreated,
                  base::Time::Now().ToInternalValue());
  dict->SetDouble(DefaultSearchManager::kLastModified,
                  base::Time::Now().ToInternalValue());
  dict->SetInteger(DefaultSearchManager::kUsageCount, 0);
  dict->SetBoolean(DefaultSearchManager::kCreatedByPolicy, true);

  // For the name and keyword, default to the host if not specified.  If
  // there is no host (as is the case with file URLs of the form:
  // "file:///c:/..."), use "_" to guarantee that the keyword is non-empty.
  std::string name, keyword;
  dict->GetString(DefaultSearchManager::kKeyword, &keyword);
  dict->GetString(DefaultSearchManager::kShortName, &name);
  dict->GetString(DefaultSearchManager::kURL, &url);

  std::string host(GURL(url).host());
  if (host.empty())
    host = "_";
  if (name.empty())
    dict->SetString(DefaultSearchManager::kShortName, host);
  if (keyword.empty())
    dict->SetString(DefaultSearchManager::kKeyword, host);

  DefaultSearchManager::AddPrefValueToMap(std::move(dict), prefs);
}

bool DefaultSearchPolicyHandler::CheckIndividualPolicies(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  for (std::vector<TypeCheckingPolicyHandler*>::const_iterator handler =
           handlers_.begin();
       handler != handlers_.end(); ++handler) {
    if (!(*handler)->CheckPolicySettings(policies, errors))
      return false;
  }
  return true;
}

bool DefaultSearchPolicyHandler::HasDefaultSearchPolicy(
    const PolicyMap& policies,
    const char* policy_name) {
  return policies.Get(policy_name) != NULL;
}

bool DefaultSearchPolicyHandler::AnyDefaultSearchPoliciesSpecified(
    const PolicyMap& policies) {
  for (std::vector<TypeCheckingPolicyHandler*>::const_iterator handler =
           handlers_.begin();
       handler != handlers_.end(); ++handler) {
    if (policies.Get((*handler)->policy_name()))
      return true;
  }
  return false;
}

bool DefaultSearchPolicyHandler::DefaultSearchProviderIsDisabled(
    const PolicyMap& policies) {
  const base::Value* provider_enabled =
      policies.GetValue(key::kDefaultSearchProviderEnabled);
  bool enabled = true;
  return provider_enabled && provider_enabled->GetAsBoolean(&enabled) &&
      !enabled;
}

bool DefaultSearchPolicyHandler::DefaultSearchURLIsValid(
    const PolicyMap& policies,
    const base::Value** url_value,
    std::string* url_string) {
  *url_value = policies.GetValue(key::kDefaultSearchProviderSearchURL);
  if (!*url_value || !(*url_value)->GetAsString(url_string) ||
      url_string->empty())
    return false;
  TemplateURLData data;
  data.SetURL(*url_string);
  SearchTermsData search_terms_data;
  return TemplateURL(data).SupportsReplacement(search_terms_data);
}

void DefaultSearchPolicyHandler::EnsureStringPrefExists(
    PrefValueMap* prefs,
    const std::string& path) {
  std::string value;
  if (!prefs->GetString(path, &value))
    prefs->SetString(path, value);
}

void DefaultSearchPolicyHandler::EnsureListPrefExists(
    PrefValueMap* prefs,
    const std::string& path) {
  base::Value* value;
  base::ListValue* list_value;
  if (!prefs->GetValue(path, &value) || !value->GetAsList(&list_value))
    prefs->SetValue(path, base::WrapUnique(new base::ListValue()));
}

}  // namespace policy
