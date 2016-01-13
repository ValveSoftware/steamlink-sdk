// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A sqlite implementation of a cookie monster persistent store.

#ifndef CONTENT_BROWSER_NET_SQLITE_PERSISTENT_COOKIE_STORE_H_
#define CONTENT_BROWSER_NET_SQLITE_PERSISTENT_COOKIE_STORE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "net/cookies/cookie_monster.h"

class Task;

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace net {
class CanonicalCookie;
}

namespace quota {
class SpecialStoragePolicy;
}

namespace content {
class CookieCryptoDelegate;

// Implements the PersistentCookieStore interface in terms of a SQLite database.
// For documentation about the actual member functions consult the documentation
// of the parent class |net::CookieMonster::PersistentCookieStore|.
// If provided, a |SpecialStoragePolicy| is consulted when the SQLite database
// is closed to decide which cookies to keep.
class CONTENT_EXPORT SQLitePersistentCookieStore
    : public net::CookieMonster::PersistentCookieStore {
 public:
  // All blocking database accesses will be performed on
  // |background_task_runner|, while |client_task_runner| is used to invoke
  // callbacks.
  SQLitePersistentCookieStore(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      bool restore_old_session_cookies,
      quota::SpecialStoragePolicy* special_storage_policy,
      CookieCryptoDelegate* crypto_delegate);

  // net::CookieMonster::PersistentCookieStore:
  virtual void Load(const LoadedCallback& loaded_callback) OVERRIDE;
  virtual void LoadCookiesForKey(const std::string& key,
      const LoadedCallback& callback) OVERRIDE;
  virtual void AddCookie(const net::CanonicalCookie& cc) OVERRIDE;
  virtual void UpdateCookieAccessTime(const net::CanonicalCookie& cc) OVERRIDE;
  virtual void DeleteCookie(const net::CanonicalCookie& cc) OVERRIDE;
  virtual void SetForceKeepSessionState() OVERRIDE;
  virtual void Flush(const base::Closure& callback) OVERRIDE;

 protected:
   virtual ~SQLitePersistentCookieStore();

 private:
  class Backend;

  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(SQLitePersistentCookieStore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NET_SQLITE_PERSISTENT_COOKIE_STORE_H_
