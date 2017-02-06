// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/quota_policy_cookie_store.h"

#include <list>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/profiler/scoped_tracker.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/gurl.h"

namespace content {

QuotaPolicyCookieStore::QuotaPolicyCookieStore(
    const scoped_refptr<net::SQLitePersistentCookieStore>& cookie_store,
    storage::SpecialStoragePolicy* special_storage_policy)
    : special_storage_policy_(special_storage_policy),
      persistent_store_(cookie_store) {
}

QuotaPolicyCookieStore::~QuotaPolicyCookieStore() {
  if (!special_storage_policy_.get() ||
      !special_storage_policy_->HasSessionOnlyOrigins()) {
    return;
  }

  std::list<net::SQLitePersistentCookieStore::CookieOrigin>
      session_only_cookies;
  for (const auto& cookie : cookies_per_origin_) {
    if (cookie.second == 0) {
      continue;
    }
    const GURL url(net::cookie_util::CookieOriginToURL(cookie.first.first,
                                                       cookie.first.second));
    if (!url.is_valid() || !special_storage_policy_->IsStorageSessionOnly(url))
      continue;

    session_only_cookies.push_back(cookie.first);
  }

  persistent_store_->DeleteAllInList(session_only_cookies);
}

void QuotaPolicyCookieStore::Load(const LoadedCallback& loaded_callback) {
  persistent_store_->Load(
      base::Bind(&QuotaPolicyCookieStore::OnLoad, this, loaded_callback));
}

void QuotaPolicyCookieStore::LoadCookiesForKey(
    const std::string& key,
    const LoadedCallback& loaded_callback) {
  persistent_store_->LoadCookiesForKey(
      key,
      base::Bind(&QuotaPolicyCookieStore::OnLoad, this, loaded_callback));
}

void QuotaPolicyCookieStore::AddCookie(const net::CanonicalCookie& cc) {
  net::SQLitePersistentCookieStore::CookieOrigin origin(
      cc.Domain(), cc.IsSecure());
  ++cookies_per_origin_[origin];
  persistent_store_->AddCookie(cc);
}

void QuotaPolicyCookieStore::UpdateCookieAccessTime(
    const net::CanonicalCookie& cc) {
  persistent_store_->UpdateCookieAccessTime(cc);
}

void QuotaPolicyCookieStore::DeleteCookie(const net::CanonicalCookie& cc) {
  net::SQLitePersistentCookieStore::CookieOrigin origin(
      cc.Domain(), cc.IsSecure());
  DCHECK_GE(cookies_per_origin_[origin], 1U);
  --cookies_per_origin_[origin];
  persistent_store_->DeleteCookie(cc);
}

void QuotaPolicyCookieStore::SetForceKeepSessionState() {
  special_storage_policy_ = nullptr;
}

void QuotaPolicyCookieStore::Flush(const base::Closure& callback) {
  persistent_store_->Flush(callback);
}

void QuotaPolicyCookieStore::OnLoad(
    const LoadedCallback& loaded_callback,
    const std::vector<net::CanonicalCookie*>& cookies) {
  for (const auto& cookie : cookies) {
    net::SQLitePersistentCookieStore::CookieOrigin origin(
        cookie->Domain(), cookie->IsSecure());
    ++cookies_per_origin_[origin];
  }

  loaded_callback.Run(cookies);
}

CookieStoreConfig::CookieStoreConfig()
  : session_cookie_mode(EPHEMERAL_SESSION_COOKIES),
    crypto_delegate(nullptr) {
  // Default to an in-memory cookie store.
}

CookieStoreConfig::CookieStoreConfig(
    const base::FilePath& path,
    SessionCookieMode session_cookie_mode,
    storage::SpecialStoragePolicy* storage_policy,
    net::CookieMonsterDelegate* cookie_delegate)
    : path(path),
      session_cookie_mode(session_cookie_mode),
      storage_policy(storage_policy),
      cookie_delegate(cookie_delegate),
      crypto_delegate(nullptr) {
  CHECK(!path.empty() || session_cookie_mode == EPHEMERAL_SESSION_COOKIES);
}

CookieStoreConfig::~CookieStoreConfig() {
}

std::unique_ptr<net::CookieStore> CreateCookieStore(
    const CookieStoreConfig& config) {
  // TODO(bcwhite): Remove ScopedTracker below once crbug.com/483686 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION("483686 content::CreateCookieStore"));

  std::unique_ptr<net::CookieMonster> cookie_monster;

  if (config.path.empty()) {
    // Empty path means in-memory store.
    cookie_monster.reset(
        new net::CookieMonster(nullptr, config.cookie_delegate.get()));
  } else {
    scoped_refptr<base::SequencedTaskRunner> client_task_runner =
        config.client_task_runner;
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        config.background_task_runner;

    if (!client_task_runner.get()) {
      client_task_runner =
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
    }

    if (!background_task_runner.get()) {
      background_task_runner =
          BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
              base::SequencedWorkerPool::GetSequenceToken());
    }

    scoped_refptr<net::SQLitePersistentCookieStore> sqlite_store(
        new net::SQLitePersistentCookieStore(
            config.path,
            client_task_runner,
            background_task_runner,
            (config.session_cookie_mode ==
             CookieStoreConfig::RESTORED_SESSION_COOKIES),
            config.crypto_delegate));

    QuotaPolicyCookieStore* persistent_store =
        new QuotaPolicyCookieStore(
            sqlite_store.get(),
            config.storage_policy.get());

    cookie_monster.reset(
        new net::CookieMonster(persistent_store, config.cookie_delegate.get()));
    if ((config.session_cookie_mode ==
         CookieStoreConfig::PERSISTANT_SESSION_COOKIES) ||
        (config.session_cookie_mode ==
         CookieStoreConfig::RESTORED_SESSION_COOKIES)) {
      cookie_monster->SetPersistSessionCookies(true);
    }
  }

  if (!config.cookieable_schemes.empty())
    cookie_monster->SetCookieableSchemes(config.cookieable_schemes);

  return std::move(cookie_monster);
}

}  // namespace content
