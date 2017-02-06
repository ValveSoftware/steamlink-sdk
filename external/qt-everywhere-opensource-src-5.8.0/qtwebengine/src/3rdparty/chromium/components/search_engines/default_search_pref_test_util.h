// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_PREF_TEST_UTIL_H_
#define COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_PREF_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "components/search_engines/default_search_manager.h"

class DefaultSearchPrefTestUtil {
 public:
  // Creates a DictionaryValue which can be used as a
  // kDefaultSearchProviderDataPrefName preference value.
  static std::unique_ptr<base::DictionaryValue>
  CreateDefaultSearchPreferenceValue(
      bool enabled,
      const std::string& name,
      const std::string& keyword,
      const std::string& search_url,
      const std::string& suggest_url,
      const std::string& icon_url,
      const std::string& encodings,
      const std::string& alternate_url,
      const std::string& search_terms_replacement_key);

  // Set the managed preferences for the default search provider and trigger
  // notification. If |alternate_url| is empty, uses an empty list of alternate
  // URLs, otherwise use a list containing a single entry.
  template<typename TestingPrefService>
  static void SetManagedPref(TestingPrefService* pref_service,
                             bool enabled,
                             const std::string& name,
                             const std::string& keyword,
                             const std::string& search_url,
                             const std::string& suggest_url,
                             const std::string& icon_url,
                             const std::string& encodings,
                             const std::string& alternate_url,
                             const std::string& search_terms_replacement_key) {
    pref_service->SetManagedPref(
        DefaultSearchManager::kDefaultSearchProviderDataPrefName,
        CreateDefaultSearchPreferenceValue(
            enabled, name, keyword, search_url, suggest_url, icon_url,
            encodings, alternate_url, search_terms_replacement_key).release());
  }

  // Remove all the managed preferences for the default search provider and
  // trigger notification.
  template<typename TestingPrefService>
  static void RemoveManagedPref(TestingPrefService* pref_service) {
    pref_service->RemoveManagedPref(
        DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  }
};

#endif  // COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_PREF_TEST_UTIL_H_
