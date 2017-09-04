// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_COOKIE_CHANGED_SUBSCRIPTION_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_COOKIE_CHANGED_SUBSCRIPTION_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/signin/core/browser/signin_client.h"
#include "net/url_request/url_request_context_getter.h"

// The subscription for a cookie changed events. This class lives on the
// main thread.
class SigninCookieChangedSubscription
    : public SigninClient::CookieChangedSubscription,
      public base::SupportsWeakPtr<SigninCookieChangedSubscription> {
 public:
  // Creates a cookie changed subscription and registers for cookie changed
  // events.
  SigninCookieChangedSubscription(
      scoped_refptr<net::URLRequestContextGetter> context_getter,
      const GURL& url,
      const std::string& name,
      const net::CookieStore::CookieChangedCallback& callback);
  ~SigninCookieChangedSubscription() override;

 private:
  // Holder of a cookie store cookie changed subscription.
  struct SubscriptionHolder {
    std::unique_ptr<net::CookieStore::CookieChangedSubscription> subscription;
    SubscriptionHolder();
    ~SubscriptionHolder();
  };

  // Adds a callback for cookie changed events. This method is called on the
  // network thread, so it is safe to access the cookie store.
  static void RegisterForCookieChangesOnIOThread(
      scoped_refptr<net::URLRequestContextGetter> context_getter,
      const GURL url,
      const std::string name,
      const net::CookieStore::CookieChangedCallback callback,
      SigninCookieChangedSubscription::SubscriptionHolder*
          out_subscription_holder);

  void RegisterForCookieChangedNotifications(const GURL& url,
                                             const std::string& name);

  // Posts a task on the |proxy| task runner that calls |OnCookieChanged| on
  // |subscription|.
  // Note that this method is called on the network thread, so |subscription|
  // must not be used here, it is only passed around.
  static void RunAsyncOnCookieChanged(
      scoped_refptr<base::TaskRunner> proxy,
      base::WeakPtr<SigninCookieChangedSubscription> subscription,
      const net::CanonicalCookie& cookie,
      net::CookieStore::ChangeCause cause);

  // Handler for cookie changed events.
  void OnCookieChanged(const net::CanonicalCookie& cookie,
                       net::CookieStore::ChangeCause cause);

  // The context getter.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // The holder of a cookie changed subscription. Must be destroyed on the
  // network thread.
  std::unique_ptr<SubscriptionHolder> subscription_holder_io_;

  // Callback to be run on cookie changed events.
  net::CookieStore::CookieChangedCallback callback_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SigninCookieChangedSubscription);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_COOKIE_CHANGED_SUBSCRIPTION_H_
