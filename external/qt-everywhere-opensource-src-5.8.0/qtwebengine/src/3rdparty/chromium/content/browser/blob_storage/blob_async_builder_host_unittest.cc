// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_async_builder_host.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/run_loop.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/common/blob_storage/blob_storage_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {
const std::string kBlobUUID = "blobUUIDYAY";
const std::string kContentType = "content_type";
const std::string kContentDisposition = "content_disposition";
const std::string kCompletedBlobUUID = "completedBlob";
const std::string kCompletedBlobData = "completedBlobData";

const size_t kTestBlobStorageIPCThresholdBytes = 5;
const size_t kTestBlobStorageMaxSharedMemoryBytes = 20;
const uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

void PopulateBytes(char* bytes, size_t length) {
  for (size_t i = 0; i < length; i++) {
    bytes[i] = static_cast<char>(i);
  }
}

void AddMemoryItem(size_t length, std::vector<DataElement>* out) {
  DataElement bytes;
  bytes.SetToBytesDescription(length);
  out->push_back(bytes);
}

void AddShortcutMemoryItem(size_t length, std::vector<DataElement>* out) {
  DataElement bytes;
  bytes.SetToAllocatedBytes(length);
  PopulateBytes(bytes.mutable_bytes(), length);
  out->push_back(bytes);
}

void AddShortcutMemoryItem(size_t length, BlobDataBuilder* out) {
  DataElement bytes;
  bytes.SetToAllocatedBytes(length);
  PopulateBytes(bytes.mutable_bytes(), length);
  out->AppendData(bytes.bytes(), length);
}

void AddBlobItem(std::vector<DataElement>* out) {
  DataElement blob;
  blob.SetToBlob(kCompletedBlobUUID);
  out->push_back(blob);
}
}  // namespace

class BlobAsyncBuilderHostTest : public testing::Test {
 public:
  BlobAsyncBuilderHostTest()
      : cancel_code_(IPCBlobCreationCancelCode::UNKNOWN),
        request_called_(false) {}
  ~BlobAsyncBuilderHostTest() override {}

  void SetUp() override {
    cancel_code_ = IPCBlobCreationCancelCode::UNKNOWN;
    request_called_ = false;
    requests_.clear();
    memory_handles_.clear();
    host_.SetMemoryConstantsForTesting(kTestBlobStorageIPCThresholdBytes,
                                       kTestBlobStorageMaxSharedMemoryBytes,
                                       kTestBlobStorageMaxFileSizeBytes);
    BlobDataBuilder builder(kCompletedBlobUUID);
    builder.AppendData(kCompletedBlobData);
    completed_blob_handle_ = context_.AddFinishedBlob(builder);
    completed_blob_uuid_set_ = {kCompletedBlobUUID};
  }

  void RequestMemoryCallback(
      std::unique_ptr<std::vector<storage::BlobItemBytesRequest>> requests,
      std::unique_ptr<std::vector<base::SharedMemoryHandle>>
          shared_memory_handles,
      std::unique_ptr<std::vector<base::File>> files) {
    requests_ = std::move(*requests);
    memory_handles_ = std::move(*shared_memory_handles);
    request_called_ = true;
  }

  BlobTransportResult BuildBlobAsync(
      const std::vector<DataElement>& descriptions,
      const std::set<std::string>& referenced_blob_uuids,
      size_t memory_available) {
    request_called_ = false;
    BlobTransportResult register_result =
        host_.RegisterBlobUUID(kBlobUUID, kContentType, kContentDisposition,
                               referenced_blob_uuids, &context_);
    if (register_result != BlobTransportResult::DONE) {
      return register_result;
    }
    return host_.StartBuildingBlob(
        kBlobUUID, descriptions, memory_available, &context_,
        base::Bind(&BlobAsyncBuilderHostTest::RequestMemoryCallback,
                   base::Unretained(this)));
  }

  void DecrementBlobRefCount(const std::string& uuid) {
    context_.DecrementBlobRefCount(uuid);
  }

  bool IsBeingBuiltInContext(const std::string& uuid) {
    return context_.IsBeingBuilt(uuid);
  }

  content::TestBrowserThreadBundle browser_thread_bundle_;
  BlobStorageContext context_;
  BlobAsyncBuilderHost host_;
  IPCBlobCreationCancelCode cancel_code_;

  bool request_called_;
  std::vector<storage::BlobItemBytesRequest> requests_;
  std::vector<base::SharedMemoryHandle> memory_handles_;
  std::set<std::string> completed_blob_uuid_set_;

  std::unique_ptr<BlobDataHandle> completed_blob_handle_;
};

// The 'shortcut' method is when the data is included in the initial IPCs and
// the browser uses that instead of requesting the memory.
TEST_F(BlobAsyncBuilderHostTest, TestShortcut) {
  std::vector<DataElement> descriptions;

  AddShortcutMemoryItem(10, &descriptions);
  AddBlobItem(&descriptions);
  AddShortcutMemoryItem(5000, &descriptions);

  BlobDataBuilder expected(kBlobUUID);
  expected.set_content_type(kContentType);
  expected.set_content_disposition(kContentDisposition);
  AddShortcutMemoryItem(10, &expected);
  expected.AppendData(kCompletedBlobData);
  AddShortcutMemoryItem(5000, &expected);

  EXPECT_EQ(BlobTransportResult::DONE,
            BuildBlobAsync(descriptions, completed_blob_uuid_set_, 5010));

  EXPECT_FALSE(request_called_);
  EXPECT_EQ(0u, host_.blob_building_count());
  std::unique_ptr<BlobDataHandle> handle =
      context_.GetBlobDataFromUUID(kBlobUUID);
  EXPECT_FALSE(handle->IsBeingBuilt());
  EXPECT_FALSE(handle->IsBroken());
  std::unique_ptr<BlobDataSnapshot> data = handle->CreateSnapshot();
  EXPECT_EQ(expected, *data);
  data.reset();
  handle.reset();
  base::RunLoop().RunUntilIdle();
};

TEST_F(BlobAsyncBuilderHostTest, TestShortcutNoRoom) {
  std::vector<DataElement> descriptions;

  AddShortcutMemoryItem(10, &descriptions);
  AddBlobItem(&descriptions);
  AddShortcutMemoryItem(5000, &descriptions);

  EXPECT_EQ(BlobTransportResult::CANCEL_MEMORY_FULL,
            BuildBlobAsync(descriptions, completed_blob_uuid_set_, 5000));

  EXPECT_FALSE(request_called_);
  EXPECT_EQ(0u, host_.blob_building_count());
};

TEST_F(BlobAsyncBuilderHostTest, TestSingleSharedMemRequest) {
  std::vector<DataElement> descriptions;
  const size_t kSize = kTestBlobStorageIPCThresholdBytes + 1;
  AddMemoryItem(kSize, &descriptions);

  EXPECT_EQ(BlobTransportResult::PENDING_RESPONSES,
            BuildBlobAsync(descriptions, std::set<std::string>(),
                           kTestBlobStorageIPCThresholdBytes + 1));

  EXPECT_TRUE(request_called_);
  EXPECT_EQ(1u, host_.blob_building_count());
  ASSERT_EQ(1u, requests_.size());
  request_called_ = false;

  EXPECT_EQ(
      BlobItemBytesRequest::CreateSharedMemoryRequest(0, 0, 0, kSize, 0, 0),
      requests_.at(0));
};

TEST_F(BlobAsyncBuilderHostTest, TestMultipleSharedMemRequests) {
  std::vector<DataElement> descriptions;
  const size_t kSize = kTestBlobStorageMaxSharedMemoryBytes + 1;
  const char kFirstBlockByte = 7;
  const char kSecondBlockByte = 19;
  AddMemoryItem(kSize, &descriptions);

  BlobDataBuilder expected(kBlobUUID);
  expected.set_content_type(kContentType);
  expected.set_content_disposition(kContentDisposition);
  char data[kSize];
  memset(data, kFirstBlockByte, kTestBlobStorageMaxSharedMemoryBytes);
  expected.AppendData(data, kTestBlobStorageMaxSharedMemoryBytes);
  expected.AppendData(&kSecondBlockByte, 1);

  EXPECT_EQ(BlobTransportResult::PENDING_RESPONSES,
            BuildBlobAsync(descriptions, std::set<std::string>(),
                           kTestBlobStorageMaxSharedMemoryBytes + 1));

  EXPECT_TRUE(request_called_);
  EXPECT_EQ(1u, host_.blob_building_count());
  ASSERT_EQ(1u, requests_.size());
  request_called_ = false;

  // We need to grab a duplicate handle so we can have two blocks open at the
  // same time.
  base::SharedMemoryHandle handle =
      base::SharedMemory::DuplicateHandle(memory_handles_.at(0));
  EXPECT_TRUE(base::SharedMemory::IsHandleValid(handle));
  base::SharedMemory shared_memory(handle, false);
  EXPECT_TRUE(shared_memory.Map(kTestBlobStorageMaxSharedMemoryBytes));

  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(
                0, 0, 0, kTestBlobStorageMaxSharedMemoryBytes, 0, 0),
            requests_.at(0));

  memset(shared_memory.memory(), kFirstBlockByte,
         kTestBlobStorageMaxSharedMemoryBytes);

  BlobItemBytesResponse response(0);
  std::vector<BlobItemBytesResponse> responses = {response};
  EXPECT_EQ(BlobTransportResult::PENDING_RESPONSES,
            host_.OnMemoryResponses(kBlobUUID, responses, &context_));

  EXPECT_TRUE(request_called_);
  EXPECT_EQ(1u, host_.blob_building_count());
  ASSERT_EQ(1u, requests_.size());
  request_called_ = false;

  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(
                1, 0, kTestBlobStorageMaxSharedMemoryBytes, 1, 0, 0),
            requests_.at(0));

  memset(shared_memory.memory(), kSecondBlockByte, 1);

  response.request_number = 1;
  responses[0] = response;
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.OnMemoryResponses(kBlobUUID, responses, &context_));
  EXPECT_FALSE(request_called_);
  EXPECT_EQ(0u, host_.blob_building_count());
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_.GetBlobDataFromUUID(kBlobUUID);
  EXPECT_FALSE(blob_handle->IsBeingBuilt());
  EXPECT_FALSE(blob_handle->IsBroken());
  std::unique_ptr<BlobDataSnapshot> blob_data = blob_handle->CreateSnapshot();
  EXPECT_EQ(expected, *blob_data);
};

TEST_F(BlobAsyncBuilderHostTest, TestBasicIPCAndStopBuilding) {
  std::vector<DataElement> descriptions;

  AddMemoryItem(2, &descriptions);
  AddBlobItem(&descriptions);
  AddMemoryItem(2, &descriptions);

  BlobDataBuilder expected(kBlobUUID);
  expected.set_content_type(kContentType);
  expected.set_content_disposition(kContentDisposition);
  AddShortcutMemoryItem(2, &expected);
  expected.AppendData(kCompletedBlobData);
  AddShortcutMemoryItem(2, &expected);

  EXPECT_EQ(BlobTransportResult::PENDING_RESPONSES,
            BuildBlobAsync(descriptions, completed_blob_uuid_set_, 5010));
  host_.CancelBuildingBlob(kBlobUUID, IPCBlobCreationCancelCode::UNKNOWN,
                           &context_);

  // Check that we're broken, and then remove the blob.
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_.GetBlobDataFromUUID(kBlobUUID);
  EXPECT_FALSE(blob_handle->IsBeingBuilt());
  EXPECT_TRUE(blob_handle->IsBroken());
  blob_handle.reset();
  DecrementBlobRefCount(kBlobUUID);
  base::RunLoop().RunUntilIdle();
  blob_handle = context_.GetBlobDataFromUUID(kBlobUUID);
  EXPECT_FALSE(blob_handle.get());

  // This should succeed because we've removed all references to the blob.
  EXPECT_EQ(BlobTransportResult::PENDING_RESPONSES,
            BuildBlobAsync(descriptions, completed_blob_uuid_set_, 5010));

  EXPECT_TRUE(request_called_);
  EXPECT_EQ(1u, host_.blob_building_count());
  request_called_ = false;

  BlobItemBytesResponse response1(0);
  PopulateBytes(response1.allocate_mutable_data(2), 2);
  BlobItemBytesResponse response2(1);
  PopulateBytes(response2.allocate_mutable_data(2), 2);
  std::vector<BlobItemBytesResponse> responses = {response1, response2};

  EXPECT_EQ(BlobTransportResult::DONE,
            host_.OnMemoryResponses(kBlobUUID, responses, &context_));
  EXPECT_FALSE(request_called_);
  EXPECT_EQ(0u, host_.blob_building_count());
  blob_handle = context_.GetBlobDataFromUUID(kBlobUUID);
  EXPECT_FALSE(blob_handle->IsBeingBuilt());
  EXPECT_FALSE(blob_handle->IsBroken());
  std::unique_ptr<BlobDataSnapshot> blob_data = blob_handle->CreateSnapshot();
  EXPECT_EQ(expected, *blob_data);
};

TEST_F(BlobAsyncBuilderHostTest, TestBreakingAllBuilding) {
  const std::string& kBlob1 = "blob1";
  const std::string& kBlob2 = "blob2";
  const std::string& kBlob3 = "blob3";

  // Register blobs.
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.RegisterBlobUUID(kBlob1, kContentType, kContentDisposition,
                                   std::set<std::string>(), &context_));
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.RegisterBlobUUID(kBlob2, kContentType, kContentDisposition,
                                   std::set<std::string>(), &context_));
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.RegisterBlobUUID(kBlob3, kContentType, kContentDisposition,
                                   std::set<std::string>(), &context_));

  // Start building one of them.
  std::vector<DataElement> descriptions;
  AddMemoryItem(2, &descriptions);
  EXPECT_EQ(BlobTransportResult::PENDING_RESPONSES,
            host_.StartBuildingBlob(
                kBlob1, descriptions, 2, &context_,
                base::Bind(&BlobAsyncBuilderHostTest::RequestMemoryCallback,
                           base::Unretained(this))));
  EXPECT_TRUE(request_called_);

  std::unique_ptr<BlobDataHandle> blob_handle1 =
      context_.GetBlobDataFromUUID(kBlob1);
  std::unique_ptr<BlobDataHandle> blob_handle2 =
      context_.GetBlobDataFromUUID(kBlob2);
  std::unique_ptr<BlobDataHandle> blob_handle3 =
      context_.GetBlobDataFromUUID(kBlob2);
  EXPECT_TRUE(blob_handle1->IsBeingBuilt() && blob_handle2->IsBeingBuilt() &&
              blob_handle3->IsBeingBuilt());
  EXPECT_FALSE(blob_handle1->IsBroken() || blob_handle2->IsBroken() ||
               blob_handle3->IsBroken());

  host_.CancelAll(&context_);

  EXPECT_FALSE(blob_handle1->IsBeingBuilt() || blob_handle2->IsBeingBuilt() ||
               blob_handle3->IsBeingBuilt());
  EXPECT_TRUE(blob_handle1->IsBroken() && blob_handle2->IsBroken() &&
              blob_handle3->IsBroken());
  blob_handle1.reset();
  blob_handle2.reset();
  blob_handle3.reset();
  base::RunLoop().RunUntilIdle();
};

TEST_F(BlobAsyncBuilderHostTest, TestBadIPCs) {
  std::vector<DataElement> descriptions;

  // Test reusing same blob uuid.
  AddMemoryItem(10, &descriptions);
  AddBlobItem(&descriptions);
  AddMemoryItem(5000, &descriptions);
  EXPECT_EQ(BlobTransportResult::PENDING_RESPONSES,
            BuildBlobAsync(descriptions, completed_blob_uuid_set_, 5010));
  EXPECT_EQ(BlobTransportResult::BAD_IPC,
            BuildBlobAsync(descriptions, completed_blob_uuid_set_, 5010));
  EXPECT_FALSE(request_called_);
  host_.CancelBuildingBlob(kBlobUUID, IPCBlobCreationCancelCode::UNKNOWN,
                           &context_);
  base::RunLoop().RunUntilIdle();
  DecrementBlobRefCount(kBlobUUID);
  EXPECT_FALSE(context_.GetBlobDataFromUUID(kBlobUUID).get());

  // Test we're an error if we get a bad uuid for responses.
  BlobItemBytesResponse response(0);
  std::vector<BlobItemBytesResponse> responses = {response};
  EXPECT_EQ(BlobTransportResult::BAD_IPC,
            host_.OnMemoryResponses(kBlobUUID, responses, &context_));

  // Test empty responses.
  responses.clear();
  EXPECT_EQ(BlobTransportResult::BAD_IPC,
            host_.OnMemoryResponses(kBlobUUID, responses, &context_));

  // Test response problems below here.
  descriptions.clear();
  AddMemoryItem(2, &descriptions);
  AddBlobItem(&descriptions);
  AddMemoryItem(2, &descriptions);
  EXPECT_EQ(BlobTransportResult::PENDING_RESPONSES,
            BuildBlobAsync(descriptions, completed_blob_uuid_set_, 5010));

  // Invalid request number.
  BlobItemBytesResponse response1(3);
  PopulateBytes(response1.allocate_mutable_data(2), 2);
  responses = {response1};
  EXPECT_EQ(BlobTransportResult::BAD_IPC,
            host_.OnMemoryResponses(kBlobUUID, responses, &context_));
  EXPECT_TRUE(context_.GetBlobDataFromUUID(kBlobUUID)->IsBroken());
  DecrementBlobRefCount(kBlobUUID);
  base::RunLoop().RunUntilIdle();

  // Duplicate request number responses.
  EXPECT_EQ(BlobTransportResult::PENDING_RESPONSES,
            BuildBlobAsync(descriptions, completed_blob_uuid_set_, 5010));
  response1.request_number = 0;
  BlobItemBytesResponse response2(0);
  PopulateBytes(response2.allocate_mutable_data(2), 2);
  responses = {response1, response2};
  EXPECT_EQ(BlobTransportResult::BAD_IPC,
            host_.OnMemoryResponses(kBlobUUID, responses, &context_));
  EXPECT_TRUE(context_.GetBlobDataFromUUID(kBlobUUID)->IsBroken());
  DecrementBlobRefCount(kBlobUUID);
  base::RunLoop().RunUntilIdle();
};

TEST_F(BlobAsyncBuilderHostTest, WaitOnReferencedBlob) {
  const std::string& kBlob1 = "blob1";
  const std::string& kBlob2 = "blob2";
  const std::string& kBlob3 = "blob3";

  // Register blobs.
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.RegisterBlobUUID(kBlob1, kContentType, kContentDisposition,
                                   std::set<std::string>(), &context_));
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.RegisterBlobUUID(kBlob2, kContentType, kContentDisposition,
                                   std::set<std::string>(), &context_));
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.RegisterBlobUUID(kBlob3, kContentType, kContentDisposition,
                                   {kBlob1, kBlob2}, &context_));

  // Finish the third one, with a reference to the first and second blob.
  std::vector<DataElement> descriptions;
  AddShortcutMemoryItem(2, &descriptions);
  DataElement element;
  element.SetToBlob(kBlob1);
  descriptions.push_back(element);
  element.SetToBlob(kBlob2);
  descriptions.push_back(element);

  // Finish the third, but we should still be 'building' it.
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.StartBuildingBlob(
                kBlob3, descriptions, 2, &context_,
                base::Bind(&BlobAsyncBuilderHostTest::RequestMemoryCallback,
                           base::Unretained(this))));
  EXPECT_FALSE(request_called_);
  EXPECT_TRUE(host_.IsBeingBuilt(kBlob3));
  EXPECT_TRUE(IsBeingBuiltInContext(kBlob3));

  // Finish the first.
  descriptions.clear();
  AddShortcutMemoryItem(2, &descriptions);
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.StartBuildingBlob(
                kBlob1, descriptions, 2, &context_,
                base::Bind(&BlobAsyncBuilderHostTest::RequestMemoryCallback,
                           base::Unretained(this))));
  EXPECT_FALSE(request_called_);
  EXPECT_FALSE(host_.IsBeingBuilt(kBlob1));
  EXPECT_FALSE(IsBeingBuiltInContext(kBlob1));
  EXPECT_TRUE(context_.GetBlobDataFromUUID(kBlob1));

  // Run the message loop so we propogate the construction complete callbacks.
  base::RunLoop().RunUntilIdle();
  // Verify we're not done.
  EXPECT_TRUE(host_.IsBeingBuilt(kBlob3));
  EXPECT_TRUE(IsBeingBuiltInContext(kBlob3));

  // Finish the second.
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.StartBuildingBlob(
                kBlob2, descriptions, 2, &context_,
                base::Bind(&BlobAsyncBuilderHostTest::RequestMemoryCallback,
                           base::Unretained(this))));
  EXPECT_FALSE(request_called_);
  EXPECT_FALSE(host_.IsBeingBuilt(kBlob2));
  EXPECT_FALSE(IsBeingBuiltInContext(kBlob2));
  EXPECT_TRUE(context_.GetBlobDataFromUUID(kBlob2));

  // Run the message loop so we propogate the construction complete callbacks.
  base::RunLoop().RunUntilIdle();
  // Finally, we should be finished with third blob.
  EXPECT_FALSE(host_.IsBeingBuilt(kBlob3));
  EXPECT_FALSE(IsBeingBuiltInContext(kBlob3));
  EXPECT_TRUE(context_.GetBlobDataFromUUID(kBlob3));
};

TEST_F(BlobAsyncBuilderHostTest, IncorrectBlobDependencies) {
  const std::string& kGoodBlob = "goodBlob";
  const std::string& kBlob1 = "blob1";
  const std::string& kBlob2 = "blob2";
  const std::string& kBlob3 = "blob3";

  // Register blobs. Blob 1 has a reference to itself, Blob 2 has a reference
  // but doesn't use it, and blob 3 doesn't list it's reference.
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.RegisterBlobUUID(kGoodBlob, kContentType, kContentDisposition,
                                   std::set<std::string>(), &context_));
  EXPECT_EQ(BlobTransportResult::BAD_IPC,
            host_.RegisterBlobUUID(kBlob1, kContentType, kContentDisposition,
                                   {kBlob1}, &context_));
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.RegisterBlobUUID(kBlob2, kContentType, kContentDisposition,
                                   {kGoodBlob}, &context_));
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.RegisterBlobUUID(kBlob3, kContentType, kContentDisposition,
                                   std::set<std::string>(), &context_));

  // The first blob shouldn't be building anymore.
  EXPECT_FALSE(host_.IsBeingBuilt(kBlob1));

  // Try to finish the second one, without a reference to the first.
  std::vector<DataElement> descriptions;
  AddShortcutMemoryItem(2, &descriptions);
  EXPECT_EQ(BlobTransportResult::BAD_IPC,
            host_.StartBuildingBlob(
                kBlob2, descriptions, 2, &context_,
                base::Bind(&BlobAsyncBuilderHostTest::RequestMemoryCallback,
                           base::Unretained(this))));
  EXPECT_FALSE(host_.IsBeingBuilt(kBlob2));

  // Try to finish the third one with the reference we didn't declare earlier.
  descriptions.clear();
  AddShortcutMemoryItem(2, &descriptions);
  DataElement element;
  element.SetToBlob(kGoodBlob);
  descriptions.push_back(element);
  EXPECT_EQ(BlobTransportResult::BAD_IPC,
            host_.StartBuildingBlob(
                kBlob3, descriptions, 2, &context_,
                base::Bind(&BlobAsyncBuilderHostTest::RequestMemoryCallback,
                           base::Unretained(this))));
  EXPECT_FALSE(host_.IsBeingBuilt(kBlob3));
};

TEST_F(BlobAsyncBuilderHostTest, BlobBreaksWhenReferenceBreaks) {
  const std::string& kBlob1 = "blob1";
  const std::string& kBlob2 = "blob2";

  // Register blobs.
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.RegisterBlobUUID(kBlob1, kContentType, kContentDisposition,
                                   std::set<std::string>(), &context_));
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.RegisterBlobUUID(kBlob2, kContentType, kContentDisposition,
                                   {kBlob1}, &context_));

  // Finish the second one, with a reference to the first.
  std::vector<DataElement> descriptions;
  AddShortcutMemoryItem(2, &descriptions);
  DataElement element;
  element.SetToBlob(kBlob1);
  descriptions.push_back(element);
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.StartBuildingBlob(
                kBlob2, descriptions, 2, &context_,
                base::Bind(&BlobAsyncBuilderHostTest::RequestMemoryCallback,
                           base::Unretained(this))));
  EXPECT_FALSE(request_called_);
  EXPECT_TRUE(host_.IsBeingBuilt(kBlob2));
  EXPECT_TRUE(IsBeingBuiltInContext(kBlob2));

  // Break the first.
  descriptions.clear();
  host_.CancelBuildingBlob(kBlob1, IPCBlobCreationCancelCode::UNKNOWN,
                           &context_);
  EXPECT_FALSE(host_.IsBeingBuilt(kBlob1));
  EXPECT_FALSE(IsBeingBuiltInContext(kBlob1));
  EXPECT_TRUE(context_.GetBlobDataFromUUID(kBlob1)->IsBroken());

  // Run the message loop so we propogate the construction complete callbacks.
  base::RunLoop().RunUntilIdle();
  // We should be finished with third blob, and it should be broken.
  EXPECT_FALSE(host_.IsBeingBuilt(kBlob2));
  EXPECT_FALSE(IsBeingBuiltInContext(kBlob2));
  EXPECT_TRUE(context_.GetBlobDataFromUUID(kBlob2)->IsBroken());
};

TEST_F(BlobAsyncBuilderHostTest, BlobBreaksWhenReferenceBroken) {
  const std::string& kBlob1 = "blob1";
  const std::string& kBlob2 = "blob2";

  // Register blobs.
  EXPECT_EQ(BlobTransportResult::DONE,
            host_.RegisterBlobUUID(kBlob1, kContentType, kContentDisposition,
                                   std::set<std::string>(), &context_));
  host_.CancelBuildingBlob(kBlob1, IPCBlobCreationCancelCode::UNKNOWN,
                           &context_);
  EXPECT_EQ(BlobTransportResult::CANCEL_REFERENCED_BLOB_BROKEN,
            host_.RegisterBlobUUID(kBlob2, kContentType, kContentDisposition,
                                   {kBlob1}, &context_));
  EXPECT_FALSE(host_.IsBeingBuilt(kBlob2));
  EXPECT_FALSE(IsBeingBuiltInContext(kBlob2));
  EXPECT_TRUE(context_.GetBlobDataFromUUID(kBlob2)->IsBroken());
};

}  // namespace storage
