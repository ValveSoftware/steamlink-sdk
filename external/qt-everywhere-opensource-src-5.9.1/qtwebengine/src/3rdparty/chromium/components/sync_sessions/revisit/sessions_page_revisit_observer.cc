// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/revisit/sessions_page_revisit_observer.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/revisit/current_tab_matcher.h"
#include "components/sync_sessions/revisit/offset_tab_matcher.h"
#include "components/sync_sessions/revisit/page_equality.h"
#include "components/sync_sessions/synced_session.h"
#include "url/gurl.h"

namespace sync_sessions {

SessionsPageRevisitObserver::SessionsPageRevisitObserver(
    std::unique_ptr<ForeignSessionsProvider> provider)
    : provider_(std::move(provider)) {}

SessionsPageRevisitObserver::~SessionsPageRevisitObserver() {}

void SessionsPageRevisitObserver::OnPageVisit(
    const GURL& url,
    const PageVisitObserver::TransitionType transition) {
  // We need to be invoked and eventually execute on the thread which owns the
  // session objects the provider will give us. However, this work is not
  // especially time sensitive, and so we post a task to perform this to get out
  // of the way of any currently executing logic.

  // Bind this task to this->AsWeakPtr() so that if we're destructed this task
  // just vanish instead of breaking things.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&SessionsPageRevisitObserver::CheckForRevisit,
                            this->AsWeakPtr(), url, transition));
}

void SessionsPageRevisitObserver::CheckForRevisit(
    const GURL& url,
    const PageVisitObserver::TransitionType transition) {
  base::TimeTicks start(base::TimeTicks::Now());

  // We want to match tabs/navigation entries in two slightly different ways. We
  // value the current url/navigation entry of a tab more highly, and want to
  // actually seperate metrics from the backwards/forwards entries. And then
  // to make things a little bit messier, we only have an accurate modified time
  // for the tabs/current entries. So use index offset for forward/back entries.
  PageEquality page_equality(url);
  CurrentTabMatcher current_matcher(page_equality);
  OffsetTabMatcher offset_matcher(page_equality);

  std::vector<const SyncedSession*> foreign_sessions;
  if (provider_->GetAllForeignSessions(&foreign_sessions)) {
    for (const SyncedSession* session : foreign_sessions) {
      for (const auto& key_value : session->windows) {
        for (const auto& tab : key_value.second->tabs) {
          // These matchers look identical and could easily implement an
          // interface and we could iterate through a vector of matchers here.
          // However this would cause quite a bit of overhead at the inner most
          // loop of something that takes linear time in relation to the number
          // of open foreign tabs. A small fraction of users have thousands of
          // open tabs.
          current_matcher.Check(tab.get());
          offset_matcher.Check(tab.get());
        }
      }
    }
  }

  // emit even if there are no foreign sessions so that that counts all match.
  current_matcher.Emit(transition);
  offset_matcher.Emit(transition);

  base::TimeDelta duration(base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_TIMES("Sync.PageRevisitSessionDuration", duration);
}

}  // namespace sync_sessions
