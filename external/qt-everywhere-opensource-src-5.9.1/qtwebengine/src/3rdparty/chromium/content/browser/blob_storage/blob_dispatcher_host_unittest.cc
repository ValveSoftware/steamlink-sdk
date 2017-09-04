// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/blob_dispatcher_host.h"

#include <memory>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/memory/shared_memory.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/common/fileapi/webblob_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_file_system_context.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_test_sink.h"
#include "ipc/message_filter.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/blob_storage/blob_item_bytes_response.h"
#include "storage/common/data_element.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::BlobDataBuilder;
using storage::BlobDataHandle;
using storage::BlobItemBytesRequest;
using storage::BlobItemBytesResponse;
using storage::BlobStatus;
using storage::BlobStorageContext;
using storage::DataElement;
using RequestMemoryCallback = storage::BlobTransportHost::RequestMemoryCallback;

namespace content {
namespace {

const char kContentType[] = "text/plain";
const char kContentDisposition[] = "content_disposition";
const char kData[] = "data!!";
const size_t kDataSize = 6;

const size_t kTestBlobStorageIPCThresholdBytes = 20;
const size_t kTestBlobStorageMaxSharedMemoryBytes = 50;
const size_t kTestBlobStorageMaxBlobMemorySize = 400;
const uint64_t kTestBlobStorageMaxDiskSpace = 1000;
const uint64_t kTestBlobStorageMinFileSizeBytes = 10;
const uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

void ConstructionCompletePopulator(bool* success_pointer,
                                   BlobStatus* reason_pointer,
                                   BlobStatus reason) {
  *reason_pointer = reason;
  *success_pointer = reason == BlobStatus::DONE;
}

// TODO(dmurph): Create file test that verifies security policy.
class TestableBlobDispatcherHost : public BlobDispatcherHost {
 public:
  TestableBlobDispatcherHost(ChromeBlobStorageContext* blob_storage_context,
                             storage::FileSystemContext* file_system_context,
                             IPC::TestSink* sink)
      : BlobDispatcherHost(0 /* process_id */,
                           make_scoped_refptr(blob_storage_context),
                           make_scoped_refptr(file_system_context)),
        sink_(sink) {}

  bool Send(IPC::Message* message) override { return sink_->Send(message); }

  void ShutdownForBadMessage() override { shutdown_for_bad_message_ = true; }

  bool shutdown_for_bad_message_ = false;

 protected:
  ~TestableBlobDispatcherHost() override {}

 private:
  friend class base::RefCountedThreadSafe<TestableBlobDispatcherHost>;

  IPC::TestSink* sink_;
};

}  // namespace

class BlobDispatcherHostTest : public testing::Test {
 protected:
  BlobDispatcherHostTest()
      : chrome_blob_storage_context_(
            ChromeBlobStorageContext::GetFor(&browser_context_)) {
    file_system_context_ =
        CreateFileSystemContextForTesting(NULL, base::FilePath());
    host_ = new TestableBlobDispatcherHost(chrome_blob_storage_context_,
                                           file_system_context_.get(), &sink_);
  }
  ~BlobDispatcherHostTest() override {}

  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (!command_line->HasSwitch(switches::kDisableKillAfterBadIPC)) {
      command_line->AppendSwitch(switches::kDisableKillAfterBadIPC);
    }
    // We run the run loop to initialize the chrome blob storage context.
    base::RunLoop().RunUntilIdle();
    context_ = chrome_blob_storage_context_->context();
    ASSERT_TRUE(context_);

    storage::BlobStorageLimits limits;
    limits.max_ipc_memory_size = kTestBlobStorageIPCThresholdBytes;
    limits.max_shared_memory_size = kTestBlobStorageMaxSharedMemoryBytes;
    limits.max_blob_in_memory_space = kTestBlobStorageMaxBlobMemorySize;
    limits.max_blob_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.min_page_file_size = kTestBlobStorageMinFileSizeBytes;
    limits.max_file_size = kTestBlobStorageMaxFileSizeBytes;
    context_->mutable_memory_controller()->set_limits_for_testing(limits);
  }

  void ExpectBlobNotExist(const std::string& id) {
    EXPECT_FALSE(context_->registry().HasEntry(id));
    EXPECT_FALSE(host_->IsInUseInHost(id));
    EXPECT_FALSE(IsBeingBuiltInHost(id));
  }

  void AsyncShortcutBlobTransfer(const std::string& id) {
    sink_.ClearMessages();
    ExpectBlobNotExist(id);
    DataElement element;
    element.SetToBytes(kData, kDataSize);
    std::vector<DataElement> elements = {element};
    host_->OnRegisterBlob(id, std::string(kContentType),
                          std::string(kContentDisposition), elements);
    EXPECT_FALSE(host_->shutdown_for_bad_message_);
    ExpectDone(id);
    sink_.ClearMessages();
  }

  void AsyncBlobTransfer(const std::string& id) {
    sink_.ClearMessages();
    ExpectBlobNotExist(id);
    DataElement element;
    element.SetToBytesDescription(kDataSize);
    std::vector<DataElement> elements = {element};
    host_->OnRegisterBlob(id, std::string(kContentType),
                          std::string(kContentDisposition), elements);
    EXPECT_FALSE(host_->shutdown_for_bad_message_);

    // Expect our request.
    std::vector<BlobItemBytesRequest> expected_requests = {
        BlobItemBytesRequest::CreateIPCRequest(0, 0, 0, kDataSize)};
    ExpectRequest(id, expected_requests);
    sink_.ClearMessages();

    // Send results;
    BlobItemBytesResponse response(0);
    std::memcpy(response.allocate_mutable_data(kDataSize), kData, kDataSize);
    std::vector<BlobItemBytesResponse> responses = {response};
    host_->OnMemoryItemResponse(id, responses);
    ExpectDone(id);
    sink_.ClearMessages();
  }

  void ExpectAndResetBadMessage() {
    EXPECT_TRUE(host_->shutdown_for_bad_message_);
    host_->shutdown_for_bad_message_ = false;
  }

  void ExpectHandleEqualsData(BlobDataHandle* handle,
                              const std::vector<DataElement>& data) {
    std::unique_ptr<storage::BlobDataSnapshot> snapshot =
        handle->CreateSnapshot();
    EXPECT_FALSE(handle->IsBeingBuilt());
    for (size_t i = 0; i < data.size(); i++) {
      const DataElement& expected = data[i];
      const DataElement& actual = snapshot->items()[i]->data_element();
      EXPECT_EQ(expected, actual);
    }
    EXPECT_EQ(data.size(), snapshot->items().size());
  }

  void ExpectRequest(
      const std::string& expected_uuid,
      const std::vector<BlobItemBytesRequest>& expected_requests) {
    EXPECT_FALSE(
        sink_.GetFirstMessageMatching(BlobStorageMsg_SendBlobStatus::ID));
    const IPC::Message* message =
        sink_.GetUniqueMessageMatching(BlobStorageMsg_RequestMemoryItem::ID);
    ASSERT_TRUE(message);
    std::tuple<std::string, std::vector<storage::BlobItemBytesRequest>,
               std::vector<base::SharedMemoryHandle>,
               std::vector<IPC::PlatformFileForTransit>>
        args;
    BlobStorageMsg_RequestMemoryItem::Read(message, &args);
    EXPECT_EQ(expected_uuid, std::get<0>(args));
    std::vector<BlobItemBytesRequest> requests = std::get<1>(args);
    ASSERT_EQ(requests.size(), expected_requests.size());
    for (size_t i = 0; i < expected_requests.size(); ++i) {
      EXPECT_EQ(expected_requests[i], requests[i]);
    }
  }

  void ExpectRequestWithSharedMemoryHandles(
      const std::string& expected_uuid,
      const std::vector<BlobItemBytesRequest>& expected_requests,
      std::vector<base::SharedMemoryHandle>* shared_memory_handles) {
    EXPECT_FALSE(
        sink_.GetFirstMessageMatching(BlobStorageMsg_SendBlobStatus::ID));
    const IPC::Message* message =
        sink_.GetUniqueMessageMatching(BlobStorageMsg_RequestMemoryItem::ID);
    ASSERT_TRUE(message);
    std::tuple<std::string, std::vector<storage::BlobItemBytesRequest>,
               std::vector<base::SharedMemoryHandle>,
               std::vector<IPC::PlatformFileForTransit>>
        args;
    BlobStorageMsg_RequestMemoryItem::Read(message, &args);
    EXPECT_EQ(expected_uuid, std::get<0>(args));
    std::vector<BlobItemBytesRequest> requests = std::get<1>(args);
    ASSERT_EQ(requests.size(), expected_requests.size());
    for (size_t i = 0; i < expected_requests.size(); ++i) {
      EXPECT_EQ(expected_requests[i], requests[i]);
    }
    *shared_memory_handles = std::move(std::get<2>(args));
  }

  void ExpectCancel(const std::string& expected_uuid, BlobStatus code) {
    EXPECT_FALSE(
        sink_.GetFirstMessageMatching(BlobStorageMsg_RequestMemoryItem::ID));
    const IPC::Message* message =
        sink_.GetUniqueMessageMatching(BlobStorageMsg_SendBlobStatus::ID);
    ASSERT_TRUE(message);
    std::tuple<std::string, BlobStatus> args;
    BlobStorageMsg_SendBlobStatus::Read(message, &args);
    EXPECT_EQ(expected_uuid, std::get<0>(args));
    EXPECT_EQ(code, std::get<1>(args));
  }

  void ExpectDone(const std::string& expected_uuid) {
    EXPECT_FALSE(
        sink_.GetFirstMessageMatching(BlobStorageMsg_RequestMemoryItem::ID));
    const IPC::Message* message =
        sink_.GetUniqueMessageMatching(BlobStorageMsg_SendBlobStatus::ID);
    ASSERT_TRUE(message);
    std::tuple<std::string, BlobStatus> args;
    BlobStorageMsg_SendBlobStatus::Read(message, &args);
    EXPECT_EQ(expected_uuid, std::get<0>(args));
    EXPECT_EQ(BlobStatus::DONE, std::get<1>(args));
  }

  bool IsBeingBuiltInHost(const std::string& uuid) {
    return host_->transport_host_.IsBeingBuilt(uuid);
  }

  bool IsBeingBuiltInContext(const std::string& uuid) {
    return BlobStatusIsPending(context_->GetBlobStatus(uuid));
  }

  IPC::TestSink sink_;
  TestBrowserThreadBundle browser_thread_bundle_;
  TestBrowserContext browser_context_;
  ChromeBlobStorageContext* chrome_blob_storage_context_;
  BlobStorageContext* context_ = nullptr;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<TestableBlobDispatcherHost> host_;
};

TEST_F(BlobDispatcherHostTest, EmptyUUIDs) {
  host_->OnRegisterBlob("", "", "", std::vector<DataElement>());
  ExpectAndResetBadMessage();
  host_->OnMemoryItemResponse("", std::vector<BlobItemBytesResponse>());
  ExpectAndResetBadMessage();
  host_->OnCancelBuildingBlob("",
                              BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
  ExpectAndResetBadMessage();
}

TEST_F(BlobDispatcherHostTest, Shortcut) {
  const std::string kId = "uuid1";
  AsyncShortcutBlobTransfer(kId);
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(handle);

  DataElement expected;
  expected.SetToBytes(kData, kDataSize);
  std::vector<DataElement> elements = {expected};
  ExpectHandleEqualsData(handle.get(), elements);
}

TEST_F(BlobDispatcherHostTest, RegularTransfer) {
  const std::string kId = "uuid1";
  AsyncBlobTransfer(kId);
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(handle);

  DataElement expected;
  expected.SetToBytes(kData, kDataSize);
  std::vector<DataElement> elements = {expected};
  ExpectHandleEqualsData(handle.get(), elements);
}

TEST_F(BlobDispatcherHostTest, MultipleTransfers) {
  const std::string kId = "uuid";
  const int kNumIters = 10;
  sink_.ClearMessages();
  for (int i = 0; i < kNumIters; i++) {
    std::string id = kId;
    id += ('0' + i);
    ExpectBlobNotExist(id);
    DataElement element;
    element.SetToBytesDescription(kDataSize);
    std::vector<DataElement> elements = {element};
    host_->OnRegisterBlob(id, std::string(kContentType),
                          std::string(kContentDisposition), elements);
    EXPECT_FALSE(host_->shutdown_for_bad_message_);
    // Expect our request.
    std::vector<BlobItemBytesRequest> expected_requests = {
        BlobItemBytesRequest::CreateIPCRequest(0, 0, 0, kDataSize)};
    ExpectRequest(id, expected_requests);
    sink_.ClearMessages();
  }
  for (int i = 0; i < kNumIters; i++) {
    std::string id = kId;
    id += ('0' + i);
    // Send results;
    BlobItemBytesResponse response(0);
    std::memcpy(response.allocate_mutable_data(kDataSize), kData, kDataSize);
    std::vector<BlobItemBytesResponse> responses = {response};
    host_->OnMemoryItemResponse(id, responses);
    ExpectDone(id);
    sink_.ClearMessages();
  }
}

TEST_F(BlobDispatcherHostTest, SharedMemoryTransfer) {
  const std::string kId = "uuid1";
  const size_t kLargeSize = kTestBlobStorageMaxSharedMemoryBytes * 2;
  std::vector<base::SharedMemoryHandle> shared_memory_handles;

  ExpectBlobNotExist(kId);
  DataElement element;
  element.SetToBytesDescription(kLargeSize);
  std::vector<DataElement> elements = {element};
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);

  // Grab the handle.
  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromUUID(kId);
  bool built = false;
  BlobStatus error_code = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  blob_data_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));
  EXPECT_FALSE(built);

  EXPECT_FALSE(host_->shutdown_for_bad_message_);

  // Expect our first request.
  std::vector<BlobItemBytesRequest> expected_requests = {
      BlobItemBytesRequest::CreateSharedMemoryRequest(
          0 /* request_number */, 0 /* renderer_item_index */,
          0 /* renderer_item_offset */,
          static_cast<uint64_t>(
              kTestBlobStorageMaxSharedMemoryBytes) /* size */,
          0 /* handle_index */, 0 /* handle_offset */)};
  ExpectRequestWithSharedMemoryHandles(kId, expected_requests,
                                       &shared_memory_handles);
  sink_.ClearMessages();

  // Populate the shared memory.
  EXPECT_EQ(1u, shared_memory_handles.size());
  EXPECT_TRUE(base::SharedMemory::IsHandleValid(shared_memory_handles[0]));
  {
    base::SharedMemory memory(
        base::SharedMemory::DuplicateHandle(shared_memory_handles[0]), false);
    memory.Map(kTestBlobStorageMaxSharedMemoryBytes);
    std::memset(memory.memory(), 'X', kTestBlobStorageMaxSharedMemoryBytes);
    memory.Close();
  }

  // Send the confirmation.
  std::vector<BlobItemBytesResponse> responses = {BlobItemBytesResponse(0)};
  host_->OnMemoryItemResponse(kId, responses);

  // Expect our second request.
  expected_requests = {BlobItemBytesRequest::CreateSharedMemoryRequest(
      1 /* request_number */, 0 /* renderer_item_index */,
      static_cast<uint64_t>(
          kTestBlobStorageMaxSharedMemoryBytes) /* renderer_item_offset */,
      static_cast<uint64_t>(kTestBlobStorageMaxSharedMemoryBytes) /* size */,
      0 /* handle_index */, 0 /* handle_offset */)};
  ExpectRequestWithSharedMemoryHandles(kId, expected_requests,
                                       &shared_memory_handles);
  sink_.ClearMessages();

  // Populate the shared memory.
  EXPECT_EQ(1u, shared_memory_handles.size());
  EXPECT_TRUE(base::SharedMemory::IsHandleValid(shared_memory_handles[0]));
  {
    base::SharedMemory memory(
        base::SharedMemory::DuplicateHandle(shared_memory_handles[0]), false);
    memory.Map(kTestBlobStorageMaxSharedMemoryBytes);
    std::memset(memory.memory(), 'Z', kTestBlobStorageMaxSharedMemoryBytes);
    memory.Close();
  }
  // Send the confirmation.
  responses = {BlobItemBytesResponse(1)};
  host_->OnMemoryItemResponse(kId, responses);

  ExpectDone(kId);
  sink_.ClearMessages();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(built) << "Error code: " << static_cast<int>(error_code);
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(handle);

  DataElement expected;
  expected.SetToAllocatedBytes(kLargeSize / 2);
  std::memset(expected.mutable_bytes(), 'X', kLargeSize / 2);
  elements = {expected};
  std::memset(expected.mutable_bytes(), 'Z', kLargeSize / 2);
  elements.push_back(expected);
  ExpectHandleEqualsData(handle.get(), elements);
}

TEST_F(BlobDispatcherHostTest, OnCancelBuildingBlob) {
  const std::string kId("id");
  // We ignore blobs that are unknown, as it could have been cancelled earlier
  // and the renderer didn't know about it yet.
  host_->OnCancelBuildingBlob(kId,
                              BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);

  // Start building blob.
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element};
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);
  // It should have requested memory here.
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  sink_.ClearMessages();

  // Cancel in middle of construction.
  host_->OnCancelBuildingBlob(kId, BlobStatus::ERR_OUT_OF_MEMORY);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(host_->IsInUseInHost(kId));
  EXPECT_FALSE(IsBeingBuiltInHost(kId));
  // Check that's it's broken.
  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(handle->IsBroken());
  handle.reset();
  base::RunLoop().RunUntilIdle();

  // Get rid of it in the host.
  host_->OnDecrementBlobRefCount(kId);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  ExpectBlobNotExist(kId);

  // Create blob again to verify we don't have any old construction state lying
  // around.
  AsyncBlobTransfer(kId);

  // Check data.
  handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(handle);
  DataElement expected;
  expected.SetToBytes(kData, kDataSize);
  std::vector<DataElement> expecteds = {expected};
  ExpectHandleEqualsData(handle.get(), expecteds);

  // Verify we can't cancel after the fact.
  host_->OnCancelBuildingBlob(kId, BlobStatus::ERR_OUT_OF_MEMORY);
  ExpectAndResetBadMessage();
}

TEST_F(BlobDispatcherHostTest, BlobDataWithHostDeletion) {
  // Build up a basic blob.
  const std::string kId("id");
  AsyncShortcutBlobTransfer(kId);
  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(handle);

  // Kill the host.
  host_ = nullptr;
  base::RunLoop().RunUntilIdle();
  // Should still be there due to the handle.
  std::unique_ptr<BlobDataHandle> another_handle =
      context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(another_handle);

  // Should disappear after dropping both handles.
  handle.reset();
  another_handle.reset();
  base::RunLoop().RunUntilIdle();

  handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_FALSE(handle);
}

TEST_F(BlobDispatcherHostTest, BlobReferenceWhileConstructing) {
  const std::string kId("id");

  // Start building blob.
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element};
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);

  // Grab the handle.
  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle);
  EXPECT_TRUE(blob_data_handle->IsBeingBuilt());
  bool built = false;
  BlobStatus error_code = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  blob_data_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));

  sink_.ClearMessages();

  // Send data.
  BlobItemBytesResponse response(0);
  std::memcpy(response.allocate_mutable_data(kDataSize), kData, kDataSize);
  std::vector<BlobItemBytesResponse> responses = {response};
  sink_.ClearMessages();
  host_->OnMemoryItemResponse(kId, responses);

  ExpectDone(kId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(built) << "Error code: " << static_cast<int>(error_code);
}

TEST_F(BlobDispatcherHostTest, BlobReferenceWhileConstructingCancelled) {
  const std::string kId("id");

  // Start building blob.
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element};
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);

  // Grab the handle.
  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle);
  EXPECT_TRUE(blob_data_handle->IsBeingBuilt());
  bool built = true;
  BlobStatus error_code = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  blob_data_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));

  // Cancel in middle of construction.
  host_->OnCancelBuildingBlob(kId, BlobStatus::ERR_OUT_OF_MEMORY);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(host_->IsInUseInHost(kId));
  EXPECT_FALSE(IsBeingBuiltInHost(kId));
  EXPECT_TRUE(blob_data_handle->IsBroken());
  EXPECT_FALSE(built);
  EXPECT_EQ(BlobStatus::ERR_OUT_OF_MEMORY, error_code);
  error_code = BlobStatus::ERR_OUT_OF_MEMORY;
  built = true;
  blob_data_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));
  EXPECT_FALSE(built);
  EXPECT_EQ(BlobStatus::ERR_OUT_OF_MEMORY, error_code);

  // Remove it.
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  host_->OnDecrementBlobRefCount(kId);
  ExpectBlobNotExist(kId);
}

TEST_F(BlobDispatcherHostTest, DecrementRefAfterOnStart) {
  const std::string kId("id");

  // Decrement the refcount while building, after we call OnStartBuildlingBlob.
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element};
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);

  std::vector<BlobItemBytesRequest> expected_requests = {
      BlobItemBytesRequest::CreateIPCRequest(0, 0, 0, kDataSize)};
  ExpectRequest(kId, expected_requests);
  sink_.ClearMessages();
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  host_->OnDecrementBlobRefCount(kId);
  EXPECT_FALSE(context_->registry().HasEntry(kId));
  EXPECT_FALSE(IsBeingBuiltInHost(kId));
  ExpectCancel(kId, BlobStatus::ERR_BLOB_DEREFERENCED_WHILE_BUILDING);
  sink_.ClearMessages();

  // Do the same, but this time grab a handle to keep it alive.
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  ExpectRequest(kId, expected_requests);
  sink_.ClearMessages();
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  // Grab the handle before decrementing.
  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromUUID(kId);
  host_->OnDecrementBlobRefCount(kId);
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(IsBeingBuiltInHost(kId));
  EXPECT_EQ(0u, sink_.message_count());

  // We finish the blob, and verify that we send 'Done' back to the renderer.
  BlobItemBytesResponse response(0);
  std::memcpy(response.allocate_mutable_data(kDataSize), kData, kDataSize);
  std::vector<BlobItemBytesResponse> responses = {response};
  host_->OnMemoryItemResponse(kId, responses);
  ExpectDone(kId);
  sink_.ClearMessages();
  // Check that it's still around.
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_FALSE(IsBeingBuiltInHost(kId));

  // Get rid of the handle, and verify it's gone.
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();
  // Check that it's no longer around.
  EXPECT_FALSE(context_->registry().HasEntry(kId));
  EXPECT_FALSE(IsBeingBuiltInHost(kId));
}

TEST_F(BlobDispatcherHostTest, DecrementRefAfterOnStartWithHandle) {
  const std::string kId("id");
  // Decrement the refcount while building, after we call
  // OnStartBuildlingBlob, except we have another handle.
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element};
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);

  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle->IsBeingBuilt());
  bool built = true;
  BlobStatus error_code = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  blob_data_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));

  // Check that we got the expected request.
  std::vector<BlobItemBytesRequest> expected_requests = {
      BlobItemBytesRequest::CreateIPCRequest(0, 0, 0, kDataSize)};
  ExpectRequest(kId, expected_requests);
  sink_.ClearMessages();
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(IsBeingBuiltInHost(kId));
  // Decrement, simulating where the ref goes out of scope in renderer.
  host_->OnDecrementBlobRefCount(kId);
  // We still have the blob as it's not done.
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(blob_data_handle->IsBeingBuilt());
  EXPECT_TRUE(IsBeingBuiltInHost(kId));
  // Cancel to clean up.
  host_->OnCancelBuildingBlob(kId, BlobStatus::ERR_OUT_OF_MEMORY);
  // Run loop to propagate the handle decrement in the host.
  base::RunLoop().RunUntilIdle();
  // We still have the entry because of our earlier handle.
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_FALSE(IsBeingBuiltInHost(kId));
  EXPECT_FALSE(built);
  EXPECT_EQ(BlobStatus::ERR_OUT_OF_MEMORY, error_code);
  sink_.ClearMessages();

  // Should disappear after dropping the handle.
  EXPECT_TRUE(blob_data_handle->IsBroken());
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->registry().HasEntry(kId));
}

TEST_F(BlobDispatcherHostTest, HostDisconnectAfterOnStart) {
  const std::string kId("id");

  // Host deleted after OnStartBuilding.
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element};
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);

  std::vector<BlobItemBytesRequest> expected_requests = {
      BlobItemBytesRequest::CreateIPCRequest(0, 0, 0, kDataSize)};
  ExpectRequest(kId, expected_requests);
  sink_.ClearMessages();
  host_ = nullptr;
  // We need to run the message loop because of the handle in the async builder.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->registry().HasEntry(kId));
}

TEST_F(BlobDispatcherHostTest, HostDisconnectAfterOnMemoryResponse) {
  const std::string kId("id");

  // Host deleted after OnMemoryItemResponse.
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element, element};
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);

  // Create list of two items.
  std::vector<BlobItemBytesRequest> expected_requests = {
      BlobItemBytesRequest::CreateIPCRequest(0, 0, 0, kDataSize),
      BlobItemBytesRequest::CreateIPCRequest(1, 1, 0, kDataSize)};
  ExpectRequest(kId, expected_requests);
  sink_.ClearMessages();

  // Send just one response so the blob isn't 'done' yet.
  BlobItemBytesResponse response(0);
  std::memcpy(response.allocate_mutable_data(kDataSize), kData, kDataSize);
  std::vector<BlobItemBytesResponse> responses = {response};
  host_->OnMemoryItemResponse(kId, responses);
  EXPECT_EQ(0u, sink_.message_count());

  host_ = nullptr;
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->registry().HasEntry(kId));
}

TEST_F(BlobDispatcherHostTest, CreateBlobWithBrokenReference) {
  const std::string kBrokenId("id1");
  const std::string kReferencingId("id2");

  // First, let's test a circular reference.
  const std::string kCircularId("id1");
  DataElement element;
  element.SetToBlob(kCircularId);
  std::vector<DataElement> elements = {element};
  host_->OnRegisterBlob(kCircularId, std::string(kContentType),
                        std::string(kContentDisposition), elements);
  ExpectAndResetBadMessage();
  // Remove the blob.
  host_->OnDecrementBlobRefCount(kCircularId);
  sink_.ClearMessages();

  // Next, test a blob that references a broken blob.
  element.SetToBytesDescription(kDataSize);
  elements = {element};
  host_->OnRegisterBlob(kBrokenId, std::string(kContentType),
                        std::string(kContentDisposition), elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  sink_.ClearMessages();
  host_->OnCancelBuildingBlob(kBrokenId, BlobStatus::ERR_OUT_OF_MEMORY);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  sink_.ClearMessages();
  EXPECT_TRUE(context_->GetBlobDataFromUUID(kBrokenId)->IsBroken());

  // Create referencing blob. We should be broken right away, but also ignore
  // the subsequent OnStart message.
  element.SetToBytesDescription(kDataSize);
  elements = {element};
  element.SetToBlob(kBrokenId);
  elements.push_back(element);
  host_->OnRegisterBlob(kReferencingId, std::string(kContentType),
                        std::string(kContentDisposition), elements);
  EXPECT_TRUE(context_->GetBlobDataFromUUID(kReferencingId)->IsBroken());
  EXPECT_FALSE(IsBeingBuiltInHost(kReferencingId));
  EXPECT_TRUE(context_->registry().HasEntry(kReferencingId));
  ExpectCancel(kReferencingId, BlobStatus::ERR_REFERENCED_BLOB_BROKEN);
  sink_.ClearMessages();
}

TEST_F(BlobDispatcherHostTest, DeferenceBlobOnDifferentHost) {
  const std::string kId("id");
  // Data elements for our transfer & checking messages.
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element};
  std::vector<BlobItemBytesRequest> expected_requests = {
      BlobItemBytesRequest::CreateIPCRequest(0, 0, 0, kDataSize)};
  BlobItemBytesResponse response(0);
  std::memcpy(response.allocate_mutable_data(kDataSize), kData, kDataSize);
  std::vector<BlobItemBytesResponse> responses = {response};

  scoped_refptr<TestableBlobDispatcherHost> host2(
      new TestableBlobDispatcherHost(chrome_blob_storage_context_,
                                     file_system_context_.get(), &sink_));

  // Delete host with another host having a referencing, then dereference on
  // second host. Verify we're still building it on first host, and then
  // verify that a building message from the renderer will kill it.

  // Test OnStartBuilding after double dereference.
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  ExpectRequest(kId, expected_requests);
  sink_.ClearMessages();
  host2->OnIncrementBlobRefCount(kId);
  host_->OnDecrementBlobRefCount(kId);
  EXPECT_FALSE(host_->IsInUseInHost(kId));
  host2->OnDecrementBlobRefCount(kId);
  // Blob is gone as we've been decremented from both hosts.
  EXPECT_FALSE(context_->registry().HasEntry(kId));
  // Send the memory. When the host sees the entry doesn't exist, it should
  // cancel and clean up.
  EXPECT_TRUE(host_->transport_host_.IsBeingBuilt(kId));
  host_->OnMemoryItemResponse(kId, responses);
  // We should be cleaned up.
  EXPECT_FALSE(context_->registry().HasEntry(kId));
  EXPECT_FALSE(host_->transport_host_.IsBeingBuilt(kId));
  ExpectCancel(kId, BlobStatus::ERR_BLOB_DEREFERENCED_WHILE_BUILDING);
  sink_.ClearMessages();

  // Same, but now for OnCancel.
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);
  host2->OnIncrementBlobRefCount(kId);
  host_->OnDecrementBlobRefCount(kId);
  EXPECT_FALSE(host_->IsInUseInHost(kId));
  ExpectRequest(kId, expected_requests);
  sink_.ClearMessages();

  host2->OnDecrementBlobRefCount(kId);
  // Blob is gone as we've been decremented from both hosts.
  EXPECT_FALSE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(host_->transport_host_.IsBeingBuilt(kId));
  host_->OnCancelBuildingBlob(kId, BlobStatus::ERR_OUT_OF_MEMORY);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  EXPECT_FALSE(context_->registry().HasEntry(kId));
  // We should be cleaned up.
  EXPECT_FALSE(host_->transport_host_.IsBeingBuilt(kId));
  ExpectCancel(kId, BlobStatus::ERR_BLOB_DEREFERENCED_WHILE_BUILDING);
}

TEST_F(BlobDispatcherHostTest, BuildingReferenceChain) {
  const std::string kId("id");
  const std::string kSameHostReferencingId("id2");
  const std::string kDifferentHostReferencingId("id3");
  // Data elements for our transfer & checking messages.
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  DataElement referencing_element;
  referencing_element.SetToBlob(kId);
  std::vector<DataElement> elements = {element};
  std::vector<DataElement> referencing_elements = {referencing_element};
  std::vector<BlobItemBytesRequest> expected_requests = {
      BlobItemBytesRequest::CreateIPCRequest(0, 0, 0, kDataSize)};
  BlobItemBytesResponse response(0);
  std::memcpy(response.allocate_mutable_data(kDataSize), kData, kDataSize);
  std::vector<BlobItemBytesResponse> responses = {response};

  scoped_refptr<TestableBlobDispatcherHost> host2(
      new TestableBlobDispatcherHost(chrome_blob_storage_context_,
                                     file_system_context_.get(), &sink_));

  // We want to have a blob referencing another blob that is building, both on
  // the same host and a different host. We should successfully build all blobs
  // after the referenced blob is finished.

  // First we start the referenced blob.
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);
  ExpectRequest(kId, expected_requests);
  sink_.ClearMessages();
  EXPECT_TRUE(host_->IsInUseInHost(kId));
  EXPECT_TRUE(IsBeingBuiltInContext(kId));

  // Next we start the referencing blobs in both the same and different host.
  host_->OnRegisterBlob(kSameHostReferencingId, std::string(kContentType),
                        std::string(kContentDisposition), referencing_elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  ExpectDone(kSameHostReferencingId);
  sink_.ClearMessages();
  EXPECT_FALSE(
      context_->GetBlobDataFromUUID(kSameHostReferencingId)->IsBroken());
  EXPECT_FALSE(host_->transport_host_.IsBeingBuilt(kSameHostReferencingId));
  EXPECT_TRUE(IsBeingBuiltInContext(kSameHostReferencingId));

  // Now the other host.
  host2->OnRegisterBlob(kDifferentHostReferencingId, std::string(kContentType),
                        std::string(kContentDisposition), referencing_elements);
  EXPECT_FALSE(host2->shutdown_for_bad_message_);
  ExpectDone(kDifferentHostReferencingId);
  sink_.ClearMessages();
  EXPECT_FALSE(
      context_->GetBlobDataFromUUID(kDifferentHostReferencingId)->IsBroken());
  EXPECT_FALSE(
      host2->transport_host_.IsBeingBuilt(kDifferentHostReferencingId));
  EXPECT_TRUE(IsBeingBuiltInContext(kDifferentHostReferencingId));

  // Now we finish the first blob, and we expect all blobs to finish.
  host_->OnMemoryItemResponse(kId, responses);
  ExpectDone(kId);
  // We need to run the message loop to propagate the construction callbacks.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      host2->transport_host_.IsBeingBuilt(kDifferentHostReferencingId));
  EXPECT_FALSE(host_->transport_host_.IsBeingBuilt(kSameHostReferencingId));
  EXPECT_FALSE(
      context_->GetBlobDataFromUUID(kSameHostReferencingId)->IsBroken());
  EXPECT_FALSE(
      context_->GetBlobDataFromUUID(kDifferentHostReferencingId)->IsBroken());
  sink_.ClearMessages();

  // Finally check that our data is correct in the child elements.
  std::unique_ptr<BlobDataHandle> handle =
      context_->GetBlobDataFromUUID(kDifferentHostReferencingId);
  DataElement expected;
  expected.SetToBytes(kData, kDataSize);
  std::vector<DataElement> expecteds = {expected};
  ExpectHandleEqualsData(handle.get(), expecteds);
}

TEST_F(BlobDispatcherHostTest, BuildingReferenceChainWithCancel) {
  const std::string kId("id");
  const std::string kSameHostReferencingId("id2");
  const std::string kDifferentHostReferencingId("id3");
  // Data elements for our transfer & checking messages.
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  DataElement referencing_element;
  referencing_element.SetToBlob(kId);
  std::vector<DataElement> elements = {element};
  std::vector<DataElement> referencing_elements = {referencing_element};
  std::vector<BlobItemBytesRequest> expected_requests = {
      BlobItemBytesRequest::CreateIPCRequest(0, 0, 0, kDataSize)};

  scoped_refptr<TestableBlobDispatcherHost> host2(
      new TestableBlobDispatcherHost(chrome_blob_storage_context_,
                                     file_system_context_.get(), &sink_));

  // We want to have a blob referencing another blob that is building, both on
  // the same host and a different host. We should successfully build all blobs
  // after the referenced blob is finished.

  // First we start the referenced blob.
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);
  ExpectRequest(kId, expected_requests);
  sink_.ClearMessages();
  EXPECT_TRUE(host_->IsInUseInHost(kId));
  EXPECT_TRUE(IsBeingBuiltInContext(kId));

  // Next we start the referencing blobs in both the same and different host.
  host_->OnRegisterBlob(kSameHostReferencingId, std::string(kContentType),
                        std::string(kContentDisposition), referencing_elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  ExpectDone(kSameHostReferencingId);
  sink_.ClearMessages();
  EXPECT_FALSE(
      context_->GetBlobDataFromUUID(kSameHostReferencingId)->IsBroken());
  EXPECT_FALSE(host_->transport_host_.IsBeingBuilt(kSameHostReferencingId));
  EXPECT_TRUE(IsBeingBuiltInContext(kSameHostReferencingId));

  // Now the other host.
  host2->OnRegisterBlob(kDifferentHostReferencingId, std::string(kContentType),
                        std::string(kContentDisposition), referencing_elements);
  EXPECT_FALSE(host2->shutdown_for_bad_message_);
  ExpectDone(kDifferentHostReferencingId);
  sink_.ClearMessages();
  EXPECT_FALSE(
      context_->GetBlobDataFromUUID(kDifferentHostReferencingId)->IsBroken());
  EXPECT_FALSE(
      host2->transport_host_.IsBeingBuilt(kDifferentHostReferencingId));
  EXPECT_TRUE(IsBeingBuiltInContext(kDifferentHostReferencingId));

  bool built = false;
  BlobStatus error_code = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  context_->GetBlobDataFromUUID(kDifferentHostReferencingId)
      ->RunOnConstructionComplete(
          base::Bind(&ConstructionCompletePopulator, &built, &error_code));

  // Now we cancel the first blob, and we expect all blobs to cancel.
  host_->OnCancelBuildingBlob(kId, BlobStatus::ERR_OUT_OF_MEMORY);
  // We need to run the message loop to propagate the construction callbacks.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      host2->transport_host_.IsBeingBuilt(kDifferentHostReferencingId));
  EXPECT_FALSE(host_->transport_host_.IsBeingBuilt(kSameHostReferencingId));
  EXPECT_TRUE(
      context_->GetBlobDataFromUUID(kSameHostReferencingId)->IsBroken());
  EXPECT_TRUE(
      context_->GetBlobDataFromUUID(kDifferentHostReferencingId)->IsBroken());
  EXPECT_FALSE(built);
  EXPECT_EQ(BlobStatus::ERR_REFERENCED_BLOB_BROKEN, error_code);
  sink_.ClearMessages();
}

TEST_F(BlobDispatcherHostTest, BuildingReferenceChainWithSourceDeath) {
  const std::string kId("id");
  const std::string kSameHostReferencingId("id2");
  const std::string kDifferentHostReferencingId("id3");
  // Data elements for our transfer & checking messages.
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  DataElement referencing_element;
  referencing_element.SetToBlob(kId);
  std::vector<DataElement> elements = {element};
  std::vector<DataElement> referencing_elements = {referencing_element};
  std::vector<BlobItemBytesRequest> expected_requests = {
      BlobItemBytesRequest::CreateIPCRequest(0, 0, 0, kDataSize)};
  BlobItemBytesResponse response(0);
  std::memcpy(response.allocate_mutable_data(kDataSize), kData, kDataSize);
  std::vector<BlobItemBytesResponse> responses = {response};

  scoped_refptr<TestableBlobDispatcherHost> host2(
      new TestableBlobDispatcherHost(chrome_blob_storage_context_,
                                     file_system_context_.get(), &sink_));

  // We want to have a blob referencing another blob that is building, both on
  // the same host and a different host. We should successfully build all blobs
  // after the referenced blob is finished.

  // First we start the referenced blob.
  host_->OnRegisterBlob(kId, std::string(kContentType),
                        std::string(kContentDisposition), elements);
  ExpectRequest(kId, expected_requests);
  sink_.ClearMessages();
  EXPECT_TRUE(host_->IsInUseInHost(kId));
  EXPECT_TRUE(IsBeingBuiltInContext(kId));

  // Next we start the referencing blobs in both the same and different host.
  host_->OnRegisterBlob(kSameHostReferencingId, std::string(kContentType),
                        std::string(kContentDisposition), referencing_elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  ExpectDone(kSameHostReferencingId);
  sink_.ClearMessages();

  // Now the other host.
  host2->OnRegisterBlob(kDifferentHostReferencingId, std::string(kContentType),
                        std::string(kContentDisposition), referencing_elements);
  EXPECT_FALSE(host2->shutdown_for_bad_message_);
  ExpectDone(kDifferentHostReferencingId);
  sink_.ClearMessages();

  // Grab handles & add listeners.
  bool built = true;
  BlobStatus error_code = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_->GetBlobDataFromUUID(kId);
  blob_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));

  bool same_host_built = true;
  BlobStatus same_host_error_code =
      BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  std::unique_ptr<BlobDataHandle> same_host_blob_handle =
      context_->GetBlobDataFromUUID(kSameHostReferencingId);
  same_host_blob_handle->RunOnConstructionComplete(base::Bind(
      &ConstructionCompletePopulator, &same_host_built, &same_host_error_code));

  bool other_host_built = true;
  BlobStatus other_host_error_code =
      BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  std::unique_ptr<BlobDataHandle> other_host_blob_handle =
      context_->GetBlobDataFromUUID(kDifferentHostReferencingId);
  other_host_blob_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &other_host_built,
                 &other_host_error_code));

  // Now we kill the host.
  host_ = nullptr;
  // We need to run the message loop to propagate the construction callbacks.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      host2->transport_host_.IsBeingBuilt(kDifferentHostReferencingId));
  EXPECT_TRUE(
      context_->GetBlobDataFromUUID(kSameHostReferencingId)->IsBroken());
  EXPECT_TRUE(
      context_->GetBlobDataFromUUID(kDifferentHostReferencingId)->IsBroken());

  // Check our callbacks
  EXPECT_FALSE(built);
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, error_code);
  EXPECT_FALSE(same_host_built);
  EXPECT_EQ(BlobStatus::ERR_REFERENCED_BLOB_BROKEN, same_host_error_code);
  EXPECT_FALSE(other_host_built);
  EXPECT_EQ(BlobStatus::ERR_REFERENCED_BLOB_BROKEN, other_host_error_code);

  sink_.ClearMessages();
}

}  // namespace content
