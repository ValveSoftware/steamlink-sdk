// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_session_cache_openssl.h"

#include <list>
#include <map>

#include <openssl/rand.h>
#include <openssl/ssl.h>

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"

namespace net {

namespace {

// A helper class to lazily create a new EX_DATA index to map SSL_CTX handles
// to their corresponding SSLSessionCacheOpenSSLImpl object.
class SSLContextExIndex {
public:
  SSLContextExIndex() {
    context_index_ = SSL_CTX_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    DCHECK_NE(-1, context_index_);
    session_index_ = SSL_SESSION_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    DCHECK_NE(-1, session_index_);
  }

  int context_index() const { return context_index_; }
  int session_index() const { return session_index_; }

 private:
  int context_index_;
  int session_index_;
};

// static
base::LazyInstance<SSLContextExIndex>::Leaky s_ssl_context_ex_instance =
    LAZY_INSTANCE_INITIALIZER;

// Retrieve the global EX_DATA index, created lazily on first call, to
// be used with SSL_CTX_set_ex_data() and SSL_CTX_get_ex_data().
static int GetSSLContextExIndex() {
  return s_ssl_context_ex_instance.Get().context_index();
}

// Retrieve the global EX_DATA index, created lazily on first call, to
// be used with SSL_SESSION_set_ex_data() and SSL_SESSION_get_ex_data().
static int GetSSLSessionExIndex() {
  return s_ssl_context_ex_instance.Get().session_index();
}

// Helper struct used to store session IDs in a SessionIdIndex container
// (see definition below). To save memory each entry only holds a pointer
// to the session ID buffer, which must outlive the entry itself. On the
// other hand, a hash is included to minimize the number of hashing
// computations during cache operations.
struct SessionId {
  SessionId(const unsigned char* a_id, unsigned a_id_len)
      : id(a_id), id_len(a_id_len), hash(ComputeHash(a_id, a_id_len)) {}

  explicit SessionId(const SessionId& other)
      : id(other.id), id_len(other.id_len), hash(other.hash) {}

  explicit SessionId(SSL_SESSION* session)
      : id(session->session_id),
        id_len(session->session_id_length),
        hash(ComputeHash(session->session_id, session->session_id_length)) {}

  bool operator==(const SessionId& other) const {
    return hash == other.hash && id_len == other.id_len &&
           !memcmp(id, other.id, id_len);
  }

  const unsigned char* id;
  unsigned id_len;
  size_t hash;

 private:
  // Session ID are random strings of bytes. This happens to compute the same
  // value as std::hash<std::string> without the extra string copy. See
  // base/containers/hash_tables.h. Other hashing computations are possible,
  // this one is just simple enough to do the job.
  size_t ComputeHash(const unsigned char* id, unsigned id_len) {
    size_t result = 0;
    for (unsigned n = 0; n < id_len; ++n)
      result += 131 * id[n];
    return result;
  }
};

}  // namespace

}  // namespace net

namespace BASE_HASH_NAMESPACE {

template <>
struct hash<net::SessionId> {
  std::size_t operator()(const net::SessionId& entry) const {
    return entry.hash;
  }
};

}  // namespace BASE_HASH_NAMESPACE

namespace net {

// Implementation of the real SSLSessionCache.
//
// The implementation is inspired by base::MRUCache, except that the deletor
// also needs to remove the entry from other containers. In a nutshell, this
// uses several basic containers:
//
//   |ordering_| is a doubly-linked list of SSL_SESSION handles, ordered in
//   MRU order.
//
//   |key_index_| is a hash table mapping unique cache keys (e.g. host/port
//   values) to a single iterator of |ordering_|. It is used to efficiently
//   find the cached session associated with a given key.
//
//   |id_index_| is a hash table mapping SessionId values to iterators
//   of |key_index_|. If is used to efficiently remove sessions from the cache,
//   as well as check for the existence of a session ID value in the cache.
//
//   SSL_SESSION objects are reference-counted, and owned by the cache. This
//   means that their reference count is incremented when they are added, and
//   decremented when they are removed.
//
// Assuming an average key size of 100 characters, each node requires the
// following memory usage on 32-bit Android, when linked against STLport:
//
//      12   (ordering_ node, including SSL_SESSION handle)
//     100   (key characters)
//    + 24   (std::string header/minimum size)
//    +  8   (key_index_ node, excluding the 2 lines above for the key).
//    + 20   (id_index_ node)
//  --------
//     164   bytes/node
//
// Hence, 41 KiB for a full cache with a maximum of 1024 entries, excluding
// the size of SSL_SESSION objects and heap fragmentation.
//

class SSLSessionCacheOpenSSLImpl {
 public:
  // Construct new instance. This registers various hooks into the SSL_CTX
  // context |ctx|. OpenSSL will call back during SSL connection
  // operations. |key_func| is used to map a SSL handle to a unique cache
  // string, according to the client's preferences.
  SSLSessionCacheOpenSSLImpl(SSL_CTX* ctx,
                             const SSLSessionCacheOpenSSL::Config& config)
      : ctx_(ctx), config_(config), expiration_check_(0) {
    DCHECK(ctx);

    // NO_INTERNAL_STORE disables OpenSSL's builtin cache, and
    // NO_AUTO_CLEAR disables the call to SSL_CTX_flush_sessions
    // every 256 connections (this number is hard-coded in the library
    // and can't be changed).
    SSL_CTX_set_session_cache_mode(ctx_,
                                   SSL_SESS_CACHE_CLIENT |
                                       SSL_SESS_CACHE_NO_INTERNAL_STORE |
                                       SSL_SESS_CACHE_NO_AUTO_CLEAR);

    SSL_CTX_sess_set_new_cb(ctx_, NewSessionCallbackStatic);
    SSL_CTX_sess_set_remove_cb(ctx_, RemoveSessionCallbackStatic);
    SSL_CTX_set_generate_session_id(ctx_, GenerateSessionIdStatic);
    SSL_CTX_set_timeout(ctx_, config_.timeout_seconds);

    SSL_CTX_set_ex_data(ctx_, GetSSLContextExIndex(), this);
  }

  // Destroy this instance. Must happen before |ctx_| is destroyed.
  ~SSLSessionCacheOpenSSLImpl() {
    Flush();
    SSL_CTX_set_ex_data(ctx_, GetSSLContextExIndex(), NULL);
    SSL_CTX_sess_set_new_cb(ctx_, NULL);
    SSL_CTX_sess_set_remove_cb(ctx_, NULL);
    SSL_CTX_set_generate_session_id(ctx_, NULL);
  }

  // Return the number of items in this cache.
  size_t size() const { return key_index_.size(); }

  // Retrieve the cache key from |ssl| and look for a corresponding
  // cached session ID. If one is found, call SSL_set_session() to associate
  // it with the |ssl| connection.
  //
  // Will also check for expired sessions every |expiration_check_count|
  // calls.
  //
  // Return true if a cached session ID was found, false otherwise.
  bool SetSSLSession(SSL* ssl) {
    std::string cache_key = config_.key_func(ssl);
    if (cache_key.empty())
      return false;

    return SetSSLSessionWithKey(ssl, cache_key);
  }

  // Variant of SetSSLSession to be used when the client already has computed
  // the cache key. Avoid a call to the configuration's |key_func| function.
  bool SetSSLSessionWithKey(SSL* ssl, const std::string& cache_key) {
    base::AutoLock locked(lock_);

    DCHECK_EQ(config_.key_func(ssl), cache_key);

    if (++expiration_check_ >= config_.expiration_check_count) {
      expiration_check_ = 0;
      FlushExpiredSessionsLocked();
    }

    KeyIndex::iterator it = key_index_.find(cache_key);
    if (it == key_index_.end())
      return false;

    SSL_SESSION* session = *it->second;
    DCHECK(session);

    DVLOG(2) << "Lookup session: " << session << " for " << cache_key;

    void* session_is_good =
        SSL_SESSION_get_ex_data(session, GetSSLSessionExIndex());
    if (!session_is_good)
      return false;  // Session has not yet been marked good. Treat as a miss.

    // Move to front of MRU list.
    ordering_.push_front(session);
    ordering_.erase(it->second);
    it->second = ordering_.begin();

    return SSL_set_session(ssl, session) == 1;
  }

  void MarkSSLSessionAsGood(SSL* ssl) {
    SSL_SESSION* session = SSL_get_session(ssl);
    if (!session)
      return;

    // Mark the session as good, allowing it to be used for future connections.
    SSL_SESSION_set_ex_data(
        session, GetSSLSessionExIndex(), reinterpret_cast<void*>(1));
  }

  // Flush all entries from the cache.
  void Flush() {
    base::AutoLock lock(lock_);
    id_index_.clear();
    key_index_.clear();
    while (!ordering_.empty()) {
      SSL_SESSION* session = ordering_.front();
      ordering_.pop_front();
      SSL_SESSION_free(session);
    }
  }

 private:
  // Type for list of SSL_SESSION handles, ordered in MRU order.
  typedef std::list<SSL_SESSION*> MRUSessionList;
  // Type for a dictionary from unique cache keys to session list nodes.
  typedef base::hash_map<std::string, MRUSessionList::iterator> KeyIndex;
  // Type for a dictionary from SessionId values to key index nodes.
  typedef base::hash_map<SessionId, KeyIndex::iterator> SessionIdIndex;

  // Return the key associated with a given session, or the empty string if
  // none exist. This shall only be used for debugging.
  std::string SessionKey(SSL_SESSION* session) {
    if (!session)
      return std::string("<null-session>");

    if (session->session_id_length == 0)
      return std::string("<empty-session-id>");

    SessionIdIndex::iterator it = id_index_.find(SessionId(session));
    if (it == id_index_.end())
      return std::string("<unknown-session>");

    return it->second->first;
  }

  // Remove a given |session| from the cache. Lock must be held.
  void RemoveSessionLocked(SSL_SESSION* session) {
    lock_.AssertAcquired();
    DCHECK(session);
    DCHECK_GT(session->session_id_length, 0U);
    SessionId session_id(session);
    SessionIdIndex::iterator id_it = id_index_.find(session_id);
    if (id_it == id_index_.end()) {
      LOG(ERROR) << "Trying to remove unknown session from cache: " << session;
      return;
    }
    KeyIndex::iterator key_it = id_it->second;
    DCHECK(key_it != key_index_.end());
    DCHECK_EQ(session, *key_it->second);

    id_index_.erase(session_id);
    ordering_.erase(key_it->second);
    key_index_.erase(key_it);

    SSL_SESSION_free(session);

    DCHECK_EQ(key_index_.size(), id_index_.size());
  }

  // Used internally to flush expired sessions. Lock must be held.
  void FlushExpiredSessionsLocked() {
    lock_.AssertAcquired();

    // Unfortunately, OpenSSL initializes |session->time| with a time()
    // timestamps, which makes mocking / unit testing difficult.
    long timeout_secs = static_cast<long>(::time(NULL));
    MRUSessionList::iterator it = ordering_.begin();
    while (it != ordering_.end()) {
      SSL_SESSION* session = *it++;

      // Important, use <= instead of < here to allow unit testing to
      // work properly. That's because unit tests that check the expiration
      // behaviour will use a session timeout of 0 seconds.
      if (session->time + session->timeout <= timeout_secs) {
        DVLOG(2) << "Expiring session " << session << " for "
                 << SessionKey(session);
        RemoveSessionLocked(session);
      }
    }
  }

  // Retrieve the cache associated with a given SSL context |ctx|.
  static SSLSessionCacheOpenSSLImpl* GetCache(SSL_CTX* ctx) {
    DCHECK(ctx);
    void* result = SSL_CTX_get_ex_data(ctx, GetSSLContextExIndex());
    DCHECK(result);
    return reinterpret_cast<SSLSessionCacheOpenSSLImpl*>(result);
  }

  // Called by OpenSSL when a new |session| was created and added to a given
  // |ssl| connection. Note that the session's reference count was already
  // incremented before the function is entered. The function must return 1
  // to indicate that it took ownership of the session, i.e. that the caller
  // should not decrement its reference count after completion.
  static int NewSessionCallbackStatic(SSL* ssl, SSL_SESSION* session) {
    GetCache(ssl->ctx)->OnSessionAdded(ssl, session);
    return 1;
  }

  // Called by OpenSSL to indicate that a session must be removed from the
  // cache. This happens when SSL_CTX is destroyed.
  static void RemoveSessionCallbackStatic(SSL_CTX* ctx, SSL_SESSION* session) {
    GetCache(ctx)->OnSessionRemoved(session);
  }

  // Called by OpenSSL to generate a new session ID. This happens during a
  // SSL connection operation, when the SSL object doesn't have a session yet.
  //
  // A session ID is a random string of bytes used to uniquely identify the
  // session between a client and a server.
  //
  // |ssl| is a SSL connection handle. Ignored here.
  // |id| is the target buffer where the ID must be generated.
  // |*id_len| is, on input, the size of the desired ID. It will be 16 for
  // SSLv2, and 32 for anything else. OpenSSL allows an implementation
  // to change it on output, but this will not happen here.
  //
  // The function must ensure the generated ID is really unique, i.e. that
  // another session in the cache doesn't already use the same value. It must
  // return 1 to indicate success, or 0 for failure.
  static int GenerateSessionIdStatic(const SSL* ssl,
                                     unsigned char* id,
                                     unsigned* id_len) {
    if (!GetCache(ssl->ctx)->OnGenerateSessionId(id, *id_len))
      return 0;

    return 1;
  }

  // Add |session| to the cache in association with |cache_key|. If a session
  // already exists, it is replaced with the new one. This assumes that the
  // caller already incremented the session's reference count.
  void OnSessionAdded(SSL* ssl, SSL_SESSION* session) {
    base::AutoLock locked(lock_);
    DCHECK(ssl);
    DCHECK_GT(session->session_id_length, 0U);
    std::string cache_key = config_.key_func(ssl);
    KeyIndex::iterator it = key_index_.find(cache_key);
    if (it == key_index_.end()) {
      DVLOG(2) << "Add session " << session << " for " << cache_key;
      // This is a new session. Add it to the cache.
      ordering_.push_front(session);
      std::pair<KeyIndex::iterator, bool> ret =
          key_index_.insert(std::make_pair(cache_key, ordering_.begin()));
      DCHECK(ret.second);
      it = ret.first;
      DCHECK(it != key_index_.end());
    } else {
      // An existing session exists for this key, so replace it if needed.
      DVLOG(2) << "Replace session " << *it->second << " with " << session
               << " for " << cache_key;
      SSL_SESSION* old_session = *it->second;
      if (old_session != session) {
        id_index_.erase(SessionId(old_session));
        SSL_SESSION_free(old_session);
      }
      ordering_.erase(it->second);
      ordering_.push_front(session);
      it->second = ordering_.begin();
    }

    id_index_[SessionId(session)] = it;

    if (key_index_.size() > config_.max_entries)
      ShrinkCacheLocked();

    DCHECK_EQ(key_index_.size(), id_index_.size());
    DCHECK_LE(key_index_.size(), config_.max_entries);
  }

  // Shrink the cache to ensure no more than config_.max_entries entries,
  // starting with older entries first. Lock must be acquired.
  void ShrinkCacheLocked() {
    lock_.AssertAcquired();
    DCHECK_EQ(key_index_.size(), ordering_.size());
    DCHECK_EQ(key_index_.size(), id_index_.size());

    while (key_index_.size() > config_.max_entries) {
      MRUSessionList::reverse_iterator it = ordering_.rbegin();
      DCHECK(it != ordering_.rend());

      SSL_SESSION* session = *it;
      DCHECK(session);
      DVLOG(2) << "Evicting session " << session << " for "
               << SessionKey(session);
      RemoveSessionLocked(session);
    }
  }

  // Remove |session| from the cache.
  void OnSessionRemoved(SSL_SESSION* session) {
    base::AutoLock locked(lock_);
    DVLOG(2) << "Remove session " << session << " for " << SessionKey(session);
    RemoveSessionLocked(session);
  }

  // See GenerateSessionIdStatic for a description of what this function does.
  bool OnGenerateSessionId(unsigned char* id, unsigned id_len) {
    base::AutoLock locked(lock_);
    // This mimics def_generate_session_id() in openssl/ssl/ssl_sess.cc,
    // I.e. try to generate a pseudo-random bit string, and check that no
    // other entry in the cache has the same value.
    const size_t kMaxTries = 10;
    for (size_t tries = 0; tries < kMaxTries; ++tries) {
      if (RAND_pseudo_bytes(id, id_len) <= 0) {
        DLOG(ERROR) << "Couldn't generate " << id_len
                    << " pseudo random bytes?";
        return false;
      }
      if (id_index_.find(SessionId(id, id_len)) == id_index_.end())
        return true;
    }
    DLOG(ERROR) << "Couldn't generate unique session ID of " << id_len
                << "bytes after " << kMaxTries << " tries.";
    return false;
  }

  SSL_CTX* ctx_;
  SSLSessionCacheOpenSSL::Config config_;

  // method to get the index which can later be used with SSL_CTX_get_ex_data()
  // or SSL_CTX_set_ex_data().
  base::Lock lock_;  // Protects access to containers below.

  MRUSessionList ordering_;
  KeyIndex key_index_;
  SessionIdIndex id_index_;

  size_t expiration_check_;
};

SSLSessionCacheOpenSSL::~SSLSessionCacheOpenSSL() { delete impl_; }

size_t SSLSessionCacheOpenSSL::size() const { return impl_->size(); }

void SSLSessionCacheOpenSSL::Reset(SSL_CTX* ctx, const Config& config) {
  if (impl_)
    delete impl_;

  impl_ = new SSLSessionCacheOpenSSLImpl(ctx, config);
}

bool SSLSessionCacheOpenSSL::SetSSLSession(SSL* ssl) {
  return impl_->SetSSLSession(ssl);
}

bool SSLSessionCacheOpenSSL::SetSSLSessionWithKey(
    SSL* ssl,
    const std::string& cache_key) {
  return impl_->SetSSLSessionWithKey(ssl, cache_key);
}

void SSLSessionCacheOpenSSL::MarkSSLSessionAsGood(SSL* ssl) {
  return impl_->MarkSSLSessionAsGood(ssl);
}

void SSLSessionCacheOpenSSL::Flush() { impl_->Flush(); }

}  // namespace net
