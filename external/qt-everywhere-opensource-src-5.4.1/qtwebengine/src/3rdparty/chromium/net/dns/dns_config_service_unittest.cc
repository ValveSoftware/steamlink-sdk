// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "base/test/test_timeouts.h"
#include "net/base/net_util.h"
#include "net/dns/dns_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const NameServerClassifier::NameServersType kNone =
    NameServerClassifier::NAME_SERVERS_TYPE_NONE;
const NameServerClassifier::NameServersType kGoogle =
    NameServerClassifier::NAME_SERVERS_TYPE_GOOGLE_PUBLIC_DNS;
const NameServerClassifier::NameServersType kPrivate =
    NameServerClassifier::NAME_SERVERS_TYPE_PRIVATE;
const NameServerClassifier::NameServersType kPublic =
    NameServerClassifier::NAME_SERVERS_TYPE_PUBLIC;
const NameServerClassifier::NameServersType kMixed =
    NameServerClassifier::NAME_SERVERS_TYPE_MIXED;

class NameServerClassifierTest : public testing::Test {
 protected:
  NameServerClassifier::NameServersType Classify(
      const std::string& servers_string) {
    std::vector<std::string> server_strings;
    base::SplitString(servers_string, ' ', &server_strings);

    std::vector<IPEndPoint> servers;
    for (std::vector<std::string>::const_iterator it = server_strings.begin();
         it != server_strings.end();
         ++it) {
      if (*it == "")
        continue;

      IPAddressNumber address;
      bool parsed = ParseIPLiteralToNumber(*it, &address);
      EXPECT_TRUE(parsed);
      servers.push_back(IPEndPoint(address, dns_protocol::kDefaultPort));
    }

    return classifier_.GetNameServersType(servers);
  }

 private:
  NameServerClassifier classifier_;
};

TEST_F(NameServerClassifierTest, None) {
  EXPECT_EQ(kNone, Classify(""));
}

TEST_F(NameServerClassifierTest, Google) {
  EXPECT_EQ(kGoogle, Classify("8.8.8.8"));
  EXPECT_EQ(kGoogle, Classify("8.8.8.8 8.8.4.4"));
  EXPECT_EQ(kGoogle, Classify("2001:4860:4860::8888"));
  EXPECT_EQ(kGoogle, Classify("2001:4860:4860::8888 2001:4860:4860::8844"));
  EXPECT_EQ(kGoogle, Classify("2001:4860:4860::8888 8.8.8.8"));

  // Make sure nobody took any shortcuts on the IP matching:
  EXPECT_EQ(kPublic, Classify("8.8.8.4"));
  EXPECT_EQ(kPublic, Classify("8.8.4.8"));
  EXPECT_EQ(kPublic, Classify("2001:4860:4860::8884"));
  EXPECT_EQ(kPublic, Classify("2001:4860:4860::8848"));
  EXPECT_EQ(kPublic, Classify("2001:4860:4860::1:8888"));
  EXPECT_EQ(kPublic, Classify("2001:4860:4860:1::8888"));
}

TEST_F(NameServerClassifierTest, PrivateLocalhost) {
  EXPECT_EQ(kPrivate, Classify("127.0.0.1"));
  EXPECT_EQ(kPrivate, Classify("::1"));
}

TEST_F(NameServerClassifierTest, PrivateRfc1918) {
  EXPECT_EQ(kPrivate, Classify("10.0.0.0 10.255.255.255"));
  EXPECT_EQ(kPrivate, Classify("172.16.0.0 172.31.255.255"));
  EXPECT_EQ(kPrivate, Classify("192.168.0.0 192.168.255.255"));
  EXPECT_EQ(kPrivate, Classify("10.1.1.1 172.16.1.1 192.168.1.1"));
}

TEST_F(NameServerClassifierTest, PrivateIPv4LinkLocal) {
  EXPECT_EQ(kPrivate, Classify("169.254.0.0 169.254.255.255"));
}

TEST_F(NameServerClassifierTest, PrivateIPv6LinkLocal) {
  EXPECT_EQ(kPrivate,
      Classify("fe80:: fe80:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
}

TEST_F(NameServerClassifierTest, Public) {
  EXPECT_EQ(kPublic, Classify("4.2.2.1"));
  EXPECT_EQ(kPublic, Classify("4.2.2.1 4.2.2.2"));
}

TEST_F(NameServerClassifierTest, Mixed) {
  EXPECT_EQ(kMixed, Classify("8.8.8.8 192.168.1.1"));
  EXPECT_EQ(kMixed, Classify("8.8.8.8 4.2.2.1"));
  EXPECT_EQ(kMixed, Classify("192.168.1.1 4.2.2.1"));
  EXPECT_EQ(kMixed, Classify("8.8.8.8 192.168.1.1 4.2.2.1"));
}

class DnsConfigServiceTest : public testing::Test {
 public:
  void OnConfigChanged(const DnsConfig& config) {
    last_config_ = config;
    if (quit_on_config_)
      base::MessageLoop::current()->Quit();
  }

 protected:
  class TestDnsConfigService : public DnsConfigService {
   public:
    virtual void ReadNow() OVERRIDE {}
    virtual bool StartWatching() OVERRIDE { return true; }

    // Expose the protected methods to this test suite.
    void InvalidateConfig() {
      DnsConfigService::InvalidateConfig();
    }

    void InvalidateHosts() {
      DnsConfigService::InvalidateHosts();
    }

    void OnConfigRead(const DnsConfig& config) {
      DnsConfigService::OnConfigRead(config);
    }

    void OnHostsRead(const DnsHosts& hosts) {
      DnsConfigService::OnHostsRead(hosts);
    }

    void set_watch_failed(bool value) {
      DnsConfigService::set_watch_failed(value);
    }
  };

  void WaitForConfig(base::TimeDelta timeout) {
    base::CancelableClosure closure(base::MessageLoop::QuitClosure());
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, closure.callback(), timeout);
    quit_on_config_ = true;
    base::MessageLoop::current()->Run();
    quit_on_config_ = false;
    closure.Cancel();
  }

  // Generate a config using the given seed..
  DnsConfig MakeConfig(unsigned seed) {
    DnsConfig config;
    IPAddressNumber ip;
    CHECK(ParseIPLiteralToNumber("1.2.3.4", &ip));
    config.nameservers.push_back(IPEndPoint(ip, seed & 0xFFFF));
    EXPECT_TRUE(config.IsValid());
    return config;
  }

  // Generate hosts using the given seed.
  DnsHosts MakeHosts(unsigned seed) {
    DnsHosts hosts;
    std::string hosts_content = "127.0.0.1 localhost";
    hosts_content.append(seed, '1');
    ParseHosts(hosts_content, &hosts);
    EXPECT_FALSE(hosts.empty());
    return hosts;
  }

  virtual void SetUp() OVERRIDE {
    quit_on_config_ = false;

    service_.reset(new TestDnsConfigService());
    service_->WatchConfig(base::Bind(&DnsConfigServiceTest::OnConfigChanged,
                                     base::Unretained(this)));
    EXPECT_FALSE(last_config_.IsValid());
  }

  DnsConfig last_config_;
  bool quit_on_config_;

  // Service under test.
  scoped_ptr<TestDnsConfigService> service_;
};

}  // namespace

TEST_F(DnsConfigServiceTest, FirstConfig) {
  DnsConfig config = MakeConfig(1);

  service_->OnConfigRead(config);
  // No hosts yet, so no config.
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  service_->OnHostsRead(config.hosts);
  EXPECT_TRUE(last_config_.Equals(config));
}

TEST_F(DnsConfigServiceTest, Timeout) {
  DnsConfig config = MakeConfig(1);
  config.hosts = MakeHosts(1);
  ASSERT_TRUE(config.IsValid());

  service_->OnConfigRead(config);
  service_->OnHostsRead(config.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config));

  service_->InvalidateConfig();
  WaitForConfig(TestTimeouts::action_timeout());
  EXPECT_FALSE(last_config_.Equals(config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  service_->OnConfigRead(config);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config));

  service_->InvalidateHosts();
  WaitForConfig(TestTimeouts::action_timeout());
  EXPECT_FALSE(last_config_.Equals(config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  DnsConfig bad_config = last_config_ = MakeConfig(0xBAD);
  service_->InvalidateConfig();
  // We don't expect an update. This should time out.
  WaitForConfig(base::TimeDelta::FromMilliseconds(100) +
                TestTimeouts::tiny_timeout());
  EXPECT_TRUE(last_config_.Equals(bad_config)) << "Unexpected change";

  last_config_ = DnsConfig();
  service_->OnConfigRead(config);
  service_->OnHostsRead(config.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config));
}

TEST_F(DnsConfigServiceTest, SameConfig) {
  DnsConfig config = MakeConfig(1);
  config.hosts = MakeHosts(1);

  service_->OnConfigRead(config);
  service_->OnHostsRead(config.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config));

  last_config_ = DnsConfig();
  service_->OnConfigRead(config);
  EXPECT_TRUE(last_config_.Equals(DnsConfig())) << "Unexpected change";

  service_->OnHostsRead(config.hosts);
  EXPECT_TRUE(last_config_.Equals(DnsConfig())) << "Unexpected change";
}

TEST_F(DnsConfigServiceTest, DifferentConfig) {
  DnsConfig config1 = MakeConfig(1);
  DnsConfig config2 = MakeConfig(2);
  DnsConfig config3 = MakeConfig(1);
  config1.hosts = MakeHosts(1);
  config2.hosts = MakeHosts(1);
  config3.hosts = MakeHosts(2);
  ASSERT_TRUE(config1.EqualsIgnoreHosts(config3));
  ASSERT_FALSE(config1.Equals(config2));
  ASSERT_FALSE(config1.Equals(config3));
  ASSERT_FALSE(config2.Equals(config3));

  service_->OnConfigRead(config1);
  service_->OnHostsRead(config1.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config1));

  // It doesn't matter for this tests, but increases coverage.
  service_->InvalidateConfig();
  service_->InvalidateHosts();

  service_->OnConfigRead(config2);
  EXPECT_TRUE(last_config_.Equals(config1)) << "Unexpected change";
  service_->OnHostsRead(config2.hosts);  // Not an actual change.
  EXPECT_FALSE(last_config_.Equals(config1));
  EXPECT_TRUE(last_config_.Equals(config2));

  service_->OnConfigRead(config3);
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config3));
  service_->OnHostsRead(config3.hosts);
  EXPECT_FALSE(last_config_.Equals(config2));
  EXPECT_TRUE(last_config_.Equals(config3));
}

TEST_F(DnsConfigServiceTest, WatchFailure) {
  DnsConfig config1 = MakeConfig(1);
  DnsConfig config2 = MakeConfig(2);
  config1.hosts = MakeHosts(1);
  config2.hosts = MakeHosts(2);

  service_->OnConfigRead(config1);
  service_->OnHostsRead(config1.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config1));

  // Simulate watch failure.
  service_->set_watch_failed(true);
  service_->InvalidateConfig();
  WaitForConfig(TestTimeouts::action_timeout());
  EXPECT_FALSE(last_config_.Equals(config1));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  DnsConfig bad_config = last_config_ = MakeConfig(0xBAD);
  // Actual change in config, so expect an update, but it should be empty.
  service_->OnConfigRead(config1);
  EXPECT_FALSE(last_config_.Equals(bad_config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  last_config_ = bad_config;
  // Actual change in config, so expect an update, but it should be empty.
  service_->InvalidateConfig();
  service_->OnConfigRead(config2);
  EXPECT_FALSE(last_config_.Equals(bad_config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  last_config_ = bad_config;
  // No change, so no update.
  service_->InvalidateConfig();
  service_->OnConfigRead(config2);
  EXPECT_TRUE(last_config_.Equals(bad_config));
}

#if (defined(OS_POSIX) && !defined(OS_ANDROID)) || defined(OS_WIN)
// TODO(szym): This is really an integration test and can time out if HOSTS is
// huge. http://crbug.com/107810
TEST_F(DnsConfigServiceTest, DISABLED_GetSystemConfig) {
  service_.reset();
  scoped_ptr<DnsConfigService> service(DnsConfigService::CreateSystemService());

  service->ReadConfig(base::Bind(&DnsConfigServiceTest::OnConfigChanged,
                                 base::Unretained(this)));
  base::TimeDelta kTimeout = TestTimeouts::action_max_timeout();
  WaitForConfig(kTimeout);
  ASSERT_TRUE(last_config_.IsValid()) << "Did not receive DnsConfig in " <<
      kTimeout.InSecondsF() << "s";
}
#endif  // OS_POSIX || OS_WIN

}  // namespace net

