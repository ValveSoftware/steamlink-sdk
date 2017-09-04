// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_pref_test_util.h"

#include "base/strings/string_split.h"
#include "components/search_engines/default_search_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

// static
std::unique_ptr<base::DictionaryValue>
DefaultSearchPrefTestUtil::CreateDefaultSearchPreferenceValue(
    bool enabled,
    const std::string& name,
    const std::string& keyword,
    const std::string& search_url,
    const std::string& suggest_url,
    const std::string& icon_url,
    const std::string& encodings,
    const std::string& alternate_url,
    const std::string& search_terms_replacement_key) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  if (!enabled) {
    value->SetBoolean(DefaultSearchManager::kDisabledByPolicy, true);
    return value;
  }

  EXPECT_FALSE(keyword.empty());
  EXPECT_FALSE(search_url.empty());
  value->Set(DefaultSearchManager::kShortName,
             new base::StringValue(name));
  value->Set(DefaultSearchManager::kKeyword,
             new base::StringValue(keyword));
  value->Set(DefaultSearchManager::kURL,
             new base::StringValue(search_url));
  value->Set(DefaultSearchManager::kSuggestionsURL,
             new base::StringValue(suggest_url));
  value->Set(DefaultSearchManager::kFaviconURL,
             new base::StringValue(icon_url));
  value->Set(DefaultSearchManager::kSearchTermsReplacementKey,
             new base::StringValue(search_terms_replacement_key));

  std::unique_ptr<base::ListValue> encodings_list(new base::ListValue);
  for (const std::string& term : base::SplitString(
           encodings, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL))
    encodings_list->AppendString(term);
  value->Set(DefaultSearchManager::kInputEncodings, encodings_list.release());

  std::unique_ptr<base::ListValue> alternate_url_list(new base::ListValue());
  if (!alternate_url.empty())
    alternate_url_list->AppendString(alternate_url);
  value->Set(DefaultSearchManager::kAlternateURLs,
             alternate_url_list.release());
  return value;
}
