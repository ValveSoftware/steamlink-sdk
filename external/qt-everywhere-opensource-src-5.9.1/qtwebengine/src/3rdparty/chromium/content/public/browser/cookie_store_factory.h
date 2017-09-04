// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COOKIE_STORE_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_COOKIE_STORE_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {
class CookieCryptoDelegate;
class CookieMonsterDelegate;
class CookieStore;
}

namespace storage {
class SpecialStoragePolicy;
}

namespace content {

struct CONTENT_EXPORT CookieStoreConfig {
  // Specifies how session cookies are persisted in the backing data store.
  //
  // EPHEMERAL_SESSION_COOKIES specifies session cookies will not be written
  // out in a manner that allows for restoration.
  //
  // PERSISTANT_SESSION_COOKIES specifies that session cookies are not restored
  // when the cookie store is opened, however they will be written in a manner
  // that allows for them to be restored if the cookie store is opened again
  // using RESTORED_SESSION_COOKIES.
  //
  // RESTORED_SESSION_COOKIES is the: same as PERSISTANT_SESSION_COOKIES
  // except when the cookie store is opened, the previously written session
  // cookies are loaded first.
  enum SessionCookieMode {
    EPHEMERAL_SESSION_COOKIES,
    PERSISTANT_SESSION_COOKIES,
    RESTORED_SESSION_COOKIES
  };

  // Convenience constructor for an in-memory cookie store with no delegate.
  CookieStoreConfig();

  // If |path| is empty, then this specifies an in-memory cookie store.
  // With in-memory cookie stores, |session_cookie_mode| must be
  // EPHEMERAL_SESSION_COOKIES.
  //
  // Note: If |crypto_delegate| is non-nullptr, it must outlive any CookieStores
  // created using this config.
  CookieStoreConfig(const base::FilePath& path,
                    SessionCookieMode session_cookie_mode,
                    storage::SpecialStoragePolicy* storage_policy,
                    net::CookieMonsterDelegate* cookie_delegate);
  ~CookieStoreConfig();

  const base::FilePath path;
  const SessionCookieMode session_cookie_mode;
  const scoped_refptr<storage::SpecialStoragePolicy> storage_policy;
  const scoped_refptr<net::CookieMonsterDelegate> cookie_delegate;

  // The following are infrequently used cookie store parameters.
  // Rather than clutter the constructor API, these are assigned a default
  // value on CookieStoreConfig construction. Clients should then override
  // them as necessary.

  // Used to provide encryption hooks for the cookie store. The
  // CookieCryptoDelegate must outlive any cookie store created with this
  // config.
  net::CookieCryptoDelegate* crypto_delegate;

  // Callbacks for data load events will be performed on |client_task_runner|.
  // If nullptr, uses the task runner for BrowserThread::IO.
  //
  // Only used for persistent cookie stores.
  scoped_refptr<base::SequencedTaskRunner> client_task_runner;

  // All blocking database accesses will be performed on
  // |background_task_runner|.  If nullptr, uses a SequencedTaskRunner from the
  // BrowserThread blocking pool.
  //
  // Only used for persistent cookie stores.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner;

  // If non-empty, overrides the default list of schemes that support cookies.
  std::vector<std::string> cookieable_schemes;
};

CONTENT_EXPORT std::unique_ptr<net::CookieStore> CreateCookieStore(
    const CookieStoreConfig& config);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COOKIE_STORE_FACTORY_H_
