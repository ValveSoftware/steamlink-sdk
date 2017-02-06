// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_cookie_changed_subscription.h"

#include "base/threading/thread_task_runner_handle.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

SigninCookieChangedSubscription::SubscriptionHolder::SubscriptionHolder() {
}

SigninCookieChangedSubscription::SubscriptionHolder::~SubscriptionHolder() {
}

SigninCookieChangedSubscription::SigninCookieChangedSubscription(
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    const GURL& url,
    const std::string& name,
    const net::CookieStore::CookieChangedCallback& callback)
    : context_getter_(context_getter),
      subscription_holder_io_(new SubscriptionHolder),
      callback_(callback) {
  RegisterForCookieChangedNotifications(url, name);
}

SigninCookieChangedSubscription::~SigninCookieChangedSubscription() {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner =
      context_getter_->GetNetworkTaskRunner();
  if (network_task_runner->BelongsToCurrentThread()) {
    subscription_holder_io_.reset();
  } else {
    network_task_runner->DeleteSoon(FROM_HERE,
                                    subscription_holder_io_.release());
  }
}

void SigninCookieChangedSubscription::RegisterForCookieChangedNotifications(
    const GURL& url,
    const std::string& name) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The cookie store can only be accessed from the context getter which lives
  // on the network thread. As |AddCookieChangedCallback| is called from the
  // main thread, a thread jump is needed to register for cookie changed
  // notifications.
  net::CookieStore::CookieChangedCallback run_on_current_thread_callback =
      base::Bind(&SigninCookieChangedSubscription::RunAsyncOnCookieChanged,
                 base::ThreadTaskRunnerHandle::Get(), this->AsWeakPtr());
  base::Closure register_closure =
      base::Bind(&RegisterForCookieChangesOnIOThread,
                 context_getter_,
                 url,
                 name,
                 run_on_current_thread_callback,
                 base::Unretained(subscription_holder_io_.get()));
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner =
      context_getter_->GetNetworkTaskRunner();
  if (network_task_runner->BelongsToCurrentThread()) {
    register_closure.Run();
  } else {
    network_task_runner->PostTask(FROM_HERE, register_closure);
  }
}

// static
void SigninCookieChangedSubscription::RegisterForCookieChangesOnIOThread(
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    const GURL url,
    const std::string name,
    const net::CookieStore::CookieChangedCallback callback,
    SigninCookieChangedSubscription::SubscriptionHolder*
        out_subscription_holder) {
  DCHECK(out_subscription_holder);
  net::CookieStore* cookie_store =
      context_getter->GetURLRequestContext()->cookie_store();
  DCHECK(cookie_store);
  out_subscription_holder->subscription =
      cookie_store->AddCallbackForCookie(url, name, callback);
}

// static
void SigninCookieChangedSubscription::RunAsyncOnCookieChanged(
    scoped_refptr<base::TaskRunner> proxy,
    base::WeakPtr<SigninCookieChangedSubscription> subscription,
    const net::CanonicalCookie& cookie,
    bool removed) {
  proxy->PostTask(FROM_HERE,
                  base::Bind(&SigninCookieChangedSubscription::OnCookieChanged,
                             subscription,
                             cookie,
                             removed));
}

void SigninCookieChangedSubscription::OnCookieChanged(
    const net::CanonicalCookie& cookie,
    bool removed) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!callback_.is_null()) {
    callback_.Run(cookie, removed);
  }
}
