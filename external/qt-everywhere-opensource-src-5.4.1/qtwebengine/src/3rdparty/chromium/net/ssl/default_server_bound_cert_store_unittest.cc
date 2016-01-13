// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/default_server_bound_cert_store.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void CallCounter(int* counter) {
  (*counter)++;
}

void GetCertCallbackNotCalled(int err,
                              const std::string& server_identifier,
                              base::Time expiration_time,
                              const std::string& private_key_result,
                              const std::string& cert_result) {
  ADD_FAILURE() << "Unexpected callback execution.";
}

class AsyncGetCertHelper {
 public:
  AsyncGetCertHelper() : called_(false) {}

  void Callback(int err,
                const std::string& server_identifier,
                base::Time expiration_time,
                const std::string& private_key_result,
                const std::string& cert_result) {
    err_ = err;
    server_identifier_ = server_identifier;
    expiration_time_ = expiration_time;
    private_key_ = private_key_result;
    cert_ = cert_result;
    called_ = true;
  }

  int err_;
  std::string server_identifier_;
  base::Time expiration_time_;
  std::string private_key_;
  std::string cert_;
  bool called_;
};

void GetAllCallback(
    ServerBoundCertStore::ServerBoundCertList* dest,
    const ServerBoundCertStore::ServerBoundCertList& result) {
  *dest = result;
}

class MockPersistentStore
    : public DefaultServerBoundCertStore::PersistentStore {
 public:
  MockPersistentStore();

  // DefaultServerBoundCertStore::PersistentStore implementation.
  virtual void Load(const LoadedCallback& loaded_callback) OVERRIDE;
  virtual void AddServerBoundCert(
      const DefaultServerBoundCertStore::ServerBoundCert& cert) OVERRIDE;
  virtual void DeleteServerBoundCert(
      const DefaultServerBoundCertStore::ServerBoundCert& cert) OVERRIDE;
  virtual void SetForceKeepSessionState() OVERRIDE;

 protected:
  virtual ~MockPersistentStore();

 private:
  typedef std::map<std::string, DefaultServerBoundCertStore::ServerBoundCert>
      ServerBoundCertMap;

  ServerBoundCertMap origin_certs_;
};

MockPersistentStore::MockPersistentStore() {}

void MockPersistentStore::Load(const LoadedCallback& loaded_callback) {
  scoped_ptr<ScopedVector<DefaultServerBoundCertStore::ServerBoundCert> >
      certs(new ScopedVector<DefaultServerBoundCertStore::ServerBoundCert>());
  ServerBoundCertMap::iterator it;

  for (it = origin_certs_.begin(); it != origin_certs_.end(); ++it) {
    certs->push_back(
        new DefaultServerBoundCertStore::ServerBoundCert(it->second));
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(loaded_callback, base::Passed(&certs)));
}

void MockPersistentStore::AddServerBoundCert(
    const DefaultServerBoundCertStore::ServerBoundCert& cert) {
  origin_certs_[cert.server_identifier()] = cert;
}

void MockPersistentStore::DeleteServerBoundCert(
    const DefaultServerBoundCertStore::ServerBoundCert& cert) {
  origin_certs_.erase(cert.server_identifier());
}

void MockPersistentStore::SetForceKeepSessionState() {}

MockPersistentStore::~MockPersistentStore() {}

}  // namespace

TEST(DefaultServerBoundCertStoreTest, TestLoading) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);

  persistent_store->AddServerBoundCert(
      DefaultServerBoundCertStore::ServerBoundCert(
          "google.com",
          base::Time(),
          base::Time(),
          "a", "b"));
  persistent_store->AddServerBoundCert(
      DefaultServerBoundCertStore::ServerBoundCert(
          "verisign.com",
          base::Time(),
          base::Time(),
          "c", "d"));

  // Make sure certs load properly.
  DefaultServerBoundCertStore store(persistent_store.get());
  // Load has not occurred yet.
  EXPECT_EQ(0, store.GetCertCount());
  store.SetServerBoundCert(
      "verisign.com",
      base::Time(),
      base::Time(),
      "e", "f");
  // Wait for load & queued set task.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, store.GetCertCount());
  store.SetServerBoundCert(
      "twitter.com",
      base::Time(),
      base::Time(),
      "g", "h");
  // Set should be synchronous now that load is done.
  EXPECT_EQ(3, store.GetCertCount());
}

//TODO(mattm): add more tests of without a persistent store?
TEST(DefaultServerBoundCertStoreTest, TestSettingAndGetting) {
  // No persistent store, all calls will be synchronous.
  DefaultServerBoundCertStore store(NULL);
  base::Time expiration_time;
  std::string private_key, cert;
  EXPECT_EQ(0, store.GetCertCount());
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            store.GetServerBoundCert("verisign.com",
                                     &expiration_time,
                                     &private_key,
                                     &cert,
                                     base::Bind(&GetCertCallbackNotCalled)));
  EXPECT_TRUE(private_key.empty());
  EXPECT_TRUE(cert.empty());
  store.SetServerBoundCert(
      "verisign.com",
      base::Time::FromInternalValue(123),
      base::Time::FromInternalValue(456),
      "i", "j");
  EXPECT_EQ(OK,
            store.GetServerBoundCert("verisign.com",
                                     &expiration_time,
                                     &private_key,
                                     &cert,
                                     base::Bind(&GetCertCallbackNotCalled)));
  EXPECT_EQ(456, expiration_time.ToInternalValue());
  EXPECT_EQ("i", private_key);
  EXPECT_EQ("j", cert);
}

TEST(DefaultServerBoundCertStoreTest, TestDuplicateCerts) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultServerBoundCertStore store(persistent_store.get());

  base::Time expiration_time;
  std::string private_key, cert;
  EXPECT_EQ(0, store.GetCertCount());
  store.SetServerBoundCert(
      "verisign.com",
      base::Time::FromInternalValue(123),
      base::Time::FromInternalValue(1234),
      "a", "b");
  store.SetServerBoundCert(
      "verisign.com",
      base::Time::FromInternalValue(456),
      base::Time::FromInternalValue(4567),
      "c", "d");

  // Wait for load & queued set tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, store.GetCertCount());
  EXPECT_EQ(OK,
            store.GetServerBoundCert("verisign.com",
                                     &expiration_time,
                                     &private_key,
                                     &cert,
                                     base::Bind(&GetCertCallbackNotCalled)));
  EXPECT_EQ(4567, expiration_time.ToInternalValue());
  EXPECT_EQ("c", private_key);
  EXPECT_EQ("d", cert);
}

TEST(DefaultServerBoundCertStoreTest, TestAsyncGet) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  persistent_store->AddServerBoundCert(ServerBoundCertStore::ServerBoundCert(
      "verisign.com",
      base::Time::FromInternalValue(123),
      base::Time::FromInternalValue(1234),
      "a", "b"));

  DefaultServerBoundCertStore store(persistent_store.get());
  AsyncGetCertHelper helper;
  base::Time expiration_time;
  std::string private_key;
  std::string cert = "not set";
  EXPECT_EQ(0, store.GetCertCount());
  EXPECT_EQ(ERR_IO_PENDING,
            store.GetServerBoundCert("verisign.com",
                                     &expiration_time,
                                     &private_key,
                                     &cert,
                                     base::Bind(&AsyncGetCertHelper::Callback,
                                                base::Unretained(&helper))));

  // Wait for load & queued get tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, store.GetCertCount());
  EXPECT_EQ("not set", cert);
  EXPECT_TRUE(helper.called_);
  EXPECT_EQ(OK, helper.err_);
  EXPECT_EQ("verisign.com", helper.server_identifier_);
  EXPECT_EQ(1234, helper.expiration_time_.ToInternalValue());
  EXPECT_EQ("a", helper.private_key_);
  EXPECT_EQ("b", helper.cert_);
}

TEST(DefaultServerBoundCertStoreTest, TestDeleteAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultServerBoundCertStore store(persistent_store.get());

  store.SetServerBoundCert(
      "verisign.com",
      base::Time(),
      base::Time(),
      "a", "b");
  store.SetServerBoundCert(
      "google.com",
      base::Time(),
      base::Time(),
      "c", "d");
  store.SetServerBoundCert(
      "harvard.com",
      base::Time(),
      base::Time(),
      "e", "f");
  // Wait for load & queued set tasks.
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(3, store.GetCertCount());
  int delete_finished = 0;
  store.DeleteAll(base::Bind(&CallCounter, &delete_finished));
  ASSERT_EQ(1, delete_finished);
  EXPECT_EQ(0, store.GetCertCount());
}

TEST(DefaultServerBoundCertStoreTest, TestAsyncGetAndDeleteAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  persistent_store->AddServerBoundCert(ServerBoundCertStore::ServerBoundCert(
      "verisign.com",
      base::Time(),
      base::Time(),
      "a", "b"));
  persistent_store->AddServerBoundCert(ServerBoundCertStore::ServerBoundCert(
      "google.com",
      base::Time(),
      base::Time(),
      "c", "d"));

  ServerBoundCertStore::ServerBoundCertList pre_certs;
  ServerBoundCertStore::ServerBoundCertList post_certs;
  int delete_finished = 0;
  DefaultServerBoundCertStore store(persistent_store.get());

  store.GetAllServerBoundCerts(base::Bind(GetAllCallback, &pre_certs));
  store.DeleteAll(base::Bind(&CallCounter, &delete_finished));
  store.GetAllServerBoundCerts(base::Bind(GetAllCallback, &post_certs));
  // Tasks have not run yet.
  EXPECT_EQ(0u, pre_certs.size());
  // Wait for load & queued tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, store.GetCertCount());
  EXPECT_EQ(2u, pre_certs.size());
  EXPECT_EQ(0u, post_certs.size());
}

TEST(DefaultServerBoundCertStoreTest, TestDelete) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultServerBoundCertStore store(persistent_store.get());

  base::Time expiration_time;
  std::string private_key, cert;
  EXPECT_EQ(0, store.GetCertCount());
  store.SetServerBoundCert(
      "verisign.com",
      base::Time(),
      base::Time(),
      "a", "b");
  // Wait for load & queued set task.
  base::MessageLoop::current()->RunUntilIdle();

  store.SetServerBoundCert(
      "google.com",
      base::Time(),
      base::Time(),
      "c", "d");

  EXPECT_EQ(2, store.GetCertCount());
  int delete_finished = 0;
  store.DeleteServerBoundCert("verisign.com",
                              base::Bind(&CallCounter, &delete_finished));
  ASSERT_EQ(1, delete_finished);
  EXPECT_EQ(1, store.GetCertCount());
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            store.GetServerBoundCert("verisign.com",
                                     &expiration_time,
                                     &private_key,
                                     &cert,
                                     base::Bind(&GetCertCallbackNotCalled)));
  EXPECT_EQ(OK,
            store.GetServerBoundCert("google.com",
                                     &expiration_time,
                                     &private_key,
                                     &cert,
                                     base::Bind(&GetCertCallbackNotCalled)));
  int delete2_finished = 0;
  store.DeleteServerBoundCert("google.com",
                              base::Bind(&CallCounter, &delete2_finished));
  ASSERT_EQ(1, delete2_finished);
  EXPECT_EQ(0, store.GetCertCount());
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            store.GetServerBoundCert("google.com",
                                     &expiration_time,
                                     &private_key,
                                     &cert,
                                     base::Bind(&GetCertCallbackNotCalled)));
}

TEST(DefaultServerBoundCertStoreTest, TestAsyncDelete) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  persistent_store->AddServerBoundCert(ServerBoundCertStore::ServerBoundCert(
      "a.com",
      base::Time::FromInternalValue(1),
      base::Time::FromInternalValue(2),
      "a", "b"));
  persistent_store->AddServerBoundCert(ServerBoundCertStore::ServerBoundCert(
      "b.com",
      base::Time::FromInternalValue(3),
      base::Time::FromInternalValue(4),
      "c", "d"));
  DefaultServerBoundCertStore store(persistent_store.get());
  int delete_finished = 0;
  store.DeleteServerBoundCert("a.com",
                              base::Bind(&CallCounter, &delete_finished));

  AsyncGetCertHelper a_helper;
  AsyncGetCertHelper b_helper;
  base::Time expiration_time;
  std::string private_key;
  std::string cert = "not set";
  EXPECT_EQ(0, store.GetCertCount());
  EXPECT_EQ(ERR_IO_PENDING,
      store.GetServerBoundCert(
          "a.com", &expiration_time, &private_key, &cert,
          base::Bind(&AsyncGetCertHelper::Callback,
                     base::Unretained(&a_helper))));
  EXPECT_EQ(ERR_IO_PENDING,
      store.GetServerBoundCert(
          "b.com", &expiration_time, &private_key, &cert,
          base::Bind(&AsyncGetCertHelper::Callback,
                     base::Unretained(&b_helper))));

  EXPECT_EQ(0, delete_finished);
  EXPECT_FALSE(a_helper.called_);
  EXPECT_FALSE(b_helper.called_);
  // Wait for load & queued tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, delete_finished);
  EXPECT_EQ(1, store.GetCertCount());
  EXPECT_EQ("not set", cert);
  EXPECT_TRUE(a_helper.called_);
  EXPECT_EQ(ERR_FILE_NOT_FOUND, a_helper.err_);
  EXPECT_EQ("a.com", a_helper.server_identifier_);
  EXPECT_EQ(0, a_helper.expiration_time_.ToInternalValue());
  EXPECT_EQ("", a_helper.private_key_);
  EXPECT_EQ("", a_helper.cert_);
  EXPECT_TRUE(b_helper.called_);
  EXPECT_EQ(OK, b_helper.err_);
  EXPECT_EQ("b.com", b_helper.server_identifier_);
  EXPECT_EQ(4, b_helper.expiration_time_.ToInternalValue());
  EXPECT_EQ("c", b_helper.private_key_);
  EXPECT_EQ("d", b_helper.cert_);
}

TEST(DefaultServerBoundCertStoreTest, TestGetAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultServerBoundCertStore store(persistent_store.get());

  EXPECT_EQ(0, store.GetCertCount());
  store.SetServerBoundCert(
      "verisign.com",
      base::Time(),
      base::Time(),
      "a", "b");
  store.SetServerBoundCert(
      "google.com",
      base::Time(),
      base::Time(),
      "c", "d");
  store.SetServerBoundCert(
      "harvard.com",
      base::Time(),
      base::Time(),
      "e", "f");
  store.SetServerBoundCert(
      "mit.com",
      base::Time(),
      base::Time(),
      "g", "h");
  // Wait for load & queued set tasks.
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(4, store.GetCertCount());
  ServerBoundCertStore::ServerBoundCertList certs;
  store.GetAllServerBoundCerts(base::Bind(GetAllCallback, &certs));
  EXPECT_EQ(4u, certs.size());
}

TEST(DefaultServerBoundCertStoreTest, TestInitializeFrom) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultServerBoundCertStore store(persistent_store.get());

  store.SetServerBoundCert(
      "preexisting.com",
      base::Time(),
      base::Time(),
      "a", "b");
  store.SetServerBoundCert(
      "both.com",
      base::Time(),
      base::Time(),
      "c", "d");
  // Wait for load & queued set tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, store.GetCertCount());

  ServerBoundCertStore::ServerBoundCertList source_certs;
  source_certs.push_back(ServerBoundCertStore::ServerBoundCert(
      "both.com",
      base::Time(),
      base::Time(),
      // Key differs from above to test that existing entries are overwritten.
      "e", "f"));
  source_certs.push_back(ServerBoundCertStore::ServerBoundCert(
      "copied.com",
      base::Time(),
      base::Time(),
      "g", "h"));
  store.InitializeFrom(source_certs);
  EXPECT_EQ(3, store.GetCertCount());

  ServerBoundCertStore::ServerBoundCertList certs;
  store.GetAllServerBoundCerts(base::Bind(GetAllCallback, &certs));
  ASSERT_EQ(3u, certs.size());

  ServerBoundCertStore::ServerBoundCertList::iterator cert = certs.begin();
  EXPECT_EQ("both.com", cert->server_identifier());
  EXPECT_EQ("e", cert->private_key());

  ++cert;
  EXPECT_EQ("copied.com", cert->server_identifier());
  EXPECT_EQ("g", cert->private_key());

  ++cert;
  EXPECT_EQ("preexisting.com", cert->server_identifier());
  EXPECT_EQ("a", cert->private_key());
}

TEST(DefaultServerBoundCertStoreTest, TestAsyncInitializeFrom) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  persistent_store->AddServerBoundCert(ServerBoundCertStore::ServerBoundCert(
      "preexisting.com",
      base::Time(),
      base::Time(),
      "a", "b"));
  persistent_store->AddServerBoundCert(ServerBoundCertStore::ServerBoundCert(
      "both.com",
      base::Time(),
      base::Time(),
      "c", "d"));

  DefaultServerBoundCertStore store(persistent_store.get());
  ServerBoundCertStore::ServerBoundCertList source_certs;
  source_certs.push_back(ServerBoundCertStore::ServerBoundCert(
      "both.com",
      base::Time(),
      base::Time(),
      // Key differs from above to test that existing entries are overwritten.
      "e", "f"));
  source_certs.push_back(ServerBoundCertStore::ServerBoundCert(
      "copied.com",
      base::Time(),
      base::Time(),
      "g", "h"));
  store.InitializeFrom(source_certs);
  EXPECT_EQ(0, store.GetCertCount());
  // Wait for load & queued tasks.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(3, store.GetCertCount());

  ServerBoundCertStore::ServerBoundCertList certs;
  store.GetAllServerBoundCerts(base::Bind(GetAllCallback, &certs));
  ASSERT_EQ(3u, certs.size());

  ServerBoundCertStore::ServerBoundCertList::iterator cert = certs.begin();
  EXPECT_EQ("both.com", cert->server_identifier());
  EXPECT_EQ("e", cert->private_key());

  ++cert;
  EXPECT_EQ("copied.com", cert->server_identifier());
  EXPECT_EQ("g", cert->private_key());

  ++cert;
  EXPECT_EQ("preexisting.com", cert->server_identifier());
  EXPECT_EQ("a", cert->private_key());
}

}  // namespace net
