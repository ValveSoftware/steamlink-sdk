// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/quirks/quirks_manager.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/quirks/pref_names.h"
#include "components/quirks/quirks_client.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace quirks {

namespace {

QuirksManager* manager_ = nullptr;

const char kIccExtension[] = ".icc";

// How often we query Quirks Server.
const int kDaysBetweenServerChecks = 30;

// Check if file exists, VLOG results.
bool CheckAndLogFile(const base::FilePath& path) {
  const bool exists = base::PathExists(path);
  VLOG(1) << (exists ? "File" : "No File") << " found at " << path.value();
  // TODO(glevin): If file exists, do we want to implement a hash to verify that
  // the file hasn't been corrupted or tampered with?
  return exists;
}

base::FilePath CheckForIccFile(base::FilePath built_in_path,
                               base::FilePath download_path,
                               bool quirks_enabled) {
  // First, look for icc file in old read-only location.  If there, we don't use
  // the Quirks server.
  // TODO(glevin): Awaiting final decision on how to handle old read-only files.
  if (CheckAndLogFile(built_in_path))
    return built_in_path;

  // If experimental Quirks flag isn't set, no other icc file is available.
  if (!quirks_enabled) {
    VLOG(1) << "Quirks Client disabled, no built-in icc file available.";
    return base::FilePath();
  }

  // Check if QuirksClient has already downloaded icc file from server.
  if (CheckAndLogFile(download_path))
    return download_path;

  return base::FilePath();
}

}  // namespace

std::string IdToHexString(int64_t product_id) {
  return base::StringPrintf("%08" PRIx64, product_id);
}

std::string IdToFileName(int64_t product_id) {
  return IdToHexString(product_id).append(kIccExtension);
}

////////////////////////////////////////////////////////////////////////////////
// QuirksManager

QuirksManager::QuirksManager(
    std::unique_ptr<Delegate> delegate,
    scoped_refptr<base::SequencedWorkerPool> blocking_pool,
    PrefService* local_state,
    scoped_refptr<net::URLRequestContextGetter> url_context_getter)
    : waiting_for_login_(true),
      delegate_(std::move(delegate)),
      blocking_pool_(blocking_pool),
      local_state_(local_state),
      url_context_getter_(url_context_getter),
      weak_ptr_factory_(this) {}

QuirksManager::~QuirksManager() {
  clients_.clear();
  manager_ = nullptr;
}

// static
void QuirksManager::Initialize(
    std::unique_ptr<Delegate> delegate,
    scoped_refptr<base::SequencedWorkerPool> blocking_pool,
    PrefService* local_state,
    scoped_refptr<net::URLRequestContextGetter> url_context_getter) {
  manager_ = new QuirksManager(std::move(delegate), blocking_pool, local_state,
                               url_context_getter);
}

// static
void QuirksManager::Shutdown() {
  delete manager_;
}

// static
QuirksManager* QuirksManager::Get() {
  DCHECK(manager_);
  return manager_;
}

// static
void QuirksManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kQuirksClientLastServerCheck);
}

// Delay downloads until after login, to ensure that device policy has been set.
void QuirksManager::OnLoginCompleted() {
  if (!waiting_for_login_)
    return;

  waiting_for_login_ = false;
  if (!clients_.empty() && !QuirksEnabled()) {
    VLOG(2) << clients_.size() << " client(s) deleted.";
    clients_.clear();
  }

  for (const std::unique_ptr<QuirksClient>& client : clients_)
    client->StartDownload();
}

void QuirksManager::RequestIccProfilePath(
    int64_t product_id,
    const RequestFinishedCallback& on_request_finished) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string name = IdToFileName(product_id);
  base::PostTaskAndReplyWithResult(
      blocking_pool_.get(), FROM_HERE,
      base::Bind(&CheckForIccFile,
                 delegate_->GetBuiltInDisplayProfileDirectory().Append(name),
                 delegate_->GetDownloadDisplayProfileDirectory().Append(name),
                 QuirksEnabled()),
      base::Bind(&QuirksManager::OnIccFilePathRequestCompleted,
                 weak_ptr_factory_.GetWeakPtr(), product_id,
                 on_request_finished));
}

void QuirksManager::ClientFinished(QuirksClient* client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetLastServerCheck(client->product_id(), base::Time::Now());
  auto it = std::find_if(clients_.begin(), clients_.end(),
                         [client](const std::unique_ptr<QuirksClient>& c) {
                           return c.get() == client;
                         });
  CHECK(it != clients_.end());
  clients_.erase(it);
}

std::unique_ptr<net::URLFetcher> QuirksManager::CreateURLFetcher(
    const GURL& url,
    net::URLFetcherDelegate* delegate) {
  if (!fake_quirks_fetcher_creator_.is_null())
    return fake_quirks_fetcher_creator_.Run(url, delegate);

  return net::URLFetcher::Create(url, net::URLFetcher::GET, delegate);
}

void QuirksManager::OnIccFilePathRequestCompleted(
    int64_t product_id,
    const RequestFinishedCallback& on_request_finished,
    base::FilePath path) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If we found a file or client is disabled, inform requester.
  if (!path.empty() || !QuirksEnabled()) {
    on_request_finished.Run(path, false);
    // TODO(glevin): If Quirks files are ever modified on the server, we'll need
    // to modify this logic to check for updates. See crbug.com/595024.
    return;
  }

  double last_check = 0.0;
  local_state_->GetDictionary(prefs::kQuirksClientLastServerCheck)
      ->GetDouble(IdToHexString(product_id), &last_check);

  // If never checked server before, need to check for new device.
  if (last_check == 0.0) {
    delegate_->GetDaysSinceOobe(base::Bind(
        &QuirksManager::OnDaysSinceOobeReceived, weak_ptr_factory_.GetWeakPtr(),
        product_id, on_request_finished));
    return;
  }

  const base::TimeDelta time_since =
      base::Time::Now() - base::Time::FromDoubleT(last_check);

  // Don't need server check if we've checked within last 30 days.
  if (time_since < base::TimeDelta::FromDays(kDaysBetweenServerChecks)) {
    VLOG(2) << time_since.InDays()
            << " days since last Quirks Server check for display "
            << IdToHexString(product_id);
    on_request_finished.Run(base::FilePath(), false);
    return;
  }

  CreateClient(product_id, on_request_finished);
}

void QuirksManager::OnDaysSinceOobeReceived(
    int64_t product_id,
    const RequestFinishedCallback& on_request_finished,
    int days_since_oobe) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // On newer devices, we want to check server immediately (after OOBE/login).
  if (days_since_oobe <= kDaysBetweenServerChecks) {
    CreateClient(product_id, on_request_finished);
    return;
  }

  // Otherwise, for the first check on an older device, we want to stagger
  // it over 30 days, so artificially set last check accordingly.
  // TODO(glevin): I believe that it makes sense to remove this random delay
  // in the next Chrome release.
  const int rand_days = base::RandInt(0, kDaysBetweenServerChecks);
  const base::Time fake_last_check =
      base::Time::Now() - base::TimeDelta::FromDays(rand_days);
  SetLastServerCheck(product_id, fake_last_check);
  VLOG(2) << "Delaying first Quirks Server check by "
          << kDaysBetweenServerChecks - rand_days << " days.";

  on_request_finished.Run(base::FilePath(), false);
}

void QuirksManager::CreateClient(
    int64_t product_id,
    const RequestFinishedCallback& on_request_finished) {
  DCHECK(thread_checker_.CalledOnValidThread());
  QuirksClient* client =
      new QuirksClient(product_id, on_request_finished, this);
  clients_.insert(base::WrapUnique(client));
  if (!waiting_for_login_)
    client->StartDownload();
  else
    VLOG(2) << "Quirks Client created; waiting for login to begin download.";
}

bool QuirksManager::QuirksEnabled() {
  if (!delegate_->DevicePolicyEnabled()) {
    VLOG(2) << "Quirks Client disabled by device policy.";
    return false;
  }
  return true;
}

void QuirksManager::SetLastServerCheck(int64_t product_id,
                                       const base::Time& last_check) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DictionaryPrefUpdate dict(local_state_, prefs::kQuirksClientLastServerCheck);
  dict->SetDouble(IdToHexString(product_id), last_check.ToDoubleT());
}

}  // namespace quirks
