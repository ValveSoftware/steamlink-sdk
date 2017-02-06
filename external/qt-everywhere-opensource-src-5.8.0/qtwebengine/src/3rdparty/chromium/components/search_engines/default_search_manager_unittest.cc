// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_manager.h"

#include <stddef.h>

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// A dictionary to hold all data related to the Default Search Engine.
// Eventually, this should replace all the data stored in the
// default_search_provider.* prefs.
const char kDefaultSearchProviderData[] =
    "default_search_provider_data.template_url_data";

// Checks that the two TemplateURLs are similar. Does not check the id, the
// date_created or the last_modified time.  Neither pointer should be NULL.
void ExpectSimilar(const TemplateURLData* expected,
                   const TemplateURLData* actual) {
  ASSERT_TRUE(expected != NULL);
  ASSERT_TRUE(actual != NULL);

  EXPECT_EQ(expected->short_name(), actual->short_name());
  EXPECT_EQ(expected->keyword(), actual->keyword());
  EXPECT_EQ(expected->url(), actual->url());
  EXPECT_EQ(expected->suggestions_url, actual->suggestions_url);
  EXPECT_EQ(expected->favicon_url, actual->favicon_url);
  EXPECT_EQ(expected->alternate_urls, actual->alternate_urls);
  EXPECT_EQ(expected->show_in_default_list, actual->show_in_default_list);
  EXPECT_EQ(expected->safe_for_autoreplace, actual->safe_for_autoreplace);
  EXPECT_EQ(expected->input_encodings, actual->input_encodings);
  EXPECT_EQ(expected->search_terms_replacement_key,
            actual->search_terms_replacement_key);
}

// TODO(caitkp): TemplateURLData-ify this.
void SetOverrides(user_prefs::TestingPrefServiceSyncable* prefs, bool update) {
  prefs->SetUserPref(prefs::kSearchProviderOverridesVersion,
                     new base::FundamentalValue(1));
  base::ListValue* overrides = new base::ListValue;
  std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);

  entry->SetString("name", update ? "new_foo" : "foo");
  entry->SetString("keyword", update ? "new_fook" : "fook");
  entry->SetString("search_url", "http://foo.com/s?q={searchTerms}");
  entry->SetString("favicon_url", "http://foi.com/favicon.ico");
  entry->SetString("encoding", "UTF-8");
  entry->SetInteger("id", 1001);
  entry->SetString("suggest_url", "http://foo.com/suggest?q={searchTerms}");
  entry->SetString("instant_url", "http://foo.com/instant?q={searchTerms}");
  base::ListValue* alternate_urls = new base::ListValue;
  alternate_urls->AppendString("http://foo.com/alternate?q={searchTerms}");
  entry->Set("alternate_urls", alternate_urls);
  entry->SetString("search_terms_replacement_key", "espv");
  overrides->Append(entry->DeepCopy());

  entry.reset(new base::DictionaryValue);
  entry->SetInteger("id", 1002);
  entry->SetString("name", update ? "new_bar" : "bar");
  entry->SetString("keyword", update ? "new_bark" : "bark");
  entry->SetString("encoding", std::string());
  overrides->Append(entry->DeepCopy());
  entry->SetInteger("id", 1003);
  entry->SetString("name", "baz");
  entry->SetString("keyword", "bazk");
  entry->SetString("encoding", "UTF-8");
  overrides->Append(entry->DeepCopy());
  prefs->SetUserPref(prefs::kSearchProviderOverrides, overrides);
}

void SetPolicy(user_prefs::TestingPrefServiceSyncable* prefs,
               bool enabled,
               TemplateURLData* data) {
  if (enabled) {
    EXPECT_FALSE(data->keyword().empty());
    EXPECT_FALSE(data->url().empty());
  }
  std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
  entry->SetString(DefaultSearchManager::kShortName, data->short_name());
  entry->SetString(DefaultSearchManager::kKeyword, data->keyword());
  entry->SetString(DefaultSearchManager::kURL, data->url());
  entry->SetString(DefaultSearchManager::kFaviconURL, data->favicon_url.spec());
  entry->SetString(DefaultSearchManager::kSuggestionsURL,
                   data->suggestions_url);
  entry->SetBoolean(DefaultSearchManager::kSafeForAutoReplace,
                    data->safe_for_autoreplace);
  std::unique_ptr<base::ListValue> alternate_urls(new base::ListValue);
  for (std::vector<std::string>::const_iterator it =
           data->alternate_urls.begin();
       it != data->alternate_urls.end();
       ++it) {
    alternate_urls->AppendString(*it);
  }
  entry->Set(DefaultSearchManager::kAlternateURLs, alternate_urls.release());

  std::unique_ptr<base::ListValue> encodings(new base::ListValue);
  for (std::vector<std::string>::const_iterator it =
           data->input_encodings.begin();
       it != data->input_encodings.end();
       ++it) {
    encodings->AppendString(*it);
  }
  entry->Set(DefaultSearchManager::kInputEncodings, encodings.release());

  entry->SetString(DefaultSearchManager::kSearchTermsReplacementKey,
                   data->search_terms_replacement_key);
  entry->SetBoolean(DefaultSearchManager::kDisabledByPolicy, !enabled);
  prefs->SetManagedPref(kDefaultSearchProviderData, entry.release());
}

std::unique_ptr<TemplateURLData> GenerateDummyTemplateURLData(
    const std::string& type) {
  std::unique_ptr<TemplateURLData> data(new TemplateURLData());
  data->SetShortName(base::UTF8ToUTF16(std::string(type).append("name")));
  data->SetKeyword(base::UTF8ToUTF16(std::string(type).append("key")));
  data->SetURL(std::string("http://").append(type).append("foo/{searchTerms}"));
  data->suggestions_url = std::string("http://").append(type).append("sugg");
  data->alternate_urls.push_back(
      std::string("http://").append(type).append("foo/alt"));
  data->favicon_url = GURL("http://icon1");
  data->safe_for_autoreplace = true;
  data->show_in_default_list = true;
  data->input_encodings = base::SplitString(
      "UTF-8;UTF-16", ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  data->date_created = base::Time();
  data->last_modified = base::Time();
  return data;
}

}  // namespace

class DefaultSearchManagerTest : public testing::Test {
 public:
  DefaultSearchManagerTest() {};

  void SetUp() override {
    pref_service_.reset(new user_prefs::TestingPrefServiceSyncable);
    DefaultSearchManager::RegisterProfilePrefs(pref_service_->registry());
    TemplateURLPrepopulateData::RegisterProfilePrefs(pref_service_->registry());
  }

  user_prefs::TestingPrefServiceSyncable* pref_service() {
    return pref_service_.get();
  }

 private:
  std::unique_ptr<user_prefs::TestingPrefServiceSyncable> pref_service_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchManagerTest);
};

// Test that a TemplateURLData object is properly written and read from Prefs.
TEST_F(DefaultSearchManagerTest, ReadAndWritePref) {
  DefaultSearchManager manager(pref_service(),
                               DefaultSearchManager::ObserverCallback());
  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16("name1"));
  data.SetKeyword(base::UTF8ToUTF16("key1"));
  data.SetURL("http://foo1/{searchTerms}");
  data.suggestions_url = "http://sugg1";
  data.alternate_urls.push_back("http://foo1/alt");
  data.favicon_url = GURL("http://icon1");
  data.safe_for_autoreplace = true;
  data.show_in_default_list = true;
  data.input_encodings = base::SplitString(
      "UTF-8;UTF-16", ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  data.date_created = base::Time();
  data.last_modified = base::Time();

  manager.SetUserSelectedDefaultSearchEngine(data);
  TemplateURLData* read_data = manager.GetDefaultSearchEngine(NULL);
  ExpectSimilar(&data, read_data);
}

// Test DefaultSearchmanager handles user-selected DSEs correctly.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByUserPref) {
  size_t default_search_index = 0;
  DefaultSearchManager manager(pref_service(),
                               DefaultSearchManager::ObserverCallback());
  ScopedVector<TemplateURLData> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(pref_service(),
                                                         &default_search_index);
  DefaultSearchManager::Source source = DefaultSearchManager::FROM_POLICY;
  // If no user pref is set, we should use the pre-populated values.
  ExpectSimilar(prepopulated_urls[default_search_index],
                manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);

  // Setting a user pref overrides the pre-populated values.
  std::unique_ptr<TemplateURLData> data = GenerateDummyTemplateURLData("user");
  manager.SetUserSelectedDefaultSearchEngine(*data.get());

  ExpectSimilar(data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Updating the user pref (externally to this instance of
  // DefaultSearchManager) triggers an update.
  std::unique_ptr<TemplateURLData> new_data =
      GenerateDummyTemplateURLData("user2");
  DefaultSearchManager other_manager(pref_service(),
                                     DefaultSearchManager::ObserverCallback());
  other_manager.SetUserSelectedDefaultSearchEngine(*new_data.get());

  ExpectSimilar(new_data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Clearing the user pref should cause the default search to revert to the
  // prepopulated vlaues.
  manager.ClearUserSelectedDefaultSearchEngine();
  ExpectSimilar(prepopulated_urls[default_search_index],
                manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);
}

// Test that DefaultSearch manager detects changes to kSearchProviderOverrides.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByOverrides) {
  SetOverrides(pref_service(), false);
  size_t default_search_index = 0;
  DefaultSearchManager manager(pref_service(),
                               DefaultSearchManager::ObserverCallback());
  ScopedVector<TemplateURLData> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(pref_service(),
                                                         &default_search_index);

  DefaultSearchManager::Source source = DefaultSearchManager::FROM_POLICY;
  TemplateURLData first_default(*manager.GetDefaultSearchEngine(&source));
  ExpectSimilar(prepopulated_urls[default_search_index], &first_default);
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);

  // Update the overrides:
  SetOverrides(pref_service(), true);
  prepopulated_urls = TemplateURLPrepopulateData::GetPrepopulatedEngines(
      pref_service(), &default_search_index);

  // Make sure DefaultSearchManager updated:
  ExpectSimilar(prepopulated_urls[default_search_index],
                manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);
  EXPECT_NE(manager.GetDefaultSearchEngine(NULL)->short_name(),
            first_default.short_name());
  EXPECT_NE(manager.GetDefaultSearchEngine(NULL)->keyword(),
            first_default.keyword());
}

// Test DefaultSearchManager handles policy-enforced DSEs correctly.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByPolicy) {
  DefaultSearchManager manager(pref_service(),
                               DefaultSearchManager::ObserverCallback());
  std::unique_ptr<TemplateURLData> data = GenerateDummyTemplateURLData("user");
  manager.SetUserSelectedDefaultSearchEngine(*data.get());

  DefaultSearchManager::Source source = DefaultSearchManager::FROM_FALLBACK;
  ExpectSimilar(data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  std::unique_ptr<TemplateURLData> policy_data =
      GenerateDummyTemplateURLData("policy");
  SetPolicy(pref_service(), true, policy_data.get());

  ExpectSimilar(policy_data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_POLICY, source);

  TemplateURLData null_policy_data;
  SetPolicy(pref_service(), false, &null_policy_data);
  EXPECT_EQ(NULL, manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_POLICY, source);

  pref_service()->RemoveManagedPref(kDefaultSearchProviderData);
  ExpectSimilar(data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
}

// Test DefaultSearchManager handles extension-controlled DSEs correctly.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByExtension) {
  DefaultSearchManager manager(pref_service(),
                               DefaultSearchManager::ObserverCallback());
  std::unique_ptr<TemplateURLData> data = GenerateDummyTemplateURLData("user");
  manager.SetUserSelectedDefaultSearchEngine(*data);

  DefaultSearchManager::Source source = DefaultSearchManager::FROM_FALLBACK;
  ExpectSimilar(data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Extension trumps prefs:
  std::unique_ptr<TemplateURLData> extension_data_1 =
      GenerateDummyTemplateURLData("ext1");
  manager.SetExtensionControlledDefaultSearchEngine(*extension_data_1);

  ExpectSimilar(extension_data_1.get(),
                manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_EXTENSION, source);

  // Policy trumps extension:
  std::unique_ptr<TemplateURLData> policy_data =
      GenerateDummyTemplateURLData("policy");
  SetPolicy(pref_service(), true, policy_data.get());

  ExpectSimilar(policy_data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_POLICY, source);
  pref_service()->RemoveManagedPref(kDefaultSearchProviderData);

  // Extensions trump each other:
  std::unique_ptr<TemplateURLData> extension_data_2 =
      GenerateDummyTemplateURLData("ext2");
  std::unique_ptr<TemplateURLData> extension_data_3 =
      GenerateDummyTemplateURLData("ext3");
  manager.SetExtensionControlledDefaultSearchEngine(*extension_data_2);
  manager.SetExtensionControlledDefaultSearchEngine(*extension_data_3);

  ExpectSimilar(extension_data_3.get(),
                manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_EXTENSION, source);

  manager.ClearExtensionControlledDefaultSearchEngine();

  ExpectSimilar(data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
}
