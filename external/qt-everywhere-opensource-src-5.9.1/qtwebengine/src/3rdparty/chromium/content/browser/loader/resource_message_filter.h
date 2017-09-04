// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_MESSAGE_FILTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/common/content_export.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/resource_type.h"

namespace storage {
class FileSystemContext;
}  // namespace storage

namespace net {
class URLRequestContext;
}  // namespace net


namespace content {
class ChromeAppCacheService;
class ChromeBlobStorageContext;
class ResourceContext;
class ServiceWorkerContextWrapper;

// This class filters out incoming IPC messages for network requests and
// processes them on the IPC thread.  As a result, network requests are not
// delayed by costly UI processing that may be occuring on the main thread of
// the browser.  It also means that any hangs in starting a network request
// will not interfere with browser UI.
class CONTENT_EXPORT ResourceMessageFilter
    : public BrowserMessageFilter,
      public BrowserAssociatedInterface<mojom::URLLoaderFactory>,
      public mojom::URLLoaderFactory {
 public:
  typedef base::Callback<void(ResourceType resource_type,
                              ResourceContext**,
                              net::URLRequestContext**)> GetContextsCallback;

  // |appcache_service|, |blob_storage_context|, |file_system_context| may be
  // NULL in unittests or for requests from the (NPAPI) plugin process.
  // The |origin_pid| argument to |get_contexts_callback| is not used
  // (and may be invalid) for requests that are NOT from the NPAPI plugin
  // process.
  ResourceMessageFilter(int child_id,
                        int process_type,
                        ChromeAppCacheService* appcache_service,
                        ChromeBlobStorageContext* blob_storage_context,
                        storage::FileSystemContext* file_system_context,
                        ServiceWorkerContextWrapper* service_worker_context,
                        const GetContextsCallback& get_contexts_callback);

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;

  void GetContexts(ResourceType resource_type,
                   ResourceContext** resource_context,
                   net::URLRequestContext** request_context);

  // Returns the net::URLRequestContext for the given request.
  net::URLRequestContext* GetURLRequestContext(ResourceType request_type);

  ChromeAppCacheService* appcache_service() const {
    return appcache_service_.get();
  }

  ChromeBlobStorageContext* blob_storage_context() const {
    return blob_storage_context_.get();
  }

  storage::FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

  ServiceWorkerContextWrapper* service_worker_context() const {
    return service_worker_context_.get();
  }

  int child_id() const { return child_id_; }
  int process_type() const { return process_type_; }

  base::WeakPtr<ResourceMessageFilter> GetWeakPtr();

  void CreateLoaderAndStart(
      mojom::URLLoaderAssociatedRequest request,
      int32_t routing_id,
      int32_t request_id,
      const ResourceRequest& url_request,
      mojom::URLLoaderClientAssociatedPtrInfo client_ptr_info) override;
  void SyncLoad(int32_t routing_id,
                int32_t request_id,
                const ResourceRequest& request,
                const SyncLoadCallback& callback) override;

 protected:
  // Protected destructor so that we can be overriden in tests.
  ~ResourceMessageFilter() override;

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<ResourceMessageFilter>;

  // The ID of the child process.
  int child_id_;

  int process_type_;
  bool is_channel_closed_;

  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  GetContextsCallback get_contexts_callback_;

  // This must come last to make sure weak pointers are invalidated first.
  base::WeakPtrFactory<ResourceMessageFilter> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ResourceMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_MESSAGE_FILTER_H_
