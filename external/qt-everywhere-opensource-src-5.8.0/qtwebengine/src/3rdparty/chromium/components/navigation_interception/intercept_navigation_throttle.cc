// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/intercept_navigation_throttle.h"

#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

using content::BrowserThread;

namespace navigation_interception {

namespace {

using ChecksPerformedCallback = base::Callback<void(bool, bool)>;

// This is used to run |should_ignore_callback| if it can destroy the
// WebContents (and the InterceptNavigationThrottle along). In that case,
// |on_checks_performed_callback| will be a no-op.
void RunCallback(
    content::WebContents* web_contents,
    const NavigationParams& navigation_params,
    InterceptNavigationThrottle::CheckCallback should_ignore_callback,
    ChecksPerformedCallback on_checks_performed_callback,
    base::WeakPtr<InterceptNavigationThrottle> throttle) {
  bool should_ignore_navigation =
      should_ignore_callback.Run(web_contents, navigation_params);

  // If the InterceptNavigationThrottle that called RunCallback is still alive
  // after |should_ignore_callback| has run, this will run
  // InterceptNavigationThrottle::OnAsynchronousChecksPerformed.
  // TODO(clamy): remove this boolean after crbug.com/570200 is fixed.
  bool throttle_was_destroyed = !throttle.get();
  on_checks_performed_callback.Run(should_ignore_navigation,
                                   throttle_was_destroyed);
}

}  // namespace

InterceptNavigationThrottle::InterceptNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    CheckCallback should_ignore_callback,
    bool run_callback_synchronously)
    : content::NavigationThrottle(navigation_handle),
      should_ignore_callback_(should_ignore_callback),
      run_callback_synchronously_(run_callback_synchronously),
      weak_factory_(this) {}

InterceptNavigationThrottle::~InterceptNavigationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return CheckIfShouldIgnoreNavigation(false);
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::WillRedirectRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return CheckIfShouldIgnoreNavigation(true);
}

content::NavigationThrottle::ThrottleCheckResult
InterceptNavigationThrottle::CheckIfShouldIgnoreNavigation(bool is_redirect) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NavigationParams navigation_params(
      navigation_handle()->GetURL(), navigation_handle()->GetReferrer(),
      navigation_handle()->HasUserGesture(), navigation_handle()->IsPost(),
      navigation_handle()->GetPageTransition(), is_redirect,
      navigation_handle()->IsExternalProtocol(), true);

  if (run_callback_synchronously_) {
    bool should_ignore_navigation = should_ignore_callback_.Run(
        navigation_handle()->GetWebContents(), navigation_params);
    return should_ignore_navigation
               ? content::NavigationThrottle::CANCEL_AND_IGNORE
               : content::NavigationThrottle::PROCEED;
  }

  // When the callback can potentially destroy the WebContents, along with the
  // NavigationHandle and this InterceptNavigationThrottle, it should be run
  // asynchronously. This will ensure that no objects on the stack can be
  // deleted, and that the stack does not unwind through them in a deleted
  // state.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&InterceptNavigationThrottle::RunCallbackAsynchronously,
                 weak_factory_.GetWeakPtr(), navigation_params));
  return DEFER;
}

void InterceptNavigationThrottle::RunCallbackAsynchronously(
    const NavigationParams& navigation_params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Run the callback in a helper function as it may lead ot the destruction of
  // this InterceptNavigationThrottle.
  RunCallback(
      navigation_handle()->GetWebContents(), navigation_params,
      should_ignore_callback_,
      base::Bind(&InterceptNavigationThrottle::OnAsynchronousChecksPerformed,
                 weak_factory_.GetWeakPtr()),
      weak_factory_.GetWeakPtr());

  // DO NOT ADD CODE AFTER HERE: at this point the InterceptNavigationThrottle
  // may have been destroyed by the |should_ignore_callback_|. Adding code here
  // will cause use-after-free bugs.
  //
  // Code that needs to act on the result of the |should_ignore_callback_|
  // should be put inside OnAsynchronousChecksPerformed. This function will be
  // called after |should_ignore_callback_| has run, if this
  // InterceptNavigationThrottle is still alive.
}

void InterceptNavigationThrottle::OnAsynchronousChecksPerformed(
    bool should_ignore_navigation,
    bool throttle_was_destroyed) {
  CHECK(!throttle_was_destroyed);
  content::NavigationHandle* handle = navigation_handle();
  CHECK(handle);
  if (should_ignore_navigation) {
    navigation_handle()->CancelDeferredNavigation(
        content::NavigationThrottle::CANCEL_AND_IGNORE);
  } else {
    handle->Resume();
  }
}

}  // namespace navigation_interception
