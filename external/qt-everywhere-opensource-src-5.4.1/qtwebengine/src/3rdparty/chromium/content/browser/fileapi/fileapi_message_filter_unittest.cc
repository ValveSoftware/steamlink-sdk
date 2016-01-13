// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/fileapi_message_filter.h"

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/streams/stream_registry.h"
#include "content/common/fileapi/file_system_messages.h"
#include "content/common/fileapi/webblob_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_file_system_context.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/blob/blob_storage_context.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/common/blob/blob_data.h"

namespace content {

namespace {

const char kFakeBlobInternalUrlSpec[] =
    "blob:blobinternal%3A///dc83ede4-9bbd-453b-be2e-60fd623fcc93";
const char kFakeBlobInternalUrlSpec2[] =
    "blob:blobinternal%3A///d28ae2e7-d233-4dda-9598-d135fe5d403e";

const char kFakeContentType[] = "fake/type";

}  // namespace

class FileAPIMessageFilterTest : public testing::Test {
 public:
  FileAPIMessageFilterTest()
      : io_browser_thread_(BrowserThread::IO, &message_loop_) {
  }

 protected:
  virtual void SetUp() OVERRIDE {
    file_system_context_ =
        CreateFileSystemContextForTesting(NULL, base::FilePath());

    std::vector<fileapi::FileSystemType> types;
    file_system_context_->GetFileSystemTypes(&types);
    for (size_t i = 0; i < types.size(); ++i) {
      ChildProcessSecurityPolicyImpl::GetInstance()->
          RegisterFileSystemPermissionPolicy(
              types[i],
              fileapi::FileSystemContext::GetPermissionPolicy(types[i]));
    }

    stream_context_ = StreamContext::GetFor(&browser_context_);
    blob_storage_context_ = ChromeBlobStorageContext::GetFor(&browser_context_);

    filter_ = new FileAPIMessageFilter(
        0 /* process_id */,
        browser_context_.GetRequestContext(),
        file_system_context_.get(),
        blob_storage_context_,
        stream_context_);

    // Complete initialization.
    message_loop_.RunUntilIdle();
  }

  base::MessageLoop message_loop_;
  TestBrowserThread io_browser_thread_;

  TestBrowserContext browser_context_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  StreamContext* stream_context_;
  ChromeBlobStorageContext* blob_storage_context_;

  scoped_refptr<FileAPIMessageFilter> filter_;
};

TEST_F(FileAPIMessageFilterTest, CloseChannelWithInflightRequest) {
  scoped_refptr<FileAPIMessageFilter> filter(
      new FileAPIMessageFilter(
          0 /* process_id */,
          browser_context_.GetRequestContext(),
          file_system_context_.get(),
          ChromeBlobStorageContext::GetFor(&browser_context_),
          StreamContext::GetFor(&browser_context_)));
  filter->OnChannelConnected(0);

  // Complete initialization.
  message_loop_.RunUntilIdle();

  int request_id = 0;
  const GURL kUrl("filesystem:http://example.com/temporary/foo");
  FileSystemHostMsg_ReadMetadata read_metadata(request_id++, kUrl);
  EXPECT_TRUE(filter->OnMessageReceived(read_metadata));

  // Close the filter while it has inflight request.
  filter->OnChannelClosing();

  // This shouldn't cause DCHECK failure.
  message_loop_.RunUntilIdle();
}

TEST_F(FileAPIMessageFilterTest, MultipleFilters) {
  scoped_refptr<FileAPIMessageFilter> filter1(
      new FileAPIMessageFilter(
          0 /* process_id */,
          browser_context_.GetRequestContext(),
          file_system_context_.get(),
          ChromeBlobStorageContext::GetFor(&browser_context_),
          StreamContext::GetFor(&browser_context_)));
  scoped_refptr<FileAPIMessageFilter> filter2(
      new FileAPIMessageFilter(
          1 /* process_id */,
          browser_context_.GetRequestContext(),
          file_system_context_.get(),
          ChromeBlobStorageContext::GetFor(&browser_context_),
          StreamContext::GetFor(&browser_context_)));
  filter1->OnChannelConnected(0);
  filter2->OnChannelConnected(1);

  // Complete initialization.
  message_loop_.RunUntilIdle();

  int request_id = 0;
  const GURL kUrl("filesystem:http://example.com/temporary/foo");
  FileSystemHostMsg_ReadMetadata read_metadata(request_id++, kUrl);
  EXPECT_TRUE(filter1->OnMessageReceived(read_metadata));

  // Close the other filter before the request for filter1 is processed.
  filter2->OnChannelClosing();

  // This shouldn't cause DCHECK failure.
  message_loop_.RunUntilIdle();
}

TEST_F(FileAPIMessageFilterTest, BuildEmptyStream) {
  StreamRegistry* stream_registry = stream_context_->registry();

  const GURL kUrl(kFakeBlobInternalUrlSpec);
  const GURL kDifferentUrl("blob:barfoo");

  EXPECT_EQ(NULL, stream_registry->GetStream(kUrl).get());

  StreamHostMsg_StartBuilding start_message(kUrl, kFakeContentType);
  EXPECT_TRUE(filter_->OnMessageReceived(start_message));

  const int kBufferSize = 10;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));
  int bytes_read = 0;

  scoped_refptr<Stream> stream = stream_registry->GetStream(kUrl);
  // Stream becomes available for read right after registration.
  ASSERT_FALSE(stream.get() == NULL);
  EXPECT_EQ(Stream::STREAM_EMPTY,
            stream->ReadRawData(buffer.get(), kBufferSize, &bytes_read));
  EXPECT_EQ(0, bytes_read);
  stream = NULL;

  StreamHostMsg_FinishBuilding finish_message(kUrl);
  EXPECT_TRUE(filter_->OnMessageReceived(finish_message));

  stream = stream_registry->GetStream(kUrl);
  ASSERT_FALSE(stream.get() == NULL);
  EXPECT_EQ(Stream::STREAM_EMPTY,
            stream->ReadRawData(buffer.get(), kBufferSize, &bytes_read));
  EXPECT_EQ(0, bytes_read);

  // Run loop to finish transfer.
  message_loop_.RunUntilIdle();

  EXPECT_EQ(Stream::STREAM_COMPLETE,
            stream->ReadRawData(buffer.get(), kBufferSize, &bytes_read));
  EXPECT_EQ(0, bytes_read);

  // Nothing should be returned for a URL we didn't use.
  EXPECT_TRUE(stream_registry->GetStream(kDifferentUrl).get() == NULL);
}

TEST_F(FileAPIMessageFilterTest, BuildNonEmptyStream) {
  StreamRegistry* stream_registry = stream_context_->registry();

  const GURL kUrl(kFakeBlobInternalUrlSpec);

  EXPECT_EQ(NULL, stream_registry->GetStream(kUrl).get());

  StreamHostMsg_StartBuilding start_message(kUrl, kFakeContentType);
  EXPECT_TRUE(filter_->OnMessageReceived(start_message));

  webkit_blob::BlobData::Item item;
  const std::string kFakeData = "foobarbaz";
  item.SetToBytes(kFakeData.data(), kFakeData.size());
  StreamHostMsg_AppendBlobDataItem append_message(kUrl, item);
  EXPECT_TRUE(filter_->OnMessageReceived(append_message));

  StreamHostMsg_FinishBuilding finish_message(kUrl);
  EXPECT_TRUE(filter_->OnMessageReceived(finish_message));

  // Run loop to finish transfer and commit finalize command.
  message_loop_.RunUntilIdle();

  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kFakeData.size()));
  int bytes_read = 0;

  scoped_refptr<Stream> stream = stream_registry->GetStream(kUrl);
  ASSERT_FALSE(stream.get() == NULL);

  EXPECT_EQ(Stream::STREAM_HAS_DATA,
            stream->ReadRawData(buffer.get(), kFakeData.size(), &bytes_read));
  EXPECT_EQ(kFakeData.size(), static_cast<size_t>(bytes_read));
  EXPECT_EQ(kFakeData, std::string(buffer->data(), bytes_read));

  EXPECT_EQ(Stream::STREAM_COMPLETE,
            stream->ReadRawData(buffer.get(), kFakeData.size(), &bytes_read));
  EXPECT_EQ(0, bytes_read);
}

TEST_F(FileAPIMessageFilterTest, BuildStreamWithSharedMemory) {
  StreamRegistry* stream_registry = stream_context_->registry();

  const GURL kUrl(kFakeBlobInternalUrlSpec);

  EXPECT_EQ(NULL, stream_registry->GetStream(kUrl).get());

  // For win, we need to set valid PID to the filter.
  // OnAppendSharedMemoryToStream passes the peer process's handle to
  // SharedMemory's constructor. If it's incorrect, DuplicateHandle won't work
  // correctly.
  filter_->set_peer_pid_for_testing(base::Process::Current().pid());

  StreamHostMsg_StartBuilding start_message(kUrl, kFakeContentType);
  EXPECT_TRUE(filter_->OnMessageReceived(start_message));

  const std::string kFakeData = "foobarbaz";

  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory);
  ASSERT_TRUE(shared_memory->CreateAndMapAnonymous(kFakeData.size()));
  memcpy(shared_memory->memory(), kFakeData.data(), kFakeData.size());
  StreamHostMsg_SyncAppendSharedMemory append_message(
      kUrl, shared_memory->handle(), kFakeData.size());
  EXPECT_TRUE(filter_->OnMessageReceived(append_message));

  StreamHostMsg_FinishBuilding finish_message(kUrl);
  EXPECT_TRUE(filter_->OnMessageReceived(finish_message));

  // Run loop to finish transfer and commit finalize command.
  message_loop_.RunUntilIdle();

  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kFakeData.size()));
  int bytes_read = 0;

  scoped_refptr<Stream> stream = stream_registry->GetStream(kUrl);
  ASSERT_FALSE(stream.get() == NULL);

  EXPECT_EQ(Stream::STREAM_HAS_DATA,
            stream->ReadRawData(buffer.get(), kFakeData.size(), &bytes_read));
  EXPECT_EQ(kFakeData.size(), static_cast<size_t>(bytes_read));
  EXPECT_EQ(kFakeData, std::string(buffer->data(), bytes_read));

  EXPECT_EQ(Stream::STREAM_COMPLETE,
            stream->ReadRawData(buffer.get(), kFakeData.size(), &bytes_read));
  EXPECT_EQ(0, bytes_read);
}

TEST_F(FileAPIMessageFilterTest, BuildStreamAndCallOnChannelClosing) {
  StreamRegistry* stream_registry = stream_context_->registry();

  const GURL kUrl(kFakeBlobInternalUrlSpec);

  StreamHostMsg_StartBuilding start_message(kUrl, kFakeContentType);
  EXPECT_TRUE(filter_->OnMessageReceived(start_message));

  ASSERT_FALSE(stream_registry->GetStream(kUrl).get() == NULL);

  filter_->OnChannelClosing();

  ASSERT_EQ(NULL, stream_registry->GetStream(kUrl).get());
}

TEST_F(FileAPIMessageFilterTest, CloneStream) {
  StreamRegistry* stream_registry = stream_context_->registry();

  const GURL kUrl(kFakeBlobInternalUrlSpec);
  const GURL kDestUrl(kFakeBlobInternalUrlSpec2);

  StreamHostMsg_StartBuilding start_message(kUrl, kFakeContentType);
  EXPECT_TRUE(filter_->OnMessageReceived(start_message));

  StreamHostMsg_Clone clone_message(kDestUrl, kUrl);
  EXPECT_TRUE(filter_->OnMessageReceived(clone_message));

  ASSERT_FALSE(stream_registry->GetStream(kUrl).get() == NULL);
  ASSERT_FALSE(stream_registry->GetStream(kDestUrl).get() == NULL);
}

}  // namespace content
