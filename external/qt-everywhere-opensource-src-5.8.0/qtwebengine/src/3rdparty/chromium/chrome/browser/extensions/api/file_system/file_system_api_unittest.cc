// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/file_system_api.h"

#include <stddef.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if defined(OS_CHROMEOS)
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/test_util.h"
#include "extensions/common/value_builder.h"
#endif

using extensions::FileSystemChooseEntryFunction;
using extensions::api::file_system::AcceptOption;

#if defined(OS_CHROMEOS)
using extensions::file_system_api::ConsentProvider;
using file_manager::Volume;
#endif

namespace extensions {
namespace {

void CheckExtensions(const std::vector<base::FilePath::StringType>& expected,
                     const std::vector<base::FilePath::StringType>& actual) {
  EXPECT_EQ(expected.size(), actual.size());
  if (expected.size() != actual.size())
    return;

  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

AcceptOption BuildAcceptOption(const std::string& description,
                               const std::string& mime_types,
                               const std::string& extensions) {
  AcceptOption option;

  if (!description.empty())
    option.description.reset(new std::string(description));

  if (!mime_types.empty()) {
    option.mime_types.reset(new std::vector<std::string>(base::SplitString(
        mime_types, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)));
  }

  if (!extensions.empty()) {
    option.extensions.reset(new std::vector<std::string>(base::SplitString(
        extensions, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)));
  }

  return option;
}

#if defined(OS_WIN)
#define ToStringType base::UTF8ToWide
#else
#define ToStringType
#endif

#if defined(OS_CHROMEOS)
class TestingConsentProviderDelegate
    : public ConsentProvider::DelegateInterface {
 public:
  TestingConsentProviderDelegate()
      : show_dialog_counter_(0),
        show_notification_counter_(0),
        dialog_button_(ui::DIALOG_BUTTON_NONE),
        is_auto_launched_(false) {}

  ~TestingConsentProviderDelegate() {}

  // Sets a fake dialog response.
  void SetDialogButton(ui::DialogButton button) { dialog_button_ = button; }

  // Sets a fake result of detection the auto launch kiosk mode.
  void SetIsAutoLaunched(bool is_auto_launched) {
    is_auto_launched_ = is_auto_launched;
  }

  // Sets a whitelisted components list with a single id.
  void SetComponentWhitelist(const std::string& extension_id) {
    whitelisted_component_id_ = extension_id;
  }

  int show_dialog_counter() const { return show_dialog_counter_; }
  int show_notification_counter() const { return show_notification_counter_; }

 private:
  // ConsentProvider::DelegateInterface overrides:
  void ShowDialog(
      const extensions::Extension& extension,
      const base::WeakPtr<Volume>& volume,
      bool writable,
      const ConsentProvider::ShowDialogCallback& callback) override {
    ++show_dialog_counter_;
    callback.Run(dialog_button_);
  }

  void ShowNotification(const extensions::Extension& extension,
                        const base::WeakPtr<Volume>& volume,
                        bool writable) override {
    ++show_notification_counter_;
  }

  bool IsAutoLaunched(const extensions::Extension& extension) override {
    return is_auto_launched_;
  }

  bool IsWhitelistedComponent(const extensions::Extension& extension) override {
    return whitelisted_component_id_.compare(extension.id()) == 0;
  }

  int show_dialog_counter_;
  int show_notification_counter_;
  ui::DialogButton dialog_button_;
  bool is_auto_launched_;
  std::string whitelisted_component_id_;

  DISALLOW_COPY_AND_ASSIGN(TestingConsentProviderDelegate);
};

// Rewrites result of a consent request from |result| to |log|.
void OnConsentReceived(ConsentProvider::Consent* log,
                       const ConsentProvider::Consent result) {
  *log = result;
}
#endif

}  // namespace

#if defined(OS_CHROMEOS)
class FileSystemApiConsentProviderTest : public testing::Test {
 public:
  FileSystemApiConsentProviderTest() {}

  void SetUp() override {
    testing_pref_service_.reset(new TestingPrefServiceSimple);
    TestingBrowserProcess::GetGlobal()->SetLocalState(
        testing_pref_service_.get());
    user_manager_ = new chromeos::FakeChromeUserManager;
    scoped_user_manager_enabler_.reset(
        new chromeos::ScopedUserManagerEnabler(user_manager_));
  }

  void TearDown() override {
    scoped_user_manager_enabler_.reset();
    user_manager_ = nullptr;
    testing_pref_service_.reset();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

 protected:
  base::WeakPtr<Volume> volume_;
  std::unique_ptr<TestingPrefServiceSimple> testing_pref_service_;
  chromeos::FakeChromeUserManager*
      user_manager_;  // Owned by the scope enabler.
  std::unique_ptr<chromeos::ScopedUserManagerEnabler>
      scoped_user_manager_enabler_;
  content::TestBrowserThreadBundle thread_bundle_;
};
#endif

TEST(FileSystemApiUnitTest, FileSystemChooseEntryFunctionFileTypeInfoTest) {
  // AcceptsAllTypes is ignored when no other extensions are available.
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  bool acceptsAllTypes = false;
  FileSystemChooseEntryFunction::BuildFileTypeInfo(&file_type_info,
      base::FilePath::StringType(), NULL, &acceptsAllTypes);
  EXPECT_TRUE(file_type_info.include_all_files);
  EXPECT_TRUE(file_type_info.extensions.empty());

  // Test grouping of multiple types.
  file_type_info = ui::SelectFileDialog::FileTypeInfo();
  std::vector<AcceptOption> options;
  options.push_back(BuildAcceptOption(std::string(),
                                      "application/x-chrome-extension", "jso"));
  acceptsAllTypes = false;
  FileSystemChooseEntryFunction::BuildFileTypeInfo(&file_type_info,
      base::FilePath::StringType(), &options, &acceptsAllTypes);
  EXPECT_FALSE(file_type_info.include_all_files);
  ASSERT_EQ(file_type_info.extensions.size(), (size_t) 1);
  EXPECT_TRUE(file_type_info.extension_description_overrides[0].empty()) <<
      "No override must be specified for boring accept types";
  // Note here (and below) that the expectedTypes are sorted, because we use a
  // set internally to generate the output: thus, the output is sorted.
  std::vector<base::FilePath::StringType> expectedTypes;
  expectedTypes.push_back(ToStringType("crx"));
  expectedTypes.push_back(ToStringType("jso"));
  CheckExtensions(expectedTypes, file_type_info.extensions[0]);

  // Test that not satisfying the extension will force all types.
  file_type_info = ui::SelectFileDialog::FileTypeInfo();
  options.clear();
  options.push_back(
      BuildAcceptOption(std::string(), std::string(), "unrelated"));
  acceptsAllTypes = false;
  FileSystemChooseEntryFunction::BuildFileTypeInfo(&file_type_info,
      ToStringType(".jso"), &options, &acceptsAllTypes);
  EXPECT_TRUE(file_type_info.include_all_files);

  // Test multiple list entries, all containing their own types.
  file_type_info = ui::SelectFileDialog::FileTypeInfo();
  options.clear();
  options.push_back(BuildAcceptOption(std::string(), std::string(), "jso,js"));
  options.push_back(BuildAcceptOption(std::string(), std::string(), "cpp,cc"));
  acceptsAllTypes = false;
  FileSystemChooseEntryFunction::BuildFileTypeInfo(&file_type_info,
      base::FilePath::StringType(), &options, &acceptsAllTypes);
  ASSERT_EQ(file_type_info.extensions.size(), options.size());

  expectedTypes.clear();
  expectedTypes.push_back(ToStringType("js"));
  expectedTypes.push_back(ToStringType("jso"));
  CheckExtensions(expectedTypes, file_type_info.extensions[0]);

  expectedTypes.clear();
  expectedTypes.push_back(ToStringType("cc"));
  expectedTypes.push_back(ToStringType("cpp"));
  CheckExtensions(expectedTypes, file_type_info.extensions[1]);

  // Test accept type that causes description override.
  file_type_info = ui::SelectFileDialog::FileTypeInfo();
  options.clear();
  options.push_back(BuildAcceptOption(std::string(), "image/*", "html"));
  acceptsAllTypes = false;
  FileSystemChooseEntryFunction::BuildFileTypeInfo(&file_type_info,
      base::FilePath::StringType(), &options, &acceptsAllTypes);
  ASSERT_EQ(file_type_info.extension_description_overrides.size(), (size_t) 1);
  EXPECT_FALSE(file_type_info.extension_description_overrides[0].empty()) <<
      "Accept type \"image/*\" must generate description override";

  // Test multiple accept types that cause description override causes us to
  // still present the default.
  file_type_info = ui::SelectFileDialog::FileTypeInfo();
  options.clear();
  options.push_back(BuildAcceptOption(std::string(), "image/*,audio/*,video/*",
                                      std::string()));
  acceptsAllTypes = false;
  FileSystemChooseEntryFunction::BuildFileTypeInfo(&file_type_info,
      base::FilePath::StringType(), &options, &acceptsAllTypes);
  ASSERT_EQ(file_type_info.extension_description_overrides.size(), (size_t) 1);
  EXPECT_TRUE(file_type_info.extension_description_overrides[0].empty());

  // Test explicit description override.
  file_type_info = ui::SelectFileDialog::FileTypeInfo();
  options.clear();
  options.push_back(
      BuildAcceptOption("File Types 101", "image/jpeg", std::string()));
  acceptsAllTypes = false;
  FileSystemChooseEntryFunction::BuildFileTypeInfo(&file_type_info,
      base::FilePath::StringType(), &options, &acceptsAllTypes);
  EXPECT_EQ(file_type_info.extension_description_overrides[0],
      base::UTF8ToUTF16("File Types 101"));
}

TEST(FileSystemApiUnitTest, FileSystemChooseEntryFunctionSuggestionTest) {
  std::string opt_name;
  base::FilePath suggested_name;
  base::FilePath::StringType suggested_extension;

  opt_name = std::string("normal_path.txt");
  FileSystemChooseEntryFunction::BuildSuggestion(&opt_name, &suggested_name,
      &suggested_extension);
  EXPECT_FALSE(suggested_name.IsAbsolute());
  EXPECT_EQ(suggested_name.MaybeAsASCII(), "normal_path.txt");
  EXPECT_EQ(suggested_extension, ToStringType("txt"));

  // We should provide just the basename, i.e., "path".
  opt_name = std::string("/a/bad/path");
  FileSystemChooseEntryFunction::BuildSuggestion(&opt_name, &suggested_name,
      &suggested_extension);
  EXPECT_FALSE(suggested_name.IsAbsolute());
  EXPECT_EQ(suggested_name.MaybeAsASCII(), "path");
  EXPECT_TRUE(suggested_extension.empty());

#if !defined(OS_WIN)
  // TODO(thorogood): Fix this test on Windows.
  // Filter out absolute paths with no basename.
  opt_name = std::string("/");
  FileSystemChooseEntryFunction::BuildSuggestion(&opt_name, &suggested_name,
      &suggested_extension);
  EXPECT_FALSE(suggested_name.IsAbsolute());
  EXPECT_TRUE(suggested_name.MaybeAsASCII().empty());
  EXPECT_TRUE(suggested_extension.empty());
#endif
}

#if defined(OS_CHROMEOS)
TEST_F(FileSystemApiConsentProviderTest, ForNonKioskApps) {
  // Component apps are not granted unless they are whitelisted.
  {
    scoped_refptr<Extension> component_extension(
        test_util::BuildApp(
            std::move(ExtensionBuilder().SetLocation(Manifest::COMPONENT)))
            .Build());
    TestingConsentProviderDelegate delegate;
    ConsentProvider provider(&delegate);
    EXPECT_FALSE(provider.IsGrantable(*component_extension));
  }

  // Whitelitsed component apps are instantly granted access without asking
  // user.
  {
    scoped_refptr<Extension> whitelisted_component_extension(
        test_util::BuildApp(
            std::move(ExtensionBuilder().SetLocation(Manifest::COMPONENT)))
            .Build());
    TestingConsentProviderDelegate delegate;
    delegate.SetComponentWhitelist(whitelisted_component_extension->id());
    ConsentProvider provider(&delegate);
    EXPECT_TRUE(provider.IsGrantable(*whitelisted_component_extension));

    ConsentProvider::Consent result = ConsentProvider::CONSENT_IMPOSSIBLE;
    provider.RequestConsent(*whitelisted_component_extension.get(), volume_,
                            true /* writable */,
                            base::Bind(&OnConsentReceived, &result));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(0, delegate.show_dialog_counter());
    EXPECT_EQ(0, delegate.show_notification_counter());
    EXPECT_EQ(ConsentProvider::CONSENT_GRANTED, result);
  }

  // Non-component apps in non-kiosk mode will be rejected instantly, without
  // asking for user consent.
  {
    scoped_refptr<Extension> non_component_extension(
        test_util::CreateEmptyExtension());
    TestingConsentProviderDelegate delegate;
    ConsentProvider provider(&delegate);
    EXPECT_FALSE(provider.IsGrantable(*non_component_extension));
  }
}

TEST_F(FileSystemApiConsentProviderTest, ForKioskApps) {
  // Non-component apps in auto-launch kiosk mode will be granted access
  // instantly without asking for user consent, but with a notification.
  {
    scoped_refptr<Extension> auto_launch_kiosk_app(
        test_util::BuildApp(ExtensionBuilder())
            .MergeManifest(DictionaryBuilder()
                               .SetBoolean("kiosk_enabled", true)
                               .SetBoolean("kiosk_only", true)
                               .Build())
            .Build());
    user_manager_->AddKioskAppUser(
        AccountId::FromUserEmail(auto_launch_kiosk_app->id()));
    user_manager_->LoginUser(
        AccountId::FromUserEmail(auto_launch_kiosk_app->id()));

    TestingConsentProviderDelegate delegate;
    delegate.SetIsAutoLaunched(true);
    ConsentProvider provider(&delegate);
    EXPECT_TRUE(provider.IsGrantable(*auto_launch_kiosk_app));

    ConsentProvider::Consent result = ConsentProvider::CONSENT_IMPOSSIBLE;
    provider.RequestConsent(*auto_launch_kiosk_app.get(), volume_,
                            true /* writable */,
                            base::Bind(&OnConsentReceived, &result));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(0, delegate.show_dialog_counter());
    EXPECT_EQ(1, delegate.show_notification_counter());
    EXPECT_EQ(ConsentProvider::CONSENT_GRANTED, result);
  }

  // Non-component apps in manual-launch kiosk mode will be granted access after
  // receiving approval from the user.
  scoped_refptr<Extension> manual_launch_kiosk_app(
      test_util::BuildApp(ExtensionBuilder())
          .MergeManifest(DictionaryBuilder()
                             .SetBoolean("kiosk_enabled", true)
                             .SetBoolean("kiosk_only", true)
                             .Build())
          .Build());
  user_manager::User* const manual_kiosk_app_user =
      user_manager_->AddKioskAppUser(
          AccountId::FromUserEmail(manual_launch_kiosk_app->id()));
  user_manager_->KioskAppLoggedIn(manual_kiosk_app_user);
  {
    TestingConsentProviderDelegate delegate;
    delegate.SetDialogButton(ui::DIALOG_BUTTON_OK);
    ConsentProvider provider(&delegate);
    EXPECT_TRUE(provider.IsGrantable(*manual_launch_kiosk_app));

    ConsentProvider::Consent result = ConsentProvider::CONSENT_IMPOSSIBLE;
    provider.RequestConsent(*manual_launch_kiosk_app.get(), volume_,
                            true /* writable */,
                            base::Bind(&OnConsentReceived, &result));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, delegate.show_dialog_counter());
    EXPECT_EQ(0, delegate.show_notification_counter());
    EXPECT_EQ(ConsentProvider::CONSENT_GRANTED, result);
  }

  // Non-component apps in manual-launch kiosk mode will be rejected access
  // after rejecting by a user.
  {
    TestingConsentProviderDelegate delegate;
    ConsentProvider provider(&delegate);
    delegate.SetDialogButton(ui::DIALOG_BUTTON_CANCEL);
    EXPECT_TRUE(provider.IsGrantable(*manual_launch_kiosk_app));

    ConsentProvider::Consent result = ConsentProvider::CONSENT_IMPOSSIBLE;
    provider.RequestConsent(*manual_launch_kiosk_app.get(), volume_,
                            true /* writable */,
                            base::Bind(&OnConsentReceived, &result));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, delegate.show_dialog_counter());
    EXPECT_EQ(0, delegate.show_notification_counter());
    EXPECT_EQ(ConsentProvider::CONSENT_REJECTED, result);
  }
}
#endif

}  // namespace extensions
