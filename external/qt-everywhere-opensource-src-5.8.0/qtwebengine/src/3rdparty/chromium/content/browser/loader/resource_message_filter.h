// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_MESSAGE_FILTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/host_zoom_level_context.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
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
class CONTENT_EXPORT ResourceMessageFilter : public BrowserMessageFilter {
 public:
  // TODO(ricea): Remove origin_pid when support for NPAPI plugins is removed.
  // crbug.com/493212 is the tracking bug for NPAPI removal.
  typedef base::Callback<void(ResourceType resource_type,
                              int origin_pid,
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
                        HostZoomLevelContext* host_zoom_level_context,
                        const GetContextsCallback& get_contexts_callback);

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // |origin_pid| is only required for NPAPI plugin processes. Its value is
  // ignored otherwise.
  void GetContexts(ResourceType resource_type,
                   int origin_pid,
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

  // Returns a raw pointer to the HostZoomLevelContext's associated HostZoomMap,
  // or NULL if no context is present.
  const HostZoomMap* GetHostZoomMap() const;

  int child_id() const { return child_id_; }
  int process_type() const { return process_type_; }

  base::WeakPtr<ResourceMessageFilter> GetWeakPtr();

 protected:
  // Protected destructor so that we can be overriden in tests.
  ~ResourceMessageFilter() override;

 private:
  // The ID of the child process.
  int child_id_;

  int process_type_;

  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  scoped_refptr<HostZoomLevelContext> host_zoom_level_context_;

  GetContextsCallback get_contexts_callback_;

  // This must come last to make sure weak pointers are invalidated first.
  base::WeakPtrFactory<ResourceMessageFilter> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ResourceMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_MESSAGE_FILTER_H_
