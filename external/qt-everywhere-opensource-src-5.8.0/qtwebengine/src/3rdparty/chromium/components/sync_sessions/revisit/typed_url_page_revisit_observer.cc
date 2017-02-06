// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/revisit/typed_url_page_revisit_observer.h"

#include "base/memory/ptr_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync_sessions/revisit/typed_url_page_revisit_task.h"
#include "url/gurl.h"

namespace sync_sessions {

TypedUrlPageRevisitObserver::TypedUrlPageRevisitObserver(
    history::HistoryService* history)
    : history_(base::AsWeakPtr(history)) {}

TypedUrlPageRevisitObserver::~TypedUrlPageRevisitObserver() {}

void TypedUrlPageRevisitObserver::OnPageVisit(
    const GURL& url,
    const PageVisitObserver::TransitionType transition) {
  if (history_) {
    history_->ScheduleDBTask(
        base::WrapUnique(new TypedUrlPageRevisitTask(url, transition)),
        &task_tracker_);
  }
}

}  // namespace sync_sessions
