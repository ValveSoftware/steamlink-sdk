// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_QUOTA_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_QUOTA_DISPATCHER_HOST_H_

#include "base/basictypes.h"
#include "base/id_map.h"
#include "content/public/browser/browser_message_filter.h"
#include "webkit/common/quota/quota_types.h"

class GURL;

namespace IPC {
class Message;
}

namespace quota {
class QuotaManager;
}

namespace content {
class QuotaPermissionContext;
struct StorageQuotaParams;

class QuotaDispatcherHost : public BrowserMessageFilter {
 public:
  QuotaDispatcherHost(int process_id,
                      quota::QuotaManager* quota_manager,
                      QuotaPermissionContext* permission_context);

  // BrowserMessageFilter:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  virtual ~QuotaDispatcherHost();

 private:
  class RequestDispatcher;
  class QueryUsageAndQuotaDispatcher;
  class RequestQuotaDispatcher;

  void OnQueryStorageUsageAndQuota(
      int request_id,
      const GURL& origin_url,
      quota::StorageType type);
  void OnRequestStorageQuota(const StorageQuotaParams& params);

  // The ID of this process.
  int process_id_;

  quota::QuotaManager* quota_manager_;
  scoped_refptr<QuotaPermissionContext> permission_context_;

  IDMap<RequestDispatcher, IDMapOwnPointer> outstanding_requests_;

  base::WeakPtrFactory<QuotaDispatcherHost> weak_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(QuotaDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_QUOTA_DISPATCHER_HOST_H_
