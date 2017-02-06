// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLOB_STORAGE_BLOB_MESSAGE_FILTER_H_
#define CONTENT_CHILD_BLOB_STORAGE_BLOB_MESSAGE_FILTER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/message_filter.h"
#include "storage/common/blob_storage/blob_storage_constants.h"

namespace base {
class TaskRunner;
}

namespace IPC {
class Sender;
class Message;
}

namespace storage {
struct BlobItemBytesRequest;
struct BlobItemBytesResponse;
}

namespace content {

// MessageFilter that handles Blob IPCs and delegates them to the
// BlobTransportController singleton. Created on the render thread, and then
// operated on by the IO thread.
class BlobMessageFilter : public IPC::MessageFilter {
 public:
  BlobMessageFilter(scoped_refptr<base::TaskRunner> file_runner);

  void OnChannelClosing() override;
  void OnFilterAdded(IPC::Sender* sender) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  bool GetSupportedMessageClasses(
      std::vector<uint32_t>* supported_message_classes) const override;

 protected:
  ~BlobMessageFilter() override;

 private:
  void OnRequestMemoryItem(
      const std::string& uuid,
      const std::vector<storage::BlobItemBytesRequest>& requests,
      std::vector<base::SharedMemoryHandle> memory_handles,
      const std::vector<IPC::PlatformFileForTransit>& file_handles);

  void OnCancelBuildingBlob(const std::string& uuid,
                            storage::IPCBlobCreationCancelCode code);

  void OnDoneBuildingBlob(const std::string& uuid);

  IPC::Sender* sender_;
  scoped_refptr<base::TaskRunner> file_runner_;

  DISALLOW_COPY_AND_ASSIGN(BlobMessageFilter);
};

}  // namespace content
#endif  // CONTENT_CHILD_BLOB_STORAGE_BLOB_MESSAGE_FILTER_H_
