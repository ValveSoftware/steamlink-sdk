// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cryptohi.h>

#include <memory>

#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/user_names.h"
#include "components/signin/core/account_id/account_id.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "net/base/net_errors.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/test_root_certs.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "policy/policy_constants.h"

namespace {

enum DeviceStatus { DEVICE_STATUS_ENROLLED, DEVICE_STATUS_NOT_ENROLLED };

enum UserStatus {
  USER_STATUS_MANAGED_AFFILIATED_DOMAIN,
  USER_STATUS_MANAGED_OTHER_DOMAIN,
  USER_STATUS_UNMANAGED
};

class PlatformKeysTest : public ExtensionApiTest {
 public:
  PlatformKeysTest(DeviceStatus device_status,
                   UserStatus user_status,
                   bool key_permission_policy)
      : device_status_(device_status),
        user_status_(user_status),
        key_permission_policy_(key_permission_policy) {
    if (user_status_ != USER_STATUS_UNMANAGED)
      SetupInitialEmptyPolicy();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);

    if (policy_helper_)
      policy_helper_->UpdateCommandLine(command_line);

    command_line->AppendSwitchASCII(
        chromeos::switches::kLoginUser,
        chromeos::login::StubAccountId().GetUserEmail());
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    if (device_status_ == DEVICE_STATUS_ENROLLED) {
      device_policy_test_helper_.device_policy()->policy_data().set_username(
          user_status_ == USER_STATUS_MANAGED_AFFILIATED_DOMAIN
              ? chromeos::login::StubAccountId().GetUserEmail()
              : "someuser@anydomain.com");

      device_policy_test_helper_.device_policy()->Build();
      device_policy_test_helper_.MarkAsEnterpriseOwned();
    }
  }

  void SetUpOnMainThread() override {
    if (policy_helper_)
      policy_helper_->WaitForInitialPolicy(browser()->profile());

    {
      base::RunLoop loop;
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE,
          base::Bind(&PlatformKeysTest::SetUpTestSystemSlotOnIO,
                     base::Unretained(this),
                     browser()->profile()->GetResourceContext(),
                     loop.QuitClosure()));
      loop.Run();
    }

    ExtensionApiTest::SetUpOnMainThread();

    {
      base::RunLoop loop;
      GetNSSCertDatabaseForProfile(
          browser()->profile(),
          base::Bind(&PlatformKeysTest::SetupTestCerts, base::Unretained(this),
                     loop.QuitClosure()));
      loop.Run();
    }

    base::FilePath extension_path = test_data_dir_.AppendASCII("platform_keys");
    extension_ = LoadExtension(extension_path);

    if (policy_helper_ && key_permission_policy_)
      SetupKeyPermissionPolicy();
  }

  void TearDownOnMainThread() override {
    ExtensionApiTest::TearDownOnMainThread();

    base::RunLoop loop;
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&PlatformKeysTest::TearDownTestSystemSlotOnIO,
                   base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  chromeos::PlatformKeysService* GetPlatformKeysService() {
    return chromeos::PlatformKeysServiceFactory::GetForBrowserContext(
        browser()->profile());
  }

  bool RunExtensionTest(const std::string& test_suite_name) {
    // By default, the system token is not available.
    std::string system_token_availability;

    // Only if the current user is of the same domain as the device is enrolled
    // to, the system token is available to the extension.
    if (device_status_ == DEVICE_STATUS_ENROLLED &&
        user_status_ == USER_STATUS_MANAGED_AFFILIATED_DOMAIN) {
      system_token_availability = "systemTokenEnabled";
    }

    GURL url = extension_->GetResourceURL(base::StringPrintf(
        "basic.html?%s#%s", system_token_availability.c_str(),
        test_suite_name.c_str()));
    return RunExtensionSubtest("", url.spec());
  }

  void RegisterClient1AsCorporateKey() {
    const extensions::Extension* const fake_gen_extension =
        LoadExtension(test_data_dir_.AppendASCII("platform_keys_genkey"));

    policy::ProfilePolicyConnector* const policy_connector =
        policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile());

    extensions::StateStore* const state_store =
        extensions::ExtensionSystem::Get(profile())->state_store();

    chromeos::KeyPermissions permissions(
        policy_connector->IsManaged(), profile()->GetPrefs(),
        policy_connector->policy_service(), state_store);

    base::RunLoop run_loop;
    permissions.GetPermissionsForExtension(
        fake_gen_extension->id(),
        base::Bind(&PlatformKeysTest::GotPermissionsForExtension,
                   base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

 protected:
  const DeviceStatus device_status_;
  const UserStatus user_status_;

  scoped_refptr<net::X509Certificate> client_cert1_;
  scoped_refptr<net::X509Certificate> client_cert2_;
  const extensions::Extension* extension_;

 private:
  void SetupInitialEmptyPolicy() {
    policy_helper_.reset(new policy::UserPolicyTestHelper(
        chromeos::login::StubAccountId().GetUserEmail()));
    policy_helper_->Init(
        base::DictionaryValue() /* empty mandatory policy */,
        base::DictionaryValue() /* empty recommended policy */);
  }

  void SetupKeyPermissionPolicy() {
    // Set up the test policy that gives |extension_| the permission to access
    // corporate keys.
    base::DictionaryValue key_permissions_policy;
    {
      std::unique_ptr<base::DictionaryValue> cert1_key_permission(
          new base::DictionaryValue);
      cert1_key_permission->SetBooleanWithoutPathExpansion(
          "allowCorporateKeyUsage", true);
      key_permissions_policy.SetWithoutPathExpansion(
          extension_->id(), cert1_key_permission.release());
    }

    std::string key_permissions_policy_str;
    base::JSONWriter::WriteWithOptions(key_permissions_policy,
                                       base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                       &key_permissions_policy_str);

    base::DictionaryValue user_policy;
    user_policy.SetStringWithoutPathExpansion(policy::key::kKeyPermissions,
                                              key_permissions_policy_str);

    policy_helper_->UpdatePolicy(
        user_policy, base::DictionaryValue() /* empty recommended policy */,
        browser()->profile());
  }

  void GotPermissionsForExtension(
      const base::Closure& done_callback,
      std::unique_ptr<chromeos::KeyPermissions::PermissionsForExtension>
          permissions_for_ext) {
    std::string client_cert1_spki =
        chromeos::platform_keys::GetSubjectPublicKeyInfo(client_cert1_);
    permissions_for_ext->RegisterKeyForCorporateUsage(client_cert1_spki);
    done_callback.Run();
  }

  void SetupTestCerts(const base::Closure& done_callback,
                      net::NSSCertDatabase* cert_db) {
    SetupTestClientCerts(cert_db);
    SetupTestCACerts();
    done_callback.Run();
  }

  void SetupTestClientCerts(net::NSSCertDatabase* cert_db) {
    client_cert1_ = net::ImportClientCertAndKeyFromFile(
        net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8",
        cert_db->GetPrivateSlot().get());
    ASSERT_TRUE(client_cert1_.get());

    // Import a second client cert signed by another CA than client_1 into the
    // system wide key slot.
    client_cert2_ = net::ImportClientCertAndKeyFromFile(
        net::GetTestCertsDirectory(), "client_2.pem", "client_2.pk8",
        test_system_slot_->slot());
    ASSERT_TRUE(client_cert2_.get());
  }

  void SetupTestCACerts() {
    net::TestRootCerts* root_certs = net::TestRootCerts::GetInstance();
    // "root_ca_cert.pem" is the issuer of "ok_cert.pem" which is loaded on the
    // JS side. Generated by create_test_certs.sh .
    base::FilePath extension_path = test_data_dir_.AppendASCII("platform_keys");
    root_certs->AddFromFile(
        test_data_dir_.AppendASCII("platform_keys").AppendASCII("root.pem"));
  }

  void SetUpTestSystemSlotOnIO(content::ResourceContext* context,
                               const base::Closure& done_callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    test_system_slot_.reset(new crypto::ScopedTestSystemNSSKeySlot());
    ASSERT_TRUE(test_system_slot_->ConstructedSuccessfully());

    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     done_callback);
  }

  void TearDownTestSystemSlotOnIO(const base::Closure& done_callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    test_system_slot_.reset();

    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     done_callback);
  }

  const bool key_permission_policy_;
  std::unique_ptr<policy::UserPolicyTestHelper> policy_helper_;
  policy::DevicePolicyCrosTestHelper device_policy_test_helper_;
  std::unique_ptr<crypto::ScopedTestSystemNSSKeySlot> test_system_slot_;
};

class TestSelectDelegate
    : public chromeos::PlatformKeysService::SelectDelegate {
 public:
  // On each Select call, selects the next entry in |certs_to_select| from back
  // to front. Once the first entry is reached, that one will be selected
  // repeatedly.
  // Entries of |certs_to_select| can be null in which case no certificate will
  // be selected.
  // If |certs_to_select| is empty, any invocation of |Select| will fail.
  explicit TestSelectDelegate(net::CertificateList certs_to_select)
      : certs_to_select_(certs_to_select) {}

  ~TestSelectDelegate() override {}

  void Select(const std::string& extension_id,
              const net::CertificateList& certs,
              const CertificateSelectedCallback& callback,
              content::WebContents* web_contents,
              content::BrowserContext* context) override {
    ASSERT_TRUE(web_contents);
    ASSERT_TRUE(context);
    ASSERT_FALSE(certs_to_select_.empty());
    scoped_refptr<net::X509Certificate> selection;
    if (certs_to_select_.back()) {
      for (scoped_refptr<net::X509Certificate> cert : certs) {
        if (cert->Equals(certs_to_select_.back().get())) {
          selection = cert;
          break;
        }
      }
    }
    if (certs_to_select_.size() > 1)
      certs_to_select_.pop_back();
    callback.Run(selection);
  }

 private:
  net::CertificateList certs_to_select_;
};

class UnmanagedPlatformKeysTest
    : public PlatformKeysTest,
      public ::testing::WithParamInterface<DeviceStatus> {
 public:
  UnmanagedPlatformKeysTest()
      : PlatformKeysTest(GetParam(),
                         USER_STATUS_UNMANAGED,
                         false /* unused */) {}
};

struct Params {
  Params(DeviceStatus device_status, UserStatus user_status)
      : device_status_(device_status), user_status_(user_status) {}

  DeviceStatus device_status_;
  UserStatus user_status_;
};

class ManagedWithPermissionPlatformKeysTest
    : public PlatformKeysTest,
      public ::testing::WithParamInterface<Params> {
 public:
  ManagedWithPermissionPlatformKeysTest()
      : PlatformKeysTest(GetParam().device_status_,
                         GetParam().user_status_,
                         true /* grant the extension key permission */) {}
};

class ManagedWithoutPermissionPlatformKeysTest
    : public PlatformKeysTest,
      public ::testing::WithParamInterface<Params> {
 public:
  ManagedWithoutPermissionPlatformKeysTest()
      : PlatformKeysTest(GetParam().device_status_,
                         GetParam().user_status_,
                         false /* do not grant key permission */) {}
};

}  // namespace

// At first interactively selects |client_cert1_| and |client_cert2_| to grant
// permissions and afterwards runs more basic tests.
// After the initial two interactive calls, the simulated user does not select
// any cert.
IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, Basic) {
  net::CertificateList certs;
  certs.push_back(nullptr);
  certs.push_back(client_cert2_);
  certs.push_back(client_cert1_);

  GetPlatformKeysService()->SetSelectDelegate(
      base::WrapUnique(new TestSelectDelegate(certs)));

  ASSERT_TRUE(RunExtensionTest("basicTests")) << message_;
}

// On interactive calls, the simulated user always selects |client_cert1_| if
// matching.
IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, Permissions) {
  net::CertificateList certs;
  certs.push_back(client_cert1_);

  GetPlatformKeysService()->SetSelectDelegate(
      base::WrapUnique(new TestSelectDelegate(certs)));

  ASSERT_TRUE(RunExtensionTest("permissionTests")) << message_;
}

INSTANTIATE_TEST_CASE_P(Unmanaged,
                        UnmanagedPlatformKeysTest,
                        ::testing::Values(DEVICE_STATUS_ENROLLED,
                                          DEVICE_STATUS_NOT_ENROLLED));

IN_PROC_BROWSER_TEST_P(ManagedWithoutPermissionPlatformKeysTest,
                       UserPermissionsBlocked) {
  // To verify that the user is not prompted for any certificate selection,
  // set up a delegate that fails on any invocation.
  GetPlatformKeysService()->SetSelectDelegate(
      base::WrapUnique(new TestSelectDelegate(net::CertificateList())));

  ASSERT_TRUE(RunExtensionTest("managedProfile")) << message_;
}

// A corporate key must not be useable if there is no policy permitting it.
IN_PROC_BROWSER_TEST_P(ManagedWithoutPermissionPlatformKeysTest,
                       CorporateKeyAccessBlocked) {
  RegisterClient1AsCorporateKey();

  // To verify that the user is not prompted for any certificate selection,
  // set up a delegate that fails on any invocation.
  GetPlatformKeysService()->SetSelectDelegate(
      base::WrapUnique(new TestSelectDelegate(net::CertificateList())));

  ASSERT_TRUE(RunExtensionTest("corporateKeyWithoutPermissionTests"))
      << message_;
}

INSTANTIATE_TEST_CASE_P(
    ManagedWithoutPermission,
    ManagedWithoutPermissionPlatformKeysTest,
    ::testing::Values(
        Params(DEVICE_STATUS_ENROLLED, USER_STATUS_MANAGED_AFFILIATED_DOMAIN),
        Params(DEVICE_STATUS_ENROLLED, USER_STATUS_MANAGED_OTHER_DOMAIN),
        Params(DEVICE_STATUS_NOT_ENROLLED, USER_STATUS_MANAGED_OTHER_DOMAIN)));

IN_PROC_BROWSER_TEST_P(ManagedWithPermissionPlatformKeysTest,
                       PolicyGrantsAccessToCorporateKey) {
  RegisterClient1AsCorporateKey();

  // Set up the test SelectDelegate to select |client_cert1_| if available for
  // selection.
  net::CertificateList certs;
  certs.push_back(client_cert1_);

  GetPlatformKeysService()->SetSelectDelegate(
      base::WrapUnique(new TestSelectDelegate(certs)));

  ASSERT_TRUE(RunExtensionTest("corporateKeyWithPermissionTests")) << message_;
}

IN_PROC_BROWSER_TEST_P(ManagedWithPermissionPlatformKeysTest,
                       PolicyDoesGrantAccessToNonCorporateKey) {
  // The policy grants access to corporate keys but none are available.
  // As the profile is managed, the user must not be able to grant any
  // certificate permission. Set up a delegate that fails on any invocation.
  GetPlatformKeysService()->SetSelectDelegate(
      base::WrapUnique(new TestSelectDelegate(net::CertificateList())));

  ASSERT_TRUE(RunExtensionTest("policyDoesGrantAccessToNonCorporateKey"))
      << message_;
}

INSTANTIATE_TEST_CASE_P(
    ManagedWithPermission,
    ManagedWithPermissionPlatformKeysTest,
    ::testing::Values(
        Params(DEVICE_STATUS_ENROLLED, USER_STATUS_MANAGED_AFFILIATED_DOMAIN),
        Params(DEVICE_STATUS_ENROLLED, USER_STATUS_MANAGED_OTHER_DOMAIN),
        Params(DEVICE_STATUS_NOT_ENROLLED, USER_STATUS_MANAGED_OTHER_DOMAIN)));
