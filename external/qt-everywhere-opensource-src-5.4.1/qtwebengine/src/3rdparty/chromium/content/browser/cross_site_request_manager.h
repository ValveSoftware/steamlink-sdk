// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CROSS_SITE_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_CROSS_SITE_REQUEST_MANAGER_H_

#include <set>
#include <utility>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"

template <typename T> struct DefaultSingletonTraits;

namespace content {

// CrossSiteRequestManager is used to handle bookkeeping for cross-site
// requests and responses between the UI and IO threads.  Such requests involve
// a transition from one RenderViewHost to another within WebContentsImpl, and
// involve coordination with ResourceDispatcherHost.
//
// CrossSiteRequestManager is a singleton that may be used on any thread.
//
class CrossSiteRequestManager {
 public:
  // Returns the singleton instance.
  static CrossSiteRequestManager* GetInstance();

  // Returns whether the RenderViewHost specified by the given IDs currently
  // has a pending cross-site request.  If so, we will have to delay the
  // response until the previous RenderViewHost runs its onunload handler.
  // Called by ResourceDispatcherHost on the IO thread and RenderViewHost on
  // the UI thread.
  bool HasPendingCrossSiteRequest(int renderer_id, int render_view_id);

  // Sets whether the RenderViewHost specified by the given IDs currently has a
  // pending cross-site request.  Called by RenderViewHost on the UI thread.
  void SetHasPendingCrossSiteRequest(int renderer_id,
                                     int render_view_id,
                                     bool has_pending);

 private:
  friend struct DefaultSingletonTraits<CrossSiteRequestManager>;
  typedef std::set<std::pair<int, int> > RenderViewSet;

  CrossSiteRequestManager();
  ~CrossSiteRequestManager();

  // You must acquire this lock before reading or writing any members of this
  // class.  You must not block while holding this lock.
  base::Lock lock_;

  // Set of (render_process_host_id, render_view_id) pairs of all
  // RenderViewHosts that have pending cross-site requests.  Used to pass
  // information about the RenderViewHosts between the UI and IO threads.
  RenderViewSet pending_cross_site_views_;

  DISALLOW_COPY_AND_ASSIGN(CrossSiteRequestManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CROSS_SITE_REQUEST_MANAGER_H_
