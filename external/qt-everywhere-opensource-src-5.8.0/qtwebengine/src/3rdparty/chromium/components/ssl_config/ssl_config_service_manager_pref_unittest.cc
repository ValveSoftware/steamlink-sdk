// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ssl_config/ssl_config_prefs.h"
#include "components/ssl_config/ssl_config_service_manager.h"
#include "components/ssl_config/ssl_config_switches.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ListValue;
using net::SSLConfig;
using net::SSLConfigService;
using ssl_config::SSLConfigServiceManager;

class SSLConfigServiceManagerPrefTest : public testing::Test {
 public:
  SSLConfigServiceManagerPrefTest() {}

 protected:
  base::MessageLoop message_loop_;
};

// Test channel id with no user prefs.
TEST_F(SSLConfigServiceManagerPrefTest, ChannelIDWithoutUserPrefs) {
  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig config;
  config_service->GetSSLConfig(&config);
  EXPECT_TRUE(config.channel_id_enabled);
}

// Test that cipher suites can be disabled. "Good" refers to the fact that
// every value is expected to be successfully parsed into a cipher suite.
TEST_F(SSLConfigServiceManagerPrefTest, GoodDisabledCipherSuites) {
  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig old_config;
  config_service->GetSSLConfig(&old_config);
  EXPECT_TRUE(old_config.disabled_cipher_suites.empty());

  base::ListValue* list_value = new base::ListValue();
  list_value->AppendString("0x0004");
  list_value->AppendString("0x0005");
  local_state.SetUserPref(ssl_config::prefs::kCipherSuiteBlacklist, list_value);

  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  base::RunLoop().RunUntilIdle();

  SSLConfig config;
  config_service->GetSSLConfig(&config);

  EXPECT_NE(old_config.disabled_cipher_suites, config.disabled_cipher_suites);
  ASSERT_EQ(2u, config.disabled_cipher_suites.size());
  EXPECT_EQ(0x0004, config.disabled_cipher_suites[0]);
  EXPECT_EQ(0x0005, config.disabled_cipher_suites[1]);
}

// Test that cipher suites can be disabled. "Bad" refers to the fact that
// there are one or more non-cipher suite strings in the preference. They
// should be ignored.
TEST_F(SSLConfigServiceManagerPrefTest, BadDisabledCipherSuites) {
  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig old_config;
  config_service->GetSSLConfig(&old_config);
  EXPECT_TRUE(old_config.disabled_cipher_suites.empty());

  base::ListValue* list_value = new base::ListValue();
  list_value->AppendString("0x0004");
  list_value->AppendString("TLS_NOT_WITH_A_CIPHER_SUITE");
  list_value->AppendString("0x0005");
  list_value->AppendString("0xBEEFY");
  local_state.SetUserPref(ssl_config::prefs::kCipherSuiteBlacklist, list_value);

  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  base::RunLoop().RunUntilIdle();

  SSLConfig config;
  config_service->GetSSLConfig(&config);

  EXPECT_NE(old_config.disabled_cipher_suites, config.disabled_cipher_suites);
  ASSERT_EQ(2u, config.disabled_cipher_suites.size());
  EXPECT_EQ(0x0004, config.disabled_cipher_suites[0]);
  EXPECT_EQ(0x0005, config.disabled_cipher_suites[1]);
}

// Test that without command-line settings for minimum and maximum SSL versions,
// TLS versions from 1.0 up to 1.1 or 1.2 are enabled.
TEST_F(SSLConfigServiceManagerPrefTest, NoCommandLinePrefs) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());
  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  // In the absence of command-line options, the default TLS version range is
  // enabled.
  EXPECT_EQ(net::kDefaultSSLVersionMin, ssl_config.version_min);
  EXPECT_EQ(net::kDefaultSSLVersionMax, ssl_config.version_max);

  // The settings should not be added to the local_state.
  EXPECT_FALSE(local_state.HasPrefPath(ssl_config::prefs::kSSLVersionMin));
  EXPECT_FALSE(local_state.HasPrefPath(ssl_config::prefs::kSSLVersionMax));

  // Explicitly double-check the settings are not in the preference store.
  std::string version_min_str;
  std::string version_max_str;
  EXPECT_FALSE(local_state_store->GetString(ssl_config::prefs::kSSLVersionMin,
                                            &version_min_str));
  EXPECT_FALSE(local_state_store->GetString(ssl_config::prefs::kSSLVersionMax,
                                            &version_max_str));
}

// Tests that "ssl3" is not treated as a valid minimum version.
TEST_F(SSLConfigServiceManagerPrefTest, NoSSL3) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  TestingPrefServiceSimple local_state;
  local_state.SetUserPref(ssl_config::prefs::kSSLVersionMin,
                          new base::StringValue("ssl3"));
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  // The command-line option must not have been honored.
  EXPECT_LE(net::SSL_PROTOCOL_VERSION_TLS1, ssl_config.version_min);
}

// Tests that DHE may be re-enabled via features.
TEST_F(SSLConfigServiceManagerPrefTest, DHEFeature) {
  // Toggle the feature.
  base::FeatureList::ClearInstanceForTesting();
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine("DHECiphers", std::string());
  base::FeatureList::SetInstance(std::move(feature_list));

  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  // The feature should have switched the default version_fallback_min value.
  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  EXPECT_TRUE(ssl_config.dhe_enabled);
}
