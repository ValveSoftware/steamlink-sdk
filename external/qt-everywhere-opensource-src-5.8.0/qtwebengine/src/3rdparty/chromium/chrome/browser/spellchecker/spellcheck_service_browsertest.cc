// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

using content::BrowserContext;

namespace {

// A corrupted BDICT data used in DeleteCorruptedBDICT. Please do not use this
// BDICT data for other tests.
const uint8_t kCorruptedBDICT[] = {
    0x42, 0x44, 0x69, 0x63, 0x02, 0x00, 0x01, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x3b, 0x00, 0x00, 0x00, 0x65, 0x72, 0xe0, 0xac, 0x27, 0xc7, 0xda, 0x66,
    0x6d, 0x1e, 0xa6, 0x35, 0xd1, 0xf6, 0xb7, 0x35, 0x32, 0x00, 0x00, 0x00,
    0x38, 0x00, 0x00, 0x00, 0x39, 0x00, 0x00, 0x00, 0x3a, 0x00, 0x00, 0x00,
    0x0a, 0x0a, 0x41, 0x46, 0x20, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe6,
    0x49, 0x00, 0x68, 0x02, 0x73, 0x06, 0x74, 0x0b, 0x77, 0x11, 0x79, 0x15,
};

// Clears IPC messages before a preference change. Runs the runloop after the
// preference change.
class ScopedPreferenceChange {
 public:
  explicit ScopedPreferenceChange(IPC::TestSink* sink) {
    sink->ClearMessages();
  }

  ~ScopedPreferenceChange() {
    base::RunLoop().RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedPreferenceChange);
};

}  // namespace

class SpellcheckServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    renderer_.reset(new content::MockRenderProcessHost(GetContext()));
    prefs_ = user_prefs::UserPrefs::Get(GetContext());
  }

  void TearDownOnMainThread() override {
    prefs_ = nullptr;
    renderer_.reset();
  }

  BrowserContext* GetContext() {
    return static_cast<BrowserContext*>(browser()->profile());
  }

  PrefService* GetPrefs() {
    return prefs_;
  }

  void InitSpellcheck(bool enable_spellcheck,
                      const std::string& single_dictionary,
                      const std::string& multiple_dictionaries) {
    prefs_->SetBoolean(prefs::kEnableContinuousSpellcheck, enable_spellcheck);
    prefs_->SetString(prefs::kSpellCheckDictionary, single_dictionary);
    base::ListValue dictionaries_value;
    dictionaries_value.AppendStrings(
        base::SplitString(multiple_dictionaries, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY));
    prefs_->Set(prefs::kSpellCheckDictionaries, dictionaries_value);
    SpellcheckService* spellcheck =
        SpellcheckServiceFactory::GetForRenderProcessId(renderer_->GetID());
    ASSERT_NE(nullptr, spellcheck);
    spellcheck->InitForRenderer(renderer_.get());
  }

  void EnableSpellcheck(bool enable_spellcheck) {
    ScopedPreferenceChange scope(&renderer_->sink());
    prefs_->SetBoolean(prefs::kEnableContinuousSpellcheck, enable_spellcheck);
  }

  void SetSingleLanguageDictionary(const std::string& single_dictionary) {
    ScopedPreferenceChange scope(&renderer_->sink());
    prefs_->SetString(prefs::kSpellCheckDictionary, single_dictionary);
  }

  void SetMultiLingualDictionaries(const std::string& multiple_dictionaries) {
    ScopedPreferenceChange scope(&renderer_->sink());
    base::ListValue dictionaries_value;
    dictionaries_value.AppendStrings(
        base::SplitString(multiple_dictionaries, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY));
    prefs_->Set(prefs::kSpellCheckDictionaries, dictionaries_value);
  }

  std::string GetMultilingualDictionaries() {
    const base::ListValue* list_value =
        prefs_->GetList(prefs::kSpellCheckDictionaries);
    std::vector<std::string> dictionaries;
    for (const auto& item_value : *list_value) {
      std::string dictionary;
      EXPECT_TRUE(item_value->GetAsString(&dictionary));
      dictionaries.push_back(dictionary);
    }
    return base::JoinString(dictionaries, ",");
  }

  void SetAcceptLanguages(const std::string& accept_languages) {
    ScopedPreferenceChange scope(&renderer_->sink());
    prefs_->SetString(prefs::kAcceptLanguages, accept_languages);
  }

  // Returns the boolean parameter sent in the first
  // SpellCheckMsg_EnableSpellCheck message. For example, if spellcheck service
  // sent the SpellCheckMsg_EnableSpellCheck(true) message, then this method
  // returns true.
  bool GetFirstEnableSpellcheckMessageParam() {
    const IPC::Message* message = renderer_->sink().GetFirstMessageMatching(
        SpellCheckMsg_EnableSpellCheck::ID);
    EXPECT_NE(nullptr, message);
    if (!message)
      return false;

    SpellCheckMsg_EnableSpellCheck::Param param;
    bool ok = SpellCheckMsg_EnableSpellCheck::Read(message, &param);
    EXPECT_TRUE(ok);
    if (!ok)
      return false;

    return std::get<0>(param);
  }

 private:
  std::unique_ptr<content::MockRenderProcessHost> renderer_;

  // Not owned preferences service.
  PrefService* prefs_;
};

// Removing a spellcheck language from accept languages should remove it from
// spellcheck languages list as well.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       RemoveSpellcheckLanguageFromAcceptLanguages) {
  InitSpellcheck(true, "", "en-US,fr");
  SetAcceptLanguages("en-US,es,ru");
  EXPECT_EQ("en-US", GetMultilingualDictionaries());
}

// Keeping spellcheck languages in accept languages should not alter spellcheck
// languages list.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       KeepSpellcheckLanguagesInAcceptLanguages) {
  InitSpellcheck(true, "", "en-US,fr");
  SetAcceptLanguages("en-US,fr,es");
  EXPECT_EQ("en-US,fr", GetMultilingualDictionaries());
}

// Starting with spellcheck enabled should send the 'enable spellcheck' message
// to the renderer. Consequently disabling spellcheck should send the 'disable
// spellcheck' message to the renderer.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, StartWithSpellcheck) {
  InitSpellcheck(true, "", "en-US,fr");
  EXPECT_TRUE(GetFirstEnableSpellcheckMessageParam());

  EnableSpellcheck(false);
  EXPECT_FALSE(GetFirstEnableSpellcheckMessageParam());
}

// Starting with only a single-language spellcheck setting should send the
// 'enable spellcheck' message to the renderer. Consequently removing spellcheck
// languages should disable spellcheck.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       StartWithSingularLanguagePreference) {
  InitSpellcheck(true, "en-US", "");
  EXPECT_TRUE(GetFirstEnableSpellcheckMessageParam());

  SetMultiLingualDictionaries("");
  EXPECT_FALSE(GetFirstEnableSpellcheckMessageParam());
}

// Starting with a multi-language spellcheck setting should send the 'enable
// spellcheck' message to the renderer. Consequently removing spellcheck
// languages should disable spellcheck.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       StartWithMultiLanguagePreference) {
  InitSpellcheck(true, "", "en-US,fr");
  EXPECT_TRUE(GetFirstEnableSpellcheckMessageParam());

  SetMultiLingualDictionaries("");
  EXPECT_FALSE(GetFirstEnableSpellcheckMessageParam());
}

// Starting with both single-language and multi-language spellcheck settings
// should send the 'enable spellcheck' message to the renderer. Consequently
// removing spellcheck languages should disable spellcheck.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       StartWithBothLanguagePreferences) {
  InitSpellcheck(true, "en-US", "en-US,fr");
  EXPECT_TRUE(GetFirstEnableSpellcheckMessageParam());

  SetMultiLingualDictionaries("");
  EXPECT_FALSE(GetFirstEnableSpellcheckMessageParam());
}

// Flaky on Windows, see https://crbug.com/611029.
#if defined(OS_WIN)
#define MAYBE_StartWithoutLanguages DISABLED_StartWithoutLanguages
#else
#define MAYBE_StartWithoutLanguages StartWithoutLanguages
#endif
// Starting without spellcheck languages should send the 'disable spellcheck'
// message to the renderer. Consequently adding spellchecking languages should
// enable spellcheck.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       MAYBE_StartWithoutLanguages) {
  InitSpellcheck(true, "", "");
  EXPECT_FALSE(GetFirstEnableSpellcheckMessageParam());

  SetMultiLingualDictionaries("en-US");
  EXPECT_TRUE(GetFirstEnableSpellcheckMessageParam());
}

// Starting with spellcheck disabled should send the 'disable spellcheck'
// message to the renderer. Consequently enabling spellcheck should send the
// 'enable spellcheck' message to the renderer.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, StartWithoutSpellcheck) {
  InitSpellcheck(false, "", "en-US,fr");
  EXPECT_FALSE(GetFirstEnableSpellcheckMessageParam());

  EnableSpellcheck(true);
  EXPECT_TRUE(GetFirstEnableSpellcheckMessageParam());
}

// Tests that we can delete a corrupted BDICT file used by hunspell. We do not
// run this test on Mac because Mac does not use hunspell by default.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, DeleteCorruptedBDICT) {
  // Write the corrupted BDICT data to create a corrupted BDICT file.
  base::FilePath dict_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir));
  base::FilePath bdict_path =
      chrome::spellcheck_common::GetVersionedFileName("en-US", dict_dir);

  size_t actual = base::WriteFile(bdict_path,
      reinterpret_cast<const char*>(kCorruptedBDICT),
      arraysize(kCorruptedBDICT));
  EXPECT_EQ(arraysize(kCorruptedBDICT), actual);

  // Attach an event to the SpellcheckService object so we can receive its
  // status updates.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  SpellcheckService::AttachStatusEvent(&event);

  BrowserContext* context = GetContext();

  // Ensure that the SpellcheckService object does not already exist. Otherwise
  // the next line will not force creation of the SpellcheckService and the
  // test will fail.
  SpellcheckService* service = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->GetServiceForBrowserContext(
          context,
          false));
  ASSERT_EQ(NULL, service);

  // Getting the spellcheck_service will initialize the SpellcheckService
  // object with the corrupted BDICT file created above since the hunspell
  // dictionary is loaded in the SpellcheckService constructor right now.
  // The SpellCheckHost object will send a BDICT_CORRUPTED event.
  SpellcheckServiceFactory::GetForContext(context);

  // Check the received event. Also we check if Chrome has successfully deleted
  // the corrupted dictionary. We delete the corrupted dictionary to avoid
  // leaking it when this test fails.
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
  content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
  EXPECT_EQ(SpellcheckService::BDICT_CORRUPTED,
            SpellcheckService::GetStatusEvent());
  if (base::PathExists(bdict_path)) {
    ADD_FAILURE();
    EXPECT_TRUE(base::DeleteFile(bdict_path, true));
  }
}

// Checks that preferences migrate correctly.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, PreferencesMigrated) {
  GetPrefs()->Set(prefs::kSpellCheckDictionaries, base::ListValue());
  GetPrefs()->SetString(prefs::kSpellCheckDictionary, "en-US");

  // Create a SpellcheckService which will migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  // Make sure the preferences have been migrated.
  std::string new_pref;
  EXPECT_TRUE(GetPrefs()
                  ->GetList(prefs::kSpellCheckDictionaries)
                  ->GetString(0, &new_pref));
  EXPECT_EQ("en-US", new_pref);
  EXPECT_TRUE(GetPrefs()->GetString(prefs::kSpellCheckDictionary).empty());
}

// Checks that preferences are not migrated when they shouldn't be.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, PreferencesNotMigrated) {
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  GetPrefs()->Set(prefs::kSpellCheckDictionaries, dictionaries);
  GetPrefs()->SetString(prefs::kSpellCheckDictionary, "fr");

  // Create a SpellcheckService which will migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  // Make sure the preferences have not been migrated.
  std::string new_pref;
  EXPECT_TRUE(GetPrefs()
                  ->GetList(prefs::kSpellCheckDictionaries)
                  ->GetString(0, &new_pref));
  EXPECT_EQ("en-US", new_pref);
  EXPECT_TRUE(GetPrefs()->GetString(prefs::kSpellCheckDictionary).empty());
}

// Checks that, if a user has spellchecking disabled, nothing changes
// during migration.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       SpellcheckingDisabledPreferenceMigration) {
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  GetPrefs()->Set(prefs::kSpellCheckDictionaries, dictionaries);
  GetPrefs()->SetBoolean(prefs::kEnableContinuousSpellcheck, false);

  // Migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  EXPECT_FALSE(GetPrefs()->GetBoolean(prefs::kEnableContinuousSpellcheck));
  EXPECT_EQ(1U, GetPrefs()->GetList(prefs::kSpellCheckDictionaries)->GetSize());
}

// Make sure preferences get preserved and spellchecking stays enabled.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       MultilingualPreferenceNotMigrated) {
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  dictionaries.AppendString("fr");
  GetPrefs()->Set(prefs::kSpellCheckDictionaries, dictionaries);
  GetPrefs()->SetBoolean(prefs::kEnableContinuousSpellcheck, true);

  // Should not migrate any preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  EXPECT_TRUE(GetPrefs()->GetBoolean(prefs::kEnableContinuousSpellcheck));
  EXPECT_EQ(2U, GetPrefs()->GetList(prefs::kSpellCheckDictionaries)->GetSize());
  std::string pref;
  ASSERT_TRUE(
      GetPrefs()->GetList(prefs::kSpellCheckDictionaries)->GetString(0, &pref));
  EXPECT_EQ("en-US", pref);
  ASSERT_TRUE(
      GetPrefs()->GetList(prefs::kSpellCheckDictionaries)->GetString(1, &pref));
  EXPECT_EQ("fr", pref);
}
