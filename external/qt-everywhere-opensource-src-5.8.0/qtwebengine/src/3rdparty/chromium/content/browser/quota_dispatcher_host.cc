// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/quota_dispatcher_host.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/common/quota_messages.h"
#include "content/public/browser/quota_permission_context.h"
#include "net/base/url_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "url/gurl.h"

using storage::QuotaClient;
using storage::QuotaManager;
using storage::QuotaStatusCode;
using storage::StorageType;

namespace content {

// Created one per request to carry the request's request_id around.
// Dispatches requests from renderer/worker to the QuotaManager and
// sends back the response to the renderer/worker.
class QuotaDispatcherHost::RequestDispatcher {
 public:
  RequestDispatcher(base::WeakPtr<QuotaDispatcherHost> dispatcher_host,
                    int request_id)
      : dispatcher_host_(dispatcher_host),
        render_process_id_(dispatcher_host->process_id_),
        request_id_(request_id) {
    dispatcher_host_->outstanding_requests_.AddWithID(this, request_id_);
  }
  virtual ~RequestDispatcher() {}

 protected:
  // Subclass must call this when it's done with the request.
  void Completed() {
    if (dispatcher_host_)
      dispatcher_host_->outstanding_requests_.Remove(request_id_);
  }

  QuotaDispatcherHost* dispatcher_host() const {
    return dispatcher_host_.get();
  }
  storage::QuotaManager* quota_manager() const {
    return dispatcher_host_ ? dispatcher_host_->quota_manager_ : NULL;
  }
  QuotaPermissionContext* permission_context() const {
    return dispatcher_host_ ?
        dispatcher_host_->permission_context_.get() : NULL;
  }
  int render_process_id() const { return render_process_id_; }
  int request_id() const { return request_id_; }

 private:
  base::WeakPtr<QuotaDispatcherHost> dispatcher_host_;
  int render_process_id_;
  int request_id_;
};

class QuotaDispatcherHost::QueryUsageAndQuotaDispatcher
    : public RequestDispatcher {
 public:
  QueryUsageAndQuotaDispatcher(
      base::WeakPtr<QuotaDispatcherHost> dispatcher_host,
      int request_id)
      : RequestDispatcher(dispatcher_host, request_id),
        weak_factory_(this) {}
  ~QueryUsageAndQuotaDispatcher() override {}

  void QueryStorageUsageAndQuota(const GURL& origin, StorageType type) {
    // crbug.com/349708
    TRACE_EVENT0("io",
                 "QuotaDispatcherHost::QueryUsageAndQuotaDispatcher"
                 "::QueryStorageUsageAndQuota");

    quota_manager()->GetUsageAndQuotaForWebApps(
        origin, type,
        base::Bind(&QueryUsageAndQuotaDispatcher::DidQueryStorageUsageAndQuota,
                   weak_factory_.GetWeakPtr()));
  }

 private:
  void DidQueryStorageUsageAndQuota(QuotaStatusCode status,
                                    int64_t usage,
                                    int64_t quota) {
    if (!dispatcher_host())
      return;
    // crbug.com/349708
    TRACE_EVENT0("io", "QuotaDispatcherHost::RequestQuotaDispatcher"
                 "::DidQueryStorageUsageAndQuota");

    if (status != storage::kQuotaStatusOk) {
      dispatcher_host()->Send(new QuotaMsg_DidFail(request_id(), status));
    } else {
      dispatcher_host()->Send(new QuotaMsg_DidQueryStorageUsageAndQuota(
          request_id(), usage, quota));
    }
    Completed();
  }

  base::WeakPtrFactory<QueryUsageAndQuotaDispatcher> weak_factory_;
};

class QuotaDispatcherHost::RequestQuotaDispatcher
    : public RequestDispatcher {
 public:
  typedef RequestQuotaDispatcher self_type;

  RequestQuotaDispatcher(base::WeakPtr<QuotaDispatcherHost> dispatcher_host,
                         const StorageQuotaParams& params)
      : RequestDispatcher(dispatcher_host, params.request_id),
        params_(params),
        current_usage_(0),
        current_quota_(0),
        requested_quota_(0),
        weak_factory_(this) {
    // Convert the requested size from uint64_t to int64_t since the quota
    // backend
    // requires int64_t values.
    // TODO(nhiroki): The backend should accept uint64_t values.
    requested_quota_ = base::saturated_cast<int64_t>(params_.requested_size);
  }
  ~RequestQuotaDispatcher() override {}

  void Start() {
    DCHECK(dispatcher_host());
    // crbug.com/349708
    TRACE_EVENT0("io", "QuotaDispatcherHost::RequestQuotaDispatcher::Start");

    DCHECK(params_.storage_type == storage::kStorageTypeTemporary ||
           params_.storage_type == storage::kStorageTypePersistent);
    if (params_.storage_type == storage::kStorageTypePersistent) {
      quota_manager()->GetUsageAndQuotaForWebApps(
          params_.origin_url, params_.storage_type,
          base::Bind(&self_type::DidGetPersistentUsageAndQuota,
                     weak_factory_.GetWeakPtr()));
    } else {
      quota_manager()->GetUsageAndQuotaForWebApps(
          params_.origin_url, params_.storage_type,
          base::Bind(&self_type::DidGetTemporaryUsageAndQuota,
                     weak_factory_.GetWeakPtr()));
    }
  }

 private:
  void DidGetPersistentUsageAndQuota(QuotaStatusCode status,
                                     int64_t usage,
                                     int64_t quota) {
    if (!dispatcher_host())
      return;
    if (status != storage::kQuotaStatusOk) {
      DidFinish(status, 0, 0);
      return;
    }

    if (quota_manager()->IsStorageUnlimited(params_.origin_url,
                                            params_.storage_type) ||
        requested_quota_ <= quota) {
      // Seems like we can just let it go.
      DidFinish(storage::kQuotaStatusOk, usage, params_.requested_size);
      return;
    }
    current_usage_ = usage;
    current_quota_ = quota;

    // Otherwise we need to consult with the permission context and
    // possibly show a prompt.
    DCHECK(permission_context());
    permission_context()->RequestQuotaPermission(params_, render_process_id(),
        base::Bind(&self_type::DidGetPermissionResponse,
                   weak_factory_.GetWeakPtr()));
  }

  void DidGetTemporaryUsageAndQuota(QuotaStatusCode status,
                                    int64_t usage,
                                    int64_t quota) {
    DidFinish(status, usage, std::min(requested_quota_, quota));
  }

  void DidGetPermissionResponse(
      QuotaPermissionContext::QuotaPermissionResponse response) {
    if (!dispatcher_host())
      return;
    if (response != QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW) {
      // User didn't allow the new quota.  Just returning the current quota.
      DidFinish(storage::kQuotaStatusOk, current_usage_, current_quota_);
      return;
    }
    // Now we're allowed to set the new quota.
    quota_manager()->SetPersistentHostQuota(
        net::GetHostOrSpecFromURL(params_.origin_url), params_.requested_size,
        base::Bind(&self_type::DidSetHostQuota, weak_factory_.GetWeakPtr()));
  }

  void DidSetHostQuota(QuotaStatusCode status, int64_t new_quota) {
    DidFinish(status, current_usage_, new_quota);
  }

  void DidFinish(QuotaStatusCode status, int64_t usage, int64_t granted_quota) {
    if (!dispatcher_host())
      return;
    DCHECK(dispatcher_host());
    if (status != storage::kQuotaStatusOk) {
      dispatcher_host()->Send(new QuotaMsg_DidFail(request_id(), status));
    } else {
      dispatcher_host()->Send(new QuotaMsg_DidGrantStorageQuota(
          request_id(), usage, granted_quota));
    }
    Completed();
  }

  StorageQuotaParams params_;
  int64_t current_usage_;
  int64_t current_quota_;
  int64_t requested_quota_;
  base::WeakPtrFactory<self_type> weak_factory_;
};

QuotaDispatcherHost::QuotaDispatcherHost(
    int process_id,
    QuotaManager* quota_manager,
    QuotaPermissionContext* permission_context)
    : BrowserMessageFilter(QuotaMsgStart),
      process_id_(process_id),
      quota_manager_(quota_manager),
      permission_context_(permission_context),
      weak_factory_(this) {
}

bool QuotaDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(QuotaDispatcherHost, message)
    IPC_MESSAGE_HANDLER(QuotaHostMsg_QueryStorageUsageAndQuota,
                        OnQueryStorageUsageAndQuota)
    IPC_MESSAGE_HANDLER(QuotaHostMsg_RequestStorageQuota,
                        OnRequestStorageQuota)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

QuotaDispatcherHost::~QuotaDispatcherHost() {}

void QuotaDispatcherHost::OnQueryStorageUsageAndQuota(
    int request_id,
    const GURL& origin,
    StorageType type) {
  QueryUsageAndQuotaDispatcher* dispatcher = new QueryUsageAndQuotaDispatcher(
      weak_factory_.GetWeakPtr(), request_id);
  dispatcher->QueryStorageUsageAndQuota(origin, type);
}

void QuotaDispatcherHost::OnRequestStorageQuota(
    const StorageQuotaParams& params) {
  if (params.storage_type != storage::kStorageTypeTemporary &&
      params.storage_type != storage::kStorageTypePersistent) {
    // Unsupported storage types.
    Send(new QuotaMsg_DidFail(params.request_id,
                              storage::kQuotaErrorNotSupported));
    return;
  }

  RequestQuotaDispatcher* dispatcher =
      new RequestQuotaDispatcher(weak_factory_.GetWeakPtr(),
                                 params);
  dispatcher->Start();
}

}  // namespace content
