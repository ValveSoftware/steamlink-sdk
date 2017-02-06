// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Cookies API.

#include "chrome/browser/extensions/api/cookies/cookies_api.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/cookies/cookies_api_constants.h"
#include "chrome/browser/extensions/api/cookies/cookies_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/extensions/api/cookies.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace extensions {

namespace cookies = api::cookies;
namespace keys = cookies_api_constants;
namespace Get = cookies::Get;
namespace GetAll = cookies::GetAll;
namespace GetAllCookieStores = cookies::GetAllCookieStores;
namespace Remove = cookies::Remove;
namespace Set = cookies::Set;

namespace {

bool ParseUrl(ChromeAsyncExtensionFunction* function,
              const std::string& url_string,
              GURL* url,
              bool check_host_permissions) {
  *url = GURL(url_string);
  if (!url->is_valid()) {
    function->SetError(
        ErrorUtils::FormatErrorMessage(keys::kInvalidUrlError, url_string));
    return false;
  }
  // Check against host permissions if needed.
  if (check_host_permissions &&
      !function->extension()->permissions_data()->HasHostPermission(*url)) {
    function->SetError(ErrorUtils::FormatErrorMessage(
        keys::kNoHostPermissionsError, url->spec()));
    return false;
  }
  return true;
}

bool ParseStoreContext(ChromeAsyncExtensionFunction* function,
                       std::string* store_id,
                       net::URLRequestContextGetter** context) {
  DCHECK((context || store_id->empty()));
  Profile* store_profile = NULL;
  if (!store_id->empty()) {
    store_profile = cookies_helpers::ChooseProfileFromStoreId(
        *store_id, function->GetProfile(), function->include_incognito());
    if (!store_profile) {
      function->SetError(ErrorUtils::FormatErrorMessage(
          keys::kInvalidStoreIdError, *store_id));
      return false;
    }
  } else {
    // The store ID was not specified; use the current execution context's
    // cookie store by default.
    // GetCurrentBrowser() already takes into account incognito settings.
    Browser* current_browser = function->GetCurrentBrowser();
    if (!current_browser) {
      function->SetError(keys::kNoCookieStoreFoundError);
      return false;
    }
    store_profile = current_browser->profile();
    *store_id = cookies_helpers::GetStoreIdFromProfile(store_profile);
  }

  if (context)
    *context = store_profile->GetRequestContext();
  DCHECK(context);

  return true;
}

}  // namespace

CookiesEventRouter::CookiesEventRouter(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  CHECK(registrar_.IsEmpty());
  registrar_.Add(this,
                 chrome::NOTIFICATION_COOKIE_CHANGED_FOR_EXTENSIONS,
                 content::NotificationService::AllBrowserContextsAndSources());
}

CookiesEventRouter::~CookiesEventRouter() {
}

void CookiesEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_COOKIE_CHANGED_FOR_EXTENSIONS, type);

  Profile* profile = content::Source<Profile>(source).ptr();
  if (!profile_->IsSameProfile(profile))
    return;

  CookieChanged(profile, content::Details<ChromeCookieDetails>(details).ptr());
}

void CookiesEventRouter::CookieChanged(
    Profile* profile,
    ChromeCookieDetails* details) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetBoolean(keys::kRemovedKey, details->removed);

  cookies::Cookie cookie = cookies_helpers::CreateCookie(
      *details->cookie, cookies_helpers::GetStoreIdFromProfile(profile));
  dict->Set(keys::kCookieKey, cookie.ToValue());

  // Map the internal cause to an external string.
  std::string cause;
  switch (details->cause) {
    case net::CookieMonsterDelegate::CHANGE_COOKIE_EXPLICIT:
      cause = keys::kExplicitChangeCause;
      break;

    case net::CookieMonsterDelegate::CHANGE_COOKIE_OVERWRITE:
      cause = keys::kOverwriteChangeCause;
      break;

    case net::CookieMonsterDelegate::CHANGE_COOKIE_EXPIRED:
      cause = keys::kExpiredChangeCause;
      break;

    case net::CookieMonsterDelegate::CHANGE_COOKIE_EVICTED:
      cause = keys::kEvictedChangeCause;
      break;

    case net::CookieMonsterDelegate::CHANGE_COOKIE_EXPIRED_OVERWRITE:
      cause = keys::kExpiredOverwriteChangeCause;
      break;

    default:
      NOTREACHED();
  }
  dict->SetString(keys::kCauseKey, cause);

  args->Append(std::move(dict));

  GURL cookie_domain =
      cookies_helpers::GetURLFromCanonicalCookie(*details->cookie);
  DispatchEvent(profile, events::COOKIES_ON_CHANGED,
                cookies::OnChanged::kEventName, std::move(args), cookie_domain);
}

void CookiesEventRouter::DispatchEvent(
    content::BrowserContext* context,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args,
    GURL& cookie_domain) {
  EventRouter* router = context ? EventRouter::Get(context) : NULL;
  if (!router)
    return;
  std::unique_ptr<Event> event(
      new Event(histogram_value, event_name, std::move(event_args)));
  event->restrict_to_browser_context = context;
  event->event_url = cookie_domain;
  router->BroadcastEvent(std::move(event));
}

CookiesGetFunction::CookiesGetFunction() {
}

CookiesGetFunction::~CookiesGetFunction() {
}

bool CookiesGetFunction::RunAsync() {
  parsed_args_ = Get::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parsed_args_.get());

  // Read/validate input parameters.
  if (!ParseUrl(this, parsed_args_->details.url, &url_, true))
    return false;

  std::string store_id =
      parsed_args_->details.store_id.get() ? *parsed_args_->details.store_id
                                           : std::string();
  net::URLRequestContextGetter* store_context = NULL;
  if (!ParseStoreContext(this, &store_id, &store_context))
    return false;
  store_browser_context_ = store_context;
  if (!parsed_args_->details.store_id.get())
    parsed_args_->details.store_id.reset(new std::string(store_id));

  store_browser_context_ = store_context;

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CookiesGetFunction::GetCookieOnIOThread, this));
  DCHECK(rv);

  // Will finish asynchronously.
  return true;
}

void CookiesGetFunction::GetCookieOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::CookieStore* cookie_store =
      store_browser_context_->GetURLRequestContext()->cookie_store();
  cookies_helpers::GetCookieListFromStore(
      cookie_store, url_,
      base::Bind(&CookiesGetFunction::GetCookieCallback, this));
}

void CookiesGetFunction::GetCookieCallback(const net::CookieList& cookie_list) {
  for (const net::CanonicalCookie& cookie : cookie_list) {
    // Return the first matching cookie. Relies on the fact that the
    // CookieMonster returns them in canonical order (longest path, then
    // earliest creation time).
    if (cookie.Name() == parsed_args_->details.name) {
      cookies::Cookie api_cookie = cookies_helpers::CreateCookie(
          cookie, *parsed_args_->details.store_id);
      results_ = Get::Results::Create(api_cookie);
      break;
    }
  }

  // The cookie doesn't exist; return null.
  if (!results_)
    SetResult(base::Value::CreateNullValue());

  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CookiesGetFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void CookiesGetFunction::RespondOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SendResponse(true);
}

CookiesGetAllFunction::CookiesGetAllFunction() {
}

CookiesGetAllFunction::~CookiesGetAllFunction() {
}

bool CookiesGetAllFunction::RunAsync() {
  parsed_args_ = GetAll::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parsed_args_.get());

  if (parsed_args_->details.url.get() &&
      !ParseUrl(this, *parsed_args_->details.url, &url_, false)) {
    return false;
  }

  std::string store_id =
      parsed_args_->details.store_id.get() ? *parsed_args_->details.store_id
                                           : std::string();
  net::URLRequestContextGetter* store_context = NULL;
  if (!ParseStoreContext(this, &store_id, &store_context))
    return false;
  store_browser_context_ = store_context;
  if (!parsed_args_->details.store_id.get())
    parsed_args_->details.store_id.reset(new std::string(store_id));

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CookiesGetAllFunction::GetAllCookiesOnIOThread, this));
  DCHECK(rv);

  // Will finish asynchronously.
  return true;
}

void CookiesGetAllFunction::GetAllCookiesOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::CookieStore* cookie_store =
      store_browser_context_->GetURLRequestContext()->cookie_store();
  cookies_helpers::GetCookieListFromStore(
      cookie_store, url_,
      base::Bind(&CookiesGetAllFunction::GetAllCookiesCallback, this));
}

void CookiesGetAllFunction::GetAllCookiesCallback(
    const net::CookieList& cookie_list) {
  if (extension()) {
    std::vector<cookies::Cookie> match_vector;
    cookies_helpers::AppendMatchingCookiesToVector(
        cookie_list, url_, &parsed_args_->details, extension(), &match_vector);

    results_ = GetAll::Results::Create(match_vector);
  }
  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CookiesGetAllFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void CookiesGetAllFunction::RespondOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SendResponse(true);
}

CookiesSetFunction::CookiesSetFunction() : success_(false) {
}

CookiesSetFunction::~CookiesSetFunction() {
}

bool CookiesSetFunction::RunAsync() {
  parsed_args_ = Set::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parsed_args_.get());

  // Read/validate input parameters.
  if (!ParseUrl(this, parsed_args_->details.url, &url_, true))
      return false;

  std::string store_id =
      parsed_args_->details.store_id.get() ? *parsed_args_->details.store_id
                                           : std::string();
  net::URLRequestContextGetter* store_context = NULL;
  if (!ParseStoreContext(this, &store_id, &store_context))
    return false;
  store_browser_context_ = store_context;
  if (!parsed_args_->details.store_id.get())
    parsed_args_->details.store_id.reset(new std::string(store_id));

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CookiesSetFunction::SetCookieOnIOThread, this));
  DCHECK(rv);

  // Will finish asynchronously.
  return true;
}

void CookiesSetFunction::SetCookieOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::CookieStore* cookie_store =
      store_browser_context_->GetURLRequestContext()->cookie_store();

  base::Time expiration_time;
  if (parsed_args_->details.expiration_date.get()) {
    // Time::FromDoubleT converts double time 0 to empty Time object. So we need
    // to do special handling here.
    expiration_time = (*parsed_args_->details.expiration_date == 0) ?
        base::Time::UnixEpoch() :
        base::Time::FromDoubleT(*parsed_args_->details.expiration_date);
  }

  net::CookieSameSite same_site = net::CookieSameSite::DEFAULT_MODE;
  switch (parsed_args_->details.same_site) {
  case cookies::SAME_SITE_STATUS_NONE:
  case cookies::SAME_SITE_STATUS_NO_RESTRICTION:
    same_site = net::CookieSameSite::DEFAULT_MODE;
    break;
  case cookies::SAME_SITE_STATUS_LAX:
    same_site = net::CookieSameSite::LAX_MODE;
    break;
  case cookies::SAME_SITE_STATUS_STRICT:
    same_site = net::CookieSameSite::STRICT_MODE;
    break;
  }

  bool are_experimental_cookie_features_enabled =
      store_browser_context_->GetURLRequestContext()
          ->network_delegate()
          ->AreExperimentalCookieFeaturesEnabled();

  // clang-format off
  cookie_store->SetCookieWithDetailsAsync(
      url_, parsed_args_->details.name.get() ? *parsed_args_->details.name
                                             : std::string(),
      parsed_args_->details.value.get() ? *parsed_args_->details.value
                                        : std::string(),
      parsed_args_->details.domain.get() ? *parsed_args_->details.domain
                                         : std::string(),
      parsed_args_->details.path.get() ? *parsed_args_->details.path
                                       : std::string(),
      base::Time(),
      expiration_time,
      base::Time(),
      parsed_args_->details.secure.get() ? *parsed_args_->details.secure.get()
                                         : false,
      parsed_args_->details.http_only.get() ? *parsed_args_->details.http_only
                                            : false,
      same_site,
      are_experimental_cookie_features_enabled,
      net::COOKIE_PRIORITY_DEFAULT,
      base::Bind(&CookiesSetFunction::PullCookie, this));
  // clang-format on
}

void CookiesSetFunction::PullCookie(bool set_cookie_result) {
  // Pull the newly set cookie.
  net::CookieStore* cookie_store =
      store_browser_context_->GetURLRequestContext()->cookie_store();
  success_ = set_cookie_result;
  cookies_helpers::GetCookieListFromStore(
      cookie_store, url_,
      base::Bind(&CookiesSetFunction::PullCookieCallback, this));
}

void CookiesSetFunction::PullCookieCallback(
    const net::CookieList& cookie_list) {
  for (const net::CanonicalCookie& cookie : cookie_list) {
    // Return the first matching cookie. Relies on the fact that the
    // CookieMonster returns them in canonical order (longest path, then
    // earliest creation time).
    std::string name =
        parsed_args_->details.name.get() ? *parsed_args_->details.name
                                         : std::string();
    if (cookie.Name() == name) {
      cookies::Cookie api_cookie = cookies_helpers::CreateCookie(
          cookie, *parsed_args_->details.store_id);
      results_ = Set::Results::Create(api_cookie);
      break;
    }
  }

  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CookiesSetFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void CookiesSetFunction::RespondOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success_) {
    std::string name =
        parsed_args_->details.name.get() ? *parsed_args_->details.name
                                         : std::string();
    error_ = ErrorUtils::FormatErrorMessage(keys::kCookieSetFailedError, name);
  }
  SendResponse(success_);
}

CookiesRemoveFunction::CookiesRemoveFunction() {
}

CookiesRemoveFunction::~CookiesRemoveFunction() {
}

bool CookiesRemoveFunction::RunAsync() {
  parsed_args_ = Remove::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parsed_args_.get());

  // Read/validate input parameters.
  if (!ParseUrl(this, parsed_args_->details.url, &url_, true))
    return false;

  std::string store_id =
      parsed_args_->details.store_id.get() ? *parsed_args_->details.store_id
                                           : std::string();
  net::URLRequestContextGetter* store_context = NULL;
  if (!ParseStoreContext(this, &store_id, &store_context))
    return false;
  store_browser_context_ = store_context;
  if (!parsed_args_->details.store_id.get())
    parsed_args_->details.store_id.reset(new std::string(store_id));

  // Pass the work off to the IO thread.
  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CookiesRemoveFunction::RemoveCookieOnIOThread, this));
  DCHECK(rv);

  // Will return asynchronously.
  return true;
}

void CookiesRemoveFunction::RemoveCookieOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Remove the cookie
  net::CookieStore* cookie_store =
      store_browser_context_->GetURLRequestContext()->cookie_store();
  cookie_store->DeleteCookieAsync(
      url_, parsed_args_->details.name,
      base::Bind(&CookiesRemoveFunction::RemoveCookieCallback, this));
}

void CookiesRemoveFunction::RemoveCookieCallback() {
  // Build the callback result
  Remove::Results::Details details;
  details.name = parsed_args_->details.name;
  details.url = url_.spec();
  details.store_id = *parsed_args_->details.store_id;
  results_ = Remove::Results::Create(details);

  // Return to UI thread
  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CookiesRemoveFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void CookiesRemoveFunction::RespondOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SendResponse(true);
}

bool CookiesGetAllCookieStoresFunction::RunSync() {
  Profile* original_profile = GetProfile();
  DCHECK(original_profile);
  std::unique_ptr<base::ListValue> original_tab_ids(new base::ListValue());
  Profile* incognito_profile = NULL;
  std::unique_ptr<base::ListValue> incognito_tab_ids;
  if (include_incognito() && GetProfile()->HasOffTheRecordProfile()) {
    incognito_profile = GetProfile()->GetOffTheRecordProfile();
    if (incognito_profile)
      incognito_tab_ids.reset(new base::ListValue());
  }
  DCHECK(original_profile != incognito_profile);

  // Iterate through all browser instances, and for each browser,
  // add its tab IDs to either the regular or incognito tab ID list depending
  // whether the browser is regular or incognito.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == original_profile) {
      cookies_helpers::AppendToTabIdList(browser, original_tab_ids.get());
    } else if (incognito_tab_ids.get() &&
               browser->profile() == incognito_profile) {
      cookies_helpers::AppendToTabIdList(browser, incognito_tab_ids.get());
    }
  }
  // Return a list of all cookie stores with at least one open tab.
  std::vector<cookies::CookieStore> cookie_stores;
  if (original_tab_ids->GetSize() > 0) {
    cookie_stores.push_back(cookies_helpers::CreateCookieStore(
        original_profile, original_tab_ids.release()));
  }
  if (incognito_tab_ids.get() && incognito_tab_ids->GetSize() > 0 &&
      incognito_profile) {
    cookie_stores.push_back(cookies_helpers::CreateCookieStore(
        incognito_profile, incognito_tab_ids.release()));
  }
  results_ = GetAllCookieStores::Results::Create(cookie_stores);
  return true;
}

CookiesAPI::CookiesAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter::Get(browser_context_)
      ->RegisterObserver(this, cookies::OnChanged::kEventName);
}

CookiesAPI::~CookiesAPI() {
}

void CookiesAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<CookiesAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<CookiesAPI>* CookiesAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void CookiesAPI::OnListenerAdded(const EventListenerInfo& details) {
  cookies_event_router_.reset(new CookiesEventRouter(browser_context_));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace extensions
