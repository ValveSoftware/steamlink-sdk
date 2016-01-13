// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_DETAILS_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_DETAILS_H_

#include <string>
#include "content/common/content_export.h"
#include "content/public/browser/navigation_type.h"
#include "url/gurl.h"

namespace content {

class NavigationEntry;

// Provides the details for a NOTIFICATION_NAV_ENTRY_COMMITTED notification.
// TODO(brettw) this mostly duplicates ProvisionalLoadDetails, it would be
// nice to unify these somehow.
struct CONTENT_EXPORT LoadCommittedDetails {
  // By default, the entry will be filled according to a new main frame
  // navigation.
  LoadCommittedDetails();

  // The committed entry. This will be the active entry in the controller.
  NavigationEntry* entry;

  // The type of navigation that just occurred. Note that not all types of
  // navigations in the enum are valid here, since some of them don't actually
  // cause a "commit" and won't generate this notification.
  content::NavigationType type;

  // The index of the previously committed navigation entry. This will be -1
  // if there are no previous entries.
  int previous_entry_index;

  // The previous URL that the user was on. This may be empty if none.
  GURL previous_url;

  // True if the committed entry has replaced the exisiting one.
  // A non-user initiated redirect causes such replacement.
  bool did_replace_entry;

  // True if the navigation was in-page. This means that the active entry's
  // URL and the |previous_url| are the same except for reference fragments.
  bool is_in_page;

  // True when the main frame was navigated. False means the navigation was a
  // sub-frame.
  bool is_main_frame;

  // When the committed load is a web page from the renderer, this string
  // specifies the security state if the page is secure.
  // See FrameHostMsg_DidCommitProvisionalLoad_Params.security_info, where it
  // comes from. Use SSLManager::DeserializeSecurityInfo to decode it.
  std::string serialized_security_info;

  // Returns whether the main frame navigated to a different page (e.g., not
  // scrolling to a fragment inside the current page). We often need this logic
  // for showing or hiding something.
  bool is_navigation_to_different_page() const {
    return is_main_frame && !is_in_page;
  }

  // The HTTP status code for this entry..
  int http_status_code;
};

// Provides the details for a NOTIFICATION_NAV_ENTRY_CHANGED notification.
struct EntryChangedDetails {
  // The changed navigation entry after it has been updated.
  const NavigationEntry* changed_entry;

  // Indicates the current index in the back/forward list of the entry.
  int index;
};

// Details sent for NOTIFY_NAV_LIST_PRUNED.
struct PrunedDetails {
  // If true, count items were removed from the front of the list, otherwise
  // count items were removed from the back of the list.
  bool from_front;

  // Number of items removed.
  int count;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_DETAILS_H_
