// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/sqlite_persistent_cookie_store.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "content/public/browser/cookie_crypto_delegate.h"
#include "content/public/browser/cookie_store_factory.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "sql/connection.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

const base::FilePath::CharType kCookieFilename[] = FILE_PATH_LITERAL("Cookies");

class CookieCryptor : public content::CookieCryptoDelegate {
 public:
  CookieCryptor();
  virtual bool EncryptString(const std::string& plaintext,
                             std::string* ciphertext) OVERRIDE;
  virtual bool DecryptString(const std::string& ciphertext,
                             std::string* plaintext) OVERRIDE;

 private:
  scoped_ptr<crypto::SymmetricKey> key_;
  crypto::Encryptor encryptor_;
};

CookieCryptor::CookieCryptor() : key_(
    crypto::SymmetricKey::DeriveKeyFromPassword(
        crypto::SymmetricKey::AES, "password", "saltiest", 1000, 256)) {
  std::string iv("the iv: 16 bytes");
  encryptor_.Init(key_.get(), crypto::Encryptor::CBC, iv);
}

bool CookieCryptor::EncryptString(const std::string& plaintext,
                                  std::string* ciphertext) {
  return encryptor_.Encrypt(plaintext, ciphertext);
}

bool CookieCryptor::DecryptString(const std::string& ciphertext,
                                  std::string* plaintext) {
  return encryptor_.Decrypt(ciphertext, plaintext);
}

}  // namespace

typedef std::vector<net::CanonicalCookie*> CanonicalCookieVector;

class SQLitePersistentCookieStoreTest : public testing::Test {
 public:
  SQLitePersistentCookieStoreTest()
      : pool_owner_(new base::SequencedWorkerPoolOwner(3, "Background Pool")),
        loaded_event_(false, false),
        key_loaded_event_(false, false),
        db_thread_event_(false, false) {
  }

  void OnLoaded(const CanonicalCookieVector& cookies) {
    cookies_ = cookies;
    loaded_event_.Signal();
  }

  void OnKeyLoaded(const CanonicalCookieVector& cookies) {
    cookies_ = cookies;
    key_loaded_event_.Signal();
  }

  void Load(CanonicalCookieVector* cookies) {
    EXPECT_FALSE(loaded_event_.IsSignaled());
    store_->Load(base::Bind(&SQLitePersistentCookieStoreTest::OnLoaded,
                            base::Unretained(this)));
    loaded_event_.Wait();
    *cookies = cookies_;
  }

  void Flush() {
    base::WaitableEvent event(false, false);
    store_->Flush(base::Bind(&base::WaitableEvent::Signal,
                             base::Unretained(&event)));
    event.Wait();
  }

  scoped_refptr<base::SequencedTaskRunner> background_task_runner() {
    return pool_owner_->pool()->GetSequencedTaskRunner(
        pool_owner_->pool()->GetNamedSequenceToken("background"));
  }

  scoped_refptr<base::SequencedTaskRunner> client_task_runner() {
    return pool_owner_->pool()->GetSequencedTaskRunner(
        pool_owner_->pool()->GetNamedSequenceToken("client"));
  }

  void DestroyStore() {
    store_ = NULL;
    // Make sure we wait until the destructor has run by shutting down the pool
    // resetting the owner (whose destructor blocks on the pool completion).
    pool_owner_->pool()->Shutdown();
    // Create a new pool for the few tests that create multiple stores. In other
    // cases this is wasted but harmless.
    pool_owner_.reset(new base::SequencedWorkerPoolOwner(3, "Background Pool"));
  }

  void CreateAndLoad(bool crypt_cookies,
                     bool restore_old_session_cookies,
                     CanonicalCookieVector* cookies) {
    if (crypt_cookies)
      cookie_crypto_delegate_.reset(new CookieCryptor());

    store_ = new SQLitePersistentCookieStore(
        temp_dir_.path().Append(kCookieFilename),
        client_task_runner(),
        background_task_runner(),
        restore_old_session_cookies,
        NULL,
        cookie_crypto_delegate_.get());
    Load(cookies);
  }

  void InitializeStore(bool crypt, bool restore_old_session_cookies) {
    CanonicalCookieVector cookies;
    CreateAndLoad(crypt, restore_old_session_cookies, &cookies);
    EXPECT_EQ(0U, cookies.size());
  }

  // We have to create this method to wrap WaitableEvent::Wait, since we cannot
  // bind a non-void returning method as a Closure.
  void WaitOnDBEvent() {
    db_thread_event_.Wait();
  }

  // Adds a persistent cookie to store_.
  void AddCookie(const std::string& name,
                 const std::string& value,
                 const std::string& domain,
                 const std::string& path,
                 const base::Time& creation) {
    store_->AddCookie(
        net::CanonicalCookie(GURL(), name, value, domain, path, creation,
                             creation, creation, false, false,
                             net::COOKIE_PRIORITY_DEFAULT));
  }

  std::string ReadRawDBContents() {
    std::string contents;
    if (!base::ReadFileToString(temp_dir_.path().Append(kCookieFilename),
                                &contents))
      return std::string();
    return contents;
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() OVERRIDE {
    DestroyStore();
    pool_owner_->pool()->Shutdown();
  }

 protected:
  base::MessageLoop main_loop_;
  scoped_ptr<base::SequencedWorkerPoolOwner> pool_owner_;
  base::WaitableEvent loaded_event_;
  base::WaitableEvent key_loaded_event_;
  base::WaitableEvent db_thread_event_;
  CanonicalCookieVector cookies_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<SQLitePersistentCookieStore> store_;
  scoped_ptr<content::CookieCryptoDelegate> cookie_crypto_delegate_;
};

TEST_F(SQLitePersistentCookieStoreTest, TestInvalidMetaTableRecovery) {
  InitializeStore(false, false);
  AddCookie("A", "B", "foo.bar", "/", base::Time::Now());
  DestroyStore();

  // Load up the store and verify that it has good data in it.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, false, &cookies);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("A", cookies[0]->Name().c_str());
  ASSERT_STREQ("B", cookies[0]->Value().c_str());
  DestroyStore();
  STLDeleteElements(&cookies);

  // Now corrupt the meta table.
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(temp_dir_.path().Append(kCookieFilename)));
    sql::MetaTable meta_table_;
    meta_table_.Init(&db, 1, 1);
    ASSERT_TRUE(db.Execute("DELETE FROM meta"));
    db.Close();
  }

  // Upon loading, the database should be reset to a good, blank state.
  CreateAndLoad(false, false, &cookies);
  ASSERT_EQ(0U, cookies.size());

  // Verify that, after, recovery, the database persists properly.
  AddCookie("X", "Y", "foo.bar", "/", base::Time::Now());
  DestroyStore();
  CreateAndLoad(false, false, &cookies);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("X", cookies[0]->Name().c_str());
  ASSERT_STREQ("Y", cookies[0]->Value().c_str());
  STLDeleteElements(&cookies);
}

// Test if data is stored as expected in the SQLite database.
TEST_F(SQLitePersistentCookieStoreTest, TestPersistance) {
  InitializeStore(false, false);
  AddCookie("A", "B", "foo.bar", "/", base::Time::Now());
  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk. Then we can see if after loading it again it
  // is still there.
  DestroyStore();
  // Reload and test for persistence
  CanonicalCookieVector cookies;
  CreateAndLoad(false, false, &cookies);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("A", cookies[0]->Name().c_str());
  ASSERT_STREQ("B", cookies[0]->Value().c_str());

  // Now delete the cookie and check persistence again.
  store_->DeleteCookie(*cookies[0]);
  DestroyStore();
  STLDeleteElements(&cookies);

  // Reload and check if the cookie has been removed.
  CreateAndLoad(false, false, &cookies);
  ASSERT_EQ(0U, cookies.size());
}

// Test that priority load of cookies for a specfic domain key could be
// completed before the entire store is loaded
TEST_F(SQLitePersistentCookieStoreTest, TestLoadCookiesForKey) {
  InitializeStore(false, false);
  base::Time t = base::Time::Now();
  AddCookie("A", "B", "foo.bar", "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie("A", "B", "www.aaa.com", "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie("A", "B", "travel.aaa.com", "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie("A", "B", "www.bbb.com", "/", t);
  DestroyStore();

  store_ = new SQLitePersistentCookieStore(
      temp_dir_.path().Append(kCookieFilename),
      client_task_runner(),
      background_task_runner(),
      false, NULL, NULL);

  // Posting a blocking task to db_thread_ makes sure that the DB thread waits
  // until both Load and LoadCookiesForKey have been posted to its task queue.
  background_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
                 base::Unretained(this)));
  store_->Load(base::Bind(&SQLitePersistentCookieStoreTest::OnLoaded,
                          base::Unretained(this)));
  store_->LoadCookiesForKey("aaa.com",
    base::Bind(&SQLitePersistentCookieStoreTest::OnKeyLoaded,
               base::Unretained(this)));
  background_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
                 base::Unretained(this)));

  // Now the DB-thread queue contains:
  // (active:)
  // 1. Wait (on db_event)
  // (pending:)
  // 2. "Init And Chain-Load First Domain"
  // 3. Priority Load (aaa.com)
  // 4. Wait (on db_event)
  db_thread_event_.Signal();
  key_loaded_event_.Wait();
  ASSERT_EQ(loaded_event_.IsSignaled(), false);
  std::set<std::string> cookies_loaded;
  for (CanonicalCookieVector::const_iterator it = cookies_.begin();
       it != cookies_.end();
       ++it) {
    cookies_loaded.insert((*it)->Domain().c_str());
  }
  STLDeleteElements(&cookies_);
  ASSERT_GT(4U, cookies_loaded.size());
  ASSERT_EQ(true, cookies_loaded.find("www.aaa.com") != cookies_loaded.end());
  ASSERT_EQ(true,
            cookies_loaded.find("travel.aaa.com") != cookies_loaded.end());

  db_thread_event_.Signal();
  loaded_event_.Wait();
  for (CanonicalCookieVector::const_iterator it = cookies_.begin();
       it != cookies_.end();
       ++it) {
    cookies_loaded.insert((*it)->Domain().c_str());
  }
  ASSERT_EQ(4U, cookies_loaded.size());
  ASSERT_EQ(cookies_loaded.find("foo.bar") != cookies_loaded.end(),
            true);
  ASSERT_EQ(cookies_loaded.find("www.bbb.com") != cookies_loaded.end(), true);
  STLDeleteElements(&cookies_);
}

// Test that we can force the database to be written by calling Flush().
TEST_F(SQLitePersistentCookieStoreTest, TestFlush) {
  InitializeStore(false, false);
  // File timestamps don't work well on all platforms, so we'll determine
  // whether the DB file has been modified by checking its size.
  base::FilePath path = temp_dir_.path().Append(kCookieFilename);
  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(path, &info));
  int64 base_size = info.size;

  // Write some large cookies, so the DB will have to expand by several KB.
  for (char c = 'a'; c < 'z'; ++c) {
    // Each cookie needs a unique timestamp for creation_utc (see DB schema).
    base::Time t = base::Time::Now() + base::TimeDelta::FromMicroseconds(c);
    std::string name(1, c);
    std::string value(1000, c);
    AddCookie(name, value, "foo.bar", "/", t);
  }

  Flush();

  // We forced a write, so now the file will be bigger.
  ASSERT_TRUE(base::GetFileInfo(path, &info));
  ASSERT_GT(info.size, base_size);
}

// Test loading old session cookies from the disk.
TEST_F(SQLitePersistentCookieStoreTest, TestLoadOldSessionCookies) {
  InitializeStore(false, true);

  // Add a session cookie.
  store_->AddCookie(
      net::CanonicalCookie(
          GURL(), "C", "D", "sessioncookie.com", "/", base::Time::Now(),
          base::Time(), base::Time::Now(), false, false,
          net::COOKIE_PRIORITY_DEFAULT));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that loads session cookies and test that the session cookie
  // was loaded.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, true, &cookies);

  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("sessioncookie.com", cookies[0]->Domain().c_str());
  ASSERT_STREQ("C", cookies[0]->Name().c_str());
  ASSERT_STREQ("D", cookies[0]->Value().c_str());
  ASSERT_EQ(net::COOKIE_PRIORITY_DEFAULT, cookies[0]->Priority());

  STLDeleteElements(&cookies);
}

// Test loading old session cookies from the disk.
TEST_F(SQLitePersistentCookieStoreTest, TestDontLoadOldSessionCookies) {
  InitializeStore(false, true);

  // Add a session cookie.
  store_->AddCookie(
      net::CanonicalCookie(
          GURL(), "C", "D", "sessioncookie.com", "/", base::Time::Now(),
          base::Time(), base::Time::Now(), false, false,
          net::COOKIE_PRIORITY_DEFAULT));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that doesn't load old session cookies and test that the
  // session cookie was not loaded.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, false, &cookies);
  ASSERT_EQ(0U, cookies.size());

  // The store should also delete the session cookie. Wait until that has been
  // done.
  DestroyStore();

  // Create a store that loads old session cookies and test that the session
  // cookie is gone.
  CreateAndLoad(false, true, &cookies);
  ASSERT_EQ(0U, cookies.size());
}

TEST_F(SQLitePersistentCookieStoreTest, PersistIsPersistent) {
  InitializeStore(false, true);
  static const char kSessionName[] = "session";
  static const char kPersistentName[] = "persistent";

  // Add a session cookie.
  store_->AddCookie(
      net::CanonicalCookie(
          GURL(), kSessionName, "val", "sessioncookie.com", "/",
          base::Time::Now(), base::Time(), base::Time::Now(), false, false,
          net::COOKIE_PRIORITY_DEFAULT));
  // Add a persistent cookie.
  store_->AddCookie(
      net::CanonicalCookie(
          GURL(), kPersistentName, "val", "sessioncookie.com", "/",
          base::Time::Now() - base::TimeDelta::FromDays(1),
          base::Time::Now() + base::TimeDelta::FromDays(1),
          base::Time::Now(), false, false,
          net::COOKIE_PRIORITY_DEFAULT));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that loads session cookie and test that the IsPersistent
  // attribute is restored.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, true, &cookies);
  ASSERT_EQ(2U, cookies.size());

  std::map<std::string, net::CanonicalCookie*> cookie_map;
  for (CanonicalCookieVector::const_iterator it = cookies.begin();
       it != cookies.end();
       ++it) {
    cookie_map[(*it)->Name()] = *it;
  }

  std::map<std::string, net::CanonicalCookie*>::const_iterator it =
      cookie_map.find(kSessionName);
  ASSERT_TRUE(it != cookie_map.end());
  EXPECT_FALSE(cookie_map[kSessionName]->IsPersistent());

  it = cookie_map.find(kPersistentName);
  ASSERT_TRUE(it != cookie_map.end());
  EXPECT_TRUE(cookie_map[kPersistentName]->IsPersistent());

  STLDeleteElements(&cookies);
}

TEST_F(SQLitePersistentCookieStoreTest, PriorityIsPersistent) {
  static const char kLowName[] = "low";
  static const char kMediumName[] = "medium";
  static const char kHighName[] = "high";
  static const char kCookieDomain[] = "sessioncookie.com";
  static const char kCookieValue[] = "value";
  static const char kCookiePath[] = "/";

  InitializeStore(false, true);

  // Add a low-priority persistent cookie.
  store_->AddCookie(
      net::CanonicalCookie(
          GURL(), kLowName, kCookieValue, kCookieDomain, kCookiePath,
          base::Time::Now() - base::TimeDelta::FromMinutes(1),
          base::Time::Now() + base::TimeDelta::FromDays(1),
          base::Time::Now(), false, false,
          net::COOKIE_PRIORITY_LOW));

  // Add a medium-priority persistent cookie.
  store_->AddCookie(
      net::CanonicalCookie(
          GURL(), kMediumName, kCookieValue, kCookieDomain, kCookiePath,
          base::Time::Now() - base::TimeDelta::FromMinutes(2),
          base::Time::Now() + base::TimeDelta::FromDays(1),
          base::Time::Now(), false, false,
          net::COOKIE_PRIORITY_MEDIUM));

  // Add a high-priority peristent cookie.
  store_->AddCookie(
      net::CanonicalCookie(
          GURL(), kHighName, kCookieValue, kCookieDomain, kCookiePath,
          base::Time::Now() - base::TimeDelta::FromMinutes(3),
          base::Time::Now() + base::TimeDelta::FromDays(1),
          base::Time::Now(), false, false,
          net::COOKIE_PRIORITY_HIGH));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that loads session cookie and test that the priority
  // attribute values are restored.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, true, &cookies);
  ASSERT_EQ(3U, cookies.size());

  // Put the cookies into a map, by name, so we can easily find them.
  std::map<std::string, net::CanonicalCookie*> cookie_map;
  for (CanonicalCookieVector::const_iterator it = cookies.begin();
       it != cookies.end();
       ++it) {
    cookie_map[(*it)->Name()] = *it;
  }

  // Validate that each cookie has the correct priority.
  std::map<std::string, net::CanonicalCookie*>::const_iterator it =
      cookie_map.find(kLowName);
  ASSERT_TRUE(it != cookie_map.end());
  EXPECT_EQ(net::COOKIE_PRIORITY_LOW, cookie_map[kLowName]->Priority());

  it = cookie_map.find(kMediumName);
  ASSERT_TRUE(it != cookie_map.end());
  EXPECT_EQ(net::COOKIE_PRIORITY_MEDIUM, cookie_map[kMediumName]->Priority());

  it = cookie_map.find(kHighName);
  ASSERT_TRUE(it != cookie_map.end());
  EXPECT_EQ(net::COOKIE_PRIORITY_HIGH, cookie_map[kHighName]->Priority());

  STLDeleteElements(&cookies);
}

TEST_F(SQLitePersistentCookieStoreTest, UpdateToEncryption) {
  CanonicalCookieVector cookies;

  // Create unencrypted cookie store and write something to it.
  InitializeStore(false, false);
  AddCookie("name", "value123XYZ", "foo.bar", "/", base::Time::Now());
  DestroyStore();

  // Verify that "value" is visible in the file.  This is necessary in order to
  // have confidence in a later test that "encrypted_value" is not visible.
  std::string contents = ReadRawDBContents();
  EXPECT_NE(0U, contents.length());
  EXPECT_NE(contents.find("value123XYZ"), std::string::npos);

  // Create encrypted cookie store and ensure old cookie still reads.
  STLDeleteElements(&cookies_);
  EXPECT_EQ(0U, cookies_.size());
  CreateAndLoad(true, false, &cookies);
  EXPECT_EQ(1U, cookies_.size());
  EXPECT_EQ("name", cookies_[0]->Name());
  EXPECT_EQ("value123XYZ", cookies_[0]->Value());

  // Make sure we can update existing cookie and add new cookie as encrypted.
  store_->DeleteCookie(*(cookies_[0]));
  AddCookie("name", "encrypted_value123XYZ", "foo.bar", "/", base::Time::Now());
  AddCookie("other", "something456ABC", "foo.bar", "/",
            base::Time::Now() + base::TimeDelta::FromInternalValue(10));
  DestroyStore();
  STLDeleteElements(&cookies_);
  CreateAndLoad(true, false, &cookies);
  EXPECT_EQ(2U, cookies_.size());
  net::CanonicalCookie* cookie_name = NULL;
  net::CanonicalCookie* cookie_other = NULL;
  if (cookies_[0]->Name() == "name") {
    cookie_name = cookies_[0];
    cookie_other = cookies_[1];
  } else {
    cookie_name = cookies_[1];
    cookie_other = cookies_[0];
  }
  EXPECT_EQ("encrypted_value123XYZ", cookie_name->Value());
  EXPECT_EQ("something456ABC", cookie_other->Value());
  DestroyStore();
  STLDeleteElements(&cookies_);

  // Examine the real record to make sure plaintext version doesn't exist.
  sql::Connection db;
  sql::Statement smt;
  int resultcount = 0;
  ASSERT_TRUE(db.Open(temp_dir_.path().Append(kCookieFilename)));
  smt.Assign(db.GetCachedStatement(SQL_FROM_HERE,
                                   "SELECT * "
                                   "FROM cookies "
                                   "WHERE host_key = 'foo.bar'"));
  while (smt.Step()) {
    resultcount++;
    for (int i=0; i < smt.ColumnCount(); i++) {
      EXPECT_EQ(smt.ColumnString(i).find("value"), std::string::npos);
      EXPECT_EQ(smt.ColumnString(i).find("something"), std::string::npos);
    }
  }
  EXPECT_EQ(2, resultcount);

  // Verify that "encrypted_value" is NOT visible in the file.
  contents = ReadRawDBContents();
  EXPECT_NE(0U, contents.length());
  EXPECT_EQ(contents.find("encrypted_value123XYZ"), std::string::npos);
  EXPECT_EQ(contents.find("something456ABC"), std::string::npos);
}

}  // namespace content
