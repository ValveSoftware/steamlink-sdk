// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blob_storage/blob_transport_controller.h"

#include <stddef.h>
#include <stdint.h>
#include <tuple>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/test/test_file_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/blob_storage/blob_consolidation.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/fileapi/webblob_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/blob_storage/blob_item_bytes_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::File;
using base::FilePath;
using base::TestSimpleTaskRunner;
using storage::BlobItemBytesRequest;
using storage::BlobItemBytesResponse;
using storage::DataElement;
using storage::IPCBlobCreationCancelCode;

namespace content {
namespace {

class OtherThreadTestSimpleTaskRunner : public base::TestSimpleTaskRunner {
 public:
  bool RunsTasksOnCurrentThread() const override { return false; }

 protected:
  ~OtherThreadTestSimpleTaskRunner() override {}
};

class BlobTransportControllerTestSender : public ThreadSafeSender {
 public:
  explicit BlobTransportControllerTestSender(IPC::TestSink* ipc_sink)
      : ThreadSafeSender(nullptr, nullptr), ipc_sink_(ipc_sink) {}

  bool Send(IPC::Message* message) override { return ipc_sink_->Send(message); }

 private:
  ~BlobTransportControllerTestSender() override {}

  IPC::TestSink* ipc_sink_;

  DISALLOW_COPY_AND_ASSIGN(BlobTransportControllerTestSender);
};

BlobItemBytesResponse ResponseWithData(size_t request_number,
                                       const std::string& str) {
  BlobItemBytesResponse response(request_number);
  response.inline_data.assign(str.begin(), str.end());
  return response;
}

static DataElement MakeBlobElement(const std::string& uuid,
                                   uint64_t offset,
                                   uint64_t size) {
  DataElement element;
  element.SetToBlobRange(uuid, offset, size);
  return element;
}

static DataElement MakeDataDescriptionElement(size_t size) {
  DataElement element;
  element.SetToBytesDescription(size);
  return element;
}

static DataElement MakeDataElement(const std::string& str) {
  DataElement element;
  element.SetToBytes(str.c_str(), str.size());
  return element;
}

static blink::WebThreadSafeData CreateData(const std::string& str) {
  return blink::WebThreadSafeData(str.c_str(), str.size());
}

}  // namespace

class BlobTransportControllerTest : public testing::Test {
 public:
  BlobTransportControllerTest() : thread_runner_handle_(io_thread_runner_) {}

  void SetUp() override {
    sender_ = new BlobTransportControllerTestSender(&sink_);
    BlobTransportController::GetInstance()->CancelAllBlobTransfers();
  }

  void OnMemoryRequest(
      BlobTransportController* holder,
      const std::string uuid,
      const std::vector<storage::BlobItemBytesRequest>& requests,
      std::vector<base::SharedMemoryHandle>* memory_handles,
      std::vector<IPC::PlatformFileForTransit> file_handles) {
    holder->OnMemoryRequest(uuid, requests, memory_handles, file_handles,
                            file_thread_runner_.get(), &sink_);
  }

  FilePath CreateTemporaryFile() {
    FilePath path;
    EXPECT_TRUE(base::CreateTemporaryFile(&path));
    files_opened_.push_back(path);
    return path;
  }

  void ExpectRegisterAndStartMessage(const std::string& expected_uuid,
                                     const std::string& expected_content_type,
                                     std::vector<DataElement>* descriptions) {
    const IPC::Message* register_message =
        sink_.GetUniqueMessageMatching(BlobStorageMsg_RegisterBlobUUID::ID);
    const IPC::Message* start_message =
        sink_.GetUniqueMessageMatching(BlobStorageMsg_StartBuildingBlob::ID);
    ASSERT_TRUE(register_message);
    ASSERT_TRUE(start_message);
    std::tuple<std::string, std::string, std::string, std::set<std::string>>
        register_contents;
    std::tuple<std::string, std::vector<DataElement>> start_contents;
    BlobStorageMsg_RegisterBlobUUID::Read(register_message, &register_contents);
    BlobStorageMsg_StartBuildingBlob::Read(start_message, &start_contents);
    EXPECT_EQ(expected_uuid, std::get<0>(register_contents));
    EXPECT_EQ(expected_uuid, std::get<0>(start_contents));
    EXPECT_EQ(expected_content_type, std::get<1>(register_contents));
    if (descriptions)
      *descriptions = std::get<1>(start_contents);
    // We don't have dispositions from the renderer.
    EXPECT_TRUE(std::get<2>(register_contents).empty());
    sink_.ClearMessages();
  }

  void ExpectMemoryResponses(
      const std::string& expected_uuid,
      std::vector<storage::BlobItemBytesResponse> expected_responses) {
    const IPC::Message* responses_message =
        sink_.GetUniqueMessageMatching(BlobStorageMsg_MemoryItemResponse::ID);
    ASSERT_TRUE(responses_message);
    std::tuple<std::string, std::vector<storage::BlobItemBytesResponse>>
        responses_content;
    BlobStorageMsg_MemoryItemResponse::Read(responses_message,
                                            &responses_content);
    EXPECT_EQ(expected_uuid, std::get<0>(responses_content));
    EXPECT_EQ(expected_responses, std::get<1>(responses_content));
    sink_.ClearMessages();
  }

  void ExpectCancel(const std::string& expected_uuid,
                    storage::IPCBlobCreationCancelCode expected_code) {
    const IPC::Message* cancel_message =
        sink_.GetUniqueMessageMatching(BlobStorageMsg_CancelBuildingBlob::ID);
    ASSERT_TRUE(cancel_message);
    std::tuple<std::string, storage::IPCBlobCreationCancelCode> cancel_content;
    BlobStorageMsg_CancelBuildingBlob::Read(cancel_message, &cancel_content);
    EXPECT_EQ(expected_uuid, std::get<0>(cancel_content));
    EXPECT_EQ(expected_code, std::get<1>(cancel_content));
  }

  void TearDown() override {
    BlobTransportController::GetInstance()->CancelAllBlobTransfers();
    for (const FilePath& path : files_opened_) {
      EXPECT_TRUE(base::DeleteFile(path, false));
    }
    EXPECT_FALSE(io_thread_runner_->HasPendingTask());
    EXPECT_FALSE(file_thread_runner_->HasPendingTask());
    EXPECT_FALSE(main_thread_runner_->HasPendingTask());
  }

 protected:
  std::vector<FilePath> files_opened_;

  IPC::TestSink sink_;
  scoped_refptr<BlobTransportControllerTestSender> sender_;

  // Thread runners.
  scoped_refptr<base::TestSimpleTaskRunner> io_thread_runner_ =
      new TestSimpleTaskRunner();
  scoped_refptr<base::TestSimpleTaskRunner> file_thread_runner_ =
      new TestSimpleTaskRunner();
  scoped_refptr<OtherThreadTestSimpleTaskRunner> main_thread_runner_ =
      new OtherThreadTestSimpleTaskRunner();

  // We set this to the IO thread runner, as this is used for the
  // OnMemoryRequest calls, which are on that thread.
  base::ThreadTaskRunnerHandle thread_runner_handle_;
};

TEST_F(BlobTransportControllerTest, Descriptions) {
  const std::string kBlobUUID = "uuid";
  const std::string KRefBlobUUID = "refuuid";
  const std::string kBadBlobUUID = "uuuidBad";
  const size_t kShortcutSize = 11;

  // The first two data elements should be combined and the data shortcut.
  scoped_refptr<BlobConsolidation> consolidation(new BlobConsolidation());
  consolidation->AddBlobItem(KRefBlobUUID, 10, 10);
  consolidation->AddDataItem(CreateData("Hello"));
  consolidation->AddDataItem(CreateData("Hello2"));
  consolidation->AddBlobItem(KRefBlobUUID, 0, 10);
  consolidation->AddDataItem(CreateData("Hello3"));

  std::vector<DataElement> out;
  BlobTransportController::GetDescriptions(consolidation.get(), kShortcutSize,
                                           &out);

  std::vector<DataElement> expected;
  expected.push_back(MakeBlobElement(KRefBlobUUID, 10, 10));
  expected.push_back(MakeDataElement("HelloHello2"));
  expected.push_back(MakeBlobElement(KRefBlobUUID, 0, 10));
  expected.push_back(MakeDataDescriptionElement(6));
  EXPECT_EQ(expected, out);
}

TEST_F(BlobTransportControllerTest, Responses) {
  const std::string kBlobUUID = "uuid";
  const std::string KRefBlobUUID = "refuuid";
  const std::string kBadBlobUUID = "uuuidBad";
  BlobTransportController* holder = BlobTransportController::GetInstance();

  // The first two data elements should be combined.
  scoped_refptr<BlobConsolidation> consolidation = new BlobConsolidation();
  consolidation->AddBlobItem(KRefBlobUUID, 10, 10);
  consolidation->AddDataItem(CreateData("Hello"));
  consolidation->AddDataItem(CreateData("Hello2"));
  consolidation->AddBlobItem(KRefBlobUUID, 0, 10);
  consolidation->AddDataItem(CreateData("Hello3"));
  // See the above test for the expected descriptions layout.

  holder->blob_storage_[kBlobUUID] = consolidation;

  std::vector<BlobItemBytesRequest> requests;
  std::vector<base::SharedMemoryHandle> memory_handles;
  std::vector<IPC::PlatformFileForTransit> file_handles;

  // Request for all of first data
  requests.push_back(BlobItemBytesRequest::CreateIPCRequest(0, 1, 0, 11));
  OnMemoryRequest(holder, kBlobUUID, requests, &memory_handles, file_handles);
  std::vector<storage::BlobItemBytesResponse> expected;
  expected.push_back(ResponseWithData(0, "HelloHello2"));
  ExpectMemoryResponses(kBlobUUID, expected);

  // Part of second data
  requests[0] = BlobItemBytesRequest::CreateIPCRequest(1000, 3, 1, 5);
  OnMemoryRequest(holder, kBlobUUID, requests, &memory_handles, file_handles);
  expected.clear();
  expected.push_back(ResponseWithData(1000, "ello3"));
  ExpectMemoryResponses(kBlobUUID, expected);

  // Both data segments
  requests[0] = BlobItemBytesRequest::CreateIPCRequest(0, 1, 0, 11);
  requests.push_back(BlobItemBytesRequest::CreateIPCRequest(1, 3, 0, 6));
  OnMemoryRequest(holder, kBlobUUID, requests, &memory_handles, file_handles);
  expected.clear();
  expected.push_back(ResponseWithData(0, "HelloHello2"));
  expected.push_back(ResponseWithData(1, "Hello3"));
  ExpectMemoryResponses(kBlobUUID, expected);
}

TEST_F(BlobTransportControllerTest, SharedMemory) {
  const std::string kBlobUUID = "uuid";
  const std::string KRefBlobUUID = "refuuid";
  const std::string kBadBlobUUID = "uuuidBad";
  BlobTransportController* holder = BlobTransportController::GetInstance();

  // The first two data elements should be combined.
  scoped_refptr<BlobConsolidation> consolidation = new BlobConsolidation();
  consolidation->AddBlobItem(KRefBlobUUID, 10, 10);
  consolidation->AddDataItem(CreateData("Hello"));
  consolidation->AddDataItem(CreateData("Hello2"));
  consolidation->AddBlobItem(KRefBlobUUID, 0, 10);
  consolidation->AddDataItem(CreateData("Hello3"));
  // See the above test for the expected descriptions layout.

  holder->blob_storage_[kBlobUUID] = consolidation;

  std::vector<BlobItemBytesRequest> requests;
  std::vector<base::SharedMemoryHandle> memory_handles;
  std::vector<IPC::PlatformFileForTransit> file_handles;

  // Request for all data in shared memory
  requests.push_back(
      BlobItemBytesRequest::CreateSharedMemoryRequest(0, 1, 0, 11, 0, 0));
  requests.push_back(
      BlobItemBytesRequest::CreateSharedMemoryRequest(1, 3, 0, 6, 0, 11));
  base::SharedMemory memory;
  memory.CreateAnonymous(11 + 6);
  base::SharedMemoryHandle handle =
      base::SharedMemory::DuplicateHandle(memory.handle());
  CHECK(base::SharedMemory::NULLHandle() != handle);
  memory_handles.push_back(handle);

  OnMemoryRequest(holder, kBlobUUID, requests, &memory_handles, file_handles);
  std::vector<storage::BlobItemBytesResponse> expected;
  expected.push_back(BlobItemBytesResponse(0));
  expected.push_back(BlobItemBytesResponse(1));
  ExpectMemoryResponses(kBlobUUID, expected);
  std::string expected_memory = "HelloHello2Hello3";
  memory.Map(11 + 6);
  const char* mem_location = static_cast<const char*>(memory.memory());
  std::vector<char> value(mem_location, mem_location + memory.requested_size());
  EXPECT_THAT(value, testing::ElementsAreArray(expected_memory.c_str(),
                                               expected_memory.size()));
}

TEST_F(BlobTransportControllerTest, Disk) {
  const std::string kBlobUUID = "uuid";
  const std::string kRefBlobUUID = "refuuid";
  const std::string kBadBlobUUID = "uuuidBad";
  const std::string kDataPart1 = "Hello";
  const std::string kDataPart2 = "Hello2";
  const std::string kDataPart3 = "Hello3";
  const std::string kData = "HelloHello2Hello3";
  BlobTransportController* holder = BlobTransportController::GetInstance();
  FilePath path1 = CreateTemporaryFile();
  File file(path1, File::FLAG_OPEN | File::FLAG_WRITE | File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());
  ASSERT_TRUE(file.SetLength(11 + 6));
  // The first two data elements should be combined.
  scoped_refptr<BlobConsolidation> consolidation(new BlobConsolidation());
  consolidation->AddBlobItem(kRefBlobUUID, 10, 10);
  consolidation->AddDataItem(CreateData(kDataPart1));
  consolidation->AddDataItem(CreateData(kDataPart2));
  consolidation->AddBlobItem(kRefBlobUUID, 0, 10);
  consolidation->AddDataItem(CreateData(kDataPart3));
  // See the above test for the expected descriptions layout.
  holder->blob_storage_[kBlobUUID] = consolidation;
  holder->main_thread_runner_ = main_thread_runner_;
  std::vector<BlobItemBytesRequest> requests;
  std::vector<base::SharedMemoryHandle> memory_handles;
  std::vector<IPC::PlatformFileForTransit> file_handles;
  // Request for all data in files.
  requests.push_back(
      BlobItemBytesRequest::CreateFileRequest(0, 1, 0, 11, 0, 0));
  requests.push_back(
      BlobItemBytesRequest::CreateFileRequest(1, 3, 0, 6, 0, 11));
  file_handles.push_back(IPC::TakePlatformFileForTransit(std::move(file)));
  OnMemoryRequest(holder, kBlobUUID, requests, &memory_handles, file_handles);
  file_handles.clear();
  EXPECT_TRUE(file_thread_runner_->HasPendingTask());
  EXPECT_FALSE(io_thread_runner_->HasPendingTask());
  file_thread_runner_->RunPendingTasks();
  EXPECT_FALSE(file_thread_runner_->HasPendingTask());
  EXPECT_TRUE(io_thread_runner_->HasPendingTask());
  io_thread_runner_->RunPendingTasks();
  std::vector<storage::BlobItemBytesResponse> expected = {
      BlobItemBytesResponse(0), BlobItemBytesResponse(1)};
  ExpectMemoryResponses(kBlobUUID, expected);
  file = File(path1, File::FLAG_OPEN | File::FLAG_READ);
  char data[11 + 6];
  file.Read(0, data, 11 + 6);
  std::vector<char> value(data, data + 11 + 6);
  EXPECT_THAT(value, testing::ElementsAreArray(kData.c_str(), kData.size()));

  // Finally, test that we get errors correctly.
  FilePath path2 = CreateTemporaryFile();
  EXPECT_TRUE(base::MakeFileUnwritable(path2));
  File file2(path2, File::FLAG_OPEN | File::FLAG_WRITE);
  EXPECT_FALSE(file2.IsValid());
  file_handles.push_back(IPC::TakePlatformFileForTransit(std::move(file2)));
  OnMemoryRequest(holder, kBlobUUID, requests, &memory_handles, file_handles);
  EXPECT_TRUE(file_thread_runner_->HasPendingTask());
  file_thread_runner_->RunPendingTasks();
  EXPECT_TRUE(io_thread_runner_->HasPendingTask());
  io_thread_runner_->RunPendingTasks();
  // Clear the main thread task, as it has the AddRef job.
  EXPECT_TRUE(main_thread_runner_->HasPendingTask());
  main_thread_runner_->ClearPendingTasks();
  ExpectCancel(kBlobUUID, IPCBlobCreationCancelCode::FILE_WRITE_FAILED);
}

TEST_F(BlobTransportControllerTest, PublicMethods) {
  const std::string kBlobUUID = "uuid";
  const std::string kBlobContentType = "content_type";
  const std::string kBlob2UUID = "uuid2";
  const std::string kBlob2ContentType = "content_type2";
  const std::string KRefBlobUUID = "refuuid";
  const std::string kDataPart1 = "Hello";
  const std::string kDataPart2 = "Hello2";
  const std::string kDataPart3 = "Hello3";
  const std::string kData = "HelloHello2Hello3";
  std::vector<DataElement> message_descriptions;
  BlobTransportController* holder = BlobTransportController::GetInstance();

  // Here we test that the
  scoped_refptr<BlobConsolidation> consolidation = new BlobConsolidation();
  consolidation->AddBlobItem(KRefBlobUUID, 10, 10);
  BlobTransportController::InitiateBlobTransfer(
      kBlobUUID, kBlobContentType, consolidation, sender_,
      io_thread_runner_.get(), main_thread_runner_);
  // Check that we have the 'increase ref' pending task.
  EXPECT_TRUE(main_thread_runner_->HasPendingTask());
  // Check that we have the 'store' pending task.
  EXPECT_TRUE(io_thread_runner_->HasPendingTask());
  // Check that we've sent the data.
  ExpectRegisterAndStartMessage(kBlobUUID, kBlobContentType,
                                &message_descriptions);
  main_thread_runner_->ClearPendingTasks();

  // Check that we got the correct start message.
  EXPECT_FALSE(holder->IsTransporting(kBlobUUID));
  io_thread_runner_->RunPendingTasks();
  EXPECT_TRUE(holder->IsTransporting(kBlobUUID));
  std::tuple<std::string, std::vector<DataElement>> message_contents;
  EXPECT_TRUE(holder->IsTransporting(kBlobUUID));
  EXPECT_EQ(MakeBlobElement(KRefBlobUUID, 10, 10), message_descriptions[0]);

  holder->OnCancel(kBlobUUID, IPCBlobCreationCancelCode::OUT_OF_MEMORY);
  EXPECT_FALSE(holder->IsTransporting(kBlobUUID));
  // Check we have the 'decrease ref' task.
  EXPECT_TRUE(main_thread_runner_->HasPendingTask());
  main_thread_runner_->ClearPendingTasks();
  sink_.ClearMessages();

  // Add the second.
  scoped_refptr<BlobConsolidation> consolidation2 = new BlobConsolidation();
  consolidation2->AddBlobItem(KRefBlobUUID, 10, 10);
  // These items should be combined.
  consolidation2->AddDataItem(CreateData(kDataPart1));
  consolidation2->AddDataItem(CreateData(kDataPart2));
  consolidation2->AddDataItem(CreateData(kDataPart3));
  BlobTransportController::InitiateBlobTransfer(
      kBlob2UUID, kBlob2ContentType, consolidation2, sender_,
      io_thread_runner_.get(), main_thread_runner_);
  // Check that we have the 'increase ref' pending task.
  EXPECT_TRUE(main_thread_runner_->HasPendingTask());
  // Check that we have the 'store' pending task.
  EXPECT_TRUE(io_thread_runner_->HasPendingTask());
  // Check that we've sent the data.
  message_descriptions.clear();
  ExpectRegisterAndStartMessage(kBlob2UUID, kBlob2ContentType,
                                &message_descriptions);
  main_thread_runner_->ClearPendingTasks();

  // Check that we got the correct start message.
  EXPECT_FALSE(holder->IsTransporting(kBlob2UUID));
  io_thread_runner_->RunPendingTasks();
  EXPECT_TRUE(holder->IsTransporting(kBlob2UUID));
  ASSERT_EQ(2u, message_descriptions.size());
  EXPECT_EQ(MakeBlobElement(KRefBlobUUID, 10, 10), message_descriptions[0]);
  EXPECT_EQ(MakeDataElement(kData), message_descriptions[1]);
  EXPECT_FALSE(main_thread_runner_->HasPendingTask());

  // Now we request the memory.
  std::vector<BlobItemBytesRequest> requests;
  std::vector<base::SharedMemoryHandle> memory_handles;
  std::vector<IPC::PlatformFileForTransit> file_handles;
  requests.push_back(
      BlobItemBytesRequest::CreateIPCRequest(0, 1, 0, kData.size()));
  OnMemoryRequest(holder, kBlob2UUID, requests, &memory_handles, file_handles);

  std::vector<BlobItemBytesResponse> expected_responses;
  BlobItemBytesResponse expected(0);
  expected.inline_data = std::vector<char>(kData.begin(), kData.end());
  expected_responses.push_back(expected);
  ExpectMemoryResponses(kBlob2UUID, expected_responses);
  EXPECT_FALSE(main_thread_runner_->HasPendingTask());

  // Finish the second one.
  holder->OnDone(kBlob2UUID);
  EXPECT_FALSE(holder->IsTransporting(kBlob2UUID));
  EXPECT_TRUE(main_thread_runner_->HasPendingTask());
  main_thread_runner_->ClearPendingTasks();
}

}  // namespace content
