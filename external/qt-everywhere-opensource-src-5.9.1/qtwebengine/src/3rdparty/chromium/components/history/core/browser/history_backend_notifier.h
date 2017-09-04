// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_NOTIFIER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_NOTIFIER_H_

#include <set>

#include "components/history/core/browser/history_types.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace history {

// The HistoryBackendNotifier forwards notifications from the HistoryBackend's
// client to all the interested observers (in both history and main thread).
class HistoryBackendNotifier {
 public:
  HistoryBackendNotifier() {}
  virtual ~HistoryBackendNotifier() {}

  // Sends notification that the favicons for the given page URLs (e.g.
  // http://www.google.com) and the given icon URL (e.g.
  // http://www.google.com/favicon.ico) have changed. It is valid to call
  // NotifyFaviconsChanged() with non-empty |page_urls| and an empty |icon_url|
  // and vice versa.
  virtual void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                                     const GURL& icon_url) = 0;

  // Sends notification that |transition| to |row| occurred at |visit_time|
  // following |redirects| (empty if there is no redirects).
  virtual void NotifyURLVisited(ui::PageTransition transition,
                                const URLRow& row,
                                const RedirectList& redirects,
                                base::Time visit_time) = 0;

  // Sends notification that |changed_urls| have been changed or added.
  virtual void NotifyURLsModified(const URLRows& changed_urls) = 0;

  // Sends notification that some or the totality of the URLs have been
  // deleted. If |all_history| is true, then all the URLs in the history have
  // been deleted, otherwise |deleted_urls| list the deleted URLs. If the URL
  // deletion is due to expiration, then |expired| is true. |favicon_urls| is
  // the list of favicon URLs that correspond to the deleted URLs (empty if
  // |all_history| is true).
  virtual void NotifyURLsDeleted(bool all_history,
                                 bool expired,
                                 const URLRows& deleted_urls,
                                 const std::set<GURL>& favicon_urls) = 0;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_NOTIFIER_H_
