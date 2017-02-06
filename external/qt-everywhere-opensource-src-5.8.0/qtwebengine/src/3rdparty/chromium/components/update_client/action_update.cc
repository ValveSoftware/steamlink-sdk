// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/action_update.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/update_client/configurator.h"
#include "components/update_client/utils.h"

using std::string;
using std::vector;

namespace update_client {

namespace {

void AppendDownloadMetrics(
    const std::vector<CrxDownloader::DownloadMetrics>& source,
    std::vector<CrxDownloader::DownloadMetrics>* destination) {
  destination->insert(destination->end(), source.begin(), source.end());
}

Action::ErrorCategory UnpackerErrorToErrorCategory(
    ComponentUnpacker::Error error) {
  Action::ErrorCategory error_category = Action::ErrorCategory::kErrorNone;
  switch (error) {
    case ComponentUnpacker::kNone:
      break;
    case ComponentUnpacker::kInstallerError:
      error_category = Action::ErrorCategory::kInstallError;
      break;
    default:
      error_category = Action::ErrorCategory::kUnpackError;
      break;
  }
  return error_category;
}

}  // namespace

ActionUpdate::ActionUpdate() {
}

ActionUpdate::~ActionUpdate() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ActionUpdate::Run(UpdateContext* update_context, Callback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ActionImpl::Run(update_context, callback);

  DCHECK(!update_context_->queue.empty());

  const std::string& id = update_context_->queue.front();
  CrxUpdateItem* item = FindUpdateItemById(id);
  DCHECK(item);

  const bool is_background_download(IsBackgroundDownload(item));

  crx_downloader_.reset((*update_context_->crx_downloader_factory)(
                            is_background_download,
                            update_context_->config->RequestContext(),
                            update_context_->blocking_task_runner)
                            .release());
  crx_downloader_->set_progress_callback(
      base::Bind(&ActionUpdate::DownloadProgress, base::Unretained(this), id));

  OnDownloadStart(item);

  crx_downloader_->StartDownload(
      GetUrls(item), GetHash(item),
      base::Bind(&ActionUpdate::DownloadComplete, base::Unretained(this), id));
}

void ActionUpdate::DownloadProgress(
    const std::string& id,
    const CrxDownloader::Result& download_result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(id == update_context_->queue.front());

  using Events = UpdateClient::Observer::Events;
  NotifyObservers(Events::COMPONENT_UPDATE_DOWNLOADING, id);
}

void ActionUpdate::DownloadComplete(
    const std::string& id,
    const CrxDownloader::Result& download_result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(id == update_context_->queue.front());

  CrxUpdateItem* item = FindUpdateItemById(id);
  DCHECK(item);

  AppendDownloadMetrics(crx_downloader_->download_metrics(),
                        &item->download_metrics);

  crx_downloader_.reset();

  if (download_result.error) {
    OnDownloadError(item, download_result);
  } else {
    OnDownloadSuccess(item, download_result);
    update_context_->main_task_runner->PostDelayedTask(
        FROM_HERE, base::Bind(&ActionUpdate::Install, base::Unretained(this),
                              id, download_result.response),
        base::TimeDelta::FromMilliseconds(
            update_context_->config->StepDelay()));
  }
}

void ActionUpdate::Install(const std::string& id,
                           const base::FilePath& crx_path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(id == update_context_->queue.front());

  CrxUpdateItem* item = FindUpdateItemById(id);
  DCHECK(item);

  OnInstallStart(item);

  update_context_->blocking_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&ActionUpdate::DoInstallOnBlockingTaskRunner,
                 base::Unretained(this), update_context_, item, crx_path));
}

void ActionUpdate::DoInstallOnBlockingTaskRunner(
    UpdateContext* update_context,
    CrxUpdateItem* item,
    const base::FilePath& crx_path) {
  unpacker_ = new ComponentUnpacker(
      item->component.pk_hash, crx_path, item->component.fingerprint,
      item->component.installer,
      update_context->config->CreateOutOfProcessPatcher(),
      update_context->blocking_task_runner);
  unpacker_->Unpack(base::Bind(&ActionUpdate::EndUnpackingOnBlockingTaskRunner,
                               base::Unretained(this), update_context, item,
                               crx_path));
}

void ActionUpdate::EndUnpackingOnBlockingTaskRunner(
    UpdateContext* update_context,
    CrxUpdateItem* item,
    const base::FilePath& crx_path,
    ComponentUnpacker::Error error,
    int extended_error) {
  unpacker_ = nullptr;
  update_client::DeleteFileAndEmptyParentDirectory(crx_path);
  update_context->main_task_runner->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ActionUpdate::DoneInstalling, base::Unretained(this),
                 item->id, error, extended_error),
      base::TimeDelta::FromMilliseconds(update_context->config->StepDelay()));
}

void ActionUpdate::DoneInstalling(const std::string& id,
                                  ComponentUnpacker::Error error,
                                  int extended_error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(id == update_context_->queue.front());

  CrxUpdateItem* item = FindUpdateItemById(id);
  DCHECK(item);

  if (error == ComponentUnpacker::kNone)
    OnInstallSuccess(item);
  else
    OnInstallError(item, error, extended_error);
}

ActionUpdateDiff::ActionUpdateDiff() {
}

ActionUpdateDiff::~ActionUpdateDiff() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

std::unique_ptr<Action> ActionUpdateDiff::Create() {
  return std::unique_ptr<Action>(new ActionUpdateDiff);
}

void ActionUpdateDiff::TryUpdateFull() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<Action> update_action(ActionUpdateFull::Create());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&Action::Run, base::Unretained(update_action.get()),
                            update_context_, callback_));

  update_context_->current_action.reset(update_action.release());
}

bool ActionUpdateDiff::IsBackgroundDownload(const CrxUpdateItem* item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return false;
}

std::vector<GURL> ActionUpdateDiff::GetUrls(const CrxUpdateItem* item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return item->crx_diffurls;
}

std::string ActionUpdateDiff::GetHash(const CrxUpdateItem* item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return item->hashdiff_sha256;
}

void ActionUpdateDiff::OnDownloadStart(CrxUpdateItem* item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(item->state == CrxUpdateItem::State::kCanUpdate);

  ChangeItemState(item, CrxUpdateItem::State::kDownloadingDiff);
}

void ActionUpdateDiff::OnDownloadSuccess(
    CrxUpdateItem* item,
    const CrxDownloader::Result& download_result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(item->state == CrxUpdateItem::State::kDownloadingDiff);

  ChangeItemState(item, CrxUpdateItem::State::kDownloaded);
}

void ActionUpdateDiff::OnDownloadError(
    CrxUpdateItem* item,
    const CrxDownloader::Result& download_result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(item->state == CrxUpdateItem::State::kDownloadingDiff);

  item->diff_error_category = static_cast<int>(ErrorCategory::kNetworkError);
  item->diff_error_code = download_result.error;
  item->diff_update_failed = true;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ActionUpdateDiff::TryUpdateFull, base::Unretained(this)));
}

void ActionUpdateDiff::OnInstallStart(CrxUpdateItem* item) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ChangeItemState(item, CrxUpdateItem::State::kUpdatingDiff);
}

void ActionUpdateDiff::OnInstallSuccess(CrxUpdateItem* item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(item->state == CrxUpdateItem::State::kUpdatingDiff);

  item->component.version = item->next_version;
  item->component.fingerprint = item->next_fp;
  ChangeItemState(item, CrxUpdateItem::State::kUpdated);

  UpdateCrxComplete(item);
}

void ActionUpdateDiff::OnInstallError(CrxUpdateItem* item,
                                      ComponentUnpacker::Error error,
                                      int extended_error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  item->diff_error_category =
      static_cast<int>(UnpackerErrorToErrorCategory(error));
  item->diff_error_code = error;
  item->diff_extra_code1 = extended_error;
  item->diff_update_failed = true;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ActionUpdateDiff::TryUpdateFull, base::Unretained(this)));
}

ActionUpdateFull::ActionUpdateFull() {
}

ActionUpdateFull::~ActionUpdateFull() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

std::unique_ptr<Action> ActionUpdateFull::Create() {
  return std::unique_ptr<Action>(new ActionUpdateFull);
}

bool ActionUpdateFull::IsBackgroundDownload(const CrxUpdateItem* item) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // On demand component updates are always downloaded in foreground.
  return !item->on_demand && item->component.allows_background_download &&
         update_context_->config->UseBackgroundDownloader();
}

std::vector<GURL> ActionUpdateFull::GetUrls(const CrxUpdateItem* item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return item->crx_urls;
}

std::string ActionUpdateFull::GetHash(const CrxUpdateItem* item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return item->hash_sha256;
}

void ActionUpdateFull::OnDownloadStart(CrxUpdateItem* item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(item->state == CrxUpdateItem::State::kCanUpdate ||
         item->diff_update_failed);

  ChangeItemState(item, CrxUpdateItem::State::kDownloading);
}

void ActionUpdateFull::OnDownloadSuccess(
    CrxUpdateItem* item,
    const CrxDownloader::Result& download_result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(item->state == CrxUpdateItem::State::kDownloading);

  ChangeItemState(item, CrxUpdateItem::State::kDownloaded);
}

void ActionUpdateFull::OnDownloadError(
    CrxUpdateItem* item,
    const CrxDownloader::Result& download_result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(item->state == CrxUpdateItem::State::kDownloading);

  item->error_category = static_cast<int>(ErrorCategory::kNetworkError);
  item->error_code = download_result.error;
  ChangeItemState(item, CrxUpdateItem::State::kNoUpdate);

  UpdateCrxComplete(item);
}

void ActionUpdateFull::OnInstallStart(CrxUpdateItem* item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(item->state == CrxUpdateItem::State::kDownloaded);

  ChangeItemState(item, CrxUpdateItem::State::kUpdating);
}

void ActionUpdateFull::OnInstallSuccess(CrxUpdateItem* item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(item->state == CrxUpdateItem::State::kUpdating);

  item->component.version = item->next_version;
  item->component.fingerprint = item->next_fp;
  ChangeItemState(item, CrxUpdateItem::State::kUpdated);

  UpdateCrxComplete(item);
}

void ActionUpdateFull::OnInstallError(CrxUpdateItem* item,
                                      ComponentUnpacker::Error error,
                                      int extended_error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(item->state == CrxUpdateItem::State::kUpdating);

  item->error_category = static_cast<int>(UnpackerErrorToErrorCategory(error));
  item->error_code = error;
  item->extra_code1 = extended_error;
  ChangeItemState(item, CrxUpdateItem::State::kNoUpdate);

  UpdateCrxComplete(item);
}

}  // namespace update_client
