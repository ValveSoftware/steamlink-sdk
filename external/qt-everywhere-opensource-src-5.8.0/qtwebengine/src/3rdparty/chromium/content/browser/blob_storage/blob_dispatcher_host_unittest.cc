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
using storage::BlobStorageContext;
using storage::BlobTransportResult;
using storage::DataElement;
using storage::IPCBlobCreationCancelCode;
using RequestMemoryCallback =
    storage::BlobAsyncBuilderHost::RequestMemoryCallback;

namespace content {
namespace {

const char kContentType[] = "text/plain";
const char kContentDisposition[] = "content_disposition";
const char kData[] = "data!!";
const size_t kDataSize = 6;

const size_t kTestBlobStorageIPCThresholdBytes = 20;
const size_t kTestBlobStorageMaxSharedMemoryBytes = 50;
const uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

void ConstructionCompletePopulator(bool* succeeded_pointer,
                                   IPCBlobCreationCancelCode* reason_pointer,
                                   bool succeeded,
                                   IPCBlobCreationCancelCode reason) {
  *succeeded_pointer = succeeded;
  *reason_pointer = reason;
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
        sink_(sink) {
    this->SetMemoryConstantsForTesting(kTestBlobStorageIPCThresholdBytes,
                                       kTestBlobStorageMaxSharedMemoryBytes,
                                       kTestBlobStorageMaxFileSizeBytes);
  }

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
    DCHECK(context_);
  }

  void ExpectBlobNotExist(const std::string& id) {
    EXPECT_FALSE(context_->registry().HasEntry(id));
    EXPECT_FALSE(host_->IsInUseInHost(id));
    EXPECT_FALSE(IsBeingBuiltInHost(id));
  }

  void AsyncShortcutBlobTransfer(const std::string& id) {
    sink_.ClearMessages();
    ExpectBlobNotExist(id);
    host_->OnRegisterBlobUUID(id, std::string(kContentType),
                              std::string(kContentDisposition),
                              std::set<std::string>());
    EXPECT_FALSE(host_->shutdown_for_bad_message_);
    DataElement element;
    element.SetToBytes(kData, kDataSize);
    std::vector<DataElement> elements = {element};
    host_->OnStartBuildingBlob(id, elements);
    EXPECT_FALSE(host_->shutdown_for_bad_message_);
    ExpectDone(id);
    sink_.ClearMessages();
  }

  void AsyncBlobTransfer(const std::string& id) {
    sink_.ClearMessages();
    ExpectBlobNotExist(id);
    host_->OnRegisterBlobUUID(id, std::string(kContentType),
                              std::string(kContentDisposition),
                              std::set<std::string>());
    EXPECT_FALSE(host_->shutdown_for_bad_message_);
    DataElement element;
    element.SetToBytesDescription(kDataSize);
    std::vector<DataElement> elements = {element};
    host_->OnStartBuildingBlob(id, elements);
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
        sink_.GetFirstMessageMatching(BlobStorageMsg_DoneBuildingBlob::ID));
    EXPECT_FALSE(
        sink_.GetFirstMessageMatching(BlobStorageMsg_CancelBuildingBlob::ID));
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
        sink_.GetFirstMessageMatching(BlobStorageMsg_DoneBuildingBlob::ID));
    EXPECT_FALSE(
        sink_.GetFirstMessageMatching(BlobStorageMsg_CancelBuildingBlob::ID));
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

  void ExpectCancel(const std::string& expected_uuid,
                    IPCBlobCreationCancelCode code) {
    EXPECT_FALSE(
        sink_.GetFirstMessageMatching(BlobStorageMsg_RequestMemoryItem::ID));
    EXPECT_FALSE(
        sink_.GetFirstMessageMatching(BlobStorageMsg_DoneBuildingBlob::ID));
    const IPC::Message* message =
        sink_.GetUniqueMessageMatching(BlobStorageMsg_CancelBuildingBlob::ID);
    ASSERT_TRUE(message);
    std::tuple<std::string, IPCBlobCreationCancelCode> args;
    BlobStorageMsg_CancelBuildingBlob::Read(message, &args);
    EXPECT_EQ(expected_uuid, std::get<0>(args));
    EXPECT_EQ(code, std::get<1>(args));
  }

  void ExpectDone(const std::string& expected_uuid) {
    EXPECT_FALSE(
        sink_.GetFirstMessageMatching(BlobStorageMsg_RequestMemoryItem::ID));
    EXPECT_FALSE(
        sink_.GetFirstMessageMatching(BlobStorageMsg_CancelBuildingBlob::ID));
    const IPC::Message* message =
        sink_.GetUniqueMessageMatching(BlobStorageMsg_DoneBuildingBlob::ID);
    std::tuple<std::string> args;
    BlobStorageMsg_DoneBuildingBlob::Read(message, &args);
    EXPECT_EQ(expected_uuid, std::get<0>(args));
  }

  bool IsBeingBuiltInHost(const std::string& uuid) {
    return host_->async_builder_.IsBeingBuilt(uuid);
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
  host_->OnRegisterBlobUUID("", "", "", std::set<std::string>());
  ExpectAndResetBadMessage();
  host_->OnStartBuildingBlob("", std::vector<DataElement>());
  ExpectAndResetBadMessage();
  host_->OnMemoryItemResponse("", std::vector<BlobItemBytesResponse>());
  ExpectAndResetBadMessage();
  host_->OnCancelBuildingBlob("", IPCBlobCreationCancelCode::UNKNOWN);
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
  for (int i = 0; i < kNumIters; i++) {
    std::string id = kId;
    id += ('0' + i);
    ExpectBlobNotExist(id);
    host_->OnRegisterBlobUUID(id, std::string(kContentType),
                              std::string(kContentDisposition),
                              std::set<std::string>());
    EXPECT_FALSE(host_->shutdown_for_bad_message_);
  }
  sink_.ClearMessages();
  for (int i = 0; i < kNumIters; i++) {
    std::string id = kId;
    id += ('0' + i);
    DataElement element;
    element.SetToBytesDescription(kDataSize);
    std::vector<DataElement> elements = {element};
    host_->OnStartBuildingBlob(id, elements);
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
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());

  // Grab the handle.
  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromUUID(kId);
  bool built = false;
  IPCBlobCreationCancelCode error_code = IPCBlobCreationCancelCode::UNKNOWN;
  blob_data_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));
  EXPECT_FALSE(built);

  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  DataElement element;
  element.SetToBytesDescription(kLargeSize);
  std::vector<DataElement> elements = {element};
  host_->OnStartBuildingBlob(kId, elements);
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
  host_->OnCancelBuildingBlob(kId, IPCBlobCreationCancelCode::UNKNOWN);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);

  // Start building blob.
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element};
  host_->OnStartBuildingBlob(kId, elements);
  // It should have requested memory here.
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  sink_.ClearMessages();

  // Cancel in middle of construction.
  host_->OnCancelBuildingBlob(kId, IPCBlobCreationCancelCode::UNKNOWN);
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
  host_->OnCancelBuildingBlob(kId, IPCBlobCreationCancelCode::UNKNOWN);
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
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());

  // Grab the handle.
  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle);
  EXPECT_TRUE(blob_data_handle->IsBeingBuilt());
  bool built = false;
  IPCBlobCreationCancelCode error_code = IPCBlobCreationCancelCode::UNKNOWN;
  blob_data_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));

  // Continue building.
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element};
  host_->OnStartBuildingBlob(kId, elements);
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

TEST_F(BlobDispatcherHostTest, BlobReferenceWhileShortcutConstructing) {
  const std::string kId("id");

  // Start building blob.
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());

  // Grab the handle.
  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle);
  EXPECT_TRUE(blob_data_handle->IsBeingBuilt());
  bool built = false;
  IPCBlobCreationCancelCode error_code = IPCBlobCreationCancelCode::UNKNOWN;
  blob_data_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));

  // Continue building.
  DataElement element;
  element.SetToBytes(kData, kDataSize);
  std::vector<DataElement> elements = {element};
  host_->OnStartBuildingBlob(kId, elements);
  ExpectDone(kId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(built) << "Error code: " << static_cast<int>(error_code);
}

TEST_F(BlobDispatcherHostTest, BlobReferenceWhileConstructingCancelled) {
  const std::string kId("id");

  // Start building blob.
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());

  // Grab the handle.
  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle);
  EXPECT_TRUE(blob_data_handle->IsBeingBuilt());
  bool built = true;
  IPCBlobCreationCancelCode error_code = IPCBlobCreationCancelCode::UNKNOWN;
  blob_data_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));

  // Cancel in middle of construction.
  host_->OnCancelBuildingBlob(kId, IPCBlobCreationCancelCode::UNKNOWN);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(host_->IsInUseInHost(kId));
  EXPECT_FALSE(IsBeingBuiltInHost(kId));
  EXPECT_TRUE(blob_data_handle->IsBroken());
  EXPECT_FALSE(built);
  EXPECT_EQ(IPCBlobCreationCancelCode::UNKNOWN, error_code);
  error_code = IPCBlobCreationCancelCode::UNKNOWN;
  built = true;
  blob_data_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));
  EXPECT_FALSE(built);
  EXPECT_EQ(IPCBlobCreationCancelCode::UNKNOWN, error_code);

  // Remove it.
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  host_->OnDecrementBlobRefCount(kId);
  ExpectBlobNotExist(kId);
}

TEST_F(BlobDispatcherHostTest, DecrementRefAfterRegister) {
  const std::string kId("id");
  // Decrement the refcount while building (renderer blob gc'd before
  // construction was completed).
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  host_->OnDecrementBlobRefCount(kId);
  EXPECT_FALSE(context_->registry().HasEntry(kId));
  EXPECT_FALSE(IsBeingBuiltInHost(kId));
  ExpectCancel(kId,
               IPCBlobCreationCancelCode::BLOB_DEREFERENCED_WHILE_BUILDING);
  sink_.ClearMessages();

  // Do the same, but this time grab a handle before we decrement.
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());
  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromUUID(kId);
  host_->OnDecrementBlobRefCount(kId);
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(IsBeingBuiltInHost(kId));

  // Finish up the blob, and verify we got the done message.
  DataElement element;
  element.SetToBytes(kData, kDataSize);
  std::vector<DataElement> elements = {element};
  host_->OnStartBuildingBlob(kId, elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  ExpectDone(kId);
  sink_.ClearMessages();
  // Get rid of the handle, and verify it's gone.
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();
  // Check that it's no longer around.
  EXPECT_FALSE(context_->registry().HasEntry(kId));
  EXPECT_FALSE(IsBeingBuiltInHost(kId));
}

TEST_F(BlobDispatcherHostTest, DecrementRefAfterOnStart) {
  const std::string kId("id");

  // Decrement the refcount while building, after we call OnStartBuildlingBlob.
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element};
  host_->OnStartBuildingBlob(kId, elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);

  std::vector<BlobItemBytesRequest> expected_requests = {
      BlobItemBytesRequest::CreateIPCRequest(0, 0, 0, kDataSize)};
  ExpectRequest(kId, expected_requests);
  sink_.ClearMessages();
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  host_->OnDecrementBlobRefCount(kId);
  EXPECT_FALSE(context_->registry().HasEntry(kId));
  EXPECT_FALSE(IsBeingBuiltInHost(kId));
  ExpectCancel(kId,
               IPCBlobCreationCancelCode::BLOB_DEREFERENCED_WHILE_BUILDING);
  sink_.ClearMessages();

  // Do the same, but this time grab a handle to keep it alive.
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  host_->OnStartBuildingBlob(kId, elements);
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
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());
  EXPECT_FALSE(host_->shutdown_for_bad_message_);

  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle->IsBeingBuilt());
  bool built = true;
  IPCBlobCreationCancelCode error_code = IPCBlobCreationCancelCode::UNKNOWN;
  blob_data_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));

  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element};
  host_->OnStartBuildingBlob(kId, elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);

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
  host_->OnCancelBuildingBlob(kId, IPCBlobCreationCancelCode::UNKNOWN);
  // Run loop to propagate the handle decrement in the host.
  base::RunLoop().RunUntilIdle();
  // We still have the entry because of our earlier handle.
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_FALSE(IsBeingBuiltInHost(kId));
  EXPECT_FALSE(built);
  EXPECT_EQ(IPCBlobCreationCancelCode::UNKNOWN, error_code);
  sink_.ClearMessages();

  // Should disappear after dropping the handle.
  EXPECT_TRUE(blob_data_handle->IsBroken());
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->registry().HasEntry(kId));
}

TEST_F(BlobDispatcherHostTest, HostDisconnectAfterRegisterWithHandle) {
  const std::string kId("id");

  // Delete host with a handle to the blob.
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());

  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle->IsBeingBuilt());
  bool built = true;
  IPCBlobCreationCancelCode error_code = IPCBlobCreationCancelCode::UNKNOWN;
  blob_data_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));
  // Get rid of host, which was doing the constructing.
  host_ = nullptr;
  EXPECT_FALSE(blob_data_handle->IsBeingBuilt());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(built);
  EXPECT_EQ(IPCBlobCreationCancelCode::SOURCE_DIED_IN_TRANSIT, error_code);

  // Should still be there due to the handle.
  std::unique_ptr<BlobDataHandle> another_handle =
      context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(another_handle);

  // Should disappear after dropping both handles.
  blob_data_handle.reset();
  another_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->registry().HasEntry(kId));
}

TEST_F(BlobDispatcherHostTest, HostDisconnectAfterOnStart) {
  const std::string kId("id");

  // Host deleted after OnStartBuilding.
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());

  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element};
  host_->OnStartBuildingBlob(kId, elements);

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
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());

  // Create list of two items.
  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element, element};
  host_->OnStartBuildingBlob(kId, elements);
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
  host_->OnRegisterBlobUUID(kCircularId, std::string(kContentType),
                            std::string(kContentDisposition), {kCircularId});
  ExpectAndResetBadMessage();

  // Next, test a blob that references a broken blob.
  host_->OnRegisterBlobUUID(kBrokenId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());
  host_->OnCancelBuildingBlob(kBrokenId, IPCBlobCreationCancelCode::UNKNOWN);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  EXPECT_TRUE(context_->GetBlobDataFromUUID(kBrokenId)->IsBroken());

  // Create referencing blob. We should be broken right away, but also ignore
  // the subsequent OnStart message.
  host_->OnRegisterBlobUUID(kReferencingId, std::string(kContentType),
                            std::string(kContentDisposition), {kBrokenId});
  EXPECT_TRUE(context_->GetBlobDataFromUUID(kReferencingId)->IsBroken());
  EXPECT_FALSE(IsBeingBuiltInHost(kReferencingId));
  EXPECT_TRUE(context_->registry().HasEntry(kReferencingId));
  ExpectCancel(kReferencingId,
               IPCBlobCreationCancelCode::REFERENCED_BLOB_BROKEN);
  sink_.ClearMessages();

  DataElement element;
  element.SetToBytesDescription(kDataSize);
  std::vector<DataElement> elements = {element};
  element.SetToBlob(kBrokenId);
  elements.push_back(element);
  host_->OnStartBuildingBlob(kReferencingId, elements);
  EXPECT_EQ(0u, sink_.message_count());
  base::RunLoop().RunUntilIdle();
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
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());
  host2->OnIncrementBlobRefCount(kId);
  host_->OnDecrementBlobRefCount(kId);
  EXPECT_FALSE(host_->IsInUseInHost(kId));
  host2->OnDecrementBlobRefCount(kId);
  // So no more blob in the context, but we're still being built in host 1.
  EXPECT_FALSE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(host_->async_builder_.IsBeingBuilt(kId));
  host_->OnStartBuildingBlob(kId, elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  // We should be cleaned up.
  EXPECT_FALSE(host_->async_builder_.IsBeingBuilt(kId));
  ExpectCancel(kId,
               IPCBlobCreationCancelCode::BLOB_DEREFERENCED_WHILE_BUILDING);
  sink_.ClearMessages();

  // Same as above, but test OnMemoryItemResponse after double dereference.
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());
  host2->OnIncrementBlobRefCount(kId);
  host_->OnDecrementBlobRefCount(kId);
  EXPECT_FALSE(host_->IsInUseInHost(kId));
  host_->OnStartBuildingBlob(kId, elements);
  ExpectRequest(kId, expected_requests);
  sink_.ClearMessages();

  host2->OnDecrementBlobRefCount(kId);
  // So no more blob in the context, but we're still being built in host 1.
  EXPECT_FALSE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(host_->async_builder_.IsBeingBuilt(kId));
  host_->OnMemoryItemResponse(kId, responses);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  // We should be cleaned up.
  EXPECT_FALSE(host_->async_builder_.IsBeingBuilt(kId));
  ExpectCancel(kId,
               IPCBlobCreationCancelCode::BLOB_DEREFERENCED_WHILE_BUILDING);
  sink_.ClearMessages();

  // Same, but now for OnCancel.
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());
  host2->OnIncrementBlobRefCount(kId);
  host_->OnDecrementBlobRefCount(kId);
  EXPECT_FALSE(host_->IsInUseInHost(kId));
  host_->OnStartBuildingBlob(kId, elements);
  ExpectRequest(kId, expected_requests);
  sink_.ClearMessages();

  host2->OnDecrementBlobRefCount(kId);
  // So no more blob in the context, but we're still being built in host 1.
  EXPECT_FALSE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(host_->async_builder_.IsBeingBuilt(kId));
  host_->OnCancelBuildingBlob(kId, IPCBlobCreationCancelCode::UNKNOWN);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  // We should be cleaned up.
  EXPECT_FALSE(host_->async_builder_.IsBeingBuilt(kId));
}

TEST_F(BlobDispatcherHostTest, BuildingReferenceChain) {
  const std::string kId("id");
  const std::string kSameHostReferencingId("id2");
  const std::string kDifferentHostReferencingId("id3");
  // Data elements for our transfer & checking messages.
  DataElement element;
  element.SetToBytes(kData, kDataSize);
  std::vector<DataElement> elements = {element};
  DataElement referencing_element;
  referencing_element.SetToBlob(kId);
  std::vector<DataElement> referencing_elements = {referencing_element};
  std::set<std::string> referenced_blobs_set = {kId};

  scoped_refptr<TestableBlobDispatcherHost> host2(
      new TestableBlobDispatcherHost(chrome_blob_storage_context_,
                                     file_system_context_.get(), &sink_));

  // We want to have a blob referencing another blob that is building, both on
  // the same host and a different host. We should successfully build all blobs
  // after the referenced blob is finished.

  // First we start the referenced blob.
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());
  EXPECT_TRUE(host_->IsInUseInHost(kId));

  // Next we start the referencing blobs in both the same and different host.
  host_->OnRegisterBlobUUID(kSameHostReferencingId, std::string(kContentType),
                            std::string(kContentDisposition),
                            referenced_blobs_set);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  host_->OnStartBuildingBlob(kSameHostReferencingId, referencing_elements);
  EXPECT_FALSE(host_->shutdown_for_bad_message_);
  ExpectDone(kSameHostReferencingId);
  EXPECT_TRUE(host_->async_builder_.IsBeingBuilt(kSameHostReferencingId));
  sink_.ClearMessages();
  // Now the other host.
  host2->OnRegisterBlobUUID(
      kDifferentHostReferencingId, std::string(kContentType),
      std::string(kContentDisposition), referenced_blobs_set);
  EXPECT_FALSE(host2->shutdown_for_bad_message_);
  host2->OnStartBuildingBlob(kDifferentHostReferencingId, referencing_elements);
  EXPECT_FALSE(host2->shutdown_for_bad_message_);
  ExpectDone(kDifferentHostReferencingId);
  EXPECT_TRUE(host2->async_builder_.IsBeingBuilt(kDifferentHostReferencingId));
  sink_.ClearMessages();

  // Now we finish the first blob, and we expect all blobs to finish.
  host_->OnStartBuildingBlob(kId, elements);
  ExpectDone(kId);
  // We need to run the message loop to propagate the construction callbacks.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host2->async_builder_.IsBeingBuilt(kDifferentHostReferencingId));
  EXPECT_FALSE(host_->async_builder_.IsBeingBuilt(kSameHostReferencingId));
  EXPECT_FALSE(
      context_->GetBlobDataFromUUID(kSameHostReferencingId)->IsBroken());
  EXPECT_FALSE(
      context_->GetBlobDataFromUUID(kDifferentHostReferencingId)->IsBroken());
  sink_.ClearMessages();

  // Finally check that our data is correct in the child elements.
  std::unique_ptr<BlobDataHandle> handle =
      context_->GetBlobDataFromUUID(kDifferentHostReferencingId);
  ExpectHandleEqualsData(handle.get(), elements);
}

TEST_F(BlobDispatcherHostTest, BuildingReferenceChainWithCancel) {
  const std::string kId("id");
  const std::string kSameHostReferencingId("id2");
  const std::string kDifferentHostReferencingId("id3");
  // Data elements for our transfer & checking messages.
  DataElement referencing_element;
  referencing_element.SetToBlob(kId);
  std::vector<DataElement> referencing_elements = {referencing_element};
  std::set<std::string> referenced_blobs_set = {kId};

  scoped_refptr<TestableBlobDispatcherHost> host2(
      new TestableBlobDispatcherHost(chrome_blob_storage_context_,
                                     file_system_context_.get(), &sink_));

  // We want to have a blob referencing another blob that is building, both on
  // the same host and a different host. After we cancel the first blob, the
  // others should cancel as well.

  // First we start the referenced blob.
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());
  EXPECT_TRUE(host_->IsInUseInHost(kId));

  // Next we start the referencing blobs in both the same and different host.
  host_->OnRegisterBlobUUID(kSameHostReferencingId, std::string(kContentType),
                            std::string(kContentDisposition),
                            referenced_blobs_set);
  host_->OnStartBuildingBlob(kSameHostReferencingId, referencing_elements);
  ExpectDone(kSameHostReferencingId);
  EXPECT_TRUE(host_->async_builder_.IsBeingBuilt(kSameHostReferencingId));
  sink_.ClearMessages();
  // Now the other host.
  host2->OnRegisterBlobUUID(
      kDifferentHostReferencingId, std::string(kContentType),
      std::string(kContentDisposition), referenced_blobs_set);
  host2->OnStartBuildingBlob(kDifferentHostReferencingId, referencing_elements);
  ExpectDone(kDifferentHostReferencingId);
  EXPECT_TRUE(host2->async_builder_.IsBeingBuilt(kDifferentHostReferencingId));
  sink_.ClearMessages();
  bool built = false;
  IPCBlobCreationCancelCode error_code = IPCBlobCreationCancelCode::UNKNOWN;
  context_->GetBlobDataFromUUID(kDifferentHostReferencingId)
      ->RunOnConstructionComplete(
          base::Bind(&ConstructionCompletePopulator, &built, &error_code));

  // Now we cancel the first blob, and we expect all blobs to cancel.
  host_->OnCancelBuildingBlob(kId, IPCBlobCreationCancelCode::UNKNOWN);
  // We need to run the message loop to propagate the construction callbacks.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host2->async_builder_.IsBeingBuilt(kDifferentHostReferencingId));
  EXPECT_FALSE(host_->async_builder_.IsBeingBuilt(kSameHostReferencingId));
  EXPECT_TRUE(
      context_->GetBlobDataFromUUID(kSameHostReferencingId)->IsBroken());
  EXPECT_TRUE(
      context_->GetBlobDataFromUUID(kDifferentHostReferencingId)->IsBroken());
  EXPECT_FALSE(built);
  EXPECT_EQ(IPCBlobCreationCancelCode::REFERENCED_BLOB_BROKEN, error_code);
  sink_.ClearMessages();
}

TEST_F(BlobDispatcherHostTest, BuildingReferenceChainWithSourceDeath) {
  const std::string kId("id");
  const std::string kSameHostReferencingId("id2");
  const std::string kDifferentHostReferencingId("id3");
  // Data elements for our transfer & checking messages.
  DataElement referencing_element;
  referencing_element.SetToBlob(kId);
  std::vector<DataElement> referencing_elements = {referencing_element};
  std::set<std::string> referenced_blobs_set = {kId};

  scoped_refptr<TestableBlobDispatcherHost> host2(
      new TestableBlobDispatcherHost(chrome_blob_storage_context_,
                                     file_system_context_.get(), &sink_));

  // We want to have a blob referencing another blob that is building, both on
  // the same host and a different host. When we destroy the host, the other
  // blob should cancel, as well as the blob on the other host.

  // First we start the referenced blob.
  host_->OnRegisterBlobUUID(kId, std::string(kContentType),
                            std::string(kContentDisposition),
                            std::set<std::string>());
  EXPECT_TRUE(host_->IsInUseInHost(kId));

  // Next we start the referencing blobs in both the same and different host.
  host_->OnRegisterBlobUUID(kSameHostReferencingId, std::string(kContentType),
                            std::string(kContentDisposition),
                            referenced_blobs_set);
  host_->OnStartBuildingBlob(kSameHostReferencingId, referencing_elements);
  ExpectDone(kSameHostReferencingId);
  EXPECT_TRUE(host_->async_builder_.IsBeingBuilt(kSameHostReferencingId));
  sink_.ClearMessages();
  // Now the other host.
  host2->OnRegisterBlobUUID(
      kDifferentHostReferencingId, std::string(kContentType),
      std::string(kContentDisposition), referenced_blobs_set);
  host2->OnStartBuildingBlob(kDifferentHostReferencingId, referencing_elements);
  ExpectDone(kDifferentHostReferencingId);
  EXPECT_TRUE(host2->async_builder_.IsBeingBuilt(kDifferentHostReferencingId));
  sink_.ClearMessages();

  // Grab handles & add listeners.
  bool built = true;
  IPCBlobCreationCancelCode error_code = IPCBlobCreationCancelCode::UNKNOWN;
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_->GetBlobDataFromUUID(kId);
  blob_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &built, &error_code));

  bool same_host_built = true;
  IPCBlobCreationCancelCode same_host_error_code =
      IPCBlobCreationCancelCode::UNKNOWN;
  std::unique_ptr<BlobDataHandle> same_host_blob_handle =
      context_->GetBlobDataFromUUID(kSameHostReferencingId);
  same_host_blob_handle->RunOnConstructionComplete(base::Bind(
      &ConstructionCompletePopulator, &same_host_built, &same_host_error_code));

  bool other_host_built = true;
  IPCBlobCreationCancelCode other_host_error_code =
      IPCBlobCreationCancelCode::UNKNOWN;
  std::unique_ptr<BlobDataHandle> other_host_blob_handle =
      context_->GetBlobDataFromUUID(kDifferentHostReferencingId);
  other_host_blob_handle->RunOnConstructionComplete(
      base::Bind(&ConstructionCompletePopulator, &other_host_built,
                 &other_host_error_code));

  // Now we kill the host.
  host_ = nullptr;
  // We need to run the message loop to propagate the construction callbacks.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host2->async_builder_.IsBeingBuilt(kDifferentHostReferencingId));
  EXPECT_TRUE(
      context_->GetBlobDataFromUUID(kSameHostReferencingId)->IsBroken());
  EXPECT_TRUE(
      context_->GetBlobDataFromUUID(kDifferentHostReferencingId)->IsBroken());

  // Check our callbacks
  EXPECT_FALSE(built);
  EXPECT_EQ(IPCBlobCreationCancelCode::SOURCE_DIED_IN_TRANSIT, error_code);
  EXPECT_FALSE(same_host_built);
  EXPECT_EQ(IPCBlobCreationCancelCode::SOURCE_DIED_IN_TRANSIT,
            same_host_error_code);
  EXPECT_FALSE(other_host_built);
  EXPECT_EQ(IPCBlobCreationCancelCode::SOURCE_DIED_IN_TRANSIT,
            other_host_error_code);

  sink_.ClearMessages();
}

}  // namespace content
