// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_pref_migration.h"

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class DefaultSearchPrefMigrationTest : public testing::Test {
 public:
  DefaultSearchPrefMigrationTest();

  void SaveDefaultSearchProviderToLegacyPrefs(const TemplateURL* t_url);

  std::unique_ptr<TemplateURL> CreateKeyword(const std::string& short_name,
                                             const std::string& keyword,
                                             const std::string& url);

  DefaultSearchManager* default_search_manager() {
    return default_search_manager_.get();
  }

  PrefService* pref_service() { return &prefs_; }

 private:
  user_prefs::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<DefaultSearchManager> default_search_manager_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchPrefMigrationTest);
};

DefaultSearchPrefMigrationTest::DefaultSearchPrefMigrationTest() {
  DefaultSearchManager::RegisterProfilePrefs(prefs_.registry());
  TemplateURLPrepopulateData::RegisterProfilePrefs(prefs_.registry());
  TemplateURLService::RegisterProfilePrefs(prefs_.registry());
  default_search_manager_.reset(new DefaultSearchManager(
      pref_service(), DefaultSearchManager::ObserverCallback()));
}

void DefaultSearchPrefMigrationTest::SaveDefaultSearchProviderToLegacyPrefs(
    const TemplateURL* t_url) {
  PrefService* prefs = pref_service();

  bool enabled = false;
  std::string search_url;
  std::string suggest_url;
  std::string instant_url;
  std::string image_url;
  std::string new_tab_url;
  std::string search_url_post_params;
  std::string suggest_url_post_params;
  std::string instant_url_post_params;
  std::string image_url_post_params;
  std::string icon_url;
  std::string encodings;
  std::string short_name;
  std::string keyword;
  std::string id_string;
  std::string prepopulate_id;
  base::ListValue alternate_urls;
  std::string search_terms_replacement_key;
  if (t_url) {
    DCHECK_EQ(TemplateURL::NORMAL, t_url->GetType());
    enabled = true;
    search_url = t_url->url();
    suggest_url = t_url->suggestions_url();
    instant_url = t_url->instant_url();
    image_url = t_url->image_url();
    new_tab_url = t_url->new_tab_url();
    search_url_post_params = t_url->search_url_post_params();
    suggest_url_post_params = t_url->suggestions_url_post_params();
    instant_url_post_params = t_url->instant_url_post_params();
    image_url_post_params = t_url->image_url_post_params();
    GURL icon_gurl = t_url->favicon_url();
    if (!icon_gurl.is_empty())
      icon_url = icon_gurl.spec();
    encodings = base::JoinString(t_url->input_encodings(), ";");
    short_name = base::UTF16ToUTF8(t_url->short_name());
    keyword = base::UTF16ToUTF8(t_url->keyword());
    id_string = base::Int64ToString(t_url->id());
    prepopulate_id = base::Int64ToString(t_url->prepopulate_id());
    for (size_t i = 0; i < t_url->alternate_urls().size(); ++i)
      alternate_urls.AppendString(t_url->alternate_urls()[i]);
    search_terms_replacement_key = t_url->search_terms_replacement_key();
  }
  prefs->SetBoolean(prefs::kDefaultSearchProviderEnabled, enabled);
  prefs->SetString(prefs::kDefaultSearchProviderSearchURL, search_url);
  prefs->SetString(prefs::kDefaultSearchProviderSuggestURL, suggest_url);
  prefs->SetString(prefs::kDefaultSearchProviderInstantURL, instant_url);
  prefs->SetString(prefs::kDefaultSearchProviderImageURL, image_url);
  prefs->SetString(prefs::kDefaultSearchProviderNewTabURL, new_tab_url);
  prefs->SetString(prefs::kDefaultSearchProviderSearchURLPostParams,
                   search_url_post_params);
  prefs->SetString(prefs::kDefaultSearchProviderSuggestURLPostParams,
                   suggest_url_post_params);
  prefs->SetString(prefs::kDefaultSearchProviderInstantURLPostParams,
                   instant_url_post_params);
  prefs->SetString(prefs::kDefaultSearchProviderImageURLPostParams,
                   image_url_post_params);
  prefs->SetString(prefs::kDefaultSearchProviderIconURL, icon_url);
  prefs->SetString(prefs::kDefaultSearchProviderEncodings, encodings);
  prefs->SetString(prefs::kDefaultSearchProviderName, short_name);
  prefs->SetString(prefs::kDefaultSearchProviderKeyword, keyword);
  prefs->SetString(prefs::kDefaultSearchProviderID, id_string);
  prefs->SetString(prefs::kDefaultSearchProviderPrepopulateID, prepopulate_id);
  prefs->Set(prefs::kDefaultSearchProviderAlternateURLs, alternate_urls);
  prefs->SetString(prefs::kDefaultSearchProviderSearchTermsReplacementKey,
      search_terms_replacement_key);
}

std::unique_ptr<TemplateURL> DefaultSearchPrefMigrationTest::CreateKeyword(
    const std::string& short_name,
    const std::string& keyword,
    const std::string& url) {
  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(short_name));
  data.SetKeyword(base::ASCIIToUTF16(keyword));
  data.SetURL(url);
  std::unique_ptr<TemplateURL> t_url(new TemplateURL(data));
  return t_url;
}

TEST_F(DefaultSearchPrefMigrationTest, MigrateUserSelectedValue) {
  std::unique_ptr<TemplateURL> t_url(
      CreateKeyword("name1", "key1", "http://foo1/{searchTerms}"));
  // Store a value in the legacy location.
  SaveDefaultSearchProviderToLegacyPrefs(t_url.get());

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(pref_service());

  // Test that it was migrated.
  DefaultSearchManager::Source source;
  const TemplateURLData* modern_default =
      default_search_manager()->GetDefaultSearchEngine(&source);
  ASSERT_TRUE(modern_default);
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
  EXPECT_EQ(t_url->short_name(), modern_default->short_name());
  EXPECT_EQ(t_url->keyword(), modern_default->keyword());
  EXPECT_EQ(t_url->url(), modern_default->url());
}

TEST_F(DefaultSearchPrefMigrationTest, MigrateOnlyOnce) {
  std::unique_ptr<TemplateURL> t_url(
      CreateKeyword("name1", "key1", "http://foo1/{searchTerms}"));
  // Store a value in the legacy location.
  SaveDefaultSearchProviderToLegacyPrefs(t_url.get());

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(pref_service());

  // Test that it was migrated.
  DefaultSearchManager::Source source;
  const TemplateURLData* modern_default =
      default_search_manager()->GetDefaultSearchEngine(&source);
  ASSERT_TRUE(modern_default);
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
  EXPECT_EQ(t_url->short_name(), modern_default->short_name());
  EXPECT_EQ(t_url->keyword(), modern_default->keyword());
  EXPECT_EQ(t_url->url(), modern_default->url());
  default_search_manager()->ClearUserSelectedDefaultSearchEngine();

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(pref_service());

  // Test that it was NOT migrated.
  modern_default = default_search_manager()->GetDefaultSearchEngine(&source);
  ASSERT_TRUE(modern_default);
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);
}

TEST_F(DefaultSearchPrefMigrationTest, ModernValuePresent) {
  std::unique_ptr<TemplateURL> t_url(
      CreateKeyword("name1", "key1", "http://foo1/{searchTerms}"));
  std::unique_ptr<TemplateURL> t_url2(
      CreateKeyword("name2", "key2", "http://foo2/{searchTerms}"));
  // Store a value in the legacy location.
  SaveDefaultSearchProviderToLegacyPrefs(t_url.get());

  // Store another value in the modern location.
  default_search_manager()->SetUserSelectedDefaultSearchEngine(t_url2->data());

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(pref_service());

  // Test that no migration occurred. The modern value is left intact.
  DefaultSearchManager::Source source;
  const TemplateURLData* modern_default =
      default_search_manager()->GetDefaultSearchEngine(&source);
  ASSERT_TRUE(modern_default);
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
  EXPECT_EQ(t_url2->short_name(), modern_default->short_name());
  EXPECT_EQ(t_url2->keyword(), modern_default->keyword());
  EXPECT_EQ(t_url2->url(), modern_default->url());
}

TEST_F(DefaultSearchPrefMigrationTest,
       AutomaticallySelectedValueIsNotMigrated) {
  DefaultSearchManager::Source source;
  TemplateURLData prepopulated_default(
      *default_search_manager()->GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);

  TemplateURL prepopulated_turl(prepopulated_default);

  // Store a value in the legacy location.
  SaveDefaultSearchProviderToLegacyPrefs(&prepopulated_turl);

  // Run the migration.
  ConfigureDefaultSearchPrefMigrationToDictionaryValue(pref_service());

  // Test that the legacy value is not migrated, as it is not user-selected.
  default_search_manager()->GetDefaultSearchEngine(&source);
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);
}
