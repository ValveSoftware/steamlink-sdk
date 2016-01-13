// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_TYPE_H_

namespace content {

// Indicates different types of navigations that can occur that we will handle
// separately.
enum NavigationType {
  // Unknown type.
  NAVIGATION_TYPE_UNKNOWN,

  // A new page was navigated in the main frame.
  NAVIGATION_TYPE_NEW_PAGE,

  // Renavigating to an existing navigation entry. The entry is guaranteed to
  // exist in the list, or else it would be a new page or IGNORE navigation.
  NAVIGATION_TYPE_EXISTING_PAGE,

  // The same page has been reloaded as a result of the user requesting
  // navigation to that same page (like pressing Enter in the URL bar). This
  // is not the same as an in-page navigation because we'll actually have a
  // pending entry for the load, which is then meaningless.
  NAVIGATION_TYPE_SAME_PAGE,

  // In page navigations are when the reference fragment changes. This will
  // be in the main frame only (we won't even get notified of in-page
  // subframe navigations). It may be for any page, not necessarily the last
  // committed one (for example, whey going back to a page with a ref).
  NAVIGATION_TYPE_IN_PAGE,

  // A new subframe was manually navigated by the user. We will create a new
  // NavigationEntry so they can go back to the previous subframe content
  // using the back button.
  NAVIGATION_TYPE_NEW_SUBFRAME,

  // A subframe in the page was automatically loaded or navigated to such that
  // a new navigation entry should not be created. There are two cases:
  //  1. Stuff like iframes containing ads that the page loads automatically.
  //     The user doesn't want to see these, so we just update the existing
  //     navigation entry.
  //  2. Going back/forward to previous subframe navigations. We don't create
  //     a new entry here either, just update the last committed entry.
  // These two cases are actually pretty different, they just happen to
  // require almost the same code to handle.
  NAVIGATION_TYPE_AUTO_SUBFRAME,

  // Nothing happened. This happens when we get information about a page we
  // don't know anything about. It can also happen when an iframe in a popup
  // navigated to about:blank is navigated. Nothing needs to be done.
  NAVIGATION_TYPE_NAV_IGNORE,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_TYPE_H_
