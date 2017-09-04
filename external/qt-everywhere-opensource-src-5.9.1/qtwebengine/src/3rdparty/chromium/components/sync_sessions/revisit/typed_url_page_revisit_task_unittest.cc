// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/revisit/typed_url_page_revisit_task.h"

#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {

namespace {

class FakeTask : public TypedUrlPageRevisitTask {
 public:
  FakeTask(const PageVisitObserver::TransitionType transition,
           bool result,
           const history::VisitVector& visits,
           const history::VisitSourceMap& sources)
      : TypedUrlPageRevisitTask(GURL("http://www.example.com"), transition),
        result_(result),
        visits_(visits),
        sources_(sources) {}

  bool FillVisitsAndSources(history::HistoryBackend* backend,
                            history::VisitVector* visits,
                            history::VisitSourceMap* sources) override {
    *visits = visits_;
    *sources = sources_;
    return result_;
  }

 private:
  const bool result_;
  const history::VisitVector visits_;
  const history::VisitSourceMap sources_;
};

void VerifyMatch(TypedUrlPageRevisitTask* task,
                 const PageVisitObserver::TransitionType transition) {
  base::HistogramTester histogram_tester;
  task->RunOnDBThread(nullptr, nullptr);
  histogram_tester.ExpectTotalCount("Sync.PageRevisitTypedUrlMatchAge", 1);
  histogram_tester.ExpectUniqueSample("Sync.PageRevisitTypedUrlMatchTransition",
                                      PageVisitObserver::kTransitionPage, 1);
  histogram_tester.ExpectTotalCount("Sync.PageRevisitTypedUrlDuration", 1);
}

void VerifyMiss(TypedUrlPageRevisitTask* task,
                const PageVisitObserver::TransitionType transition) {
  base::HistogramTester histogram_tester;
  task->RunOnDBThread(nullptr, nullptr);
  histogram_tester.ExpectUniqueSample("Sync.PageRevisitTypedUrlMissTransition",
                                      PageVisitObserver::kTransitionPage, 1);
  histogram_tester.ExpectTotalCount("Sync.PageRevisitTypedUrlDuration", 1);
}

history::VisitRow Row(history::VisitID id) {
  history::VisitRow row;
  row.visit_id = id;
  return row;
}

history::VisitRow Row(history::VisitID id, base::Time visit_time) {
  history::VisitRow row;
  row.visit_id = id;
  row.visit_time = visit_time;
  return row;
}

}  // namespace

TEST(TypedUrlPageRevisitTaskTest, NoMatchesFillReturnsFalse) {
  history::VisitVector visits;
  history::VisitSourceMap sources;
  FakeTask task(PageVisitObserver::TransitionType::kTransitionPage, false,
                visits, sources);
  VerifyMiss(&task, PageVisitObserver::kTransitionPage);
}

TEST(TypedUrlPageRevisitTaskTest, NoMatchesFillReturnsTrue) {
  history::VisitVector visits;
  history::VisitSourceMap sources;
  FakeTask task(PageVisitObserver::TransitionType::kTransitionPage, true,
                visits, sources);
  VerifyMiss(&task, PageVisitObserver::kTransitionPage);
}

TEST(TypedUrlPageRevisitTaskTest, NoSyncedSources) {
  history::VisitVector visits;
  visits.push_back(Row(1));
  visits.push_back(Row(2));
  visits.push_back(Row(3));
  visits.push_back(Row(4));
  visits.push_back(Row(5));
  visits.push_back(Row(6));
  visits.push_back(Row(7));

  history::VisitSourceMap sources;
  sources[1] = history::SOURCE_BROWSED;
  sources[2] = history::SOURCE_EXTENSION;
  sources[3] = history::SOURCE_EXTENSION;
  sources[4] = history::SOURCE_FIREFOX_IMPORTED;
  sources[5] = history::SOURCE_IE_IMPORTED;
  sources[6] = history::SOURCE_SAFARI_IMPORTED;
  // No source for id 7.

  FakeTask task(PageVisitObserver::TransitionType::kTransitionPage, true,
                visits, sources);
  VerifyMiss(&task, PageVisitObserver::kTransitionPage);
}

TEST(TypedUrlPageRevisitTaskTest, SingleMatch) {
  history::VisitVector visits;
  visits.push_back(Row(1));

  history::VisitSourceMap sources;
  sources[1] = history::SOURCE_SYNCED;

  FakeTask task(PageVisitObserver::TransitionType::kTransitionPage, true,
                visits, sources);
  VerifyMatch(&task, PageVisitObserver::kTransitionPage);
}

TEST(TypedUrlPageRevisitTaskTest, MultipleMatches) {
  base::Time expected = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  history::VisitVector visits;
  visits.push_back(Row(1, base::Time::UnixEpoch()));
  visits.push_back(Row(2, expected));
  visits.push_back(
      Row(3, base::Time::UnixEpoch() + base::TimeDelta::FromDays(2)));

  history::VisitSourceMap sources;
  sources[1] = history::SOURCE_SYNCED;
  sources[2] = history::SOURCE_SYNCED;
  // No source for id 3.

  FakeTask task(PageVisitObserver::TransitionType::kTransitionPage, true,
                visits, sources);
  base::Time lastVisitTime;
  ASSERT_TRUE(task.FindLastSyncedMatchAge(nullptr, &lastVisitTime));
  ASSERT_EQ(expected, lastVisitTime);
}

}  // namespace sync_sessions
