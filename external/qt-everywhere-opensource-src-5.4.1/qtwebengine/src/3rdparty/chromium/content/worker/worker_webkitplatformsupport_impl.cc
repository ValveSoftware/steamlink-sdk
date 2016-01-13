// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/worker_webkitplatformsupport_impl.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/blink_glue.h"
#include "content/child/database_util.h"
#include "content/child/fileapi/webfilesystem_impl.h"
#include "content/child/indexed_db/webidbfactory_impl.h"
#include "content/child/quota_dispatcher.h"
#include "content/child/quota_message_filter.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/web_database_observer_impl.h"
#include "content/child/webblobregistry_impl.h"
#include "content/child/webfileutilities_impl.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/file_utilities_messages.h"
#include "content/common/mime_registry_messages.h"
#include "content/worker/worker_thread.h"
#include "ipc/ipc_sync_message_filter.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebBlobRegistry.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "webkit/common/quota/quota_types.h"

using blink::Platform;
using blink::WebBlobRegistry;
using blink::WebClipboard;
using blink::WebFileInfo;
using blink::WebFileSystem;
using blink::WebFileUtilities;
using blink::WebMessagePortChannel;
using blink::WebMimeRegistry;
using blink::WebSandboxSupport;
using blink::WebStorageNamespace;
using blink::WebString;
using blink::WebURL;

namespace content {

// TODO(kinuko): Probably this could be consolidated into
// RendererWebKitPlatformSupportImpl::FileUtilities.
class WorkerWebKitPlatformSupportImpl::FileUtilities
    : public WebFileUtilitiesImpl {
 public:
  explicit FileUtilities(ThreadSafeSender* sender)
      : thread_safe_sender_(sender) {}
  virtual bool getFileInfo(const WebString& path, WebFileInfo& result);
 private:
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
};

bool WorkerWebKitPlatformSupportImpl::FileUtilities::getFileInfo(
    const WebString& path,
    WebFileInfo& web_file_info) {
  base::File::Info file_info;
  base::File::Error status;
  if (!thread_safe_sender_.get() ||
      !thread_safe_sender_->Send(new FileUtilitiesMsg_GetFileInfo(
           base::FilePath::FromUTF16Unsafe(path), &file_info, &status)) ||
      status != base::File::FILE_OK) {
    return false;
  }
  FileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platformPath = path;
  return true;
}

//------------------------------------------------------------------------------

WorkerWebKitPlatformSupportImpl::WorkerWebKitPlatformSupportImpl(
    ThreadSafeSender* sender,
    IPC::SyncMessageFilter* sync_message_filter,
    QuotaMessageFilter* quota_message_filter)
    : thread_safe_sender_(sender),
      child_thread_loop_(base::MessageLoopProxy::current()),
      sync_message_filter_(sync_message_filter),
      quota_message_filter_(quota_message_filter) {
  if (sender) {
    blob_registry_.reset(new WebBlobRegistryImpl(sender));
    web_idb_factory_.reset(new WebIDBFactoryImpl(sender));
    web_database_observer_impl_.reset(
        new WebDatabaseObserverImpl(sync_message_filter));
  }
}

WorkerWebKitPlatformSupportImpl::~WorkerWebKitPlatformSupportImpl() {
  WebFileSystemImpl::DeleteThreadSpecificInstance();
}

WebClipboard* WorkerWebKitPlatformSupportImpl::clipboard() {
  NOTREACHED();
  return NULL;
}

WebMimeRegistry* WorkerWebKitPlatformSupportImpl::mimeRegistry() {
  return this;
}

WebFileSystem* WorkerWebKitPlatformSupportImpl::fileSystem() {
  return WebFileSystemImpl::ThreadSpecificInstance(child_thread_loop_.get());
}

WebFileUtilities* WorkerWebKitPlatformSupportImpl::fileUtilities() {
  if (!file_utilities_) {
    file_utilities_.reset(new FileUtilities(thread_safe_sender_.get()));
    file_utilities_->set_sandbox_enabled(sandboxEnabled());
  }
  return file_utilities_.get();
}

WebSandboxSupport* WorkerWebKitPlatformSupportImpl::sandboxSupport() {
  NOTREACHED();
  return NULL;
}

bool WorkerWebKitPlatformSupportImpl::sandboxEnabled() {
  // Always return true because WebKit should always act as though the Sandbox
  // is enabled for workers.  See the comment in WebKitPlatformSupport for
  // more info.
  return true;
}

unsigned long long WorkerWebKitPlatformSupportImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  NOTREACHED();
  return 0;
}

bool WorkerWebKitPlatformSupportImpl::isLinkVisited(
    unsigned long long link_hash) {
  NOTREACHED();
  return false;
}

void WorkerWebKitPlatformSupportImpl::createMessageChannel(
    blink::WebMessagePortChannel** channel1,
    blink::WebMessagePortChannel** channel2) {
  WebMessagePortChannelImpl::CreatePair(
      child_thread_loop_.get(), channel1, channel2);
}

void WorkerWebKitPlatformSupportImpl::setCookies(
    const WebURL& url,
    const WebURL& first_party_for_cookies,
    const WebString& value) {
  NOTREACHED();
}

WebString WorkerWebKitPlatformSupportImpl::cookies(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  // WebSocketHandshake may access cookies in worker process.
  return WebString();
}

WebString WorkerWebKitPlatformSupportImpl::defaultLocale() {
  NOTREACHED();
  return WebString();
}

WebStorageNamespace*
WorkerWebKitPlatformSupportImpl::createLocalStorageNamespace() {
  NOTREACHED();
  return 0;
}

void WorkerWebKitPlatformSupportImpl::dispatchStorageEvent(
    const WebString& key, const WebString& old_value,
    const WebString& new_value, const WebString& origin,
    const blink::WebURL& url, bool is_local_storage) {
  NOTREACHED();
}

Platform::FileHandle
WorkerWebKitPlatformSupportImpl::databaseOpenFile(
    const WebString& vfs_file_name, int desired_flags) {
  return DatabaseUtil::DatabaseOpenFile(
      vfs_file_name, desired_flags, sync_message_filter_.get());
}

int WorkerWebKitPlatformSupportImpl::databaseDeleteFile(
    const WebString& vfs_file_name, bool sync_dir) {
  return DatabaseUtil::DatabaseDeleteFile(
      vfs_file_name, sync_dir, sync_message_filter_.get());
}

long WorkerWebKitPlatformSupportImpl::databaseGetFileAttributes(
    const WebString& vfs_file_name) {
  return DatabaseUtil::DatabaseGetFileAttributes(vfs_file_name,
                                                 sync_message_filter_.get());
}

long long WorkerWebKitPlatformSupportImpl::databaseGetFileSize(
    const WebString& vfs_file_name) {
  return DatabaseUtil::DatabaseGetFileSize(vfs_file_name,
                                           sync_message_filter_.get());
}

long long WorkerWebKitPlatformSupportImpl::databaseGetSpaceAvailableForOrigin(
    const WebString& origin_identifier) {
  return DatabaseUtil::DatabaseGetSpaceAvailable(origin_identifier,
                                                 sync_message_filter_.get());
}

blink::WebIDBFactory* WorkerWebKitPlatformSupportImpl::idbFactory() {
  if (!web_idb_factory_)
    web_idb_factory_.reset(new WebIDBFactoryImpl(thread_safe_sender_.get()));
  return web_idb_factory_.get();
}

blink::WebDatabaseObserver*
WorkerWebKitPlatformSupportImpl::databaseObserver() {
  return web_database_observer_impl_.get();
}

WebMimeRegistry::SupportsType
WorkerWebKitPlatformSupportImpl::supportsMIMEType(
    const WebString&) {
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType
WorkerWebKitPlatformSupportImpl::supportsImageMIMEType(
    const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType
WorkerWebKitPlatformSupportImpl::supportsJavaScriptMIMEType(const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType
WorkerWebKitPlatformSupportImpl::supportsMediaMIMEType(
    const WebString&, const WebString&, const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

bool WorkerWebKitPlatformSupportImpl::supportsMediaSourceMIMEType(
    const blink::WebString& mimeType, const blink::WebString& codecs) {
  NOTREACHED();
  return false;
}

bool WorkerWebKitPlatformSupportImpl::supportsEncryptedMediaMIMEType(
    const blink::WebString& key_system,
    const blink::WebString& mime_type,
    const blink::WebString& codecs) {
  NOTREACHED();
  return false;
}

WebMimeRegistry::SupportsType
WorkerWebKitPlatformSupportImpl::supportsNonImageMIMEType(
    const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebString WorkerWebKitPlatformSupportImpl::mimeTypeForExtension(
    const WebString& file_extension) {
  std::string mime_type;
  thread_safe_sender_->Send(new MimeRegistryMsg_GetMimeTypeFromExtension(
      base::FilePath::FromUTF16Unsafe(file_extension).value(), &mime_type));
  return base::ASCIIToUTF16(mime_type);
}

WebString WorkerWebKitPlatformSupportImpl::wellKnownMimeTypeForExtension(
    const WebString& file_extension) {
  std::string mime_type;
  net::GetWellKnownMimeTypeFromExtension(
      base::FilePath::FromUTF16Unsafe(file_extension).value(), &mime_type);
  return base::ASCIIToUTF16(mime_type);
}

WebString WorkerWebKitPlatformSupportImpl::mimeTypeFromFile(
    const WebString& file_path) {
  std::string mime_type;
  thread_safe_sender_->Send(
      new MimeRegistryMsg_GetMimeTypeFromFile(
          base::FilePath::FromUTF16Unsafe(file_path),
          &mime_type));
  return base::ASCIIToUTF16(mime_type);
}

WebBlobRegistry* WorkerWebKitPlatformSupportImpl::blobRegistry() {
  return blob_registry_.get();
}

void WorkerWebKitPlatformSupportImpl::queryStorageUsageAndQuota(
    const blink::WebURL& storage_partition,
    blink::WebStorageQuotaType type,
    blink::WebStorageQuotaCallbacks callbacks) {
  if (!thread_safe_sender_.get() || !quota_message_filter_.get())
    return;
  QuotaDispatcher::ThreadSpecificInstance(
      thread_safe_sender_.get(),
      quota_message_filter_.get())->QueryStorageUsageAndQuota(
          storage_partition,
          static_cast<quota::StorageType>(type),
          QuotaDispatcher::CreateWebStorageQuotaCallbacksWrapper(callbacks));
}

}  // namespace content
