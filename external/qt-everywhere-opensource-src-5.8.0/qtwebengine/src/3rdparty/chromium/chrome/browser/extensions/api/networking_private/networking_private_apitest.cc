// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/onc/onc_constants.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/networking_private/networking_private_delegate.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "extensions/common/switches.h"

namespace extensions {

// This tests just the interface for the networkingPrivate API, i.e. it ensures
// that the delegate methods are called as expected.

// The implementations (which differ significantly between chromeos and
// windows/mac) are tested independently in
// networking_private_[chromeos|service_client]_apitest.cc.
// See also crbug.com/460119.

namespace {

const char kFailure[] = "Failure";
const char kSuccess[] = "Success";
const char kGuid[] = "SOME_GUID";

class TestDelegate : public NetworkingPrivateDelegate {
 public:
  explicit TestDelegate(std::unique_ptr<VerifyDelegate> verify_delegate)
      : NetworkingPrivateDelegate(std::move(verify_delegate)), fail_(false) {}

  ~TestDelegate() override {}

  // Asynchronous methods
  void GetProperties(const std::string& guid,
                     const DictionaryCallback& success_callback,
                     const FailureCallback& failure_callback) override {
    DictionaryResult(guid, success_callback, failure_callback);
  }

  void GetManagedProperties(const std::string& guid,
                            const DictionaryCallback& success_callback,
                            const FailureCallback& failure_callback) override {
    DictionaryResult(guid, success_callback, failure_callback);
  }

  void GetState(const std::string& guid,
                const DictionaryCallback& success_callback,
                const FailureCallback& failure_callback) override {
    DictionaryResult(guid, success_callback, failure_callback);
  }

  void SetProperties(const std::string& guid,
                     std::unique_ptr<base::DictionaryValue> properties,
                     const VoidCallback& success_callback,
                     const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  void CreateNetwork(bool shared,
                     std::unique_ptr<base::DictionaryValue> properties,
                     const StringCallback& success_callback,
                     const FailureCallback& failure_callback) override {
    StringResult(success_callback, failure_callback);
  }

  void ForgetNetwork(const std::string& guid,
                     const VoidCallback& success_callback,
                     const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  void GetNetworks(const std::string& network_type,
                   bool configured_only,
                   bool visible_only,
                   int limit,
                   const NetworkListCallback& success_callback,
                   const FailureCallback& failure_callback) override {
    if (fail_) {
      failure_callback.Run(kFailure);
    } else {
      std::unique_ptr<base::ListValue> result(new base::ListValue);
      std::unique_ptr<base::DictionaryValue> network(new base::DictionaryValue);
      network->SetString(::onc::network_config::kType,
                         ::onc::network_config::kEthernet);
      network->SetString(::onc::network_config::kGUID, kGuid);
      result->Append(network.release());
      success_callback.Run(std::move(result));
    }
  }

  void StartConnect(const std::string& guid,
                    const VoidCallback& success_callback,
                    const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  void StartDisconnect(const std::string& guid,
                       const VoidCallback& success_callback,
                       const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  void StartActivate(const std::string& guid,
                     const std::string& carrier,
                     const VoidCallback& success_callback,
                     const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  void SetWifiTDLSEnabledState(
      const std::string& ip_or_mac_address,
      bool enabled,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override {
    StringResult(success_callback, failure_callback);
  }

  void GetWifiTDLSStatus(const std::string& ip_or_mac_address,
                         const StringCallback& success_callback,
                         const FailureCallback& failure_callback) override {
    StringResult(success_callback, failure_callback);
  }

  void GetCaptivePortalStatus(
      const std::string& guid,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override {
    StringResult(success_callback, failure_callback);
  }

  void UnlockCellularSim(const std::string& guid,
                         const std::string& pin,
                         const std::string& puk,
                         const VoidCallback& success_callback,
                         const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  void SetCellularSimState(const std::string& guid,
                           bool require_pin,
                           const std::string& current_pin,
                           const std::string& new_pin,
                           const VoidCallback& success_callback,
                           const FailureCallback& failure_callback) override {
    VoidResult(success_callback, failure_callback);
  }

  // Synchronous methods
  std::unique_ptr<base::ListValue> GetEnabledNetworkTypes() override {
    std::unique_ptr<base::ListValue> result;
    if (!fail_) {
      result.reset(new base::ListValue);
      result->AppendString(::onc::network_config::kEthernet);
    }
    return result;
  }

  std::unique_ptr<DeviceStateList> GetDeviceStateList() override {
    std::unique_ptr<DeviceStateList> result;
    if (fail_)
      return result;
    result.reset(new DeviceStateList);
    std::unique_ptr<api::networking_private::DeviceStateProperties> properties(
        new api::networking_private::DeviceStateProperties);
    properties->type = api::networking_private::NETWORK_TYPE_ETHERNET;
    properties->state = api::networking_private::DEVICE_STATE_TYPE_ENABLED;
    result->push_back(std::move(properties));
    return result;
  }

  bool EnableNetworkType(const std::string& type) override {
    enabled_[type] = true;
    return !fail_;
  }

  bool DisableNetworkType(const std::string& type) override {
    disabled_[type] = true;
    return !fail_;
  }

  bool RequestScan() override {
    scan_requested_.push_back(true);
    return !fail_;
  }

  void set_fail(bool fail) { fail_ = fail; }
  bool GetEnabled(const std::string& type) { return enabled_[type]; }
  bool GetDisabled(const std::string& type) { return disabled_[type]; }
  size_t GetScanRequested() { return scan_requested_.size(); }

  void DictionaryResult(const std::string& guid,
                        const DictionaryCallback& success_callback,
                        const FailureCallback& failure_callback) {
    if (fail_) {
      failure_callback.Run(kFailure);
    } else {
      std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
      result->SetString(::onc::network_config::kGUID, guid);
      result->SetString(::onc::network_config::kType,
                        ::onc::network_config::kWiFi);
      success_callback.Run(std::move(result));
    }
  }

  void StringResult(const StringCallback& success_callback,
                    const FailureCallback& failure_callback) {
    if (fail_) {
      failure_callback.Run(kFailure);
    } else {
      success_callback.Run(kSuccess);
    }
  }

  void BoolResult(const BoolCallback& success_callback,
                  const FailureCallback& failure_callback) {
    if (fail_) {
      failure_callback.Run(kFailure);
    } else {
      success_callback.Run(true);
    }
  }

  void VoidResult(const VoidCallback& success_callback,
                  const FailureCallback& failure_callback) {
    if (fail_) {
      failure_callback.Run(kFailure);
    } else {
      success_callback.Run();
    }
  }

 private:
  bool fail_;
  std::map<std::string, bool> enabled_;
  std::map<std::string, bool> disabled_;
  std::vector<bool> scan_requested_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class TestVerifyDelegate : public NetworkingPrivateDelegate::VerifyDelegate {
 public:
  TestVerifyDelegate() : owner_(NULL) {}

  ~TestVerifyDelegate() override {}

  void VerifyDestination(
      const VerificationProperties& verification_properties,
      const BoolCallback& success_callback,
      const FailureCallback& failure_callback) override {
    owner_->BoolResult(success_callback, failure_callback);
  }
  void VerifyAndEncryptCredentials(
      const std::string& guid,
      const VerificationProperties& verification_properties,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override {
    owner_->StringResult(success_callback, failure_callback);
  }
  void VerifyAndEncryptData(
      const VerificationProperties& verification_properties,
      const std::string& data,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override {
    owner_->StringResult(success_callback, failure_callback);
  }

  void set_owner(TestDelegate* owner) { owner_ = owner; }

 private:
  TestDelegate* owner_;

  DISALLOW_COPY_AND_ASSIGN(TestVerifyDelegate);
};

class NetworkingPrivateApiTest : public ExtensionApiTest {
 public:
  NetworkingPrivateApiTest() {
    if (!s_test_delegate_) {
      TestVerifyDelegate* verify_delegate = new TestVerifyDelegate;
      std::unique_ptr<NetworkingPrivateDelegate::VerifyDelegate>
          verify_delegate_ptr(verify_delegate);
      s_test_delegate_ = new TestDelegate(std::move(verify_delegate_ptr));
      verify_delegate->set_owner(s_test_delegate_);
    }
  }

  static std::unique_ptr<KeyedService> GetNetworkingPrivateDelegate(
      content::BrowserContext* profile) {
    CHECK(s_test_delegate_);
    return base::WrapUnique(s_test_delegate_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Whitelist the extension ID of the test extension.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "epcifkihnkjgphfkloaaleeakhpmgdmn");
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    NetworkingPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), &NetworkingPrivateApiTest::GetNetworkingPrivateDelegate);
    content::RunAllPendingInMessageLoop();
  }

  bool GetEnabled(const std::string& type) {
    return s_test_delegate_->GetEnabled(type);
  }

  bool GetDisabled(const std::string& type) {
    return s_test_delegate_->GetDisabled(type);
  }

  size_t GetScanRequested() {
    return s_test_delegate_->GetScanRequested();
  }

 protected:
  bool RunNetworkingSubtest(const std::string& subtest) {
    return RunExtensionSubtest("networking_private",
                               "main.html?" + subtest,
                               kFlagEnableFileAccess | kFlagLoadAsComponent);
  }

  // Static pointer to the TestDelegate so that it can be accessed in
  // GetNetworkingPrivateDelegate() passed to SetTestingFactory().
  static TestDelegate* s_test_delegate_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateApiTest);
};

// static
TestDelegate* NetworkingPrivateApiTest::s_test_delegate_ = NULL;

}  // namespace

// Place each subtest into a separate browser test so that the stub networking
// library state is reset for each subtest run. This way they won't affect each
// other. TODO(stevenjb): Use extensions::ApiUnitTest once moved to
// src/extensions.

// These fail on Windows due to crbug.com/177163. Note: we still have partial
// coverage in NetworkingPrivateServiceClientApiTest. TODO(stevenjb): Enable
// these on Windows once we switch to extensions::ApiUnitTest.

#if !defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("getProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetManagedProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("getManagedProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetState) {
  EXPECT_TRUE(RunNetworkingSubtest("getState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, SetProperties) {
  EXPECT_TRUE(RunNetworkingSubtest("setProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, CreateNetwork) {
  EXPECT_TRUE(RunNetworkingSubtest("createNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, ForgetNetwork) {
  EXPECT_TRUE(RunNetworkingSubtest("forgetNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetNetworks) {
  EXPECT_TRUE(RunNetworkingSubtest("getNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetVisibleNetworks) {
  EXPECT_TRUE(RunNetworkingSubtest("getVisibleNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetEnabledNetworkTypes) {
  EXPECT_TRUE(RunNetworkingSubtest("getEnabledNetworkTypes")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetDeviceStates) {
  EXPECT_TRUE(RunNetworkingSubtest("getDeviceStates")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, EnableNetworkType) {
  EXPECT_TRUE(RunNetworkingSubtest("enableNetworkType")) << message_;
  EXPECT_TRUE(GetEnabled(::onc::network_config::kEthernet));
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, DisableNetworkType) {
  EXPECT_TRUE(RunNetworkingSubtest("disableNetworkType")) << message_;
  EXPECT_TRUE(GetDisabled(::onc::network_config::kEthernet));
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, RequestNetworkScan) {
  EXPECT_TRUE(RunNetworkingSubtest("requestNetworkScan")) << message_;
  EXPECT_EQ(1u, GetScanRequested());
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, StartConnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startConnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, StartDisconnect) {
  EXPECT_TRUE(RunNetworkingSubtest("startDisconnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, StartActivate) {
  EXPECT_TRUE(RunNetworkingSubtest("startActivate")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, VerifyDestination) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyDestination")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, VerifyAndEncryptCredentials) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptCredentials")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, VerifyAndEncryptData) {
  EXPECT_TRUE(RunNetworkingSubtest("verifyAndEncryptData")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, SetWifiTDLSEnabledState) {
  EXPECT_TRUE(RunNetworkingSubtest("setWifiTDLSEnabledState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetWifiTDLSStatus) {
  EXPECT_TRUE(RunNetworkingSubtest("getWifiTDLSStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, GetCaptivePortalStatus) {
  EXPECT_TRUE(RunNetworkingSubtest("getCaptivePortalStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, UnlockCellularSim) {
  EXPECT_TRUE(RunNetworkingSubtest("unlockCellularSim")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTest, SetCellularSimState) {
  EXPECT_TRUE(RunNetworkingSubtest("setCellularSimState")) << message_;
}

// Test failure case

class NetworkingPrivateApiTestFail : public NetworkingPrivateApiTest {
 public:
  NetworkingPrivateApiTestFail() { s_test_delegate_->set_fail(true); }

 protected:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateApiTestFail);
};

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetProperties) {
  EXPECT_FALSE(RunNetworkingSubtest("getProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetManagedProperties) {
  EXPECT_FALSE(RunNetworkingSubtest("getManagedProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetState) {
  EXPECT_FALSE(RunNetworkingSubtest("getState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, SetProperties) {
  EXPECT_FALSE(RunNetworkingSubtest("setProperties")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, CreateNetwork) {
  EXPECT_FALSE(RunNetworkingSubtest("createNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, ForgetNetwork) {
  EXPECT_FALSE(RunNetworkingSubtest("forgetNetwork")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetNetworks) {
  EXPECT_FALSE(RunNetworkingSubtest("getNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetVisibleNetworks) {
  EXPECT_FALSE(RunNetworkingSubtest("getVisibleNetworks")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetEnabledNetworkTypes) {
  EXPECT_FALSE(RunNetworkingSubtest("getEnabledNetworkTypes")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetDeviceStates) {
  EXPECT_FALSE(RunNetworkingSubtest("getDeviceStates")) << message_;
}

// Note: Synchronous methods never fail:
// * disableNetworkType
// * enableNetworkType
// * requestNetworkScan

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, StartConnect) {
  EXPECT_FALSE(RunNetworkingSubtest("startConnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, StartDisconnect) {
  EXPECT_FALSE(RunNetworkingSubtest("startDisconnect")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, StartActivate) {
  EXPECT_FALSE(RunNetworkingSubtest("startActivate")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, VerifyDestination) {
  EXPECT_FALSE(RunNetworkingSubtest("verifyDestination")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail,
                       VerifyAndEncryptCredentials) {
  EXPECT_FALSE(RunNetworkingSubtest("verifyAndEncryptCredentials")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, VerifyAndEncryptData) {
  EXPECT_FALSE(RunNetworkingSubtest("verifyAndEncryptData")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, SetWifiTDLSEnabledState) {
  EXPECT_FALSE(RunNetworkingSubtest("setWifiTDLSEnabledState")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetWifiTDLSStatus) {
  EXPECT_FALSE(RunNetworkingSubtest("getWifiTDLSStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, GetCaptivePortalStatus) {
  EXPECT_FALSE(RunNetworkingSubtest("getCaptivePortalStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, UnlockCellularSim) {
  EXPECT_FALSE(RunNetworkingSubtest("unlockCellularSim")) << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateApiTestFail, SetCellularSimState) {
  EXPECT_FALSE(RunNetworkingSubtest("setCellularSimState")) << message_;
}

#endif // defined(OS_WIN)

}  // namespace extensions
