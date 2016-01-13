// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_session_cache_openssl.h"

#include <openssl/ssl.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "crypto/openssl_util.h"

#include "testing/gtest/include/gtest/gtest.h"

// This is an internal OpenSSL function that can be used to create a new
// session for an existing SSL object. This shall force a call to the
// 'generate_session_id' callback from the SSL's session context.
// |s| is the target SSL connection handle.
// |session| is non-0 to ask for the creation of a new session. If 0,
// this will set an empty session with no ID instead.
extern "C" int ssl_get_new_session(SSL* s, int session);

// This is an internal OpenSSL function which is used internally to add
// a new session to the cache. It is normally triggered by a succesful
// connection. However, this unit test does not use the network at all.
extern "C" void ssl_update_cache(SSL* s, int mode);

namespace net {

namespace {

typedef crypto::ScopedOpenSSL<SSL, SSL_free> ScopedSSL;

// Helper class used to associate arbitrary std::string keys with SSL objects.
class SSLKeyHelper {
 public:
  // Return the string associated with a given SSL handle |ssl|, or the
  // empty string if none exists.
  static std::string Get(const SSL* ssl) {
    return GetInstance()->GetValue(ssl);
  }

  // Associate a string with a given SSL handle |ssl|.
  static void Set(SSL* ssl, const std::string& value) {
    GetInstance()->SetValue(ssl, value);
  }

  static SSLKeyHelper* GetInstance() {
    static base::LazyInstance<SSLKeyHelper>::Leaky s_instance =
        LAZY_INSTANCE_INITIALIZER;
    return s_instance.Pointer();
  }

  SSLKeyHelper() {
    ex_index_ = SSL_get_ex_new_index(0, NULL, NULL, KeyDup, KeyFree);
    CHECK_NE(-1, ex_index_);
  }

  std::string GetValue(const SSL* ssl) {
    std::string* value =
        reinterpret_cast<std::string*>(SSL_get_ex_data(ssl, ex_index_));
    if (!value)
      return std::string();
    return *value;
  }

  void SetValue(SSL* ssl, const std::string& value) {
    int ret = SSL_set_ex_data(ssl, ex_index_, new std::string(value));
    CHECK_EQ(1, ret);
  }

  // Called when an SSL object is copied through SSL_dup(). This needs to copy
  // the value as well.
  static int KeyDup(CRYPTO_EX_DATA* to,
                    CRYPTO_EX_DATA* from,
                    void* from_fd,
                    int idx,
                    long argl,
                    void* argp) {
    // |from_fd| is really the address of a temporary pointer. On input, it
    // points to the value from the original SSL object. The function must
    // update it to the address of a copy.
    std::string** ptr = reinterpret_cast<std::string**>(from_fd);
    std::string* old_string = *ptr;
    std::string* new_string = new std::string(*old_string);
    *ptr = new_string;
    return 0;  // Ignored by the implementation.
  }

  // Called to destroy the value associated with an SSL object.
  static void KeyFree(void* parent,
                      void* ptr,
                      CRYPTO_EX_DATA* ad,
                      int index,
                      long argl,
                      void* argp) {
    std::string* value = reinterpret_cast<std::string*>(ptr);
    delete value;
  }

  int ex_index_;
};

}  // namespace

class SSLSessionCacheOpenSSLTest : public testing::Test {
 public:
  SSLSessionCacheOpenSSLTest() {
    crypto::EnsureOpenSSLInit();
    ctx_.reset(SSL_CTX_new(SSLv23_client_method()));
    cache_.Reset(ctx_.get(), kDefaultConfig);
  }

  // Reset cache configuration.
  void ResetConfig(const SSLSessionCacheOpenSSL::Config& config) {
    cache_.Reset(ctx_.get(), config);
  }

  // Helper function to create a new SSL connection object associated with
  // a given unique |cache_key|. This does _not_ add the session to the cache.
  // Caller must free the object with SSL_free().
  SSL* NewSSL(const std::string& cache_key) {
    SSL* ssl = SSL_new(ctx_.get());
    if (!ssl)
      return NULL;

    SSLKeyHelper::Set(ssl, cache_key);  // associate cache key.
    ResetSessionID(ssl);                // create new unique session ID.
    return ssl;
  }

  // Reset the session ID of a given SSL object. This creates a new session
  // with a new unique random ID. Does not add it to the cache.
  static void ResetSessionID(SSL* ssl) { ssl_get_new_session(ssl, 1); }

  // Add a given SSL object and its session to the cache.
  void AddToCache(SSL* ssl) {
    ssl_update_cache(ssl, ctx_.get()->session_cache_mode);
  }

  static const SSLSessionCacheOpenSSL::Config kDefaultConfig;

 protected:
  crypto::ScopedOpenSSL<SSL_CTX, SSL_CTX_free> ctx_;
  // |cache_| must be destroyed before |ctx_| and thus appears after it.
  SSLSessionCacheOpenSSL cache_;
};

// static
const SSLSessionCacheOpenSSL::Config
    SSLSessionCacheOpenSSLTest::kDefaultConfig = {
        &SSLKeyHelper::Get,  // key_func
        1024,                // max_entries
        256,                 // expiration_check_count
        60 * 60,             // timeout_seconds
};

TEST_F(SSLSessionCacheOpenSSLTest, EmptyCacheCreation) {
  EXPECT_EQ(0U, cache_.size());
}

TEST_F(SSLSessionCacheOpenSSLTest, CacheOneSession) {
  ScopedSSL ssl(NewSSL("hello"));

  EXPECT_EQ(0U, cache_.size());
  AddToCache(ssl.get());
  EXPECT_EQ(1U, cache_.size());
  ssl.reset(NULL);
  EXPECT_EQ(1U, cache_.size());
}

TEST_F(SSLSessionCacheOpenSSLTest, CacheMultipleSessions) {
  const size_t kNumItems = 100;
  int local_id = 1;

  // Add kNumItems to the cache.
  for (size_t n = 0; n < kNumItems; ++n) {
    std::string local_id_string = base::StringPrintf("%d", local_id++);
    ScopedSSL ssl(NewSSL(local_id_string));
    AddToCache(ssl.get());
    EXPECT_EQ(n + 1, cache_.size());
  }
}

TEST_F(SSLSessionCacheOpenSSLTest, Flush) {
  const size_t kNumItems = 100;
  int local_id = 1;

  // Add kNumItems to the cache.
  for (size_t n = 0; n < kNumItems; ++n) {
    std::string local_id_string = base::StringPrintf("%d", local_id++);
    ScopedSSL ssl(NewSSL(local_id_string));
    AddToCache(ssl.get());
  }
  EXPECT_EQ(kNumItems, cache_.size());

  cache_.Flush();
  EXPECT_EQ(0U, cache_.size());
}

TEST_F(SSLSessionCacheOpenSSLTest, SetSSLSession) {
  const std::string key("hello");
  ScopedSSL ssl(NewSSL(key));

  // First call should fail because the session is not in the cache.
  EXPECT_FALSE(cache_.SetSSLSession(ssl.get()));
  SSL_SESSION* session = ssl.get()->session;
  EXPECT_TRUE(session);
  EXPECT_EQ(1, session->references);

  AddToCache(ssl.get());
  EXPECT_EQ(2, session->references);

  // Mark the session as good, so that it is re-used for the second connection.
  cache_.MarkSSLSessionAsGood(ssl.get());

  ssl.reset(NULL);
  EXPECT_EQ(1, session->references);

  // Second call should find the session ID and associate it with |ssl2|.
  ScopedSSL ssl2(NewSSL(key));
  EXPECT_TRUE(cache_.SetSSLSession(ssl2.get()));

  EXPECT_EQ(session, ssl2.get()->session);
  EXPECT_EQ(2, session->references);
}

TEST_F(SSLSessionCacheOpenSSLTest, SetSSLSessionWithKey) {
  const std::string key("hello");
  ScopedSSL ssl(NewSSL(key));
  AddToCache(ssl.get());
  cache_.MarkSSLSessionAsGood(ssl.get());
  ssl.reset(NULL);

  ScopedSSL ssl2(NewSSL(key));
  EXPECT_TRUE(cache_.SetSSLSessionWithKey(ssl2.get(), key));
}

TEST_F(SSLSessionCacheOpenSSLTest, CheckSessionReplacement) {
  // Check that if two SSL connections have the same key, only one
  // corresponding session can be stored in the cache.
  const std::string common_key("common-key");
  ScopedSSL ssl1(NewSSL(common_key));
  ScopedSSL ssl2(NewSSL(common_key));

  AddToCache(ssl1.get());
  EXPECT_EQ(1U, cache_.size());
  EXPECT_EQ(2, ssl1.get()->session->references);

  // This ends up calling OnSessionAdded which will discover that there is
  // already one session ID associated with the key, and will replace it.
  AddToCache(ssl2.get());
  EXPECT_EQ(1U, cache_.size());
  EXPECT_EQ(1, ssl1.get()->session->references);
  EXPECT_EQ(2, ssl2.get()->session->references);
}

// Check that when two connections have the same key, a new session is created
// if the existing session has not yet been marked "good". Further, after the
// first session completes, if the second session has replaced it in the cache,
// new sessions should continue to fail until the currently cached session
// succeeds.
TEST_F(SSLSessionCacheOpenSSLTest, CheckSessionReplacementWhenNotGood) {
  const std::string key("hello");
  ScopedSSL ssl(NewSSL(key));

  // First call should fail because the session is not in the cache.
  EXPECT_FALSE(cache_.SetSSLSession(ssl.get()));
  SSL_SESSION* session = ssl.get()->session;
  ASSERT_TRUE(session);
  EXPECT_EQ(1, session->references);

  AddToCache(ssl.get());
  EXPECT_EQ(2, session->references);

  // Second call should find the session ID, but because it is not yet good,
  // fail to associate it with |ssl2|.
  ScopedSSL ssl2(NewSSL(key));
  EXPECT_FALSE(cache_.SetSSLSession(ssl2.get()));
  SSL_SESSION* session2 = ssl2.get()->session;
  ASSERT_TRUE(session2);
  EXPECT_EQ(1, session2->references);

  EXPECT_NE(session, session2);

  // Add the second connection to the cache. It should replace the first
  // session, and the cache should hold on to the second session.
  AddToCache(ssl2.get());
  EXPECT_EQ(1, session->references);
  EXPECT_EQ(2, session2->references);

  // Mark the first session as good, simulating it completing.
  cache_.MarkSSLSessionAsGood(ssl.get());

  // Third call should find the session ID, but because the second session (the
  // current cache entry) is not yet good, fail to associate it with |ssl3|.
  ScopedSSL ssl3(NewSSL(key));
  EXPECT_FALSE(cache_.SetSSLSession(ssl3.get()));
  EXPECT_NE(session, ssl3.get()->session);
  EXPECT_NE(session2, ssl3.get()->session);
  EXPECT_EQ(1, ssl3.get()->session->references);
}

TEST_F(SSLSessionCacheOpenSSLTest, CheckEviction) {
  const size_t kMaxItems = 20;
  int local_id = 1;

  SSLSessionCacheOpenSSL::Config config = kDefaultConfig;
  config.max_entries = kMaxItems;
  ResetConfig(config);

  // Add kMaxItems to the cache.
  for (size_t n = 0; n < kMaxItems; ++n) {
    std::string local_id_string = base::StringPrintf("%d", local_id++);
    ScopedSSL ssl(NewSSL(local_id_string));

    AddToCache(ssl.get());
    EXPECT_EQ(n + 1, cache_.size());
  }

  // Continue adding new items to the cache, check that old ones are
  // evicted.
  for (size_t n = 0; n < kMaxItems; ++n) {
    std::string local_id_string = base::StringPrintf("%d", local_id++);
    ScopedSSL ssl(NewSSL(local_id_string));

    AddToCache(ssl.get());
    EXPECT_EQ(kMaxItems, cache_.size());
  }
}

// Check that session expiration works properly.
TEST_F(SSLSessionCacheOpenSSLTest, CheckExpiration) {
  const size_t kMaxCheckCount = 10;
  const size_t kNumEntries = 20;

  SSLSessionCacheOpenSSL::Config config = kDefaultConfig;
  config.expiration_check_count = kMaxCheckCount;
  config.timeout_seconds = 1000;
  ResetConfig(config);

  // Add |kNumItems - 1| session entries with crafted time values.
  for (size_t n = 0; n < kNumEntries - 1U; ++n) {
    std::string key = base::StringPrintf("%d", static_cast<int>(n));
    ScopedSSL ssl(NewSSL(key));
    // Cheat a little: Force the session |time| value, this guarantees that they
    // are expired, given that ::time() will always return a value that is
    // past the first 100 seconds after the Unix epoch.
    ssl.get()->session->time = static_cast<long>(n);
    AddToCache(ssl.get());
  }
  EXPECT_EQ(kNumEntries - 1U, cache_.size());

  // Add nother session which will get the current time, and thus not be
  // expirable until 1000 seconds have passed.
  ScopedSSL good_ssl(NewSSL("good-key"));
  AddToCache(good_ssl.get());
  good_ssl.reset(NULL);
  EXPECT_EQ(kNumEntries, cache_.size());

  // Call SetSSLSession() |kMaxCheckCount - 1| times, this shall not expire
  // any session
  for (size_t n = 0; n < kMaxCheckCount - 1U; ++n) {
    ScopedSSL ssl(NewSSL("unknown-key"));
    cache_.SetSSLSession(ssl.get());
    EXPECT_EQ(kNumEntries, cache_.size());
  }

  // Call SetSSLSession another time, this shall expire all sessions except
  // the last one.
  ScopedSSL bad_ssl(NewSSL("unknown-key"));
  cache_.SetSSLSession(bad_ssl.get());
  bad_ssl.reset(NULL);
  EXPECT_EQ(1U, cache_.size());
}

}  // namespace net
