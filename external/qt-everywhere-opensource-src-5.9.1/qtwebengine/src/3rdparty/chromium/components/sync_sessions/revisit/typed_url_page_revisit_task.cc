// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/revisit/typed_url_page_revisit_task.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_row.h"

namespace sync_sessions {

TypedUrlPageRevisitTask::TypedUrlPageRevisitTask(
    const GURL& url,
    const PageVisitObserver::TransitionType transition)
    : url_(url), transition_(transition) {}

TypedUrlPageRevisitTask::~TypedUrlPageRevisitTask() {}

bool TypedUrlPageRevisitTask::FillVisitsAndSources(
    history::HistoryBackend* backend,
    history::VisitVector* visits,
    history::VisitSourceMap* sources) {
  history::URLRow row;
  return backend->GetURL(url_, &row) &&
         backend->GetVisitsForURL(row.id(), visits) &&
         backend->GetVisitsSource(*visits, sources);
}

bool TypedUrlPageRevisitTask::FindLastSyncedMatchAge(
    history::HistoryBackend* backend,
    base::Time* lastVisitTime) {
  history::VisitVector visits;
  history::VisitSourceMap sources;
  if (FillVisitsAndSources(backend, &visits, &sources)) {
    // The visits are in chronological order. We only care about the most
    // recent remote visit, so iterate backwards.
    for (auto row_itr = visits.rbegin(); row_itr != visits.rend(); ++row_itr) {
      auto map_itr = sources.find(row_itr->visit_id);
      if (map_itr != sources.end() &&
          map_itr->second == history::SOURCE_SYNCED) {
        *lastVisitTime = row_itr->visit_time;
        return true;
      }
    }
  }
  return false;
}

bool TypedUrlPageRevisitTask::RunOnDBThread(history::HistoryBackend* backend,
                                            history::HistoryDatabase* db) {
  base::TimeTicks start(base::TimeTicks::Now());
  base::Time lastVisitTime;
  if (FindLastSyncedMatchAge(backend, &lastVisitTime)) {
    REVISIT_HISTOGRAM_AGE("Sync.PageRevisitTypedUrlMatchAge", lastVisitTime);
    UMA_HISTOGRAM_ENUMERATION("Sync.PageRevisitTypedUrlMatchTransition",
                              transition_,
                              PageVisitObserver::kTransitionTypeLast);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Sync.PageRevisitTypedUrlMissTransition",
                              transition_,
                              PageVisitObserver::kTransitionTypeLast);
  }

  base::TimeDelta duration(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_TIMES("Sync.PageRevisitTypedUrlDuration", duration);

  // This indicates success and retring is not needed.
  return true;
}

void TypedUrlPageRevisitTask::DoneRunOnMainThread() {}

}  // namespace sync_sessions
