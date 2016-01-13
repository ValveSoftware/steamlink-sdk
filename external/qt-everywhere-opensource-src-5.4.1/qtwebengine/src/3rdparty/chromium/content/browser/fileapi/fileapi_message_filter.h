// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILEAPI_FILEAPI_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_FILEAPI_FILEAPI_MESSAGE_FILTER_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_util_proxy.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/common/blob/blob_data.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/quota/quota_types.h"

class GURL;

namespace base {
class FilePath;
class Time;
}

namespace fileapi {
class FileSystemURL;
class FileSystemOperationRunner;
struct DirectoryEntry;
struct FileSystemInfo;
}

namespace net {
class URLRequestContext;
class URLRequestContextGetter;
}  // namespace net

namespace content {
class BlobStorageHost;
}

namespace webkit_blob {
class ShareableFileReference;
}

namespace content {
class ChildProcessSecurityPolicyImpl;
class ChromeBlobStorageContext;

// TODO(tyoshino): Factor out code except for IPC gluing from
// FileAPIMessageFilter into separate classes. See crbug.com/263741.
class CONTENT_EXPORT FileAPIMessageFilter : public BrowserMessageFilter {
 public:
  // Used by the renderer process host on the UI thread.
  FileAPIMessageFilter(
      int process_id,
      net::URLRequestContextGetter* request_context_getter,
      fileapi::FileSystemContext* file_system_context,
      ChromeBlobStorageContext* blob_storage_context,
      StreamContext* stream_context);
  // Used by the worker process host on the IO thread.
  FileAPIMessageFilter(
      int process_id,
      net::URLRequestContext* request_context,
      fileapi::FileSystemContext* file_system_context,
      ChromeBlobStorageContext* blob_storage_context,
      StreamContext* stream_context);

  // BrowserMessageFilter implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;
  virtual base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& message) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  virtual ~FileAPIMessageFilter();

  virtual void BadMessageReceived() OVERRIDE;

 private:
  typedef fileapi::FileSystemOperationRunner::OperationID OperationID;

  void OnOpenFileSystem(int request_id,
                        const GURL& origin_url,
                        fileapi::FileSystemType type);
  void OnResolveURL(int request_id,
                    const GURL& filesystem_url);
  void OnDeleteFileSystem(int request_id,
                          const GURL& origin_url,
                          fileapi::FileSystemType type);
  void OnMove(int request_id,
              const GURL& src_path,
              const GURL& dest_path);
  void OnCopy(int request_id,
              const GURL& src_path,
              const GURL& dest_path);
  void OnRemove(int request_id, const GURL& path, bool recursive);
  void OnReadMetadata(int request_id, const GURL& path);
  void OnCreate(int request_id,
                const GURL& path,
                bool exclusive,
                bool is_directory,
                bool recursive);
  void OnExists(int request_id, const GURL& path, bool is_directory);
  void OnReadDirectory(int request_id, const GURL& path);
  void OnWrite(int request_id,
               const GURL& path,
               const std::string& blob_uuid,
               int64 offset);
  void OnTruncate(int request_id, const GURL& path, int64 length);
  void OnTouchFile(int request_id,
                   const GURL& path,
                   const base::Time& last_access_time,
                   const base::Time& last_modified_time);
  void OnCancel(int request_id, int request_to_cancel);
  void OnSyncGetPlatformPath(const GURL& path,
                             base::FilePath* platform_path);
  void OnCreateSnapshotFile(int request_id,
                            const GURL& path);
  void OnDidReceiveSnapshotFile(int request_id);

  // Handlers for BlobHostMsg_ family messages.

  void OnStartBuildingBlob(const std::string& uuid);
  void OnAppendBlobDataItemToBlob(const std::string& uuid,
                                  const webkit_blob::BlobData::Item& item);
  void OnAppendSharedMemoryToBlob(const std::string& uuid,
                                  base::SharedMemoryHandle handle,
                                  size_t buffer_size);
  void OnFinishBuildingBlob(const std::string& uuid,
                             const std::string& content_type);
  void OnIncrementBlobRefCount(const std::string& uuid);
  void OnDecrementBlobRefCount(const std::string& uuid);
  void OnRegisterPublicBlobURL(const GURL& public_url, const std::string& uuid);
  void OnRevokePublicBlobURL(const GURL& public_url);

  // Handlers for StreamHostMsg_ family messages.
  //
  // TODO(tyoshino): Consider renaming BlobData to more generic one as it's now
  // used for Stream.

  // Currently |content_type| is ignored.
  //
  // TODO(tyoshino): Set |content_type| to the stream.
  void OnStartBuildingStream(const GURL& url, const std::string& content_type);
  void OnAppendBlobDataItemToStream(
      const GURL& url, const webkit_blob::BlobData::Item& item);
  void OnAppendSharedMemoryToStream(
      const GURL& url, base::SharedMemoryHandle handle, size_t buffer_size);
  void OnFinishBuildingStream(const GURL& url);
  void OnAbortBuildingStream(const GURL& url);
  void OnCloneStream(const GURL& url, const GURL& src_url);
  void OnRemoveStream(const GURL& url);

  // Callback functions to be used when each file operation is finished.
  void DidFinish(int request_id, base::File::Error result);
  void DidGetMetadata(int request_id,
                      base::File::Error result,
                      const base::File::Info& info);
  void DidGetMetadataForStreaming(int request_id,
                                  base::File::Error result,
                                  const base::File::Info& info);
  void DidReadDirectory(int request_id,
                        base::File::Error result,
                        const std::vector<fileapi::DirectoryEntry>& entries,
                        bool has_more);
  void DidWrite(int request_id,
                base::File::Error result,
                int64 bytes,
                bool complete);
  void DidOpenFileSystem(int request_id,
                         const GURL& root,
                         const std::string& filesystem_name,
                         base::File::Error result);
  void DidResolveURL(int request_id,
                     base::File::Error result,
                     const fileapi::FileSystemInfo& info,
                     const base::FilePath& file_path,
                     fileapi::FileSystemContext::ResolvedEntryType type);
  void DidDeleteFileSystem(int request_id,
                           base::File::Error result);
  void DidCreateSnapshot(
      int request_id,
      const fileapi::FileSystemURL& url,
      base::File::Error result,
      const base::File::Info& info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);

  // Sends a FileSystemMsg_DidFail and returns false if |url| is invalid.
  bool ValidateFileSystemURL(int request_id, const fileapi::FileSystemURL& url);

  // Retrieves the Stream object for |url| from |stream_context_|. Returns unset
  // scoped_refptr when there's no Stream instance for the given |url|
  // registered with stream_context_->registry().
  scoped_refptr<Stream> GetStreamForURL(const GURL& url);

  fileapi::FileSystemOperationRunner* operation_runner() {
    return operation_runner_.get();
  }

  int process_id_;

  fileapi::FileSystemContext* context_;
  ChildProcessSecurityPolicyImpl* security_policy_;

  // Keeps map from request_id to OperationID for ongoing operations.
  // (Primarily for Cancel operation)
  typedef std::map<int, OperationID> OperationsMap;
  OperationsMap operations_;

  // The getter holds the context until OnChannelConnected() can be called from
  // the IO thread, which will extract the net::URLRequestContext from it.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  net::URLRequestContext* request_context_;

  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  scoped_refptr<StreamContext> stream_context_;

  scoped_ptr<fileapi::FileSystemOperationRunner> operation_runner_;

  // Keeps track of blobs used in this process and cleans up
  // when the renderer process dies.
  scoped_ptr<BlobStorageHost> blob_storage_host_;

  // Keep track of stream URLs registered in this process. Need to unregister
  // all of them when the renderer process dies.
  base::hash_set<std::string> stream_urls_;

  // Used to keep snapshot files alive while a DidCreateSnapshot
  // is being sent to the renderer.
  std::map<int, scoped_refptr<webkit_blob::ShareableFileReference> >
      in_transit_snapshot_files_;

  DISALLOW_COPY_AND_ASSIGN(FileAPIMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILEAPI_FILEAPI_MESSAGE_FILTER_H_
