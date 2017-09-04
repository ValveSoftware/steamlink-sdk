// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/logging.h"
#include "storage/browser/blob/blob_transport_request_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {

const char kNewUUID[] = "newUUID";
const char kFakeBlobUUID[] = "fakeBlob";

void AddMemoryItem(size_t length, std::vector<DataElement>* out) {
  DataElement bytes;
  bytes.SetToBytesDescription(length);
  out->push_back(bytes);
}

void AddShortcutMemoryItem(size_t length, std::vector<DataElement>* out) {
  DataElement bytes;
  bytes.SetToAllocatedBytes(length);
  for (size_t i = 0; i < length; i++) {
    bytes.mutable_bytes()[i] = static_cast<char>(i);
  }
  out->push_back(bytes);
}

void AddBlobItem(std::vector<DataElement>* out) {
  DataElement blob;
  blob.SetToBlob(kFakeBlobUUID);
  out->push_back(blob);
}

TEST(BlobAsyncTransportRequestBuilderTest, TestNoMemoryItems) {
  BlobTransportRequestBuilder strategy;
  BlobDataBuilder builder(kNewUUID);
  std::vector<DataElement> infos;

  // Here we test that we don't do any requests when there are no memory items.
  AddBlobItem(&infos);
  AddBlobItem(&infos);
  AddBlobItem(&infos);
  strategy.InitializeForIPCRequests(100,  // max_ipc_memory_size
                                    0,    // blob_total_size
                                    infos, &builder);

  EXPECT_EQ(0u, strategy.shared_memory_sizes().size());
  EXPECT_EQ(0u, strategy.file_sizes().size());
  EXPECT_EQ(0u, strategy.requests().size());

  BlobDataBuilder expected_builder(kNewUUID);
  expected_builder.AppendBlob(kFakeBlobUUID);
  expected_builder.AppendBlob(kFakeBlobUUID);
  expected_builder.AppendBlob(kFakeBlobUUID);
  EXPECT_EQ(expected_builder, builder);
}

TEST(BlobAsyncTransportRequestBuilderTest, TestLargeBlockToFile) {
  BlobTransportRequestBuilder strategy;
  BlobDataBuilder builder(kNewUUID);
  std::vector<DataElement> infos;

  AddMemoryItem(305, &infos);
  strategy.InitializeForFileRequests(400,  // max_file_size
                                     305,  // blob_total_size
                                     infos, &builder);

  EXPECT_EQ(0u, strategy.shared_memory_sizes().size());
  EXPECT_EQ(1u, strategy.file_sizes().size());
  EXPECT_EQ(305ul, strategy.file_sizes().at(0));
  EXPECT_EQ(1u, strategy.requests().size());

  auto& memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(0u, 0u, 0ull, 305ull, 0u, 0ull),
      memory_item_request.message);

  BlobDataBuilder expected_builder(kNewUUID);
  expected_builder.AppendFutureFile(0, 305, 0);
  EXPECT_EQ(expected_builder, builder);
}

TEST(BlobAsyncTransportRequestBuilderTest, TestLargeBlockToFiles) {
  BlobTransportRequestBuilder strategy;
  BlobDataBuilder builder(kNewUUID);
  std::vector<DataElement> infos;

  AddMemoryItem(1000, &infos);
  strategy.InitializeForFileRequests(400,   // max_file_size
                                     1000,  // blob_total_size
                                     infos, &builder);

  EXPECT_EQ(0u, strategy.shared_memory_sizes().size());
  EXPECT_EQ(3u, strategy.file_sizes().size());
  EXPECT_EQ(400ul, strategy.file_sizes().at(0));
  EXPECT_EQ(400ul, strategy.file_sizes().at(1));
  EXPECT_EQ(200ul, strategy.file_sizes().at(2));
  EXPECT_EQ(3u, strategy.requests().size());

  auto memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(0u, 0u, 0ull, 400ull, 0u, 0ull),
      memory_item_request.message);

  memory_item_request = strategy.requests().at(1);
  EXPECT_EQ(1u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(1u, 0u, 400ull, 400ull, 1u, 0ull),
      memory_item_request.message);

  memory_item_request = strategy.requests().at(2);
  EXPECT_EQ(2u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(2u, 0u, 800ull, 200ull, 2u, 0ull),
      memory_item_request.message);

  BlobDataBuilder expected_builder(kNewUUID);
  expected_builder.AppendFutureFile(0, 400, 0);
  expected_builder.AppendFutureFile(0, 400, 1);
  expected_builder.AppendFutureFile(0, 200, 2);
  EXPECT_EQ(expected_builder, builder);
}

TEST(BlobAsyncTransportRequestBuilderTest,
     TestLargeBlocksConsolidatingInFiles) {
  BlobTransportRequestBuilder strategy;
  BlobDataBuilder builder(kNewUUID);
  std::vector<DataElement> infos;

  // We should have 3 storage items for the memory, two files, 400 each.
  // We end up with storage items:
  // 1: File A, 300MB
  // 2: Blob
  // 3: File A, 100MB (300MB offset)
  // 4: File B, 400MB
  AddMemoryItem(300, &infos);
  AddBlobItem(&infos);
  AddMemoryItem(500, &infos);

  strategy.InitializeForFileRequests(400,  // max_file_size
                                     800,  // blob_total_size
                                     infos, &builder);

  EXPECT_EQ(0u, strategy.shared_memory_sizes().size());
  EXPECT_EQ(2u, strategy.file_sizes().size());
  EXPECT_EQ(400ul, strategy.file_sizes().at(0));
  EXPECT_EQ(400ul, strategy.file_sizes().at(1));
  EXPECT_EQ(3u, strategy.requests().size());

  auto memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(0u, 0u, 0ull, 300ull, 0u, 0ull),
      memory_item_request.message);

  memory_item_request = strategy.requests().at(1);
  EXPECT_EQ(2u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(1u, 2u, 0ull, 100ull, 0u, 300ull),
      memory_item_request.message);

  memory_item_request = strategy.requests().at(2);
  EXPECT_EQ(3u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(2u, 2u, 100ull, 400ull, 1u, 0ull),
      memory_item_request.message);
}

TEST(BlobAsyncTransportRequestBuilderTest, TestSharedMemorySegmentation) {
  BlobTransportRequestBuilder strategy;
  BlobDataBuilder builder(kNewUUID);
  std::vector<DataElement> infos;

  // For transport we should have 3 shared memories, and then storage in 3
  // browser items.
  AddMemoryItem(500, &infos);
  strategy.InitializeForSharedMemoryRequests(200,  // max_shared_memory_size
                                             500,  // total_blob_size
                                             infos, &builder);

  EXPECT_EQ(0u, strategy.file_sizes().size());
  EXPECT_EQ(3u, strategy.shared_memory_sizes().size());
  EXPECT_EQ(200u, strategy.shared_memory_sizes().at(0));
  EXPECT_EQ(200u, strategy.shared_memory_sizes().at(1));
  EXPECT_EQ(100u, strategy.shared_memory_sizes().at(2));
  EXPECT_EQ(3u, strategy.requests().size());

  auto memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(0u, 0u, 0ull,
                                                            200ull, 0u, 0ull),
            memory_item_request.message);

  memory_item_request = strategy.requests().at(1);
  EXPECT_EQ(1u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(1u, 0u, 200ull,
                                                            200ull, 1u, 0ull),
            memory_item_request.message);

  memory_item_request = strategy.requests().at(2);
  EXPECT_EQ(2u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(2u, 0u, 400ull,
                                                            100ull, 2u, 0ull),
            memory_item_request.message);

  BlobDataBuilder expected_builder(kNewUUID);
  expected_builder.AppendFutureData(200);
  expected_builder.AppendFutureData(200);
  expected_builder.AppendFutureData(100);
  EXPECT_EQ(expected_builder, builder);
}

TEST(BlobAsyncTransportRequestBuilderTest,
     TestSharedMemorySegmentationAndStorage) {
  BlobTransportRequestBuilder strategy;
  BlobDataBuilder builder(kNewUUID);
  std::vector<DataElement> infos;

  // For transport, we should have 2 shared memories, where the first one
  // have half 0 and half 3, and then the last one has half 3.
  //
  // For storage, we should have 3 browser items that match the pre-transport
  // version:
  // 1: Bytes 100MB
  // 2: Blob
  // 3: Bytes 200MB
  AddShortcutMemoryItem(100, &infos);  // should have no behavior change
  AddBlobItem(&infos);
  AddMemoryItem(200, &infos);

  strategy.InitializeForSharedMemoryRequests(200,  // max_shared_memory_size
                                             300,  // total_blob_size
                                             infos, &builder);

  EXPECT_EQ(0u, strategy.file_sizes().size());
  EXPECT_EQ(2u, strategy.shared_memory_sizes().size());
  EXPECT_EQ(200u, strategy.shared_memory_sizes().at(0));
  EXPECT_EQ(100u, strategy.shared_memory_sizes().at(1));
  EXPECT_EQ(3u, strategy.requests().size());

  auto memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(0u, 0u, 0ull,
                                                            100ull, 0u, 0ull),
            memory_item_request.message);

  memory_item_request = strategy.requests().at(1);
  EXPECT_EQ(2u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(1u, 2u, 0ull,
                                                            100ull, 0u, 100ull),
            memory_item_request.message);

  memory_item_request = strategy.requests().at(2);
  EXPECT_EQ(2u, memory_item_request.browser_item_index);
  EXPECT_EQ(100u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(2u, 2u, 100ull,
                                                            100ull, 1u, 0ull),
            memory_item_request.message);

  BlobDataBuilder expected_builder(kNewUUID);
  expected_builder.AppendFutureData(100);
  expected_builder.AppendBlob(kFakeBlobUUID);
  expected_builder.AppendFutureData(200);
  EXPECT_EQ(expected_builder, builder);
}

TEST(BlobAsyncTransportRequestBuilderTest, TestSimpleIPC) {
  // Test simple IPC strategy, where size < max_ipc_memory_size and we have
  // just one item.
  std::vector<DataElement> infos;
  BlobTransportRequestBuilder strategy;
  BlobDataBuilder builder(kNewUUID);
  AddMemoryItem(10, &infos);
  AddBlobItem(&infos);

  strategy.InitializeForIPCRequests(100,  // max_ipc_memory_size
                                    10,   // total_blob_size
                                    infos, &builder);

  EXPECT_EQ(0u, strategy.file_sizes().size());
  EXPECT_EQ(0u, strategy.shared_memory_sizes().size());
  ASSERT_EQ(1u, strategy.requests().size());

  auto memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateIPCRequest(0u, 0u, 0ull, 10ull),
            memory_item_request.message);
}

TEST(BlobAsyncTransportRequestBuilderTest, TestMultipleIPC) {
  // Same as above, but with 2 items and a blob in-between.
  std::vector<DataElement> infos;
  BlobTransportRequestBuilder strategy;
  BlobDataBuilder builder(kNewUUID);
  AddShortcutMemoryItem(10, &infos);  // should have no behavior change
  AddBlobItem(&infos);
  AddMemoryItem(80, &infos);

  strategy.InitializeForIPCRequests(100,  // max_ipc_memory_size
                                    90,   // total_blob_size
                                    infos, &builder);

  EXPECT_EQ(0u, strategy.file_sizes().size());
  EXPECT_EQ(0u, strategy.shared_memory_sizes().size());
  ASSERT_EQ(2u, strategy.requests().size());

  auto memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateIPCRequest(0u, 0u, 0ull, 10ull),
            memory_item_request.message);

  memory_item_request = strategy.requests().at(1);
  EXPECT_EQ(2u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateIPCRequest(1u, 2u, 0ull, 80ull),
            memory_item_request.message);

  // We still populate future data, as the strategy assumes we will be
  // requesting the data.
  BlobDataBuilder expected_builder(kNewUUID);
  expected_builder.AppendFutureData(10);
  expected_builder.AppendBlob(kFakeBlobUUID);
  expected_builder.AppendFutureData(80);
  EXPECT_EQ(expected_builder, builder);
}
}  // namespace
}  // namespace storage
