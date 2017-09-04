// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_REVISIT_TYPED_URL_PAGE_REVISIT_TASK_H_
#define COMPONENTS_SYNC_SESSIONS_REVISIT_TYPED_URL_PAGE_REVISIT_TASK_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_types.h"
#include "components/sync_sessions/revisit/page_visit_observer.h"
#include "url/gurl.h"

namespace base {
class Time;
}  // namespace base

namespace history {
class HistoryBackend;
class HistoryDatabase;
}  // namespace history

namespace sync_sessions {

// This is the actual logic to check if the history database has a foreign,
// synced, typed URL record for a given page/URL or not. This class implements
// the HistoryDBTask interface with the assumption that is is being run by the
// history mechanisms and on the correct thread.
class TypedUrlPageRevisitTask : public history::HistoryDBTask {
 public:
  TypedUrlPageRevisitTask(const GURL& url,
                          const PageVisitObserver::TransitionType transition);
  ~TypedUrlPageRevisitTask() override;
  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override;
  void DoneRunOnMainThread() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TypedUrlPageRevisitTaskTest, MultipleMatches);

  // Returns if there was a previously synced match. lastVisitTime is an out
  // parameter. Its value is not read, but will be set if the return value is
  // true.
  bool FindLastSyncedMatchAge(history::HistoryBackend* backend,
                              base::Time* lastVisitTime);

  // Returns if there are visits and sources the instance url value, and
  // populates the parameters repspectively. Virtual so that unit tests can
  // override this functionality.
  virtual bool FillVisitsAndSources(history::HistoryBackend* backend,
                                    history::VisitVector* visits,
                                    history::VisitSourceMap* sources);

  const GURL url_;
  const PageVisitObserver::TransitionType transition_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlPageRevisitTask);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_REVISIT_TYPED_URL_PAGE_REVISIT_TASK_H_
