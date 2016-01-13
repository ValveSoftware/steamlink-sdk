// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_WORKER_WEBKITPLATFORMSUPPORT_IMPL_H_
#define CONTENT_WORKER_WORKER_WEBKITPLATFORMSUPPORT_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "content/child/blink_platform_impl.h"
#include "third_party/WebKit/public/platform/WebIDBFactory.h"
#include "third_party/WebKit/public/platform/WebMimeRegistry.h"

namespace base {
class MessageLoopProxy;
}

namespace IPC {
class SyncMessageFilter;
}

namespace blink {
class WebFileUtilities;
}

namespace content {
class QuotaMessageFilter;
class ThreadSafeSender;
class WebDatabaseObserverImpl;
class WebFileSystemImpl;

class WorkerWebKitPlatformSupportImpl : public BlinkPlatformImpl,
                                        public blink::WebMimeRegistry {
 public:
  WorkerWebKitPlatformSupportImpl(
      ThreadSafeSender* sender,
      IPC::SyncMessageFilter* sync_message_filter,
      QuotaMessageFilter* quota_message_filter);
  virtual ~WorkerWebKitPlatformSupportImpl();

  // WebKitPlatformSupport methods:
  virtual blink::WebClipboard* clipboard();
  virtual blink::WebMimeRegistry* mimeRegistry();
  virtual blink::WebFileSystem* fileSystem();
  virtual blink::WebFileUtilities* fileUtilities();
  virtual blink::WebSandboxSupport* sandboxSupport();
  virtual bool sandboxEnabled();
  virtual unsigned long long visitedLinkHash(const char* canonicalURL,
                                             size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual void createMessageChannel(blink::WebMessagePortChannel** channel1,
                                    blink::WebMessagePortChannel** channel2);
  virtual void setCookies(const blink::WebURL& url,
                          const blink::WebURL& first_party_for_cookies,
                          const blink::WebString& value);
  virtual blink::WebString cookies(
      const blink::WebURL& url,
      const blink::WebURL& first_party_for_cookies);
  virtual blink::WebString defaultLocale();
  virtual blink::WebStorageNamespace* createLocalStorageNamespace();
  virtual void dispatchStorageEvent(
      const blink::WebString& key, const blink::WebString& old_value,
      const blink::WebString& new_value, const blink::WebString& origin,
      const blink::WebURL& url, bool is_local_storage);

  virtual blink::Platform::FileHandle databaseOpenFile(
      const blink::WebString& vfs_file_name, int desired_flags);
  virtual int databaseDeleteFile(const blink::WebString& vfs_file_name,
                                 bool sync_dir);
  virtual long databaseGetFileAttributes(
      const blink::WebString& vfs_file_name);
  virtual long long databaseGetFileSize(
      const blink::WebString& vfs_file_name);
  virtual long long databaseGetSpaceAvailableForOrigin(
      const blink::WebString& origin_identifier);
  virtual blink::WebBlobRegistry* blobRegistry();
  virtual blink::WebIDBFactory* idbFactory();
  virtual blink::WebDatabaseObserver* databaseObserver();

  // WebMimeRegistry methods:
  virtual blink::WebMimeRegistry::SupportsType supportsMIMEType(
      const blink::WebString&);
  virtual blink::WebMimeRegistry::SupportsType supportsImageMIMEType(
      const blink::WebString&);
  virtual blink::WebMimeRegistry::SupportsType supportsJavaScriptMIMEType(
      const blink::WebString&);
  virtual blink::WebMimeRegistry::SupportsType supportsMediaMIMEType(
      const blink::WebString&,
      const blink::WebString&,
      const blink::WebString&);
  virtual bool supportsMediaSourceMIMEType(
      const blink::WebString&,
      const blink::WebString&);
  virtual bool supportsEncryptedMediaMIMEType(const blink::WebString&,
                                              const blink::WebString&,
                                              const blink::WebString&);
  virtual blink::WebMimeRegistry::SupportsType supportsNonImageMIMEType(
      const blink::WebString&);
  virtual blink::WebString mimeTypeForExtension(const blink::WebString&);
  virtual blink::WebString wellKnownMimeTypeForExtension(
      const blink::WebString&);
  virtual blink::WebString mimeTypeFromFile(const blink::WebString&);
  virtual void queryStorageUsageAndQuota(
      const blink::WebURL& storage_partition,
      blink::WebStorageQuotaType,
      blink::WebStorageQuotaCallbacks) OVERRIDE;

  WebDatabaseObserverImpl* web_database_observer_impl() {
    return web_database_observer_impl_.get();
  }

 private:

  class FileUtilities;
  scoped_ptr<FileUtilities> file_utilities_;
  scoped_ptr<blink::WebBlobRegistry> blob_registry_;
  scoped_ptr<blink::WebIDBFactory> web_idb_factory_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<base::MessageLoopProxy> child_thread_loop_;
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;
  scoped_refptr<QuotaMessageFilter> quota_message_filter_;
  scoped_ptr<WebDatabaseObserverImpl> web_database_observer_impl_;

  DISALLOW_COPY_AND_ASSIGN(WorkerWebKitPlatformSupportImpl);
};

}  // namespace content

#endif  // CONTENT_WORKER_WORKER_WEBKITPLATFORMSUPPORT_IMPL_H_
