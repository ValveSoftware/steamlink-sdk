// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_quota_client.h"

#include <vector>

#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_util.h"
#include "webkit/browser/database/database_util.h"

using quota::QuotaClient;
using webkit_database::DatabaseUtil;

namespace content {
namespace {

quota::QuotaStatusCode DeleteOriginDataOnIndexedDBThread(
    IndexedDBContextImpl* context,
    const GURL& origin) {
  context->DeleteForOrigin(origin);
  return quota::kQuotaStatusOk;
}

int64 GetOriginUsageOnIndexedDBThread(IndexedDBContextImpl* context,
                                      const GURL& origin) {
  DCHECK(context->TaskRunner()->RunsTasksOnCurrentThread());
  return context->GetOriginDiskUsage(origin);
}

void GetAllOriginsOnIndexedDBThread(IndexedDBContextImpl* context,
                                    std::set<GURL>* origins_to_return) {
  DCHECK(context->TaskRunner()->RunsTasksOnCurrentThread());
  std::vector<GURL> all_origins = context->GetAllOrigins();
  origins_to_return->insert(all_origins.begin(), all_origins.end());
}

void DidGetOrigins(const IndexedDBQuotaClient::GetOriginsCallback& callback,
                   const std::set<GURL>* origins) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback.Run(*origins);
}

void GetOriginsForHostOnIndexedDBThread(IndexedDBContextImpl* context,
                                        const std::string& host,
                                        std::set<GURL>* origins_to_return) {
  DCHECK(context->TaskRunner()->RunsTasksOnCurrentThread());
  std::vector<GURL> all_origins = context->GetAllOrigins();
  for (std::vector<GURL>::const_iterator iter = all_origins.begin();
       iter != all_origins.end();
       ++iter) {
    if (host == net::GetHostOrSpecFromURL(*iter))
      origins_to_return->insert(*iter);
  }
}

}  // namespace

// IndexedDBQuotaClient --------------------------------------------------------

IndexedDBQuotaClient::IndexedDBQuotaClient(
    IndexedDBContextImpl* indexed_db_context)
    : indexed_db_context_(indexed_db_context) {}

IndexedDBQuotaClient::~IndexedDBQuotaClient() {}

QuotaClient::ID IndexedDBQuotaClient::id() const { return kIndexedDatabase; }

void IndexedDBQuotaClient::OnQuotaManagerDestroyed() { delete this; }

void IndexedDBQuotaClient::GetOriginUsage(const GURL& origin_url,
                                          quota::StorageType type,
                                          const GetUsageCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(indexed_db_context_);

  // IndexedDB is in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback.Run(0);
    return;
  }

  // No task runner means unit test; no cleanup necessary.
  if (!indexed_db_context_->TaskRunner()) {
    callback.Run(0);
    return;
  }

  base::PostTaskAndReplyWithResult(
      indexed_db_context_->TaskRunner(),
      FROM_HERE,
      base::Bind(
          &GetOriginUsageOnIndexedDBThread, indexed_db_context_, origin_url),
      callback);
}

void IndexedDBQuotaClient::GetOriginsForType(
    quota::StorageType type,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(indexed_db_context_);

  // All databases are in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback.Run(std::set<GURL>());
    return;
  }

  // No task runner means unit test; no cleanup necessary.
  if (!indexed_db_context_->TaskRunner()) {
    callback.Run(std::set<GURL>());
    return;
  }

  std::set<GURL>* origins_to_return = new std::set<GURL>();
  indexed_db_context_->TaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetAllOriginsOnIndexedDBThread,
                 indexed_db_context_,
                 base::Unretained(origins_to_return)),
      base::Bind(&DidGetOrigins, callback, base::Owned(origins_to_return)));
}

void IndexedDBQuotaClient::GetOriginsForHost(
    quota::StorageType type,
    const std::string& host,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(indexed_db_context_);

  // All databases are in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback.Run(std::set<GURL>());
    return;
  }

  // No task runner means unit test; no cleanup necessary.
  if (!indexed_db_context_->TaskRunner()) {
    callback.Run(std::set<GURL>());
    return;
  }

  std::set<GURL>* origins_to_return = new std::set<GURL>();
  indexed_db_context_->TaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetOriginsForHostOnIndexedDBThread,
                 indexed_db_context_,
                 host,
                 base::Unretained(origins_to_return)),
      base::Bind(&DidGetOrigins, callback, base::Owned(origins_to_return)));
}

void IndexedDBQuotaClient::DeleteOriginData(const GURL& origin,
                                            quota::StorageType type,
                                            const DeletionCallback& callback) {
  if (type != quota::kStorageTypeTemporary) {
    callback.Run(quota::kQuotaErrorNotSupported);
    return;
  }

  // No task runner means unit test; no cleanup necessary.
  if (!indexed_db_context_->TaskRunner()) {
    callback.Run(quota::kQuotaStatusOk);
    return;
  }

  base::PostTaskAndReplyWithResult(
      indexed_db_context_->TaskRunner(),
      FROM_HERE,
      base::Bind(
          &DeleteOriginDataOnIndexedDBThread, indexed_db_context_, origin),
      callback);
}

bool IndexedDBQuotaClient::DoesSupport(quota::StorageType type) const {
  return type == quota::kStorageTypeTemporary;
}

}  // namespace content
