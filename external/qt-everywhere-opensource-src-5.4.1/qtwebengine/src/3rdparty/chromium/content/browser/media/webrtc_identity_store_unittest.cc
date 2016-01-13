// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "content/browser/media/webrtc_identity_store.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"
#include "sql/connection.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

static const char* kFakeOrigin = "http://foo.com";
static const char* kFakeIdentityName1 = "name1";
static const char* kFakeIdentityName2 = "name2";
static const char* kFakeCommonName1 = "cname1";
static const char* kFakeCommonName2 = "cname2";

static void OnRequestCompleted(bool* completed,
                               std::string* out_cert,
                               std::string* out_key,
                               int error,
                               const std::string& certificate,
                               const std::string& private_key) {
  ASSERT_EQ(net::OK, error);
  ASSERT_NE("", certificate);
  ASSERT_NE("", private_key);
  *completed = true;
  *out_cert = certificate;
  *out_key = private_key;
}

class WebRTCIdentityStoreTest : public testing::Test {
 public:
  WebRTCIdentityStoreTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP |
                               TestBrowserThreadBundle::REAL_DB_THREAD),
        pool_owner_(
            new base::SequencedWorkerPoolOwner(3, "WebRTCIdentityStoreTest")),
        webrtc_identity_store_(
            new WebRTCIdentityStore(base::FilePath(), NULL)) {
    webrtc_identity_store_->SetTaskRunnerForTesting(pool_owner_->pool());
  }

  virtual ~WebRTCIdentityStoreTest() {
    pool_owner_->pool()->Shutdown();
  }

  void SetValidityPeriod(base::TimeDelta validity_period) {
    webrtc_identity_store_->SetValidityPeriodForTesting(validity_period);
  }

  void RunUntilIdle() {
    RunAllPendingInMessageLoop(BrowserThread::DB);
    RunAllPendingInMessageLoop(BrowserThread::IO);
    pool_owner_->pool()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  base::Closure RequestIdentityAndRunUtilIdle(const std::string& origin,
                                              const std::string& identity_name,
                                              const std::string& common_name,
                                              bool* completed,
                                              std::string* certificate,
                                              std::string* private_key) {
    base::Closure cancel_callback = webrtc_identity_store_->RequestIdentity(
        GURL(origin),
        identity_name,
        common_name,
        base::Bind(&OnRequestCompleted, completed, certificate, private_key));
    EXPECT_FALSE(cancel_callback.is_null());
    RunUntilIdle();
    return cancel_callback;
  }

  void Restart(const base::FilePath& path) {
    webrtc_identity_store_ = new WebRTCIdentityStore(path, NULL);
    webrtc_identity_store_->SetTaskRunnerForTesting(pool_owner_->pool());
  }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<base::SequencedWorkerPoolOwner> pool_owner_;
  scoped_refptr<WebRTCIdentityStore> webrtc_identity_store_;
};

TEST_F(WebRTCIdentityStoreTest, RequestIdentity) {
  bool completed = false;
  std::string dummy;
  base::Closure cancel_callback =
      RequestIdentityAndRunUtilIdle(kFakeOrigin,
                                    kFakeIdentityName1,
                                    kFakeCommonName1,
                                    &completed,
                                    &dummy,
                                    &dummy);
  EXPECT_TRUE(completed);
}

TEST_F(WebRTCIdentityStoreTest, CancelRequest) {
  bool completed = false;
  std::string dummy;
  base::Closure cancel_callback = webrtc_identity_store_->RequestIdentity(
      GURL(kFakeOrigin),
      kFakeIdentityName1,
      kFakeCommonName1,
      base::Bind(&OnRequestCompleted, &completed, &dummy, &dummy));
  ASSERT_FALSE(cancel_callback.is_null());
  cancel_callback.Run();

  RunUntilIdle();
  EXPECT_FALSE(completed);
}

TEST_F(WebRTCIdentityStoreTest, ConcurrentUniqueRequests) {
  bool completed_1 = false;
  bool completed_2 = false;
  std::string dummy;
  base::Closure cancel_callback_1 = webrtc_identity_store_->RequestIdentity(
      GURL(kFakeOrigin),
      kFakeIdentityName1,
      kFakeCommonName1,
      base::Bind(&OnRequestCompleted, &completed_1, &dummy, &dummy));
  ASSERT_FALSE(cancel_callback_1.is_null());

  base::Closure cancel_callback_2 = webrtc_identity_store_->RequestIdentity(
      GURL(kFakeOrigin),
      kFakeIdentityName2,
      kFakeCommonName1,
      base::Bind(&OnRequestCompleted, &completed_2, &dummy, &dummy));
  ASSERT_FALSE(cancel_callback_2.is_null());

  RunUntilIdle();
  EXPECT_TRUE(completed_1);
  EXPECT_TRUE(completed_2);
}

TEST_F(WebRTCIdentityStoreTest, DifferentCommonNameReturnNewIdentity) {
  bool completed_1 = false;
  bool completed_2 = false;
  std::string cert_1, cert_2, key_1, key_2;

  base::Closure cancel_callback_1 =
      RequestIdentityAndRunUtilIdle(kFakeOrigin,
                                    kFakeIdentityName1,
                                    kFakeCommonName1,
                                    &completed_1,
                                    &cert_1,
                                    &key_1);

  base::Closure cancel_callback_2 =
      RequestIdentityAndRunUtilIdle(kFakeOrigin,
                                    kFakeIdentityName1,
                                    kFakeCommonName2,
                                    &completed_2,
                                    &cert_2,
                                    &key_2);

  EXPECT_TRUE(completed_1);
  EXPECT_TRUE(completed_2);
  EXPECT_NE(cert_1, cert_2);
  EXPECT_NE(key_1, key_2);
}

TEST_F(WebRTCIdentityStoreTest, SerialIdenticalRequests) {
  bool completed_1 = false;
  bool completed_2 = false;
  std::string cert_1, cert_2, key_1, key_2;

  base::Closure cancel_callback_1 =
      RequestIdentityAndRunUtilIdle(kFakeOrigin,
                                    kFakeIdentityName1,
                                    kFakeCommonName1,
                                    &completed_1,
                                    &cert_1,
                                    &key_1);

  base::Closure cancel_callback_2 =
      RequestIdentityAndRunUtilIdle(kFakeOrigin,
                                    kFakeIdentityName1,
                                    kFakeCommonName1,
                                    &completed_2,
                                    &cert_2,
                                    &key_2);

  EXPECT_TRUE(completed_1);
  EXPECT_TRUE(completed_2);
  EXPECT_EQ(cert_1, cert_2);
  EXPECT_EQ(key_1, key_2);
}

TEST_F(WebRTCIdentityStoreTest, ConcurrentIdenticalRequestsJoined) {
  bool completed_1 = false;
  bool completed_2 = false;
  std::string cert_1, cert_2, key_1, key_2;

  base::Closure cancel_callback_1 = webrtc_identity_store_->RequestIdentity(
      GURL(kFakeOrigin),
      kFakeIdentityName1,
      kFakeCommonName1,
      base::Bind(&OnRequestCompleted, &completed_1, &cert_1, &key_1));
  ASSERT_FALSE(cancel_callback_1.is_null());

  base::Closure cancel_callback_2 = webrtc_identity_store_->RequestIdentity(
      GURL(kFakeOrigin),
      kFakeIdentityName1,
      kFakeCommonName1,
      base::Bind(&OnRequestCompleted, &completed_2, &cert_2, &key_2));
  ASSERT_FALSE(cancel_callback_2.is_null());

  RunUntilIdle();
  EXPECT_TRUE(completed_1);
  EXPECT_TRUE(completed_2);
  EXPECT_EQ(cert_1, cert_2);
  EXPECT_EQ(key_1, key_2);
}

TEST_F(WebRTCIdentityStoreTest, CancelOneOfIdenticalRequests) {
  bool completed_1 = false;
  bool completed_2 = false;
  std::string cert_1, cert_2, key_1, key_2;

  base::Closure cancel_callback_1 = webrtc_identity_store_->RequestIdentity(
      GURL(kFakeOrigin),
      kFakeIdentityName1,
      kFakeCommonName1,
      base::Bind(&OnRequestCompleted, &completed_1, &cert_1, &key_1));
  ASSERT_FALSE(cancel_callback_1.is_null());

  base::Closure cancel_callback_2 = webrtc_identity_store_->RequestIdentity(
      GURL(kFakeOrigin),
      kFakeIdentityName1,
      kFakeCommonName1,
      base::Bind(&OnRequestCompleted, &completed_2, &cert_2, &key_2));
  ASSERT_FALSE(cancel_callback_2.is_null());

  cancel_callback_1.Run();

  RunUntilIdle();
  EXPECT_FALSE(completed_1);
  EXPECT_TRUE(completed_2);
}

TEST_F(WebRTCIdentityStoreTest, DeleteDataAndGenerateNewIdentity) {
  bool completed_1 = false;
  bool completed_2 = false;
  std::string cert_1, cert_2, key_1, key_2;

  // Generate the first identity.
  base::Closure cancel_callback_1 =
      RequestIdentityAndRunUtilIdle(kFakeOrigin,
                                    kFakeIdentityName1,
                                    kFakeCommonName1,
                                    &completed_1,
                                    &cert_1,
                                    &key_1);

  // Clear the data and the second request should return a new identity.
  webrtc_identity_store_->DeleteBetween(
      base::Time(), base::Time::Now(), base::Bind(&base::DoNothing));
  RunUntilIdle();

  base::Closure cancel_callback_2 =
      RequestIdentityAndRunUtilIdle(kFakeOrigin,
                                    kFakeIdentityName1,
                                    kFakeCommonName1,
                                    &completed_2,
                                    &cert_2,
                                    &key_2);

  EXPECT_TRUE(completed_1);
  EXPECT_TRUE(completed_2);
  EXPECT_NE(cert_1, cert_2);
  EXPECT_NE(key_1, key_2);
}

TEST_F(WebRTCIdentityStoreTest, ExpiredIdentityDeleted) {
  // The identities will expire immediately after creation.
  SetValidityPeriod(base::TimeDelta::FromMilliseconds(0));

  bool completed_1 = false;
  bool completed_2 = false;
  std::string cert_1, cert_2, key_1, key_2;

  base::Closure cancel_callback_1 =
      RequestIdentityAndRunUtilIdle(kFakeOrigin,
                                    kFakeIdentityName1,
                                    kFakeCommonName1,
                                    &completed_1,
                                    &cert_1,
                                    &key_1);
  EXPECT_TRUE(completed_1);

  // Check that the old identity is not returned.
  base::Closure cancel_callback_2 =
      RequestIdentityAndRunUtilIdle(kFakeOrigin,
                                    kFakeIdentityName1,
                                    kFakeCommonName1,
                                    &completed_2,
                                    &cert_2,
                                    &key_2);
  EXPECT_TRUE(completed_2);
  EXPECT_NE(cert_1, cert_2);
  EXPECT_NE(key_1, key_2);
}

TEST_F(WebRTCIdentityStoreTest, IdentityPersistentAcrossRestart) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  Restart(temp_dir.path());

  bool completed_1 = false;
  bool completed_2 = false;
  std::string cert_1, cert_2, key_1, key_2;

  // Creates an identity.
  base::Closure cancel_callback_1 =
      RequestIdentityAndRunUtilIdle(kFakeOrigin,
                                    kFakeIdentityName1,
                                    kFakeCommonName1,
                                    &completed_1,
                                    &cert_1,
                                    &key_1);
  EXPECT_TRUE(completed_1);

  Restart(temp_dir.path());

  // Check that the same identity is returned after the restart.
  base::Closure cancel_callback_2 =
      RequestIdentityAndRunUtilIdle(kFakeOrigin,
                                    kFakeIdentityName1,
                                    kFakeCommonName1,
                                    &completed_2,
                                    &cert_2,
                                    &key_2);
  EXPECT_TRUE(completed_2);
  EXPECT_EQ(cert_1, cert_2);
  EXPECT_EQ(key_1, key_2);
}

TEST_F(WebRTCIdentityStoreTest, HandleDBErrors) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  Restart(temp_dir.path());

  bool completed_1 = false;
  std::string cert_1, key_1;

  // Creates an identity.
  RequestIdentityAndRunUtilIdle(kFakeOrigin,
                                kFakeIdentityName1,
                                kFakeCommonName1,
                                &completed_1,
                                &cert_1,
                                &key_1);

  // Make the table corrupted.
  base::FilePath db_path =
      temp_dir.path().Append(FILE_PATH_LITERAL("WebRTCIdentityStore"));
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(db_path));

  // Reset to commit the DB changes, which should fail and not crash.
  webrtc_identity_store_ = NULL;
  RunUntilIdle();

  // Verifies the corrupted table was razed.
  scoped_ptr<sql::Connection> db(new sql::Connection());
  EXPECT_TRUE(db->Open(db_path));
  EXPECT_EQ(0U, sql::test::CountSQLTables(db.get()));
}

}  // namespace content
