// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/renderer/blob_channel_sender_proxy.h"

#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/common/blob_cache/id_util.h"
#include "blimp/common/blob_cache/test_util.h"
#include "blimp/engine/mojo/blob_channel.mojom.h"
#include "blimp/engine/mojo/blob_channel_service.h"
#include "blimp/net/blob_channel/mock_blob_channel_sender.h"
#include "blimp/net/test_common.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace engine {
namespace {

const char kBlobId[] = "foo";
const char kBlobPayload[] = "bar";

class BlobChannelSenderProxyTest : public testing::Test {
 public:
  BlobChannelSenderProxyTest()
      : sender_weak_factory_(&mock_sender_),
        mojo_service_impl_(base::MakeUnique<BlobChannelService>(
            sender_weak_factory_.GetWeakPtr(),
            message_loop_.task_runner())) {}

  void SetUp() override {
    // Set up communication path from the Proxy to a sender mock:
    // blob_channel_sender_proxy_ => (mojo) => mojo_service_impl_ =>
    //    mock_sender_;
    mojom::BlobChannelPtr mojo_ptr;
    mojo_service_impl_->BindRequest(GetProxy(&mojo_ptr));
    blob_channel_sender_proxy_ =
        BlobChannelSenderProxy::CreateForTest(std::move(mojo_ptr));
  }

 protected:
  void DeliverMessages() { base::RunLoop().RunUntilIdle(); }

  const std::string blob_id_ = CalculateBlobId(kBlobId);
  base::MessageLoop message_loop_;
  std::unique_ptr<BlobChannelSenderProxy> blob_channel_sender_proxy_;
  MockBlobChannelSender mock_sender_;
  base::WeakPtrFactory<BlobChannelSender> sender_weak_factory_;
  std::unique_ptr<BlobChannelService> mojo_service_impl_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlobChannelSenderProxyTest);
};

TEST_F(BlobChannelSenderProxyTest, PutBlob) {
  EXPECT_CALL(mock_sender_,
              PutBlob(blob_id_, BlobDataPtrEqualsString(kBlobPayload)));
  blob_channel_sender_proxy_->PutBlob(blob_id_,
                                      CreateBlobDataPtr(kBlobPayload));
  DeliverMessages();
  EXPECT_TRUE(blob_channel_sender_proxy_->IsInEngineCache(blob_id_));
  EXPECT_FALSE(blob_channel_sender_proxy_->IsInClientCache(blob_id_));
}

TEST_F(BlobChannelSenderProxyTest, DeliverBlob) {
  EXPECT_CALL(mock_sender_,
              PutBlob(blob_id_, BlobDataPtrEqualsString(kBlobPayload)));
  EXPECT_CALL(mock_sender_, DeliverBlob(blob_id_));

  blob_channel_sender_proxy_->PutBlob(blob_id_,
                                      CreateBlobDataPtr(kBlobPayload));
  DeliverMessages();
  EXPECT_TRUE(blob_channel_sender_proxy_->IsInEngineCache(blob_id_));
  EXPECT_FALSE(blob_channel_sender_proxy_->IsInClientCache(blob_id_));

  blob_channel_sender_proxy_->DeliverBlob(blob_id_);
  DeliverMessages();
  EXPECT_TRUE(blob_channel_sender_proxy_->IsInEngineCache(blob_id_));
  EXPECT_TRUE(blob_channel_sender_proxy_->IsInClientCache(blob_id_));
}

}  // namespace
}  // namespace engine
}  // namespace blimp
