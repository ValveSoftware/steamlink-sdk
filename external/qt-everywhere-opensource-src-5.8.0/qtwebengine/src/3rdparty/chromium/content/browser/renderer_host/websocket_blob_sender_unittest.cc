// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/websocket_blob_sender.h"

#include <string.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/common/fileapi/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kDummyUrl[] = "http://www.example.com/";
const char kBanana[] = "banana";

// This is small so that the tests do not waste too much time just copying bytes
// around. But it has to be larger than kMinimumNonFinalFrameSize defined in
// websocket_blob_sender.cc.
const size_t kInitialQuota = 16 * 1024;

using net::TestCompletionCallback;

// A fake channel for testing. Records the contents of the message that was sent
// through it. Quota is restricted, and is refreshed asynchronously in response
// to calls to SendFrame().
class FakeChannel : public WebSocketBlobSender::Channel {
 public:
  // |notify_new_quota| will be run asynchronously on the current MessageLoop
  // every time GetSendQuota() increases.
  FakeChannel() : weak_factory_(this) {}

  // This method must be called before SendFrame() is.
  void set_notify_new_quota(const base::Closure& notify_new_quota) {
    notify_new_quota_ = notify_new_quota;
  }

  size_t GetSendQuota() const override { return current_send_quota_; }

  ChannelState SendFrame(bool fin, const std::vector<char>& data) override {
    ++frames_sent_;
    EXPECT_FALSE(got_fin_);
    if (fin)
      got_fin_ = true;
    EXPECT_LE(data.size(), current_send_quota_);
    message_.insert(message_.end(), data.begin(), data.end());
    current_send_quota_ -= data.size();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&FakeChannel::RefreshQuota, weak_factory_.GetWeakPtr()));
    return net::WebSocketEventInterface::CHANNEL_ALIVE;
  }

  bool got_fin() const { return got_fin_; }

  int frames_sent() const { return frames_sent_; }

  const std::vector<char>& message() const { return message_; }

 private:
  void RefreshQuota() {
    if (current_send_quota_ == kInitialQuota)
      return;
    current_send_quota_ = kInitialQuota;
    DCHECK(!notify_new_quota_.is_null());
    notify_new_quota_.Run();
  }

  base::Closure notify_new_quota_;
  size_t current_send_quota_ = kInitialQuota;
  int frames_sent_ = 0;
  bool got_fin_ = false;
  std::vector<char> message_;
  base::WeakPtrFactory<FakeChannel> weak_factory_;
};

class WebSocketBlobSenderTest : public ::testing::Test {
 protected:
  // The Windows implementation of net::FileStream::Context requires a real IO
  // MessageLoop.
  WebSocketBlobSenderTest()
      : threads_(TestBrowserThreadBundle::IO_MAINLOOP),
        chrome_blob_storage_context_(
            ChromeBlobStorageContext::GetFor(&browser_context_)),
        fake_channel_(nullptr),
        sender_() {}
  ~WebSocketBlobSenderTest() override {}

  void SetUp() override {
    // ChromeBlobStorageContext::GetFor() does some work asynchronously.
    base::RunLoop().RunUntilIdle();
    SetUpSender();
  }

  // This method can be overriden to use a different channel implementation.
  virtual void SetUpSender() {
    fake_channel_ = new FakeChannel;
    sender_.reset(new WebSocketBlobSender(base::WrapUnique(fake_channel_)));
    fake_channel_->set_notify_new_quota(base::Bind(
        &WebSocketBlobSender::OnNewSendQuota, base::Unretained(sender_.get())));
  }

  storage::BlobStorageContext* context() {
    return chrome_blob_storage_context_->context();
  }

  storage::FileSystemContext* GetFileSystemContext() {
    StoragePartition* partition = BrowserContext::GetStoragePartitionForSite(
        &browser_context_, GURL(kDummyUrl));
    return partition->GetFileSystemContext();
  }

  // |string| is copied.
  std::unique_ptr<BlobHandle> CreateMemoryBackedBlob(const char* string) {
    std::unique_ptr<BlobHandle> handle =
        chrome_blob_storage_context_->CreateMemoryBackedBlob(string,
                                                             strlen(string));
    EXPECT_TRUE(handle);
    return handle;
  }

  // Call sender_.Start() with the other parameters filled in appropriately for
  // this test fixture.
  int Start(const std::string& uuid,
            uint64_t expected_size,
            const net::CompletionCallback& callback) {
    net::WebSocketEventInterface::ChannelState channel_state =
        net::WebSocketEventInterface::CHANNEL_ALIVE;
    return sender_->Start(
        uuid, expected_size, context(), GetFileSystemContext(),
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE).get(),
        &channel_state, callback);
  }

  void NotCalledCallbackImpl(int rv) {
    ADD_FAILURE()
        << "Callback that should not be called was called with argument " << rv;
  }

  net::CompletionCallback NotCalled() {
    return base::Bind(&WebSocketBlobSenderTest::NotCalledCallbackImpl,
                      base::Unretained(this));
  }

  void ExpectOkAndQuit(base::RunLoop* run_loop, int result) {
    EXPECT_EQ(net::OK, result);
    run_loop->Quit();
  }

  net::CompletionCallback ExpectOkAndQuitCallback(base::RunLoop* run_loop) {
    return base::Bind(&WebSocketBlobSenderTest::ExpectOkAndQuit,
                      base::Unretained(this), run_loop);
  }

  TestBrowserThreadBundle threads_;
  TestBrowserContext browser_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_storage_context_;
  // |fake_channel_| is owned by |sender_|.
  FakeChannel* fake_channel_;
  std::unique_ptr<WebSocketBlobSender> sender_;
};

TEST_F(WebSocketBlobSenderTest, Construction) {}

TEST_F(WebSocketBlobSenderTest, EmptyBlob) {
  std::unique_ptr<BlobHandle> handle = CreateMemoryBackedBlob("");

  // The APIs allow for this to be asynchronous but that is unlikely in
  // practice.
  int result = Start(handle->GetUUID(), UINT64_C(0), NotCalled());
  // If this fails with result == -1, someone has changed the code to be
  // asynchronous and this test should be adapted to match.
  EXPECT_EQ(net::OK, result);
  EXPECT_TRUE(fake_channel_->got_fin());
  EXPECT_EQ(0U, fake_channel_->message().size());
}

TEST_F(WebSocketBlobSenderTest, SmallBlob) {
  std::unique_ptr<BlobHandle> handle = CreateMemoryBackedBlob(kBanana);

  EXPECT_EQ(net::OK, Start(handle->GetUUID(), UINT64_C(6), NotCalled()));
  EXPECT_TRUE(fake_channel_->got_fin());
  EXPECT_EQ(1, fake_channel_->frames_sent());
  EXPECT_EQ(std::vector<char>(kBanana, kBanana + 6), fake_channel_->message());
}

TEST_F(WebSocketBlobSenderTest, SizeMismatch) {
  std::unique_ptr<BlobHandle> handle = CreateMemoryBackedBlob(kBanana);

  EXPECT_EQ(net::ERR_UPLOAD_FILE_CHANGED,
            Start(handle->GetUUID(), UINT64_C(5), NotCalled()));
  EXPECT_EQ(0, fake_channel_->frames_sent());
}

TEST_F(WebSocketBlobSenderTest, InvalidUUID) {
  EXPECT_EQ(net::ERR_INVALID_HANDLE,
            Start("sandwich", UINT64_C(0), NotCalled()));
}

TEST_F(WebSocketBlobSenderTest, LargeMessage) {
  std::string message(kInitialQuota + 10, 'a');
  std::unique_ptr<BlobHandle> handle = CreateMemoryBackedBlob(message.c_str());

  base::RunLoop run_loop;
  int rv = Start(handle->GetUUID(), message.size(),
                 ExpectOkAndQuitCallback(&run_loop));
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  EXPECT_EQ(1, fake_channel_->frames_sent());
  run_loop.Run();
  EXPECT_EQ(2, fake_channel_->frames_sent());
  EXPECT_TRUE(fake_channel_->got_fin());
  std::vector<char> expected_message(message.begin(), message.end());
  EXPECT_EQ(expected_message, fake_channel_->message());
}

// A message exactly equal to the available quota should be sent in one frame.
TEST_F(WebSocketBlobSenderTest, ExactSizeMessage) {
  std::string message(kInitialQuota, 'a');
  std::unique_ptr<BlobHandle> handle = CreateMemoryBackedBlob(message.c_str());

  EXPECT_EQ(net::OK, Start(handle->GetUUID(), message.size(), NotCalled()));
  EXPECT_EQ(1, fake_channel_->frames_sent());
  EXPECT_TRUE(fake_channel_->got_fin());
  std::vector<char> expected_message(message.begin(), message.end());
  EXPECT_EQ(expected_message, fake_channel_->message());
}

// If the connection is closed while sending a message, the WebSocketBlobSender
// object will be destroyed. It needs to handle this case without error.
TEST_F(WebSocketBlobSenderTest, AbortedSend) {
  std::string message(kInitialQuota + 10, 'a');
  std::unique_ptr<BlobHandle> handle = CreateMemoryBackedBlob(message.c_str());

  int rv = Start(handle->GetUUID(), message.size(), NotCalled());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  sender_.reset();
}

// Invalid file-backed blob.
TEST_F(WebSocketBlobSenderTest, InvalidFileBackedBlob) {
  base::FilePath path(FILE_PATH_LITERAL(
      "WebSocketBlobSentTest.InvalidFileBackedBlob.NonExistentFile"));
  std::unique_ptr<BlobHandle> handle =
      chrome_blob_storage_context_->CreateFileBackedBlob(path, 0u, 32u,
                                                         base::Time::Now());
  EXPECT_TRUE(handle);

  TestCompletionCallback callback;
  int rv =
      callback.GetResult(Start(handle->GetUUID(), 5u, callback.callback()));
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, rv);
}

// A test fixture that does the additional work necessary to create working
// file-backed blobs.
class WebSocketFileBackedBlobSenderTest : public WebSocketBlobSenderTest {
 protected:
  void SetUp() override {
    WebSocketBlobSenderTest::SetUp();
    // temp_dir_ is recursively deleted on destruction.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void CreateFile(const std::string& contents,
                  const base::FilePath& path,
                  base::File::Info* info) {
    ASSERT_EQ(contents.size(), static_cast<size_t>(base::WriteFile(
                                   path, contents.data(), contents.size())));
    ASSERT_TRUE(base::GetFileInfo(path, info));
  }

  std::unique_ptr<BlobHandle> CreateFileBackedBlob(
      const std::string& contents) {
    base::FilePath path = temp_dir_.path().AppendASCII("blob.dat");
    base::File::Info info;
    CreateFile(contents, path, &info);
    if (HasFatalFailure())
      return nullptr;
    return chrome_blob_storage_context_->CreateFileBackedBlob(
        path, 0u, contents.size(), info.last_modified);
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(WebSocketFileBackedBlobSenderTest, EmptyBlob) {
  std::unique_ptr<BlobHandle> handle = CreateFileBackedBlob("");
  ASSERT_TRUE(handle);

  TestCompletionCallback callback;
  int result = callback.GetResult(
      Start(handle->GetUUID(), UINT64_C(0), callback.callback()));
  EXPECT_EQ(net::OK, result);
  EXPECT_TRUE(fake_channel_->got_fin());
  EXPECT_EQ(0U, fake_channel_->message().size());
}

TEST_F(WebSocketFileBackedBlobSenderTest, SizeMismatch) {
  std::unique_ptr<BlobHandle> handle = CreateFileBackedBlob(kBanana);
  ASSERT_TRUE(handle);

  TestCompletionCallback callback;
  int result = Start(handle->GetUUID(), UINT64_C(8), callback.callback());
  // This test explicitly aims to test the asynchronous code path, otherwise it
  // would be identical to the other SizeMismatch test above.
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  EXPECT_EQ(net::ERR_UPLOAD_FILE_CHANGED, callback.WaitForResult());
  EXPECT_EQ(0, fake_channel_->frames_sent());
}

TEST_F(WebSocketFileBackedBlobSenderTest, LargeMessage) {
  std::string message = "the green potato had lunch with the angry cat. ";
  while (message.size() <= kInitialQuota) {
    message = message + message;
  }
  std::unique_ptr<BlobHandle> handle = CreateFileBackedBlob(message);
  ASSERT_TRUE(handle);

  TestCompletionCallback callback;
  int result = Start(handle->GetUUID(), message.size(), callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(result));
  std::vector<char> expected_message(message.begin(), message.end());
  EXPECT_EQ(expected_message, fake_channel_->message());
}

// The WebSocketBlobSender needs to handle a connection close while doing file
// IO cleanly.
TEST_F(WebSocketFileBackedBlobSenderTest, Aborted) {
  std::unique_ptr<BlobHandle> handle = CreateFileBackedBlob(kBanana);

  int rv = Start(handle->GetUUID(), UINT64_C(6), NotCalled());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  sender_.reset();
}

class DeletingFakeChannel : public WebSocketBlobSender::Channel {
 public:
  explicit DeletingFakeChannel(
      std::unique_ptr<WebSocketBlobSender>* sender_to_delete)
      : sender_(sender_to_delete) {}

  size_t GetSendQuota() const override { return kInitialQuota; }

  ChannelState SendFrame(bool fin, const std::vector<char>& data) override {
    sender_->reset();
    // |this| is deleted here.
    return net::WebSocketEventInterface::CHANNEL_DELETED;
  }

 private:
  std::unique_ptr<WebSocketBlobSender>* sender_;
};

class WebSocketBlobSenderDeletingTest : public WebSocketBlobSenderTest {
 protected:
  void SetUpSender() override {
    sender_.reset(new WebSocketBlobSender(
        base::WrapUnique(new DeletingFakeChannel(&sender_))));
  }
};

// This test only does something useful when run under AddressSanitizer or a
// similar tool that can detect use-after-free bugs.
TEST_F(WebSocketBlobSenderDeletingTest, SenderDeleted) {
  std::unique_ptr<BlobHandle> handle = CreateMemoryBackedBlob(kBanana);

  EXPECT_EQ(net::ERR_CONNECTION_RESET,
            Start(handle->GetUUID(), UINT64_C(6), NotCalled()));
  EXPECT_FALSE(sender_);
}

// SendFrame() calls OnSendNewQuota() synchronously while filling the operating
// system's socket write buffer. The purpose of this Channel implementation is
// to verify that the synchronous case works correctly.
class SynchronousFakeChannel : public WebSocketBlobSender::Channel {
 public:
  // This method must be called before SendFrame() is.
  void set_notify_new_quota(const base::Closure& notify_new_quota) {
    notify_new_quota_ = notify_new_quota;
  }

  size_t GetSendQuota() const override { return kInitialQuota; }

  ChannelState SendFrame(bool fin, const std::vector<char>& data) override {
    message_.insert(message_.end(), data.begin(), data.end());
    notify_new_quota_.Run();
    return net::WebSocketEventInterface::CHANNEL_ALIVE;
  }

  const std::vector<char>& message() const { return message_; }

 private:
  base::Closure notify_new_quota_;
  std::vector<char> message_;
};

class WebSocketBlobSenderSynchronousTest : public WebSocketBlobSenderTest {
 protected:
  void SetUpSender() override {
    synchronous_fake_channel_ = new SynchronousFakeChannel;
    sender_.reset(
        new WebSocketBlobSender(base::WrapUnique(synchronous_fake_channel_)));
    synchronous_fake_channel_->set_notify_new_quota(base::Bind(
        &WebSocketBlobSender::OnNewSendQuota, base::Unretained(sender_.get())));
  }

  SynchronousFakeChannel* synchronous_fake_channel_ = nullptr;
};

TEST_F(WebSocketBlobSenderSynchronousTest, LargeMessage) {
  std::string message(kInitialQuota + 10, 'a');
  std::unique_ptr<BlobHandle> handle = CreateMemoryBackedBlob(message.c_str());

  int rv = Start(handle->GetUUID(), message.size(), NotCalled());
  EXPECT_EQ(net::OK, rv);
  std::vector<char> expected_message(message.begin(), message.end());
  EXPECT_EQ(expected_message, synchronous_fake_channel_->message());
}

}  // namespace

}  // namespace content
