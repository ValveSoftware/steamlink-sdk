// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_persister.h"

#include <map>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "net/http/transport_security_state.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::TransportSecurityPersister;
using net::TransportSecurityState;

class TransportSecurityPersisterTest : public testing::Test {
 public:
  TransportSecurityPersisterTest() {
  }

  virtual ~TransportSecurityPersisterTest() {
    base::MessageLoopForIO::current()->RunUntilIdle();
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    persister_.reset(new TransportSecurityPersister(
        &state_,
        temp_dir_.path(),
        base::MessageLoopForIO::current()->message_loop_proxy(),
        false));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  TransportSecurityState state_;
  scoped_ptr<TransportSecurityPersister> persister_;
};

TEST_F(TransportSecurityPersisterTest, SerializeData1) {
  std::string output;
  bool dirty;

  EXPECT_TRUE(persister_->SerializeData(&output));
  EXPECT_TRUE(persister_->LoadEntries(output, &dirty));
  EXPECT_FALSE(dirty);
}

TEST_F(TransportSecurityPersisterTest, SerializeData2) {
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  static const char kYahooDomain[] = "yahoo.com";

  EXPECT_FALSE(state_.GetStaticDomainState(kYahooDomain, true, &domain_state));
  EXPECT_FALSE(state_.GetDynamicDomainState(kYahooDomain, &domain_state));

  bool include_subdomains = true;
  state_.AddHSTS(kYahooDomain, expiry, include_subdomains);

  std::string output;
  bool dirty;
  EXPECT_TRUE(persister_->SerializeData(&output));
  EXPECT_TRUE(persister_->LoadEntries(output, &dirty));

  EXPECT_TRUE(state_.GetDynamicDomainState(kYahooDomain, &domain_state));
  EXPECT_EQ(domain_state.sts.upgrade_mode,
            TransportSecurityState::DomainState::MODE_FORCE_HTTPS);
  EXPECT_TRUE(state_.GetDynamicDomainState("foo.yahoo.com", &domain_state));
  EXPECT_EQ(domain_state.sts.upgrade_mode,
            TransportSecurityState::DomainState::MODE_FORCE_HTTPS);
  EXPECT_TRUE(state_.GetDynamicDomainState("foo.bar.yahoo.com", &domain_state));
  EXPECT_EQ(domain_state.sts.upgrade_mode,
            TransportSecurityState::DomainState::MODE_FORCE_HTTPS);
  EXPECT_TRUE(
      state_.GetDynamicDomainState("foo.bar.baz.yahoo.com", &domain_state));
  EXPECT_EQ(domain_state.sts.upgrade_mode,
            TransportSecurityState::DomainState::MODE_FORCE_HTTPS);
  EXPECT_FALSE(state_.GetStaticDomainState("com", true, &domain_state));
}

TEST_F(TransportSecurityPersisterTest, SerializeData3) {
  // Add an entry.
  net::HashValue fp1(net::HASH_VALUE_SHA1);
  memset(fp1.data(), 0, fp1.size());
  net::HashValue fp2(net::HASH_VALUE_SHA1);
  memset(fp2.data(), 1, fp2.size());
  base::Time expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(1000);
  net::HashValueVector dynamic_spki_hashes;
  dynamic_spki_hashes.push_back(fp1);
  dynamic_spki_hashes.push_back(fp2);
  bool include_subdomains = false;
  state_.AddHSTS("www.example.com", expiry, include_subdomains);
  state_.AddHPKP("www.example.com", expiry, include_subdomains,
                 dynamic_spki_hashes);

  // Add another entry.
  memset(fp1.data(), 2, fp1.size());
  memset(fp2.data(), 3, fp2.size());
  expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(3000);
  dynamic_spki_hashes.push_back(fp1);
  dynamic_spki_hashes.push_back(fp2);
  state_.AddHSTS("www.example.net", expiry, include_subdomains);
  state_.AddHPKP("www.example.net", expiry, include_subdomains,
                 dynamic_spki_hashes);

  // Save a copy of everything.
  std::map<std::string, TransportSecurityState::DomainState> saved;
  TransportSecurityState::Iterator i(state_);
  while (i.HasNext()) {
    saved[i.hostname()] = i.domain_state();
    i.Advance();
  }

  std::string serialized;
  EXPECT_TRUE(persister_->SerializeData(&serialized));

  // Persist the data to the file. For the test to be fast and not flaky, we
  // just do it directly rather than call persister_->StateIsDirty. (That uses
  // ImportantFileWriter, which has an asynchronous commit interval rather
  // than block.) Use a different basename just for cleanliness.
  base::FilePath path =
      temp_dir_.path().AppendASCII("TransportSecurityPersisterTest");
  EXPECT_TRUE(base::WriteFile(path, serialized.c_str(), serialized.size()));

  // Read the data back.
  std::string persisted;
  EXPECT_TRUE(base::ReadFileToString(path, &persisted));
  EXPECT_EQ(persisted, serialized);
  bool dirty;
  EXPECT_TRUE(persister_->LoadEntries(persisted, &dirty));
  EXPECT_FALSE(dirty);

  // Check that states are the same as saved.
  size_t count = 0;
  TransportSecurityState::Iterator j(state_);
  while (j.HasNext()) {
    count++;
    j.Advance();
  }
  EXPECT_EQ(count, saved.size());
}

TEST_F(TransportSecurityPersisterTest, SerializeDataOld) {
  // This is an old-style piece of transport state JSON, which has no creation
  // date.
  std::string output =
      "{ "
      "\"NiyD+3J1r6z1wjl2n1ALBu94Zj9OsEAMo0kCN8js0Uk=\": {"
      "\"expiry\": 1266815027.983453, "
      "\"include_subdomains\": false, "
      "\"mode\": \"strict\" "
      "}"
      "}";
  bool dirty;
  EXPECT_TRUE(persister_->LoadEntries(output, &dirty));
  EXPECT_TRUE(dirty);
}

TEST_F(TransportSecurityPersisterTest, PublicKeyHashes) {
  TransportSecurityState::DomainState domain_state;
  static const char kTestDomain[] = "example.com";
  EXPECT_FALSE(state_.GetDynamicDomainState(kTestDomain, &domain_state));
  net::HashValueVector hashes;
  std::string failure_log;
  EXPECT_FALSE(domain_state.CheckPublicKeyPins(hashes, &failure_log));

  net::HashValue sha1(net::HASH_VALUE_SHA1);
  memset(sha1.data(), '1', sha1.size());
  domain_state.pkp.spki_hashes.push_back(sha1);

  EXPECT_FALSE(domain_state.CheckPublicKeyPins(hashes, &failure_log));

  hashes.push_back(sha1);
  EXPECT_TRUE(domain_state.CheckPublicKeyPins(hashes, &failure_log));

  hashes[0].data()[0] = '2';
  EXPECT_FALSE(domain_state.CheckPublicKeyPins(hashes, &failure_log));

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  bool include_subdomains = false;
  state_.AddHSTS(kTestDomain, expiry, include_subdomains);
  state_.AddHPKP(
      kTestDomain, expiry, include_subdomains, domain_state.pkp.spki_hashes);
  std::string serialized;
  EXPECT_TRUE(persister_->SerializeData(&serialized));
  bool dirty;
  EXPECT_TRUE(persister_->LoadEntries(serialized, &dirty));

  TransportSecurityState::DomainState new_domain_state;
  EXPECT_TRUE(state_.GetDynamicDomainState(kTestDomain, &new_domain_state));
  EXPECT_EQ(1u, new_domain_state.pkp.spki_hashes.size());
  EXPECT_EQ(sha1.tag, new_domain_state.pkp.spki_hashes[0].tag);
  EXPECT_EQ(0,
            memcmp(new_domain_state.pkp.spki_hashes[0].data(),
                   sha1.data(),
                   sha1.size()));
}
