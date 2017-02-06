// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_engine.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/action_update_check.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/update_checker.h"

namespace update_client {

UpdateContext::UpdateContext(
    const scoped_refptr<Configurator>& config,
    bool is_foreground,
    const std::vector<std::string>& ids,
    const UpdateClient::CrxDataCallback& crx_data_callback,
    const UpdateEngine::NotifyObserversCallback& notify_observers_callback,
    const UpdateEngine::CompletionCallback& callback,
    UpdateChecker::Factory update_checker_factory,
    CrxDownloader::Factory crx_downloader_factory,
    PingManager* ping_manager)
    : config(config),
      is_foreground(is_foreground),
      ids(ids),
      crx_data_callback(crx_data_callback),
      notify_observers_callback(notify_observers_callback),
      callback(callback),
      main_task_runner(base::ThreadTaskRunnerHandle::Get()),
      blocking_task_runner(config->GetSequencedTaskRunner()),
      update_checker_factory(update_checker_factory),
      crx_downloader_factory(crx_downloader_factory),
      ping_manager(ping_manager),
      retry_after_sec_(0) {}

UpdateContext::~UpdateContext() {
  STLDeleteElements(&update_items);
}

UpdateEngine::UpdateEngine(
    const scoped_refptr<Configurator>& config,
    UpdateChecker::Factory update_checker_factory,
    CrxDownloader::Factory crx_downloader_factory,
    PingManager* ping_manager,
    const NotifyObserversCallback& notify_observers_callback)
    : config_(config),
      update_checker_factory_(update_checker_factory),
      crx_downloader_factory_(crx_downloader_factory),
      ping_manager_(ping_manager),
      metadata_(new PersistedData(config->GetPrefService())),
      notify_observers_callback_(notify_observers_callback) {}

UpdateEngine::~UpdateEngine() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool UpdateEngine::GetUpdateState(const std::string& id,
                                  CrxUpdateItem* update_item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (const auto& context : update_contexts_) {
    const auto& update_items = context->update_items;
    const auto it = std::find_if(update_items.begin(), update_items.end(),
                                 [id](const CrxUpdateItem* update_item) {
                                   return id == update_item->id;
                                 });
    if (it != update_items.end()) {
      *update_item = **it;
      return true;
    }
  }
  return false;
}

void UpdateEngine::Update(
    bool is_foreground,
    const std::vector<std::string>& ids,
    const UpdateClient::CrxDataCallback& crx_data_callback,
    const CompletionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (IsThrottled(is_foreground)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, Error::ERROR_UPDATE_RETRY_LATER));
    return;
  }

  std::unique_ptr<UpdateContext> update_context(new UpdateContext(
      config_, is_foreground, ids, crx_data_callback,
      notify_observers_callback_, callback, update_checker_factory_,
      crx_downloader_factory_, ping_manager_));

  CrxUpdateItem update_item;
  std::unique_ptr<ActionUpdateCheck> update_check_action(new ActionUpdateCheck(
      (*update_context->update_checker_factory)(config_, metadata_.get()),
      config_->GetBrowserVersion(), config_->ExtraRequestParams()));

  update_context->current_action.reset(update_check_action.release());
  update_contexts_.insert(update_context.get());

  update_context->current_action->Run(
      update_context.get(),
      base::Bind(&UpdateEngine::UpdateComplete, base::Unretained(this),
                 update_context.get()));

  ignore_result(update_context.release());
}

void UpdateEngine::UpdateComplete(UpdateContext* update_context, int error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(update_contexts_.find(update_context) != update_contexts_.end());

  const int throttle_sec(update_context->retry_after_sec_);
  DCHECK_LE(throttle_sec, 24 * 60 * 60);

  // Only positive values for throttle_sec are effective. 0 means that no
  // throttling occurs and has the effect of resetting the member.
  // Negative values are not trusted and are ignored.
  if (throttle_sec >= 0)
    throttle_updates_until_ =
        throttle_sec
            ? base::Time::Now() + base::TimeDelta::FromSeconds(throttle_sec)
            : base::Time();

  auto callback = update_context->callback;

  update_contexts_.erase(update_context);
  delete update_context;

  callback.Run(error);
}

bool UpdateEngine::IsThrottled(bool is_foreground) const {
  if (is_foreground || throttle_updates_until_.is_null())
    return false;

  const auto now(base::Time::Now());

  // Throttle the calls in the interval (t - 1 day, t) to limit the effect of
  // unset clocks or clock drift.
  return throttle_updates_until_ - base::TimeDelta::FromDays(1) < now &&
         now < throttle_updates_until_;
}

}  // namespace update_client
