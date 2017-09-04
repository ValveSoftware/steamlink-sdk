// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLOB_STORAGE_BLOB_TRANSPORT_CONTROLLER_H_
#define CONTENT_CHILD_BLOB_STORAGE_BLOB_TRANSPORT_CONTROLLER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_handle.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "ipc/ipc_platform_file.h"
#include "storage/common/blob_storage/blob_storage_constants.h"

namespace base {
template <typename T>
struct DefaultLazyInstanceTraits;
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace storage {
class DataElement;
struct BlobItemBytesRequest;
struct BlobItemBytesResponse;
}

namespace IPC {
class Sender;
}

namespace content {

class BlobConsolidation;
class ThreadSafeSender;

// This class is used to manage all the asynchronous transporation of blobs from
// the Renderer to the Browser process, where it's handling the Renderer side.
// The function of this class is to:
// * Be a lazy singleton,
// * hold all of the blob data that is being transported to the Browser process,
// * create the blob item 'descriptions' for the browser,
// * include shortcut data in the descriptions,
// * generate responses to blob memory requests, and
// * send IPC responses.
// We try to keep the renderer alive during blob transportation by calling
// ChildProcess::AddProcessRef and
// blink::Platform::current()->suddenTerminationChanged.
// Must be used on the IO thread.
class CONTENT_EXPORT BlobTransportController {
 public:
  static BlobTransportController* GetInstance();

  // This kicks off a blob transfer to the browser thread, which involves
  // sending an IPC message and storing the blob consolidation object. Designed
  // to be called by the main thread or a worker thread.
  // This also calls ChildProcess::AddRefProcess to keep our process around
  // while we transfer.
  static void InitiateBlobTransfer(
      const std::string& uuid,
      const std::string& content_type,
      scoped_refptr<BlobConsolidation> consolidation,
      scoped_refptr<ThreadSafeSender> sender,
      base::SingleThreadTaskRunner* io_runner,
      scoped_refptr<base::SingleThreadTaskRunner> main_runner);

  // This responds to the request using the |sender|. If we need to save files
  // then we we hold onto the sender to send the (possibly multiple) reponses
  // asynchronously. Use CancelAllBlobTransfers to stop usage of the |sender|.
  // We close the file handles once we're done writing to them.
  void OnMemoryRequest(
      const std::string& uuid,
      const std::vector<storage::BlobItemBytesRequest>& requests,
      std::vector<base::SharedMemoryHandle>* memory_handles,
      const std::vector<IPC::PlatformFileForTransit>& file_handles,
      base::TaskRunner* file_runner,
      IPC::Sender* sender);

  void OnBlobFinalStatus(const std::string& uuid, storage::BlobStatus code);

  bool IsTransporting(const std::string& uuid) {
    return blob_storage_.find(uuid) != blob_storage_.end();
  }

  // Invalidates all asynchronously running memory request handlers and clears
  // the internal state. If our map wasn't previously empty, then we call
  // ChildProcess::ReleaseProcess to release our previous reference.
  void CancelAllBlobTransfers();

 private:
  friend struct base::DefaultLazyInstanceTraits<BlobTransportController>;
  friend class BlobTransportControllerTest;
  FRIEND_TEST_ALL_PREFIXES(BlobTransportControllerTest, Descriptions);
  FRIEND_TEST_ALL_PREFIXES(BlobTransportControllerTest, Responses);
  FRIEND_TEST_ALL_PREFIXES(BlobTransportControllerTest, SharedMemory);
  FRIEND_TEST_ALL_PREFIXES(BlobTransportControllerTest, Disk);
  FRIEND_TEST_ALL_PREFIXES(BlobTransportControllerTest, ResponsesErrors);

  enum class ResponsesStatus {
    BLOB_NOT_FOUND,
    SHARED_MEMORY_MAP_FAILED,
    PENDING_IO,
    SUCCESS
  };

  static void GetDescriptions(BlobConsolidation* consolidation,
                              size_t max_data_population,
                              std::vector<storage::DataElement>* out);

  BlobTransportController();
  ~BlobTransportController();

  void OnFileWriteComplete(
      IPC::Sender* sender,
      const std::string& uuid,
      const base::Optional<std::vector<storage::BlobItemBytesResponse>>&
          result);

  void StoreBlobDataForRequests(
      const std::string& uuid,
      scoped_refptr<BlobConsolidation> consolidation,
      scoped_refptr<base::SingleThreadTaskRunner> main_runner);

  // Deletes the consolidation and calls ChildProcess::ReleaseProcess.
  void ReleaseBlobConsolidation(const std::string& uuid);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;
  std::map<std::string, scoped_refptr<BlobConsolidation>> blob_storage_;
  base::WeakPtrFactory<BlobTransportController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobTransportController);
};

}  // namespace content
#endif  // CONTENT_CHILD_BLOB_STORAGE_BLOB_TRANSPORT_CONTROLLER_H_
