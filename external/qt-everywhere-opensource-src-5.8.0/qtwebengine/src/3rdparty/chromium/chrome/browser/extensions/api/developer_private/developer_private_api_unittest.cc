// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_error_test_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/test_util.h"
#include "extensions/common/value_builder.h"

namespace extensions {

namespace {

std::unique_ptr<KeyedService> BuildAPI(content::BrowserContext* context) {
  return base::WrapUnique(new DeveloperPrivateAPI(context));
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return base::WrapUnique(
      new EventRouter(profile, ExtensionPrefs::Get(profile)));
}

}  // namespace

class DeveloperPrivateApiUnitTest : public ExtensionServiceTestBase {
 protected:
  DeveloperPrivateApiUnitTest() {}
  ~DeveloperPrivateApiUnitTest() override {}

  // A wrapper around extension_function_test_utils::RunFunction that runs with
  // the associated browser, no flags, and can take stack-allocated arguments.
  bool RunFunction(const scoped_refptr<UIThreadExtensionFunction>& function,
                   const base::ListValue& args);

  // Loads an unpacked extension that is backed by a real directory, allowing
  // it to be reloaded.
  const Extension* LoadUnpackedExtension();

  // Loads an extension with no real directory; this is faster, but means the
  // extension can't be reloaded.
  const Extension* LoadSimpleExtension();

  // Tests modifying the extension's configuration.
  void TestExtensionPrefSetting(
      bool (*has_pref)(const std::string&, content::BrowserContext*),
      const std::string& key,
      const std::string& extension_id);

  testing::AssertionResult TestPackExtensionFunction(
      const base::ListValue& args,
      api::developer_private::PackStatus expected_status,
      int expected_flags);

  Browser* browser() { return browser_.get(); }

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  ScopedVector<TestExtensionDir> test_extension_dirs_;

  DISALLOW_COPY_AND_ASSIGN(DeveloperPrivateApiUnitTest);
};

bool DeveloperPrivateApiUnitTest::RunFunction(
    const scoped_refptr<UIThreadExtensionFunction>& function,
    const base::ListValue& args) {
  return extension_function_test_utils::RunFunction(
      function.get(), base::WrapUnique(args.DeepCopy()), browser(),
      extension_function_test_utils::NONE);
}

const Extension* DeveloperPrivateApiUnitTest::LoadUnpackedExtension() {
  const char kManifest[] =
      "{"
      " \"name\": \"foo\","
      " \"version\": \"1.0\","
      " \"manifest_version\": 2,"
      " \"permissions\": [\"*://*/*\"]"
      "}";

  test_extension_dirs_.push_back(new TestExtensionDir);
  TestExtensionDir* dir = test_extension_dirs_.back();
  dir->WriteManifest(kManifest);

  // TODO(devlin): We should extract out methods to load an unpacked extension
  // synchronously. We do it in ExtensionBrowserTest, but that's not helpful
  // for unittests.
  TestExtensionRegistryObserver registry_observer(registry());
  scoped_refptr<UnpackedInstaller> installer(
      UnpackedInstaller::Create(service()));
  installer->Load(dir->unpacked_path());
  base::FilePath extension_path =
      base::MakeAbsoluteFilePath(dir->unpacked_path());
  const Extension* extension = nullptr;
  do {
    extension = registry_observer.WaitForExtensionLoaded();
  } while (extension->path() != extension_path);
  // The fact that unpacked extensions get file access by default is an
  // irrelevant detail to these tests. Disable it.
  ExtensionPrefs::Get(browser_context())->SetAllowFileAccess(extension->id(),
                                                             false);
  return extension;
}

const Extension* DeveloperPrivateApiUnitTest::LoadSimpleExtension() {
  const char kName[] = "extension name";
  const char kVersion[] = "1.0.0.1";
  std::string id = crx_file::id_util::GenerateId(kName);
  DictionaryBuilder manifest;
  manifest.Set("name", kName)
          .Set("version", kVersion)
          .Set("manifest_version", 2)
          .Set("description", "an extension");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(manifest.Build())
          .SetLocation(Manifest::INTERNAL)
          .SetID(id)
          .Build();
  service()->AddExtension(extension.get());
  return extension.get();
}

void DeveloperPrivateApiUnitTest::TestExtensionPrefSetting(
    bool (*has_pref)(const std::string&, content::BrowserContext*),
    const std::string& key,
    const std::string& extension_id) {
  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivateUpdateExtensionConfigurationFunction());

  base::ListValue args;
  base::DictionaryValue* parameters = new base::DictionaryValue();
  parameters->SetString("extensionId", extension_id);
  parameters->SetBoolean(key, true);
  args.Append(parameters);

  EXPECT_FALSE(has_pref(extension_id, profile())) << key;

  EXPECT_FALSE(RunFunction(function, args)) << key;
  EXPECT_EQ(std::string("This action requires a user gesture."),
            function->GetError());

  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  function = new api::DeveloperPrivateUpdateExtensionConfigurationFunction();
  EXPECT_TRUE(RunFunction(function, args)) << key;
  EXPECT_TRUE(has_pref(extension_id, profile())) << key;

  parameters->SetBoolean(key, false);
  function = new api::DeveloperPrivateUpdateExtensionConfigurationFunction();
  EXPECT_TRUE(RunFunction(function, args)) << key;
  EXPECT_FALSE(has_pref(extension_id, profile())) << key;
}

testing::AssertionResult DeveloperPrivateApiUnitTest::TestPackExtensionFunction(
    const base::ListValue& args,
    api::developer_private::PackStatus expected_status,
    int expected_flags) {
  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivatePackDirectoryFunction());
  if (!RunFunction(function, args))
    return testing::AssertionFailure() << "Could not run function.";

  // Extract the result. We don't have to test this here, since it's verified as
  // part of the general extension api system.
  const base::Value* response_value = nullptr;
  CHECK(function->GetResultList()->Get(0u, &response_value));
  std::unique_ptr<api::developer_private::PackDirectoryResponse> response =
      api::developer_private::PackDirectoryResponse::FromValue(*response_value);
  CHECK(response);

  if (response->status != expected_status) {
    return testing::AssertionFailure() << "Expected status: " <<
        expected_status << ", found status: " << response->status <<
        ", message: " << response->message;
  }

  if (response->override_flags != expected_flags) {
    return testing::AssertionFailure() << "Expected flags: " <<
        expected_flags << ", found flags: " << response->override_flags;
  }

  return testing::AssertionSuccess();
}

void DeveloperPrivateApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  browser_window_.reset(new TestBrowserWindow());
  Browser::CreateParams params(profile());
  params.type = Browser::TYPE_TABBED;
  params.window = browser_window_.get();
  browser_.reset(new Browser(params));

  // Allow the API to be created.
  EventRouterFactory::GetInstance()->SetTestingFactory(profile(),
                                                       &BuildEventRouter);

  DeveloperPrivateAPI::GetFactoryInstance()->SetTestingFactory(
      profile(), &BuildAPI);
}

void DeveloperPrivateApiUnitTest::TearDown() {
  test_extension_dirs_.clear();
  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
}

// Test developerPrivate.updateExtensionConfiguration.
TEST_F(DeveloperPrivateApiUnitTest,
       DeveloperPrivateUpdateExtensionConfiguration) {
  FeatureSwitch::ScopedOverride scripts_require_action(
      FeatureSwitch::scripts_require_action(), true);
  // Sadly, we need a "real" directory here, because toggling prefs causes
  // a reload (which needs a path).
  std::string id = LoadUnpackedExtension()->id();

  TestExtensionPrefSetting(&util::IsIncognitoEnabled, "incognitoAccess", id);
  TestExtensionPrefSetting(&util::AllowFileAccess, "fileAccess", id);
  TestExtensionPrefSetting(
      &util::AllowedScriptingOnAllUrls, "runOnAllUrls", id);
}

// Test developerPrivate.reload.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateReload) {
  const Extension* extension = LoadUnpackedExtension();
  std::string extension_id = extension->id();
  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivateReloadFunction());
  base::ListValue reload_args;
  reload_args.AppendString(extension_id);

  TestExtensionRegistryObserver registry_observer(registry());
  EXPECT_TRUE(RunFunction(function, reload_args));
  const Extension* unloaded_extension =
      registry_observer.WaitForExtensionUnloaded();
  EXPECT_EQ(extension, unloaded_extension);
  const Extension* reloaded_extension =
      registry_observer.WaitForExtensionLoaded();
  EXPECT_EQ(extension_id, reloaded_extension->id());
}

// Test developerPrivate.packDirectory.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivatePackFunction) {
  // Use a temp dir isolating the extension dir and its generated files.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath root_path = data_dir().AppendASCII("good_unpacked");
  ASSERT_TRUE(base::CopyDirectory(root_path, temp_dir.path(), true));

  base::FilePath temp_root_path = temp_dir.path().Append(root_path.BaseName());
  base::FilePath crx_path = temp_dir.path().AppendASCII("good_unpacked.crx");
  base::FilePath pem_path = temp_dir.path().AppendASCII("good_unpacked.pem");

  EXPECT_FALSE(base::PathExists(crx_path))
      << "crx should not exist before the test is run!";
  EXPECT_FALSE(base::PathExists(pem_path))
      << "pem should not exist before the test is run!";

  // First, test a directory that should pack properly.
  base::ListValue pack_args;
  pack_args.AppendString(temp_root_path.AsUTF8Unsafe());
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PACK_STATUS_SUCCESS, 0));

  // Should have created crx file and pem file.
  EXPECT_TRUE(base::PathExists(crx_path));
  EXPECT_TRUE(base::PathExists(pem_path));

  // Deliberately don't cleanup the files, and append the pem path.
  pack_args.AppendString(pem_path.AsUTF8Unsafe());

  // Try to pack again - we should get a warning abot overwriting the crx.
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args,
      api::developer_private::PACK_STATUS_WARNING,
      ExtensionCreator::kOverwriteCRX));

  // Try to pack again, with the overwrite flag; this should succeed.
  pack_args.AppendInteger(ExtensionCreator::kOverwriteCRX);
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PACK_STATUS_SUCCESS, 0));

  // Try to pack a final time when omitting (an existing) pem file. We should
  // get an error.
  base::DeleteFile(crx_path, false);
  EXPECT_TRUE(pack_args.Remove(1u, nullptr));  // Remove the pem key argument.
  EXPECT_TRUE(pack_args.Remove(1u, nullptr));  // Remove the flags argument.
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PACK_STATUS_ERROR, 0));
}

// Test developerPrivate.choosePath.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateChoosePath) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath expected_dir_path = data_dir().AppendASCII("good_unpacked");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&expected_dir_path);

  // Try selecting a directory.
  base::ListValue choose_args;
  choose_args.AppendString("FOLDER");
  choose_args.AppendString("LOAD");
  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivateChoosePathFunction());
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  EXPECT_TRUE(RunFunction(function, choose_args)) << function->GetError();
  std::string path;
  EXPECT_TRUE(function->GetResultList() &&
              function->GetResultList()->GetString(0, &path));
  EXPECT_EQ(path, expected_dir_path.AsUTF8Unsafe());

  // Try selecting a pem file.
  base::FilePath expected_file_path =
      data_dir().AppendASCII("good_unpacked.pem");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&expected_file_path);
  choose_args.Clear();
  choose_args.AppendString("FILE");
  choose_args.AppendString("PEM");
  function = new api::DeveloperPrivateChoosePathFunction();
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  EXPECT_TRUE(RunFunction(function, choose_args)) << function->GetError();
  EXPECT_TRUE(function->GetResultList() &&
              function->GetResultList()->GetString(0, &path));
  EXPECT_EQ(path, expected_file_path.AsUTF8Unsafe());

  // Try canceling the file dialog.
  api::EntryPicker::SkipPickerAndAlwaysCancelForTest();
  function = new api::DeveloperPrivateChoosePathFunction();
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  EXPECT_FALSE(RunFunction(function, choose_args));
  EXPECT_EQ(std::string("File selection was canceled."), function->GetError());
}

// Test developerPrivate.loadUnpacked.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateLoadUnpacked) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath path = data_dir().AppendASCII("good_unpacked");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

  // Try loading a good extension (it should succeed, and the extension should
  // be added).
  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivateLoadUnpackedFunction());
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  ExtensionIdSet current_ids = registry()->enabled_extensions().GetIDs();
  EXPECT_TRUE(RunFunction(function, base::ListValue())) << function->GetError();
  // We should have added one new extension.
  ExtensionIdSet id_difference = base::STLSetDifference<ExtensionIdSet>(
      registry()->enabled_extensions().GetIDs(), current_ids);
  ASSERT_EQ(1u, id_difference.size());
  // The new extension should have the same path.
  EXPECT_EQ(
      path,
      registry()->enabled_extensions().GetByID(*id_difference.begin())->path());

  path = data_dir().AppendASCII("empty_manifest");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

  // Try loading a bad extension (it should fail, and we should get an error).
  function = new api::DeveloperPrivateLoadUnpackedFunction();
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  base::ListValue unpacked_args;
  std::unique_ptr<base::DictionaryValue> options(new base::DictionaryValue());
  options->SetBoolean("failQuietly", true);
  unpacked_args.Append(std::move(options));
  current_ids = registry()->enabled_extensions().GetIDs();
  EXPECT_FALSE(RunFunction(function, unpacked_args));
  EXPECT_EQ(manifest_errors::kManifestUnreadable, function->GetError());
  // We should have no new extensions installed.
  EXPECT_EQ(0u, base::STLSetDifference<ExtensionIdSet>(
                    registry()->enabled_extensions().GetIDs(),
                    current_ids).size());
}

// Test developerPrivate.requestFileSource.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateRequestFileSource) {
  // Testing of this function seems light, but that's because it basically just
  // forwards to reading a file to a string, and highlighting it - both of which
  // are already tested separately.
  const Extension* extension = LoadUnpackedExtension();
  const char kErrorMessage[] = "Something went wrong";
  api::developer_private::RequestFileSourceProperties properties;
  properties.extension_id = extension->id();
  properties.path_suffix = "manifest.json";
  properties.message = kErrorMessage;
  properties.manifest_key.reset(new std::string("name"));

  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivateRequestFileSourceFunction());
  base::ListValue file_source_args;
  file_source_args.Append(properties.ToValue());
  EXPECT_TRUE(RunFunction(function, file_source_args)) << function->GetError();

  const base::Value* response_value = nullptr;
  ASSERT_TRUE(function->GetResultList()->Get(0u, &response_value));
  std::unique_ptr<api::developer_private::RequestFileSourceResponse> response =
      api::developer_private::RequestFileSourceResponse::FromValue(
          *response_value);
  EXPECT_FALSE(response->before_highlight.empty());
  EXPECT_EQ("\"name\": \"foo\"", response->highlight);
  EXPECT_FALSE(response->after_highlight.empty());
  EXPECT_EQ("foo: manifest.json", response->title);
  EXPECT_EQ(kErrorMessage, response->message);
}

// Test developerPrivate.getExtensionsInfo.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateGetExtensionsInfo) {
  LoadSimpleExtension();

  // The test here isn't so much about the generated value (that's tested in
  // ExtensionInfoGenerator's unittest), but rather just to make sure we can
  // serialize/deserialize the result - which implicity tests that everything
  // has a sane value.
  scoped_refptr<UIThreadExtensionFunction> function(
      new api::DeveloperPrivateGetExtensionsInfoFunction());
  EXPECT_TRUE(RunFunction(function, base::ListValue())) << function->GetError();
  const base::ListValue* results = function->GetResultList();
  ASSERT_EQ(1u, results->GetSize());
  const base::ListValue* list = nullptr;
  ASSERT_TRUE(results->GetList(0u, &list));
  ASSERT_EQ(1u, list->GetSize());
  const base::Value* value = nullptr;
  ASSERT_TRUE(list->Get(0u, &value));
  std::unique_ptr<api::developer_private::ExtensionInfo> info =
      api::developer_private::ExtensionInfo::FromValue(*value);
  ASSERT_TRUE(info);

  // As a sanity check, also run the GetItemsInfo and make sure it returns a
  // sane value.
  function = new api::DeveloperPrivateGetItemsInfoFunction();
  base::ListValue args;
  args.AppendBoolean(false);
  args.AppendBoolean(false);
  EXPECT_TRUE(RunFunction(function, args)) << function->GetError();
  results = function->GetResultList();
  ASSERT_EQ(1u, results->GetSize());
  ASSERT_TRUE(results->GetList(0u, &list));
  ASSERT_EQ(1u, list->GetSize());
  ASSERT_TRUE(list->Get(0u, &value));
  std::unique_ptr<api::developer_private::ItemInfo> item_info =
      api::developer_private::ItemInfo::FromValue(*value);
  ASSERT_TRUE(item_info);
}

// Test developerPrivate.deleteExtensionErrors.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateDeleteExtensionErrors) {
  FeatureSwitch::ScopedOverride error_console_override(
      FeatureSwitch::error_console(), true);
  profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
  const Extension* extension = LoadSimpleExtension();

  // Report some errors.
  ErrorConsole* error_console = ErrorConsole::Get(profile());
  error_console->SetReportingAllForExtension(extension->id(), true);
  error_console->ReportError(
      error_test_util::CreateNewRuntimeError(extension->id(), "foo"));
  error_console->ReportError(
      error_test_util::CreateNewRuntimeError(extension->id(), "bar"));
  error_console->ReportError(
      error_test_util::CreateNewManifestError(extension->id(), "baz"));
  EXPECT_EQ(3u, error_console->GetErrorsForExtension(extension->id()).size());

  // Start by removing all errors for the extension of a given type (manifest).
  std::string type_string = api::developer_private::ToString(
      api::developer_private::ERROR_TYPE_MANIFEST);
  std::unique_ptr<base::ListValue> args =
      ListBuilder()
          .Append(DictionaryBuilder()
                      .Set("extensionId", extension->id())
                      .Set("type", type_string)
                      .Build())
          .Build();
  scoped_refptr<UIThreadExtensionFunction> function =
      new api::DeveloperPrivateDeleteExtensionErrorsFunction();
  EXPECT_TRUE(RunFunction(function, *args)) << function->GetError();
  // Two errors should remain.
  const ErrorList& error_list =
      error_console->GetErrorsForExtension(extension->id());
  ASSERT_EQ(2u, error_list.size());

  // Next remove errors by id.
  int error_id = error_list[0]->id();
  args =
      ListBuilder()
          .Append(DictionaryBuilder()
                      .Set("extensionId", extension->id())
                      .Set("errorIds", ListBuilder().Append(error_id).Build())
                      .Build())
          .Build();
  function = new api::DeveloperPrivateDeleteExtensionErrorsFunction();
  EXPECT_TRUE(RunFunction(function, *args)) << function->GetError();
  // And then there was one.
  EXPECT_EQ(1u, error_console->GetErrorsForExtension(extension->id()).size());

  // Finally remove all errors for the extension.
  args =
      ListBuilder()
          .Append(
              DictionaryBuilder().Set("extensionId", extension->id()).Build())
          .Build();
  function = new api::DeveloperPrivateDeleteExtensionErrorsFunction();
  EXPECT_TRUE(RunFunction(function, *args)) << function->GetError();
  // No more errors!
  EXPECT_TRUE(error_console->GetErrorsForExtension(extension->id()).empty());
}

}  // namespace extensions
