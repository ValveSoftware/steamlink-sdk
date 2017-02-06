// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/common/blob_cache/id_util.h"
#include "blimp/common/blob_cache/in_memory_blob_cache.h"
#include "blimp/common/blob_cache/test_util.h"
#include "blimp/net/blob_channel/blob_channel_receiver.h"
#include "blimp/net/blob_channel/blob_channel_sender.h"
#include "blimp/net/blob_channel/blob_channel_sender_impl.h"
#include "blimp/net/blob_channel/mock_blob_channel_receiver.h"
#include "blimp/net/test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

using testing::_;
using testing::SaveArg;

const char kBlobId[] = "foo1";
const char kBlobPayload[] = "bar1";

// Routes sender delegate calls to a receiver delegate object.
// The caller is responsible for ensuring that the receiver delegate is deleted
// after |this| is deleted.
class SenderDelegateProxy : public BlobChannelSenderImpl::Delegate {
 public:
  explicit SenderDelegateProxy(BlobChannelReceiver* receiver)
      : receiver_(receiver) {}
  ~SenderDelegateProxy() override {}

 private:
  // BlobChannelSender implementation.
  void DeliverBlob(const BlobId& id, BlobDataPtr data) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&BlobChannelReceiver::OnBlobReceived,
                              base::Unretained(receiver_), id, data));
  }

  BlobChannelReceiver* receiver_;

  DISALLOW_COPY_AND_ASSIGN(SenderDelegateProxy);
};

// Verifies compatibility between the sender and receiver, independent of
// transport-level implementation details.
class BlobChannelIntegrationTest : public testing::Test {
 public:
  BlobChannelIntegrationTest() {
    BlobChannelReceiver* stored_receiver;
    std::unique_ptr<MockBlobChannelReceiverDelegate> receiver_delegate(
        new MockBlobChannelReceiverDelegate);
    receiver_delegate_ = receiver_delegate.get();

    EXPECT_CALL(*receiver_delegate, SetReceiver(_))
        .WillOnce(SaveArg<0>(&stored_receiver));

    receiver_ = BlobChannelReceiver::Create(
        base::WrapUnique(new InMemoryBlobCache), std::move(receiver_delegate));

    EXPECT_EQ(receiver_.get(), stored_receiver);

    sender_.reset(new BlobChannelSenderImpl(
        base::WrapUnique(new InMemoryBlobCache),
        base::WrapUnique(new SenderDelegateProxy(receiver_.get()))));
  }

  ~BlobChannelIntegrationTest() override {}

 protected:
  MockBlobChannelReceiverDelegate* receiver_delegate_;
  std::unique_ptr<BlobChannelReceiver> receiver_;
  std::unique_ptr<BlobChannelSender> sender_;
  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlobChannelIntegrationTest);
};

TEST_F(BlobChannelIntegrationTest, Deliver) {
  const std::string blob_id = CalculateBlobId(kBlobId);

  EXPECT_EQ(nullptr, receiver_->Get(blob_id).get());
  sender_->PutBlob(blob_id, CreateBlobDataPtr(kBlobPayload));
  EXPECT_EQ(nullptr, receiver_->Get(blob_id).get());

  EXPECT_EQ(nullptr, receiver_->Get(blob_id).get());
  sender_->DeliverBlob(blob_id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kBlobPayload, receiver_->Get(blob_id)->data);
}

}  // namespace
}  // namespace blimp
