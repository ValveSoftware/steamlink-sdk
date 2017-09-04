// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_REVISIT_TYPED_URL_PAGE_REVISIT_OBSERVER_H_
#define COMPONENTS_SYNC_SESSIONS_REVISIT_TYPED_URL_PAGE_REVISIT_OBSERVER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/sync_sessions/revisit/page_visit_observer.h"

class GURL;

namespace history {
class HistoryService;
}  // namespace history

namespace sync_sessions {

// This class's job is to respond to OnPageVisit events and launch a task to
// the history thread to perform the requisite checks and instrumentation. It is
// important that this object is created and called on the same thread, since it
// creates and uses a weak pointer to the HistoryService.
class TypedUrlPageRevisitObserver : public PageVisitObserver {
 public:
  explicit TypedUrlPageRevisitObserver(history::HistoryService* history);
  ~TypedUrlPageRevisitObserver() override;
  void OnPageVisit(const GURL& url,
                   const PageVisitObserver::TransitionType transition) override;

 private:
  const base::WeakPtr<history::HistoryService> history_;
  // This is never used to cancel tasks, but required by the history interface.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlPageRevisitObserver);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_REVISIT_TYPED_URL_PAGE_REVISIT_OBSERVER_H_
