// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/quota_dispatcher.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "content/child/child_thread.h"
#include "content/child/quota_message_filter.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/quota_messages.h"
#include "third_party/WebKit/public/platform/WebStorageQuotaCallbacks.h"
#include "third_party/WebKit/public/platform/WebStorageQuotaType.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "url/gurl.h"

using blink::WebStorageQuotaCallbacks;
using blink::WebStorageQuotaError;
using blink::WebStorageQuotaType;
using quota::QuotaStatusCode;
using quota::StorageType;

namespace content {

static base::LazyInstance<base::ThreadLocalPointer<QuotaDispatcher> >::Leaky
    g_quota_dispatcher_tls = LAZY_INSTANCE_INITIALIZER;

namespace {

// QuotaDispatcher::Callback implementation for WebStorageQuotaCallbacks.
class WebStorageQuotaDispatcherCallback : public QuotaDispatcher::Callback {
 public:
  explicit WebStorageQuotaDispatcherCallback(
      blink::WebStorageQuotaCallbacks callback)
      : callbacks_(callback) {}
  virtual ~WebStorageQuotaDispatcherCallback() {}

  virtual void DidQueryStorageUsageAndQuota(int64 usage, int64 quota) OVERRIDE {
    callbacks_.didQueryStorageUsageAndQuota(usage, quota);
  }
  virtual void DidGrantStorageQuota(int64 usage, int64 granted_quota) OVERRIDE {
    callbacks_.didGrantStorageQuota(usage, granted_quota);
  }
  virtual void DidFail(quota::QuotaStatusCode error) OVERRIDE {
    callbacks_.didFail(static_cast<WebStorageQuotaError>(error));
  }

 private:
  blink::WebStorageQuotaCallbacks callbacks_;

  DISALLOW_COPY_AND_ASSIGN(WebStorageQuotaDispatcherCallback);
};

int CurrentWorkerId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}

}  // namespace

QuotaDispatcher::QuotaDispatcher(ThreadSafeSender* thread_safe_sender,
                                 QuotaMessageFilter* quota_message_filter)
    : thread_safe_sender_(thread_safe_sender),
      quota_message_filter_(quota_message_filter) {
  g_quota_dispatcher_tls.Pointer()->Set(this);
}

QuotaDispatcher::~QuotaDispatcher() {
  IDMap<Callback, IDMapOwnPointer>::iterator iter(&pending_quota_callbacks_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->DidFail(quota::kQuotaErrorAbort);
    iter.Advance();
  }

  g_quota_dispatcher_tls.Pointer()->Set(NULL);
}

QuotaDispatcher* QuotaDispatcher::ThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender,
    QuotaMessageFilter* quota_message_filter) {
  if (g_quota_dispatcher_tls.Pointer()->Get())
    return g_quota_dispatcher_tls.Pointer()->Get();

  QuotaDispatcher* dispatcher = new QuotaDispatcher(
      thread_safe_sender, quota_message_filter);
  if (WorkerTaskRunner::Instance()->CurrentWorkerId())
    WorkerTaskRunner::Instance()->AddStopObserver(dispatcher);
  return dispatcher;
}

void QuotaDispatcher::OnWorkerRunLoopStopped() {
  delete this;
}

void QuotaDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(QuotaDispatcher, msg)
    IPC_MESSAGE_HANDLER(QuotaMsg_DidGrantStorageQuota,
                        DidGrantStorageQuota)
    IPC_MESSAGE_HANDLER(QuotaMsg_DidQueryStorageUsageAndQuota,
                        DidQueryStorageUsageAndQuota);
    IPC_MESSAGE_HANDLER(QuotaMsg_DidFail, DidFail);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << "Unhandled message:" << msg.type();
}

void QuotaDispatcher::QueryStorageUsageAndQuota(
    const GURL& origin_url,
    StorageType type,
    Callback* callback) {
  DCHECK(callback);
  int request_id = quota_message_filter_->GenerateRequestID(CurrentWorkerId());
  pending_quota_callbacks_.AddWithID(callback, request_id);
  thread_safe_sender_->Send(new QuotaHostMsg_QueryStorageUsageAndQuota(
      request_id, origin_url, type));
}

void QuotaDispatcher::RequestStorageQuota(
    int render_view_id,
    const GURL& origin_url,
    StorageType type,
    uint64 requested_size,
    Callback* callback) {
  DCHECK(callback);
  DCHECK(CurrentWorkerId() == 0);
  int request_id = quota_message_filter_->GenerateRequestID(CurrentWorkerId());
  pending_quota_callbacks_.AddWithID(callback, request_id);

  StorageQuotaParams params;
  params.render_view_id = render_view_id;
  params.request_id = request_id;
  params.origin_url = origin_url;
  params.storage_type = type;
  params.requested_size = requested_size;
  params.user_gesture =
      blink::WebUserGestureIndicator::isProcessingUserGesture();
  thread_safe_sender_->Send(new QuotaHostMsg_RequestStorageQuota(params));
}

// static
QuotaDispatcher::Callback*
QuotaDispatcher::CreateWebStorageQuotaCallbacksWrapper(
    blink::WebStorageQuotaCallbacks callbacks) {
  return new WebStorageQuotaDispatcherCallback(callbacks);
}

void QuotaDispatcher::DidGrantStorageQuota(
    int request_id,
    int64 current_usage,
    int64 granted_quota) {
  Callback* callback = pending_quota_callbacks_.Lookup(request_id);
  DCHECK(callback);
  callback->DidGrantStorageQuota(current_usage, granted_quota);
  pending_quota_callbacks_.Remove(request_id);
}

void QuotaDispatcher::DidQueryStorageUsageAndQuota(
    int request_id,
    int64 current_usage,
    int64 current_quota) {
  Callback* callback = pending_quota_callbacks_.Lookup(request_id);
  DCHECK(callback);
  callback->DidQueryStorageUsageAndQuota(current_usage, current_quota);
  pending_quota_callbacks_.Remove(request_id);
}

void QuotaDispatcher::DidFail(
    int request_id,
    QuotaStatusCode error) {
  Callback* callback = pending_quota_callbacks_.Lookup(request_id);
  DCHECK(callback);
  callback->DidFail(error);
  pending_quota_callbacks_.Remove(request_id);
}

COMPILE_ASSERT(int(blink::WebStorageQuotaTypeTemporary) == \
               int(quota::kStorageTypeTemporary), mismatching_enums);
COMPILE_ASSERT(int(blink::WebStorageQuotaTypePersistent) == \
               int(quota::kStorageTypePersistent), mismatching_enums);

COMPILE_ASSERT(int(blink::WebStorageQuotaErrorNotSupported) == \
               int(quota::kQuotaErrorNotSupported), mismatching_enums);
COMPILE_ASSERT(int(blink::WebStorageQuotaErrorAbort) == \
               int(quota::kQuotaErrorAbort), mismatching_enums);

}  // namespace content
