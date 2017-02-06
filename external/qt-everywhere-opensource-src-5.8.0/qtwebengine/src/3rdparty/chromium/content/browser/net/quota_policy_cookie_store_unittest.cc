// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "content/browser/net/quota_policy_cookie_store.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/cookies/cookie_util.h"
#include "net/ssl/ssl_client_cert_type.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const base::FilePath::CharType kTestCookiesFilename[] =
    FILE_PATH_LITERAL("Cookies");
}

namespace content {
namespace {

typedef std::vector<net::CanonicalCookie*> CanonicalCookieVector;

class QuotaPolicyCookieStoreTest : public testing::Test {
 public:
  QuotaPolicyCookieStoreTest()
      : pool_owner_(new base::SequencedWorkerPoolOwner(3, "Background Pool")),
        loaded_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
        destroy_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void OnLoaded(const CanonicalCookieVector& cookies) {
    cookies_ = cookies;
    loaded_event_.Signal();
  }

  void Load(CanonicalCookieVector* cookies) {
    EXPECT_FALSE(loaded_event_.IsSignaled());
    store_->Load(base::Bind(&QuotaPolicyCookieStoreTest::OnLoaded,
                            base::Unretained(this)));
    loaded_event_.Wait();
    *cookies = cookies_;
  }

  void ReleaseStore() {
    EXPECT_TRUE(background_task_runner()->RunsTasksOnCurrentThread());
    store_ = nullptr;
    destroy_event_.Signal();
  }

  void DestroyStoreOnBackgroundThread() {
    background_task_runner()->PostTask(
        FROM_HERE, base::Bind(&QuotaPolicyCookieStoreTest::ReleaseStore,
                              base::Unretained(this)));
    destroy_event_.Wait();
    DestroyStore();
  }

 protected:
  scoped_refptr<base::SequencedTaskRunner> background_task_runner() {
    return pool_owner_->pool()->GetSequencedTaskRunner(
        pool_owner_->pool()->GetNamedSequenceToken("background"));
  }

  scoped_refptr<base::SequencedTaskRunner> client_task_runner() {
    return pool_owner_->pool()->GetSequencedTaskRunner(
        pool_owner_->pool()->GetNamedSequenceToken("client"));
  }

  void CreateAndLoad(storage::SpecialStoragePolicy* storage_policy,
                     CanonicalCookieVector* cookies) {
    scoped_refptr<net::SQLitePersistentCookieStore> sqlite_store(
        new net::SQLitePersistentCookieStore(
            temp_dir_.path().Append(kTestCookiesFilename),
            client_task_runner(),
            background_task_runner(),
            true, nullptr));
    store_ = new QuotaPolicyCookieStore(sqlite_store.get(), storage_policy);
    Load(cookies);
  }

  // Adds a persistent cookie to store_.
  void AddCookie(const GURL& url,
                 const std::string& name,
                 const std::string& value,
                 const std::string& domain,
                 const std::string& path,
                 const base::Time& creation) {
    store_->AddCookie(*net::CanonicalCookie::Create(
        url, name, value, domain, path, creation, creation, false, false,
        net::CookieSameSite::DEFAULT_MODE, false,
        net::COOKIE_PRIORITY_DEFAULT));
  }

  void DestroyStore() {
    store_ = nullptr;
    // Ensure that |store_|'s destructor has run by shutting down the pool and
    // then forcing the pool to be destructed. This will ensure that all the
    // tasks that block pool shutdown (e.g. |store_|'s cleanup) have run before
    // yielding control.
    pool_owner_->pool()->FlushForTesting();
    pool_owner_.reset(new base::SequencedWorkerPoolOwner(3, "Background Pool"));
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    DestroyStore();
  }

  TestBrowserThreadBundle bundle_;
  std::unique_ptr<base::SequencedWorkerPoolOwner> pool_owner_;
  base::WaitableEvent loaded_event_;
  base::WaitableEvent destroy_event_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<QuotaPolicyCookieStore> store_;
  CanonicalCookieVector cookies_;
};

// Test if data is stored as expected in the QuotaPolicy database.
TEST_F(QuotaPolicyCookieStoreTest, TestPersistence) {
  CanonicalCookieVector cookies;
  CreateAndLoad(nullptr, &cookies);
  ASSERT_EQ(0U, cookies.size());

  base::Time t = base::Time::Now();
  AddCookie(GURL("http://foo.com"), "A", "B", std::string(), "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie(GURL("http://persistent.com"), "A", "B", std::string(), "/", t);

  // Replace the store, which forces the current store to flush data to
  // disk. Then, after reloading the store, confirm that the data was flushed by
  // making sure it loads successfully.  This ensures that all pending commits
  // are made to the store before allowing it to be closed.
  DestroyStore();

  // Reload and test for persistence.
  STLDeleteElements(&cookies);
  CreateAndLoad(nullptr, &cookies);
  EXPECT_EQ(2U, cookies.size());
  bool found_foo_cookie = false;
  bool found_persistent_cookie = false;
  for (const auto& cookie : cookies) {
    if (cookie->Domain() == "foo.com")
      found_foo_cookie = true;
    else if (cookie->Domain() == "persistent.com")
      found_persistent_cookie = true;
  }
  EXPECT_TRUE(found_foo_cookie);
  EXPECT_TRUE(found_persistent_cookie);

  // Now delete the cookies and check persistence again.
  store_->DeleteCookie(*cookies[0]);
  store_->DeleteCookie(*cookies[1]);
  DestroyStore();

  // Reload and check if the cookies have been removed.
  STLDeleteElements(&cookies);
  CreateAndLoad(nullptr, &cookies);
  EXPECT_EQ(0U, cookies.size());
  STLDeleteElements(&cookies);
}

// Test if data is stored as expected in the QuotaPolicy database.
TEST_F(QuotaPolicyCookieStoreTest, TestPolicy) {
  CanonicalCookieVector cookies;
  CreateAndLoad(nullptr, &cookies);
  ASSERT_EQ(0U, cookies.size());

  base::Time t = base::Time::Now();
  AddCookie(GURL("http://foo.com"), "A", "B", std::string(), "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie(GURL("http://persistent.com"), "A", "B", std::string(), "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie(GURL("http://nonpersistent.com"), "A", "B", std::string(), "/", t);

  // Replace the store, which forces the current store to flush data to
  // disk. Then, after reloading the store, confirm that the data was flushed by
  // making sure it loads successfully.  This ensures that all pending commits
  // are made to the store before allowing it to be closed.
  DestroyStore();
  // Specify storage policy that makes "nonpersistent.com" session only.
  scoped_refptr<content::MockSpecialStoragePolicy> storage_policy =
      new content::MockSpecialStoragePolicy();
  storage_policy->AddSessionOnly(
      net::cookie_util::CookieOriginToURL("nonpersistent.com", false));

  // Reload and test for persistence.
  STLDeleteElements(&cookies);
  CreateAndLoad(storage_policy.get(), &cookies);
  EXPECT_EQ(3U, cookies.size());

  t += base::TimeDelta::FromInternalValue(10);
  AddCookie(GURL("http://nonpersistent.com"), "A", "B", std::string(),
            "/second", t);

  // Now close the store, and "nonpersistent.com" should be deleted according to
  // policy.
  DestroyStore();
  STLDeleteElements(&cookies);
  CreateAndLoad(nullptr, &cookies);

  EXPECT_EQ(2U, cookies.size());
  for (const auto& cookie : cookies) {
    EXPECT_NE("nonpersistent.com", cookie->Domain());
  }
  STLDeleteElements(&cookies);
}

TEST_F(QuotaPolicyCookieStoreTest, ForceKeepSessionState) {
  CanonicalCookieVector cookies;
  CreateAndLoad(nullptr, &cookies);
  ASSERT_EQ(0U, cookies.size());

  base::Time t = base::Time::Now();
  AddCookie(GURL("http://foo.com"), "A", "B", std::string(), "/", t);

  // Recreate |store_| with a storage policy that makes "nonpersistent.com"
  // session only, but then instruct the store to forcibly keep all cookies.
  DestroyStore();
  scoped_refptr<content::MockSpecialStoragePolicy> storage_policy =
      new content::MockSpecialStoragePolicy();
  storage_policy->AddSessionOnly(
      net::cookie_util::CookieOriginToURL("nonpersistent.com", false));

  // Reload and test for persistence
  STLDeleteElements(&cookies);
  CreateAndLoad(storage_policy.get(), &cookies);
  EXPECT_EQ(1U, cookies.size());

  t += base::TimeDelta::FromInternalValue(10);
  AddCookie(GURL("http://persistent.com"), "A", "B", std::string(), "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie(GURL("http://nonpersistent.com"), "A", "B", std::string(), "/", t);

  // Now close the store, but the "nonpersistent.com" cookie should not be
  // deleted.
  store_->SetForceKeepSessionState();
  DestroyStore();
  STLDeleteElements(&cookies);
  CreateAndLoad(nullptr, &cookies);

  EXPECT_EQ(3U, cookies.size());
  STLDeleteElements(&cookies);
}

// Tests that the special storage policy is properly applied even when the store
// is destroyed on a background thread.
TEST_F(QuotaPolicyCookieStoreTest, TestDestroyOnBackgroundThread) {
  // Specify storage policy that makes "nonpersistent.com" session only.
  scoped_refptr<content::MockSpecialStoragePolicy> storage_policy =
      new content::MockSpecialStoragePolicy();
  storage_policy->AddSessionOnly(
      net::cookie_util::CookieOriginToURL("nonpersistent.com", false));

  CanonicalCookieVector cookies;
  CreateAndLoad(storage_policy.get(), &cookies);
  ASSERT_EQ(0U, cookies.size());

  base::Time t = base::Time::Now();
  AddCookie(GURL("http://nonpersistent.com"), "A", "B", std::string(), "/", t);

  // Replace the store, which forces the current store to flush data to
  // disk. Then, after reloading the store, confirm that the data was flushed by
  // making sure it loads successfully.  This ensures that all pending commits
  // are made to the store before allowing it to be closed.
  DestroyStoreOnBackgroundThread();

  // Reload and test for persistence.
  STLDeleteElements(&cookies);
  CreateAndLoad(storage_policy.get(), &cookies);
  EXPECT_EQ(0U, cookies.size());

  STLDeleteElements(&cookies);
}

}  // namespace
}  // namespace content
