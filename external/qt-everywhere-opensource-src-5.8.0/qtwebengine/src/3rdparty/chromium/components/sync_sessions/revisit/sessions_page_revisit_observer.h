// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_REVISIT_SESSIONS_PAGE_REVISIT_OBSERVER_H_
#define COMPONENTS_SYNC_SESSIONS_REVISIT_SESSIONS_PAGE_REVISIT_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/sync_sessions/revisit/page_visit_observer.h"

class GURL;

namespace sessions {
struct SessionTab;
}  // namespace sessions

namespace sync_driver {
struct SyncedSession;
}  // namespace sync_driver

namespace sync_sessions {

class CurrentTabMatcher;
class OffsetTabMatcher;

// A simple interface to abstract away who is providing sessions.
class ForeignSessionsProvider {
 public:
  // Fills the already instantiated passed vector with all foreign sessions.
  // Returned boolean representes if there were foreign sessions and the vector
  // should be examimed.
  virtual bool GetAllForeignSessions(
      std::vector<const sync_driver::SyncedSession*>* sessions) = 0;
  virtual ~ForeignSessionsProvider() {}
};

// An implementation of PageVisitObserver that checks the given page's url
// against in memory session information to detect if we've seen this page
// before, constituting a revisit. Then histogram information is emitted about
// this page navigation.
class SessionsPageRevisitObserver
    : public PageVisitObserver,
      public base::SupportsWeakPtr<SessionsPageRevisitObserver> {
 public:
  explicit SessionsPageRevisitObserver(
      std::unique_ptr<ForeignSessionsProvider> provider);
  ~SessionsPageRevisitObserver() override;
  void OnPageVisit(const GURL& url, const TransitionType transition) override;

 private:
  friend class SessionsPageRevisitObserverTest;

  // Although the signature is identical to OnPageVisit(...), this method
  // actually does all of the work. The assumption is that this method is the
  // target of a PostTask call coming from OnPageVisit(...).
  void CheckForRevisit(const GURL& url, const TransitionType transition);

  std::unique_ptr<ForeignSessionsProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(SessionsPageRevisitObserver);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_REVISIT_SESSIONS_PAGE_REVISIT_OBSERVER_H_
