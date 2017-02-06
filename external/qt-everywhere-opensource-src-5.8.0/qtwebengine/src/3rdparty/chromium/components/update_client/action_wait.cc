// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/action_wait.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/update_client/update_engine.h"

namespace update_client {

ActionWait::ActionWait(const base::TimeDelta& time_delta)
    : time_delta_(time_delta) {
}

ActionWait::~ActionWait() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ActionWait::Run(UpdateContext* update_context, Callback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(update_context);

  ActionImpl::Run(update_context, callback);

  const bool result = base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&ActionWait::WaitComplete, base::Unretained(this)),
      time_delta_);

  if (!result) {
    // Move all items pending updates to the |kNoUpdate| state then return the
    // control flow to the update engine, as the updates in this context are
    // completed with an error.
    while (!update_context->queue.empty()) {
      const auto item = FindUpdateItemById(update_context->queue.front());
      if (!item) {
        item->error_category = static_cast<int>(ErrorCategory::kServiceError);
        item->error_code = static_cast<int>(ServiceError::ERROR_WAIT);
        ChangeItemState(item, CrxUpdateItem::State::kNoUpdate);
      } else {
        NOTREACHED();
      }
      update_context->queue.pop();
    }
    callback.Run(static_cast<int>(ServiceError::ERROR_WAIT));
  }

  NotifyObservers(UpdateClient::Observer::Events::COMPONENT_WAIT,
                  update_context_->queue.front());
}

void ActionWait::WaitComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateCrx();
}

}  // namespace update_client
