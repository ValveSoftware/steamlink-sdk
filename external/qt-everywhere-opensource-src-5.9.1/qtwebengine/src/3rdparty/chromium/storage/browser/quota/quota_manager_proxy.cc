// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_manager_proxy.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/trace_event/trace_event.h"

namespace storage {

namespace {

void DidGetUsageAndQuota(
    base::SequencedTaskRunner* original_task_runner,
    const QuotaManagerProxy::GetUsageAndQuotaCallback& callback,
    QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  if (!original_task_runner->RunsTasksOnCurrentThread()) {
    original_task_runner->PostTask(
        FROM_HERE, base::Bind(&DidGetUsageAndQuota,
                              base::RetainedRef(original_task_runner), callback,
                              status, usage, quota));
    return;
  }

  // crbug.com/349708
  TRACE_EVENT0("io", "QuotaManagerProxy DidGetUsageAndQuota");
  callback.Run(status, usage, quota);
}

}  // namespace

void QuotaManagerProxy::RegisterClient(QuotaClient* client) {
  if (!io_thread_->BelongsToCurrentThread() &&
      io_thread_->PostTask(
          FROM_HERE,
          base::Bind(&QuotaManagerProxy::RegisterClient, this, client))) {
    return;
  }

  if (manager_)
    manager_->RegisterClient(client);
  else
    client->OnQuotaManagerDestroyed();
}

void QuotaManagerProxy::NotifyStorageAccessed(
    QuotaClient::ID client_id,
    const GURL& origin,
    StorageType type) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::Bind(&QuotaManagerProxy::NotifyStorageAccessed, this, client_id,
                   origin, type));
    return;
  }

  if (manager_)
    manager_->NotifyStorageAccessed(client_id, origin, type);
}

void QuotaManagerProxy::NotifyStorageModified(QuotaClient::ID client_id,
                                              const GURL& origin,
                                              StorageType type,
                                              int64_t delta) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::Bind(&QuotaManagerProxy::NotifyStorageModified, this, client_id,
                   origin, type, delta));
    return;
  }

  if (manager_)
    manager_->NotifyStorageModified(client_id, origin, type, delta);
}

void QuotaManagerProxy::NotifyOriginInUse(
    const GURL& origin) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::Bind(&QuotaManagerProxy::NotifyOriginInUse, this, origin));
    return;
  }

  if (manager_)
    manager_->NotifyOriginInUse(origin);
}

void QuotaManagerProxy::NotifyOriginNoLongerInUse(
    const GURL& origin) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::Bind(&QuotaManagerProxy::NotifyOriginNoLongerInUse, this,
                   origin));
    return;
  }
  if (manager_)
    manager_->NotifyOriginNoLongerInUse(origin);
}

void QuotaManagerProxy::SetUsageCacheEnabled(QuotaClient::ID client_id,
                                             const GURL& origin,
                                             StorageType type,
                                             bool enabled) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::Bind(&QuotaManagerProxy::SetUsageCacheEnabled, this,
                   client_id, origin, type, enabled));
    return;
  }
  if (manager_)
    manager_->SetUsageCacheEnabled(client_id, origin, type, enabled);
}

void QuotaManagerProxy::GetUsageAndQuota(
    base::SequencedTaskRunner* original_task_runner,
    const GURL& origin,
    StorageType type,
    const GetUsageAndQuotaCallback& callback) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE, base::Bind(&QuotaManagerProxy::GetUsageAndQuota, this,
                              base::RetainedRef(original_task_runner), origin,
                              type, callback));
    return;
  }
  if (!manager_) {
    DidGetUsageAndQuota(original_task_runner, callback, kQuotaErrorAbort, 0, 0);
    return;
  }

  // crbug.com/349708
  TRACE_EVENT0("io", "QuotaManagerProxy::GetUsageAndQuota");

  manager_->GetUsageAndQuota(
      origin, type,
      base::Bind(&DidGetUsageAndQuota, base::RetainedRef(original_task_runner),
                 callback));
}

QuotaManager* QuotaManagerProxy::quota_manager() const {
  DCHECK(!io_thread_.get() || io_thread_->BelongsToCurrentThread());
  return manager_;
}

QuotaManagerProxy::QuotaManagerProxy(
    QuotaManager* manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread)
    : manager_(manager), io_thread_(io_thread) {
}

QuotaManagerProxy::~QuotaManagerProxy() {
}

}  // namespace storage
