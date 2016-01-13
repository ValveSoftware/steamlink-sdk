// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_SESSION_CACHE_OPENSSL_H
#define NET_SOCKET_SSL_SESSION_CACHE_OPENSSL_H

#include <string>

#include "base/basictypes.h"
#include "net/base/net_export.h"

// Avoid including OpenSSL headers here.
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;

namespace net {

class SSLSessionCacheOpenSSLImpl;

// A class used to implement a custom cache of SSL_SESSION objects.
// Usage is as follows:
//
//  - Client creates a new cache instance with appropriate configuration,
//    associating it with a given SSL_CTX object.
//
//    The configuration must include a pointer to a client-provided function
//    that can retrieve a unique cache key from an existing SSL handle.
//
//  - When creating a new SSL connection, call SetSSLSession() with the newly
//    created SSL handle, and a cache key for the current host/port. If a
//    session is already in the cache, it will be added to the connection
//    through SSL_set_session().
//
//  - Otherwise, OpenSSL will create a new SSL_SESSION object during the
//    connection, and will pass it to the cache's internal functions,
//    transparently to the client.
//
//  - Each session has a timeout in seconds, which are checked every N-th call
//    to SetSSLSession(), where N is the current configuration's
//    |check_expiration_count|. Expired sessions are removed automatically
//    from the cache.
//
//  - Clients can call Flush() to remove all sessions from the cache, this is
//    useful when the system's certificate store has changed.
//
// This class is thread-safe. There shouldn't be any issue with multiple
// SSL connections being performed in parallel in multiple threads.
class NET_EXPORT SSLSessionCacheOpenSSL {
 public:
  // Type of a function that takes a SSL handle and returns a unique cache
  // key string to identify it.
  typedef std::string GetSessionKeyFunction(const SSL* ssl);

  // A small structure used to configure a cache on creation.
  // |key_func| is a function used at runtime to retrieve the unique cache key
  // from a given SSL connection handle.
  // |max_entries| is the maximum number of entries in the cache.
  // |expiration_check_count| is the number of calls to SetSSLSession() that
  // will trigger a check for expired sessions.
  // |timeout_seconds| is the timeout of new cached sessions in seconds.
  struct Config {
    GetSessionKeyFunction* key_func;
    size_t max_entries;
    size_t expiration_check_count;
    int timeout_seconds;
  };

  SSLSessionCacheOpenSSL() : impl_(NULL) {}

  // Construct a new cache instance.
  // |ctx| is a SSL_CTX context handle that will be associated with this cache.
  // |key_func| is a function that will be used at runtime to retrieve the
  // unique cache key from a SSL connection handle.
  // |max_entries| is the maximum number of entries in the cache.
  // |timeout_seconds| is the timeout of new cached sessions in seconds.
  // |expiration_check_count| is the number of calls to SetSSLSession() that
  // will trigger a check for expired sessions.
  SSLSessionCacheOpenSSL(SSL_CTX* ctx, const Config& config) : impl_(NULL) {
    Reset(ctx, config);
  }

  // Destroy this instance. This must be called before the SSL_CTX handle
  // is destroyed.
  ~SSLSessionCacheOpenSSL();

  // Reset the cache configuration. This flushes any existing entries.
  void Reset(SSL_CTX* ctx, const Config& config);

  size_t size() const;

  // Lookup the unique cache key associated with |ssl| connection handle,
  // and find a cached session for it in the cache. If one is found, associate
  // it with the |ssl| connection through SSL_set_session(). Consider using
  // SetSSLSessionWithKey() if you already have the key.
  //
  // Every |check_expiration_count| call to either SetSSLSession() or
  // SetSSLSessionWithKey() triggers a check for, and removal of, expired
  // sessions.
  //
  // Return true iff a cached session was associated with the |ssl| connection.
  bool SetSSLSession(SSL* ssl);

  // A more efficient variant of SetSSLSession() that can be used if the caller
  // already has the cache key for the session of interest. The caller must
  // ensure that the value of |cache_key| matches the result of calling the
  // configuration's |key_func| function with the |ssl| as parameter.
  //
  // Every |check_expiration_count| call to either SetSSLSession() or
  // SetSSLSessionWithKey() triggers a check for, and removal of, expired
  // sessions.
  //
  // Return true iff a cached session was associated with the |ssl| connection.
  bool SetSSLSessionWithKey(SSL* ssl, const std::string& cache_key);

  // Indicates that the SSL session associated with |ssl| is "good" - that is,
  // that all associated cryptographic parameters that were negotiated,
  // including the peer's certificate, were successfully validated. Because
  // OpenSSL does not provide an asynchronous certificate verification
  // callback, it's necessary to manually manage the sessions to ensure that
  // only validated sessions are resumed.
  void MarkSSLSessionAsGood(SSL* ssl);

  // Flush removes all entries from the cache. This is typically called when
  // the system's certificate store has changed.
  void Flush();

  // TODO(digit): Move to client code.
  static const int kDefaultTimeoutSeconds = 60 * 60;
  static const size_t kMaxEntries = 1024;
  static const size_t kMaxExpirationChecks = 256;

 private:
  DISALLOW_COPY_AND_ASSIGN(SSLSessionCacheOpenSSL);

  SSLSessionCacheOpenSSLImpl* impl_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_SESSION_CACHE_OPENSSL_H
