// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLOB_STORAGE_BLOB_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_BLOB_STORAGE_BLOB_DISPATCHER_HOST_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "storage/browser/blob/blob_transport_host.h"
#include "storage/common/blob_storage/blob_storage_constants.h"

class GURL;

namespace storage {
class DataElement;
class BlobDataHandle;
struct BlobItemBytesRequest;
struct BlobItemBytesResponse;
class BlobStorageContext;
class FileSystemContext;
}

namespace content {
class ChromeBlobStorageContext;

// This class's responsibility is to listen for and dispatch blob storage
// messages and handle logistics of blob storage for a single child process.
// When the child process terminates all blob references attributable to
// that process go away upon destruction of the instance.
// This lives in the browser process, is single threaded (IO thread), and there
// is one per child process.
class CONTENT_EXPORT BlobDispatcherHost : public BrowserMessageFilter {
 public:
  BlobDispatcherHost(
      int process_id,
      scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
      scoped_refptr<storage::FileSystemContext> file_system_context);

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;

 protected:
  ~BlobDispatcherHost() override;

 private:
  friend class base::RefCountedThreadSafe<BlobDispatcherHost>;
  friend class BlobDispatcherHostTest;
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest, EmptyUUIDs);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest, MultipleTransfers);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest, SharedMemoryTransfer);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest, OnCancelBuildingBlob);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest,
                           BlobReferenceWhileConstructing);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest,
                           BlobReferenceWhileShortcutConstructing);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest,
                           BlobReferenceWhileConstructingCancelled);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest, DecrementRefAfterRegister);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest, DecrementRefAfterOnStart);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest,
                           DecrementRefAfterOnStartWithHandle);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest,
                           HostDisconnectAfterRegisterWithHandle);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest, HostDisconnectAfterOnStart);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest,
                           HostDisconnectAfterOnMemoryResponse);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest,
                           CreateBlobWithBrokenReference);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest,
                           DeferenceBlobOnDifferentHost);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest, BuildingReferenceChain);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest,
                           BuildingReferenceChainWithCancel);
  FRIEND_TEST_ALL_PREFIXES(BlobDispatcherHostTest,
                           BuildingReferenceChainWithSourceDeath);

  void OnRegisterBlob(const std::string& uuid,
                      const std::string& content_type,
                      const std::string& content_disposition,
                      const std::vector<storage::DataElement>& descriptions);
  void OnMemoryItemResponse(
      const std::string& uuid,
      const std::vector<storage::BlobItemBytesResponse>& response);
  void OnCancelBuildingBlob(const std::string& uuid,
                            const storage::BlobStatus code);

  void OnIncrementBlobRefCount(const std::string& uuid);
  void OnDecrementBlobRefCount(const std::string& uuid);
  void OnRegisterPublicBlobURL(const GURL& public_url, const std::string& uuid);
  void OnRevokePublicBlobURL(const GURL& public_url);

  storage::BlobStorageContext* context();

  void SendMemoryRequest(const std::string& uuid,
                         std::vector<storage::BlobItemBytesRequest> requests,
                         std::vector<base::SharedMemoryHandle> memory_handles,
                         std::vector<base::File> files);

  // The status should never be a pending status (see BlobStatusIsPending), and
  // we ignore calls for |uuid|s that aren't in use in this host.
  void SendFinalBlobStatus(const std::string& uuid, storage::BlobStatus status);

  bool IsInUseInHost(const std::string& uuid);
  bool IsUrlRegisteredInHost(const GURL& blob_url);

  // Unregisters all blobs and urls that were registered in this host.
  void ClearHostFromBlobStorageContext();

  struct HostedBlobState {
    explicit HostedBlobState(std::unique_ptr<storage::BlobDataHandle> handle);
    ~HostedBlobState();
    HostedBlobState(HostedBlobState&&);
    HostedBlobState& operator=(HostedBlobState&&);
    DISALLOW_COPY_AND_ASSIGN(HostedBlobState);

    int refcount = 1;
    std::unique_ptr<storage::BlobDataHandle> handle;
  };

  const int process_id_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Collection of blob ids and a count of how many usages
  // of that id are attributable to this consumer.
  std::unordered_map<std::string, HostedBlobState> blobs_inuse_map_;

  // The set of public blob urls coined by this consumer.
  std::set<GURL> public_blob_urls_;

  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  storage::BlobTransportHost transport_host_;

  DISALLOW_COPY_AND_ASSIGN(BlobDispatcherHost);
};
}  // namespace content
#endif  // CONTENT_BROWSER_BLOB_STORAGE_BLOB_DISPATCHER_HOST_H_
