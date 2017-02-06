// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/action.h"

#include <algorithm>
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/update_client/action_update.h"
#include "components/update_client/action_wait.h"
#include "components/update_client/configurator.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/utils.h"

namespace update_client {

using Events = UpdateClient::Observer::Events;

namespace {

// Returns true if a differential update is available, it has not failed yet,
// and the configuration allows this update.
bool CanTryDiffUpdate(const CrxUpdateItem* update_item,
                      const scoped_refptr<Configurator>& config) {
  return HasDiffUpdate(update_item) && !update_item->diff_update_failed &&
         config->DeltasEnabled();
}

}  // namespace

ActionImpl::ActionImpl() : update_context_(nullptr) {
}

ActionImpl::~ActionImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ActionImpl::Run(UpdateContext* update_context, Action::Callback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  update_context_ = update_context;
  callback_ = callback;
}

CrxUpdateItem* ActionImpl::FindUpdateItemById(const std::string& id) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto it(std::find_if(
      update_context_->update_items.begin(),
      update_context_->update_items.end(),
      [&id](const CrxUpdateItem* item) { return item->id == id; }));

  return it != update_context_->update_items.end() ? *it : nullptr;
}

void ActionImpl::ChangeItemState(CrxUpdateItem* item, CrxUpdateItem::State to) {
  DCHECK(thread_checker_.CalledOnValidThread());

  item->state = to;

  using Events = UpdateClient::Observer::Events;

  const std::string& id(item->id);
  switch (to) {
    case CrxUpdateItem::State::kChecking:
      NotifyObservers(Events::COMPONENT_CHECKING_FOR_UPDATES, id);
      break;
    case CrxUpdateItem::State::kCanUpdate:
      NotifyObservers(Events::COMPONENT_UPDATE_FOUND, id);
      break;
    case CrxUpdateItem::State::kUpdatingDiff:
    case CrxUpdateItem::State::kUpdating:
      NotifyObservers(Events::COMPONENT_UPDATE_READY, id);
      break;
    case CrxUpdateItem::State::kUpdated:
      NotifyObservers(Events::COMPONENT_UPDATED, id);
      break;
    case CrxUpdateItem::State::kUpToDate:
    case CrxUpdateItem::State::kNoUpdate:
      NotifyObservers(Events::COMPONENT_NOT_UPDATED, id);
      break;
    case CrxUpdateItem::State::kNew:
    case CrxUpdateItem::State::kDownloading:
    case CrxUpdateItem::State::kDownloadingDiff:
    case CrxUpdateItem::State::kDownloaded:
    case CrxUpdateItem::State::kUninstalled:
    case CrxUpdateItem::State::kLastStatus:
      // No notification for these states.
      break;
  }
}

size_t ActionImpl::ChangeAllItemsState(CrxUpdateItem::State from,
                                       CrxUpdateItem::State to) {
  DCHECK(thread_checker_.CalledOnValidThread());
  size_t count = 0;
  for (auto item : update_context_->update_items) {
    if (item->state == from) {
      ChangeItemState(item, to);
      ++count;
    }
  }
  return count;
}

void ActionImpl::NotifyObservers(UpdateClient::Observer::Events event,
                                 const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  update_context_->notify_observers_callback.Run(event, id);
}

void ActionImpl::UpdateCrx() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!update_context_->queue.empty());

  const std::string& id = update_context_->queue.front();
  CrxUpdateItem* item = FindUpdateItemById(id);
  DCHECK(item);

  std::unique_ptr<Action> update_action(
      CanTryDiffUpdate(item, update_context_->config)
          ? ActionUpdateDiff::Create()
          : ActionUpdateFull::Create());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&Action::Run, base::Unretained(update_action.get()),
                            update_context_, callback_));

  update_context_->current_action.reset(update_action.release());
}

void ActionImpl::UpdateCrxComplete(CrxUpdateItem* item) {
  update_context_->ping_manager->SendPing(item);

  update_context_->queue.pop();

  if (update_context_->queue.empty()) {
    UpdateComplete(0);
  } else {
    // TODO(sorin): the value of timing interval between CRX updates might have
    // to be injected at the call site of update_client::UpdateClient::Update.
    const int wait_sec = update_context_->config->UpdateDelay();

    std::unique_ptr<ActionWait> action_wait(
        new ActionWait(base::TimeDelta::FromSeconds(wait_sec)));

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&Action::Run, base::Unretained(action_wait.get()),
                              update_context_, callback_));

    update_context_->current_action.reset(action_wait.release());
  }
}

void ActionImpl::UpdateComplete(int error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback_, error));
}

}  // namespace update_client
