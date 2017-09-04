// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/action_update.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/update_client/component_unpacker.h"
#include "components/update_client/configurator.h"
#include "components/update_client/update_client_errors.h"
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

  StartDownload(item);
}

void ActionUpdate::StartDownload(CrxUpdateItem* item) {
  DCHECK(thread_checker_.CalledOnValidThread());

  crx_downloader_.reset((*update_context_->crx_downloader_factory)(
                            IsBackgroundDownload(item),
                            update_context_->config->RequestContext(),
                            update_context_->blocking_task_runner)
                            .release());
  OnDownloadStart(item);

  const std::string id = item->id;
  crx_downloader_->set_progress_callback(
      base::Bind(&ActionUpdate::DownloadProgress, base::Unretained(this), id));
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
        FROM_HERE,
        base::Bind(&ActionUpdate::StartInstall, base::Unretained(this), item,
                   download_result.response),
        base::TimeDelta::FromMilliseconds(
            update_context_->config->StepDelay()));
  }
}

void ActionUpdate::StartInstall(CrxUpdateItem* item,
                                const base::FilePath& crx_path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(item->id == update_context_->queue.front());

  OnInstallStart(item);

  update_context_->blocking_task_runner->PostTask(
      FROM_HERE, base::Bind(&ActionUpdate::StartUnpackOnBlockingTaskRunner,
                            base::Unretained(this), item, crx_path));
}

void ActionUpdate::StartUnpackOnBlockingTaskRunner(
    CrxUpdateItem* item,
    const base::FilePath& crx_path) {
  DCHECK(update_context_->blocking_task_runner->RunsTasksOnCurrentThread());
  unpacker_ = new ComponentUnpacker(
      item->component.pk_hash, crx_path,
      item->component.installer,
      update_context_->config->CreateOutOfProcessPatcher(),
      update_context_->blocking_task_runner);
  unpacker_->Unpack(
      base::Bind(&ActionUpdate::UnpackCompleteOnBlockingTaskRunner,
                 base::Unretained(this), item, crx_path));
}

void ActionUpdate::UnpackCompleteOnBlockingTaskRunner(
    CrxUpdateItem* item,
    const base::FilePath& crx_path,
    const ComponentUnpacker::Result& result) {
  DCHECK(update_context_->blocking_task_runner->RunsTasksOnCurrentThread());
  unpacker_ = nullptr;

  if (result.error == UnpackerError::kNone) {
    update_context_->blocking_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&ActionUpdate::StartInstallOnBlockingTaskRunner,
                   base::Unretained(this), item, crx_path, result.unpack_path));
  } else {
    update_context_->blocking_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&ActionUpdate::InstallCompleteOnBlockingTaskRunner,
                   base::Unretained(this), item, crx_path,
                   ErrorCategory::kUnpackError, static_cast<int>(result.error),
                   result.extended_error));
  }
}

void ActionUpdate::StartInstallOnBlockingTaskRunner(
    CrxUpdateItem* item,
    const base::FilePath& crx_path,
    const base::FilePath& unpack_path) {
  DCHECK(update_context_->blocking_task_runner->RunsTasksOnCurrentThread());
  DCHECK(!unpack_path.empty());

  const auto result = DoInstall(item, crx_path, unpack_path);
  const ErrorCategory error_category =
      result.error ? ErrorCategory::kInstallError : ErrorCategory::kErrorNone;
  update_context_->blocking_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&ActionUpdate::InstallCompleteOnBlockingTaskRunner,
                 base::Unretained(this), item, crx_path, error_category,
                 result.error, result.extended_error));
}

void ActionUpdate::InstallCompleteOnBlockingTaskRunner(
    CrxUpdateItem* item,
    const base::FilePath& crx_path,
    ErrorCategory error_category,
    int error,
    int extended_error) {
  update_client::DeleteFileAndEmptyParentDirectory(crx_path);
  update_context_->main_task_runner->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ActionUpdate::InstallComplete, base::Unretained(this),
                 item->id, error_category, error, extended_error),
      base::TimeDelta::FromMilliseconds(update_context_->config->StepDelay()));
}

CrxInstaller::Result ActionUpdate::DoInstall(
    CrxUpdateItem* item,
    const base::FilePath& crx_path,
    const base::FilePath& unpack_path) {
  const auto& fingerprint = item->next_fp;
  if (static_cast<int>(fingerprint.size()) !=
      base::WriteFile(
          unpack_path.Append(FILE_PATH_LITERAL("manifest.fingerprint")),
          fingerprint.c_str(), base::checked_cast<int>(fingerprint.size()))) {
    return CrxInstaller::Result(InstallError::FINGERPRINT_WRITE_FAILED);
  }

  std::unique_ptr<base::DictionaryValue> manifest = ReadManifest(unpack_path);
  if (!manifest.get())
    return CrxInstaller::Result(InstallError::BAD_MANIFEST);

  return item->component.installer->Install(*manifest, unpack_path);
}

void ActionUpdate::InstallComplete(const std::string& id,
                                   ErrorCategory error_category,
                                   int error,
                                   int extended_error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(id == update_context_->queue.front());

  CrxUpdateItem* item = FindUpdateItemById(id);
  DCHECK(item);

  if (error == 0) {
    DCHECK_EQ(ErrorCategory::kErrorNone, error_category);
    DCHECK_EQ(0, extended_error);
    OnInstallSuccess(item);
  } else {
    OnInstallError(item, error_category, error, extended_error);
  }
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
                                      ErrorCategory error_category,
                                      int error,
                                      int extended_error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  item->diff_error_category = static_cast<int>(error_category);
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
         update_context_->config->EnabledBackgroundDownloader();
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
                                      ErrorCategory error_category,
                                      int error,
                                      int extended_error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(item->state == CrxUpdateItem::State::kUpdating);

  item->error_category = static_cast<int>(error_category);
  item->error_code = error;
  item->extra_code1 = extended_error;
  ChangeItemState(item, CrxUpdateItem::State::kNoUpdate);

  UpdateCrxComplete(item);
}

}  // namespace update_client
