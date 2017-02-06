// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/test_util.h"

namespace extensions {

namespace {

std::unique_ptr<KeyedService> BuildManagementApi(
    content::BrowserContext* context) {
  return base::WrapUnique(new ManagementAPI(context));
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return base::WrapUnique(
      new extensions::EventRouter(profile, ExtensionPrefs::Get(profile)));
}

}  // namespace

namespace constants = extension_management_api_constants;

// TODO(devlin): Unittests are awesome. Test more with unittests and less with
// heavy api/browser tests.
class ManagementApiUnitTest : public ExtensionServiceTestWithInstall {
 protected:
  ManagementApiUnitTest() {}
  ~ManagementApiUnitTest() override {}

  // A wrapper around extension_function_test_utils::RunFunction that runs with
  // the associated browser, no flags, and can take stack-allocated arguments.
  bool RunFunction(const scoped_refptr<UIThreadExtensionFunction>& function,
                   const base::ListValue& args);

  Browser* browser() { return browser_.get(); }

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  DISALLOW_COPY_AND_ASSIGN(ManagementApiUnitTest);
};

bool ManagementApiUnitTest::RunFunction(
    const scoped_refptr<UIThreadExtensionFunction>& function,
    const base::ListValue& args) {
  return extension_function_test_utils::RunFunction(
      function.get(), base::WrapUnique(args.DeepCopy()), browser(),
      extension_function_test_utils::NONE);
}

void ManagementApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();
  ManagementAPI::GetFactoryInstance()->SetTestingFactory(profile(),
                                                         &BuildManagementApi);

  EventRouterFactory::GetInstance()->SetTestingFactory(profile(),
                                                       &BuildEventRouter);

  browser_window_.reset(new TestBrowserWindow());
  Browser::CreateParams params(profile());
  params.type = Browser::TYPE_TABBED;
  params.window = browser_window_.get();
  browser_.reset(new Browser(params));
}

void ManagementApiUnitTest::TearDown() {
  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
}

// Test the basic parts of management.setEnabled.
TEST_F(ManagementApiUnitTest, ManagementSetEnabled) {
  scoped_refptr<const Extension> extension = test_util::CreateEmptyExtension();
  service()->AddExtension(extension.get());
  std::string extension_id = extension->id();
  scoped_refptr<ManagementSetEnabledFunction> function(
      new ManagementSetEnabledFunction());

  base::ListValue disable_args;
  disable_args.AppendString(extension_id);
  disable_args.AppendBoolean(false);

  // Test disabling an (enabled) extension.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
  EXPECT_TRUE(RunFunction(function, disable_args)) << function->GetError();
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));

  base::ListValue enable_args;
  enable_args.AppendString(extension_id);
  enable_args.AppendBoolean(true);

  // Test re-enabling it.
  function = new ManagementSetEnabledFunction();
  EXPECT_TRUE(RunFunction(function, enable_args)) << function->GetError();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));

  // Test that the enable function checks management policy, so that we can't
  // disable an extension that is required.
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  ManagementPolicy* policy =
      ExtensionSystem::Get(profile())->management_policy();
  policy->RegisterProvider(&provider);

  function = new ManagementSetEnabledFunction();
  EXPECT_FALSE(RunFunction(function, disable_args));
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUserCantModifyError,
                                           extension_id),
            function->GetError());
  policy->UnregisterProvider(&provider);
}

// Tests management.uninstall.
TEST_F(ManagementApiUnitTest, ManagementUninstall) {
  scoped_refptr<const Extension> extension = test_util::CreateEmptyExtension();
  service()->AddExtension(extension.get());
  std::string extension_id = extension->id();

  base::ListValue uninstall_args;
  uninstall_args.AppendString(extension->id());

  // Auto-accept any uninstalls.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);

    // Uninstall requires a user gesture, so this should fail.
    scoped_refptr<UIThreadExtensionFunction> function(
        new ManagementUninstallFunction());
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    EXPECT_EQ(std::string(constants::kGestureNeededForUninstallError),
              function->GetError());

    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    function = new ManagementUninstallFunction();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                              ExtensionRegistry::EVERYTHING));
  }

  // Install the extension again, and try uninstalling, auto-canceling the
  // dialog.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    service()->AddExtension(extension.get());
    scoped_refptr<UIThreadExtensionFunction> function =
        new ManagementUninstallFunction();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // The uninstall should have failed.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());

    // Try again, using showConfirmDialog: false.
    std::unique_ptr<base::DictionaryValue> options(new base::DictionaryValue());
    options->SetBoolean("showConfirmDialog", false);
    uninstall_args.Append(std::move(options));
    function = new ManagementUninstallFunction();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // This should still fail, since extensions can only suppress the dialog for
    // uninstalling themselves.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());

    // If we try uninstall the extension itself, the uninstall should succeed
    // (even though we auto-cancel any dialog), because the dialog is never
    // shown.
    uninstall_args.Remove(0u, nullptr);
    function = new ManagementUninstallSelfFunction();
    function->set_extension(extension);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                              ExtensionRegistry::EVERYTHING));
  }
}

// Tests uninstalling a blacklisted extension via management.uninstall.
TEST_F(ManagementApiUnitTest, ManagementUninstallBlacklisted) {
  scoped_refptr<const Extension> extension = test_util::CreateEmptyExtension();
  service()->AddExtension(extension.get());
  std::string id = extension->id();

  service()->BlacklistExtensionForTest(id);
  EXPECT_NE(nullptr, registry()->GetInstalledExtension(id));

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  scoped_refptr<UIThreadExtensionFunction> function(
      new ManagementUninstallFunction());
  base::ListValue uninstall_args;
  uninstall_args.AppendString(id);
  EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();

  EXPECT_EQ(nullptr, registry()->GetInstalledExtension(id));
}

TEST_F(ManagementApiUnitTest, ManagementEnableOrDisableBlacklisted) {
  scoped_refptr<const Extension> extension = test_util::CreateEmptyExtension();
  service()->AddExtension(extension.get());
  std::string id = extension->id();

  service()->BlacklistExtensionForTest(id);
  EXPECT_NE(nullptr, registry()->GetInstalledExtension(id));

  scoped_refptr<UIThreadExtensionFunction> function;

  // Test enabling it.
  {
    base::ListValue enable_args;
    enable_args.AppendString(id);
    enable_args.AppendBoolean(true);
    function = new ManagementSetEnabledFunction();
    EXPECT_TRUE(RunFunction(function, enable_args)) << function->GetError();
    EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
    EXPECT_FALSE(registry()->disabled_extensions().Contains(id));
  }

  // Test disabling it
  {
    base::ListValue disable_args;
    disable_args.AppendString(id);
    disable_args.AppendBoolean(false);

    function = new ManagementSetEnabledFunction();
    EXPECT_TRUE(RunFunction(function, disable_args)) << function->GetError();
    EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
    EXPECT_FALSE(registry()->disabled_extensions().Contains(id));
  }
}

// Tests enabling an extension via management API after it was disabled due to
// permission increase.
TEST_F(ManagementApiUnitTest, SetEnabledAfterIncreasedPermissions) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension = PackAndInstallCRX(path, pem_path, INSTALL_NEW);
  // The extension must now be installed and enabled.
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  // Save the id, as |extension| will be destroyed during updating.
  std::string extension_id = extension->id();

  std::unique_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  // v1 extension doesn't have any permissions.
  EXPECT_TRUE(known_perms->IsEmpty());

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(extension_id, path, pem_path, DISABLED);

  // The extension should be disabled.
  ASSERT_FALSE(registry()->enabled_extensions().Contains(extension_id));

  // Due to a permission increase, prefs will contain escalation information.
  EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  auto enable_extension_via_management_api = [this](
      const std::string& extension_id, bool use_user_gesture,
      bool accept_dialog, bool expect_success) {
    ScopedTestDialogAutoConfirm auto_confirm(
        accept_dialog ? ScopedTestDialogAutoConfirm::ACCEPT
                      : ScopedTestDialogAutoConfirm::CANCEL);
    std::unique_ptr<ExtensionFunction::ScopedUserGestureForTests> gesture;
    if (use_user_gesture)
      gesture.reset(new ExtensionFunction::ScopedUserGestureForTests);
    scoped_refptr<ManagementSetEnabledFunction> function(
        new ManagementSetEnabledFunction());
    base::ListValue args;
    args.AppendString(extension_id);
    args.AppendBoolean(true);
    if (expect_success) {
      EXPECT_TRUE(RunFunction(function, args)) << function->GetError();
    } else {
      EXPECT_FALSE(RunFunction(function, args)) << function->GetError();
    }
  };

  // 1) Confirm re-enable prompt without user gesture, expect the extension to
  // stay disabled.
  {
    enable_extension_via_management_api(
        extension_id, false /* use_user_gesture */, true /* accept_dialog */,
        false /* expect_success */);
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 2) Deny re-enable prompt without user gesture, expect the extension to stay
  // disabled.
  {
    enable_extension_via_management_api(
        extension_id, false /* use_user_gesture */, false /* accept_dialog */,
        false /* expect_success */);
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 3) Deny re-enable prompt with user gesture, expect the extension to stay
  // disabled.
  {
    enable_extension_via_management_api(
        extension_id, true /* use_user_gesture */, false /* accept_dialog */,
        false /* expect_success */);
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 4) Accept re-enable prompt with user gesture, expect the extension to be
  // enabled.
  {
    enable_extension_via_management_api(
        extension_id, true /* use_user_gesture */, true /* accept_dialog */,
        true /* expect_success */);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs will no longer contain the escalation information as user has
    // accepted increased permissions.
    EXPECT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // Some permissions for v2 extension should be granted by now.
  known_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  EXPECT_FALSE(known_perms->IsEmpty());
}

}  // namespace extensions
