// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/pref_proxy_config_tracker_impl.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Mock;

namespace {

const char kFixedPacUrl[] = "http://chromium.org/fixed_pac_url";

// Testing proxy config service that allows us to fire notifications at will.
class TestProxyConfigService : public net::ProxyConfigService {
 public:
  TestProxyConfigService(const net::ProxyConfig& config,
                         ConfigAvailability availability)
      : config_(config),
        availability_(availability) {}

  void SetProxyConfig(const net::ProxyConfig config,
                      ConfigAvailability availability) {
    config_ = config;
    availability_ = availability;
    for (net::ProxyConfigService::Observer& observer : observers_)
      observer.OnProxyConfigChanged(config, availability);
  }

 private:
  void AddObserver(net::ProxyConfigService::Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(net::ProxyConfigService::Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  net::ProxyConfigService::ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfig* config) override {
    *config = config_;
    return availability_;
  }

  net::ProxyConfig config_;
  ConfigAvailability availability_;
  base::ObserverList<net::ProxyConfigService::Observer, true> observers_;
};

// A mock observer for capturing callbacks.
class MockObserver : public net::ProxyConfigService::Observer {
 public:
  MOCK_METHOD2(OnProxyConfigChanged,
               void(const net::ProxyConfig&,
                    net::ProxyConfigService::ConfigAvailability));
};

class PrefProxyConfigTrackerImplTest : public testing::Test {
 protected:
  PrefProxyConfigTrackerImplTest() {
    pref_service_.reset(new TestingPrefServiceSimple());
    PrefProxyConfigTrackerImpl::RegisterPrefs(pref_service_->registry());
    fixed_config_.set_pac_url(GURL(kFixedPacUrl));
    delegate_service_ =
        new TestProxyConfigService(fixed_config_,
                                   net::ProxyConfigService::CONFIG_VALID);
    proxy_config_tracker_.reset(new PrefProxyConfigTrackerImpl(
        pref_service_.get(), base::ThreadTaskRunnerHandle::Get()));
    proxy_config_service_ =
        proxy_config_tracker_->CreateTrackingProxyConfigService(
            std::unique_ptr<net::ProxyConfigService>(delegate_service_));
    // SetProxyConfigServiceImpl triggers update of initial prefs proxy
    // config by tracker to chrome proxy config service, so flush all pending
    // tasks so that tests start fresh.
    base::RunLoop().RunUntilIdle();
  }

  ~PrefProxyConfigTrackerImplTest() override {
    proxy_config_tracker_->DetachFromPrefService();
    base::RunLoop().RunUntilIdle();
    proxy_config_tracker_.reset();
    proxy_config_service_.reset();
  }

  base::MessageLoop loop_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  TestProxyConfigService* delegate_service_; // weak
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  net::ProxyConfig fixed_config_;

 private:
  std::unique_ptr<PrefProxyConfigTrackerImpl> proxy_config_tracker_;
};

TEST_F(PrefProxyConfigTrackerImplTest, BaseConfiguration) {
  net::ProxyConfig actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_EQ(GURL(kFixedPacUrl), actual_config.pac_url());
}

TEST_F(PrefProxyConfigTrackerImplTest, DynamicPrefOverrides) {
  pref_service_->SetManagedPref(proxy_config::prefs::kProxy,
                                ProxyConfigDictionary::CreateFixedServers(
                                    "http://example.com:3128", std::string()));
  base::RunLoop().RunUntilIdle();

  net::ProxyConfig actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_FALSE(actual_config.auto_detect());
  EXPECT_EQ(net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY,
            actual_config.proxy_rules().type);
  EXPECT_EQ(actual_config.proxy_rules().single_proxies.Get(),
            net::ProxyServer::FromURI("http://example.com:3128",
                                      net::ProxyServer::SCHEME_HTTP));

  pref_service_->SetManagedPref(proxy_config::prefs::kProxy,
                                ProxyConfigDictionary::CreateAutoDetect());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.auto_detect());
}

// Compares proxy configurations, but allows different identifiers.
MATCHER_P(ProxyConfigMatches, config, "") {
  net::ProxyConfig reference(config);
  reference.set_id(arg.id());
  return reference.Equals(arg);
}

TEST_F(PrefProxyConfigTrackerImplTest, Observers) {
  const net::ProxyConfigService::ConfigAvailability CONFIG_VALID =
      net::ProxyConfigService::CONFIG_VALID;
  MockObserver observer;
  proxy_config_service_->AddObserver(&observer);

  // Firing the observers in the delegate should trigger a notification.
  net::ProxyConfig config2;
  config2.set_auto_detect(true);
  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(config2),
                                             CONFIG_VALID)).Times(1);
  delegate_service_->SetProxyConfig(config2, CONFIG_VALID);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Override configuration, this should trigger a notification.
  net::ProxyConfig pref_config;
  pref_config.set_pac_url(GURL(kFixedPacUrl));

  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(pref_config),
                                             CONFIG_VALID)).Times(1);
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript(kFixedPacUrl, false));
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Since there are pref overrides, delegate changes should be ignored.
  net::ProxyConfig config3;
  config3.proxy_rules().ParseFromString("http=config3:80");
  EXPECT_CALL(observer, OnProxyConfigChanged(_, _)).Times(0);
  fixed_config_.set_auto_detect(true);
  delegate_service_->SetProxyConfig(config3, CONFIG_VALID);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Clear the override should switch back to the fixed configuration.
  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(config3),
                                             CONFIG_VALID)).Times(1);
  pref_service_->RemoveManagedPref(proxy_config::prefs::kProxy);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Delegate service notifications should show up again.
  net::ProxyConfig config4;
  config4.proxy_rules().ParseFromString("socks:config4");
  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(config4),
                                             CONFIG_VALID)).Times(1);
  delegate_service_->SetProxyConfig(config4, CONFIG_VALID);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  proxy_config_service_->RemoveObserver(&observer);
}

TEST_F(PrefProxyConfigTrackerImplTest, Fallback) {
  const net::ProxyConfigService::ConfigAvailability CONFIG_VALID =
      net::ProxyConfigService::CONFIG_VALID;
  MockObserver observer;
  net::ProxyConfig actual_config;
  delegate_service_->SetProxyConfig(net::ProxyConfig::CreateDirect(),
                                    net::ProxyConfigService::CONFIG_UNSET);
  proxy_config_service_->AddObserver(&observer);

  // Prepare test data.
  net::ProxyConfig recommended_config = net::ProxyConfig::CreateAutoDetect();
  net::ProxyConfig user_config =
      net::ProxyConfig::CreateFromCustomPacURL(GURL(kFixedPacUrl));

  // Set a recommended pref.
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(recommended_config),
                                   CONFIG_VALID)).Times(1);
  pref_service_->SetRecommendedPref(proxy_config::prefs::kProxy,
                                    ProxyConfigDictionary::CreateAutoDetect());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.Equals(recommended_config));

  // Override in user prefs.
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(user_config),
                                   CONFIG_VALID)).Times(1);
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript(kFixedPacUrl, false));
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.Equals(user_config));

  // Go back to recommended pref.
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(recommended_config),
                                   CONFIG_VALID)).Times(1);
  pref_service_->RemoveManagedPref(proxy_config::prefs::kProxy);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.Equals(recommended_config));

  proxy_config_service_->RemoveObserver(&observer);
}

TEST_F(PrefProxyConfigTrackerImplTest, ExplicitSystemSettings) {
  pref_service_->SetRecommendedPref(proxy_config::prefs::kProxy,
                                    ProxyConfigDictionary::CreateAutoDetect());
  pref_service_->SetUserPref(proxy_config::prefs::kProxy,
                             ProxyConfigDictionary::CreateSystem());
  base::RunLoop().RunUntilIdle();

  // Test if we actually use the system setting, which is |kFixedPacUrl|.
  net::ProxyConfig actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_EQ(GURL(kFixedPacUrl), actual_config.pac_url());
}

}  // namespace
