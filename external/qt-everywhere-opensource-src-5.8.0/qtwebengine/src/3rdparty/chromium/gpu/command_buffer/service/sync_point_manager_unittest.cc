// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <queue>

#include "base/bind.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class SyncPointManagerTest : public testing::Test {
 public:
  SyncPointManagerTest() {}

  ~SyncPointManagerTest() override {}

 protected:
  void SetUp() override {
    sync_point_manager_.reset(new SyncPointManager(false));
  }

  void TearDown() override { sync_point_manager_.reset(); }

  // Simple static function which can be used to test callbacks.
  static void SetIntegerFunction(int* test, int value) { *test = value; }

  // Simple static function used for testing OnWaitCallback.
  static void OnWait(CommandBufferNamespace* namespace_id_ptr,
                     CommandBufferId* client_id_ptr,
                     CommandBufferNamespace namespace_id,
                     CommandBufferId client_id) {
    *namespace_id_ptr = namespace_id;
    *client_id_ptr = client_id;
  }

  std::unique_ptr<SyncPointManager> sync_point_manager_;
};

struct SyncPointStream {
  scoped_refptr<SyncPointOrderData> order_data;
  std::unique_ptr<SyncPointClient> client;
  std::queue<uint32_t> order_numbers;

  SyncPointStream(SyncPointManager* sync_point_manager,
                  CommandBufferNamespace namespace_id,
                  CommandBufferId command_buffer_id)
      : order_data(SyncPointOrderData::Create()),
        client(sync_point_manager->CreateSyncPointClient(order_data,
                                                         namespace_id,
                                                         command_buffer_id)) {}

  ~SyncPointStream() {
    order_data->Destroy();
    order_data = nullptr;
  }

  void AllocateOrderNum(SyncPointManager* sync_point_manager) {
    order_numbers.push(
        order_data->GenerateUnprocessedOrderNumber(sync_point_manager));
  }

  void BeginProcessing() {
    ASSERT_FALSE(order_numbers.empty());
    order_data->BeginProcessingOrderNumber(order_numbers.front());
  }

  void EndProcessing() {
    ASSERT_FALSE(order_numbers.empty());
    order_data->FinishProcessingOrderNumber(order_numbers.front());
    order_numbers.pop();
  }
};

TEST_F(SyncPointManagerTest, BasicSyncPointOrderDataTest) {
  scoped_refptr<SyncPointOrderData> order_data = SyncPointOrderData::Create();

  EXPECT_EQ(0u, order_data->current_order_num());
  EXPECT_EQ(0u, order_data->processed_order_num());
  EXPECT_EQ(0u, order_data->unprocessed_order_num());

  const uint32_t order_num =
      order_data->GenerateUnprocessedOrderNumber(sync_point_manager_.get());
  EXPECT_EQ(1u, order_num);

  EXPECT_EQ(0u, order_data->current_order_num());
  EXPECT_EQ(0u, order_data->processed_order_num());
  EXPECT_EQ(order_num, order_data->unprocessed_order_num());

  order_data->BeginProcessingOrderNumber(order_num);
  EXPECT_EQ(order_num, order_data->current_order_num());
  EXPECT_EQ(0u, order_data->processed_order_num());
  EXPECT_EQ(order_num, order_data->unprocessed_order_num());
  EXPECT_TRUE(order_data->IsProcessingOrderNumber());

  order_data->PauseProcessingOrderNumber(order_num);
  EXPECT_FALSE(order_data->IsProcessingOrderNumber());

  order_data->BeginProcessingOrderNumber(order_num);
  EXPECT_TRUE(order_data->IsProcessingOrderNumber());

  order_data->FinishProcessingOrderNumber(order_num);
  EXPECT_EQ(order_num, order_data->current_order_num());
  EXPECT_EQ(order_num, order_data->processed_order_num());
  EXPECT_EQ(order_num, order_data->unprocessed_order_num());
  EXPECT_FALSE(order_data->IsProcessingOrderNumber());
}

TEST_F(SyncPointManagerTest, SyncPointClientRegistration) {
  const CommandBufferNamespace kNamespaceId =
      gpu::CommandBufferNamespace::GPU_IO;
  const CommandBufferId kBufferId = CommandBufferId::FromUnsafeValue(0x123);

  scoped_refptr<SyncPointClientState> empty_state =
      sync_point_manager_->GetSyncPointClientState(kNamespaceId, kBufferId);
  EXPECT_FALSE(empty_state);

  scoped_refptr<SyncPointOrderData> order_data = SyncPointOrderData::Create();

  std::unique_ptr<SyncPointClient> client =
      sync_point_manager_->CreateSyncPointClient(order_data, kNamespaceId,
                                                 kBufferId);

  EXPECT_EQ(order_data, client->client_state()->order_data());
  EXPECT_EQ(
      client->client_state(),
      sync_point_manager_->GetSyncPointClientState(kNamespaceId, kBufferId));
}

TEST_F(SyncPointManagerTest, BasicFenceSyncRelease) {
  const CommandBufferNamespace kNamespaceId =
      gpu::CommandBufferNamespace::GPU_IO;
  const CommandBufferId kBufferId = CommandBufferId::FromUnsafeValue(0x123);

  scoped_refptr<SyncPointOrderData> order_data = SyncPointOrderData::Create();
  std::unique_ptr<SyncPointClient> client =
      sync_point_manager_->CreateSyncPointClient(order_data, kNamespaceId,
                                                 kBufferId);
  scoped_refptr<SyncPointClientState> client_state = client->client_state();

  EXPECT_EQ(0u, client_state->fence_sync_release());
  EXPECT_FALSE(client_state->IsFenceSyncReleased(1));

  const uint32_t order_num =
      order_data->GenerateUnprocessedOrderNumber(sync_point_manager_.get());
  order_data->BeginProcessingOrderNumber(order_num);
  client->ReleaseFenceSync(1);
  order_data->FinishProcessingOrderNumber(order_num);

  EXPECT_EQ(1u, client_state->fence_sync_release());
  EXPECT_TRUE(client_state->IsFenceSyncReleased(1));
}

TEST_F(SyncPointManagerTest, MultipleClientsPerOrderData) {
  const CommandBufferNamespace kNamespaceId =
      gpu::CommandBufferNamespace::GPU_IO;
  const CommandBufferId kBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  const CommandBufferId kBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  scoped_refptr<SyncPointOrderData> order_data = SyncPointOrderData::Create();
  std::unique_ptr<SyncPointClient> client1 =
      sync_point_manager_->CreateSyncPointClient(order_data, kNamespaceId,
                                                 kBufferId1);
  std::unique_ptr<SyncPointClient> client2 =
      sync_point_manager_->CreateSyncPointClient(order_data, kNamespaceId,
                                                 kBufferId2);

  scoped_refptr<SyncPointClientState> client_state1 = client1->client_state();
  scoped_refptr<SyncPointClientState> client_state2 = client2->client_state();

  const uint32_t order_num =
      order_data->GenerateUnprocessedOrderNumber(sync_point_manager_.get());
  order_data->BeginProcessingOrderNumber(order_num);
  client1->ReleaseFenceSync(1);
  order_data->FinishProcessingOrderNumber(order_num);

  EXPECT_TRUE(client_state1->IsFenceSyncReleased(1));
  EXPECT_FALSE(client_state2->IsFenceSyncReleased(1));
}

TEST_F(SyncPointManagerTest, BasicFenceSyncWaitRelease) {
  const CommandBufferNamespace kNamespaceId =
      gpu::CommandBufferNamespace::GPU_IO;
  const CommandBufferId kBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  const CommandBufferId kBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kBufferId1);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kBufferId2);

  release_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());

  wait_stream.BeginProcessing();
  int test_num = 10;
  const bool valid_wait = wait_stream.client->Wait(
      release_stream.client->client_state().get(), 1,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  ASSERT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);

  release_stream.BeginProcessing();
  release_stream.client->ReleaseFenceSync(1);
  EXPECT_EQ(123, test_num);
}

TEST_F(SyncPointManagerTest, WaitOnSelfFails) {
  const CommandBufferNamespace kNamespaceId =
      gpu::CommandBufferNamespace::GPU_IO;
  const CommandBufferId kBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  const CommandBufferId kBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kBufferId1);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kBufferId2);

  // Generate wait order number first.
  release_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());

  wait_stream.BeginProcessing();
  int test_num = 10;
  const bool valid_wait = wait_stream.client->Wait(
      wait_stream.client->client_state().get(), 1,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_FALSE(valid_wait);
  EXPECT_EQ(123, test_num);
}

TEST_F(SyncPointManagerTest, OutOfOrderRelease) {
  const CommandBufferNamespace kNamespaceId =
      gpu::CommandBufferNamespace::GPU_IO;
  const CommandBufferId kBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  const CommandBufferId kBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kBufferId1);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kBufferId2);

  // Generate wait order number first.
  wait_stream.AllocateOrderNum(sync_point_manager_.get());
  release_stream.AllocateOrderNum(sync_point_manager_.get());

  wait_stream.BeginProcessing();
  int test_num = 10;
  const bool valid_wait = wait_stream.client->Wait(
      release_stream.client->client_state().get(), 1,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_FALSE(valid_wait);
  EXPECT_EQ(123, test_num);
}

TEST_F(SyncPointManagerTest, HigherOrderNumberRelease) {
  const CommandBufferNamespace kNamespaceId =
      gpu::CommandBufferNamespace::GPU_IO;
  const CommandBufferId kBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  const CommandBufferId kBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kBufferId1);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kBufferId2);

  // Generate wait order number first.
  wait_stream.AllocateOrderNum(sync_point_manager_.get());
  release_stream.AllocateOrderNum(sync_point_manager_.get());

  // Order number was higher but it was actually released.
  release_stream.BeginProcessing();
  release_stream.client->ReleaseFenceSync(1);
  release_stream.EndProcessing();

  wait_stream.BeginProcessing();
  int test_num = 10;
  const bool valid_wait = wait_stream.client->Wait(
      release_stream.client->client_state().get(), 1,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(123, test_num);
}

TEST_F(SyncPointManagerTest, DestroyedClientRelease) {
  const CommandBufferNamespace kNamespaceId =
      gpu::CommandBufferNamespace::GPU_IO;
  const CommandBufferId kBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  const CommandBufferId kBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kBufferId1);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kBufferId2);

  release_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());

  wait_stream.BeginProcessing();
  int test_num = 10;
  const bool valid_wait = wait_stream.client->Wait(
      release_stream.client->client_state().get(), 1,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);

  // Destroying the client should release the wait.
  release_stream.client.reset();
  EXPECT_EQ(123, test_num);
}

TEST_F(SyncPointManagerTest, NonExistentRelease) {
  const CommandBufferNamespace kNamespaceId =
      gpu::CommandBufferNamespace::GPU_IO;
  const CommandBufferId kBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  const CommandBufferId kBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kBufferId1);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kBufferId2);

  // Assign release stream order [1] and wait stream order [2].
  // This test simply tests that a wait stream of order [2] waiting on
  // release stream of order [1] will still release the fence sync even
  // though nothing was released.
  release_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());

  wait_stream.BeginProcessing();
  int test_num = 10;
  const bool valid_wait = wait_stream.client->Wait(
      release_stream.client->client_state().get(), 1,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);

  // No release but finishing the order number should automatically release.
  release_stream.BeginProcessing();
  EXPECT_EQ(10, test_num);
  release_stream.EndProcessing();
  EXPECT_EQ(123, test_num);
}

TEST_F(SyncPointManagerTest, NonExistentRelease2) {
  const CommandBufferNamespace kNamespaceId =
      gpu::CommandBufferNamespace::GPU_IO;
  const CommandBufferId kBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  const CommandBufferId kBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kBufferId1);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kBufferId2);

  // Assign Release stream order [1] and assign Wait stream orders [2, 3].
  // This test is similar to the NonExistentRelease case except
  // we place an extra order number in between the release and wait.
  // The wait stream [3] is waiting on release stream [1] even though
  // order [2] was also generated. Although order [2] only exists on the
  // wait stream so the release stream should only know about order [1].
  release_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());

  // Have wait with order [3] to wait on release.
  wait_stream.BeginProcessing();
  ASSERT_EQ(2u, wait_stream.order_data->current_order_num());
  wait_stream.EndProcessing();
  wait_stream.BeginProcessing();
  ASSERT_EQ(3u, wait_stream.order_data->current_order_num());
  int test_num = 10;
  const bool valid_wait = wait_stream.client->Wait(
      release_stream.client->client_state().get(), 1,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);

  // Even though release stream order [1] did not have a release, it
  // should have changed test_num although the fence sync is still not released.
  release_stream.BeginProcessing();
  ASSERT_EQ(1u, release_stream.order_data->current_order_num());
  release_stream.EndProcessing();
  EXPECT_FALSE(release_stream.client->client_state()->IsFenceSyncReleased(1));
  EXPECT_EQ(123, test_num);

  // Ensure that the wait callback does not get triggered again when it is
  // actually released.
  test_num = 1;
  release_stream.AllocateOrderNum(sync_point_manager_.get());
  release_stream.BeginProcessing();
  release_stream.client->ReleaseFenceSync(1);
  release_stream.EndProcessing();
  EXPECT_EQ(1, test_num);
}

TEST_F(SyncPointManagerTest, NonExistentOrderNumRelease) {
  const CommandBufferNamespace kNamespaceId =
      gpu::CommandBufferNamespace::GPU_IO;
  const CommandBufferId kBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  const CommandBufferId kBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kBufferId1);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kBufferId2);

  // Assign Release stream orders [1, 4] and assign Wait stream orders [2, 3].
  // Here we are testing that wait order [3] will wait on a fence sync
  // in either order [1] or order [2]. Order [2] was not actually assigned
  // to the release stream so it is essentially non-existent to the release
  // stream's point of view. Once the release stream begins processing the next
  // order [3], it should realize order [2] didn't exist and release the fence.
  release_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());
  release_stream.AllocateOrderNum(sync_point_manager_.get());

  // Have wait with order [3] to wait on release order [1] or [2].
  wait_stream.BeginProcessing();
  ASSERT_EQ(2u, wait_stream.order_data->current_order_num());
  wait_stream.EndProcessing();
  wait_stream.BeginProcessing();
  ASSERT_EQ(3u, wait_stream.order_data->current_order_num());
  int test_num = 10;
  const bool valid_wait = wait_stream.client->Wait(
      release_stream.client->client_state().get(), 1,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);

  // Release stream should know it should release fence sync by order [3],
  // so going through order [1] should not release it yet.
  release_stream.BeginProcessing();
  ASSERT_EQ(1u, release_stream.order_data->current_order_num());
  release_stream.EndProcessing();
  EXPECT_FALSE(release_stream.client->client_state()->IsFenceSyncReleased(1));
  EXPECT_EQ(10, test_num);

  // Beginning order [4] should immediately trigger the wait although the fence
  // sync is still not released yet.
  release_stream.BeginProcessing();
  ASSERT_EQ(4u, release_stream.order_data->current_order_num());
  EXPECT_FALSE(release_stream.client->client_state()->IsFenceSyncReleased(1));
  EXPECT_EQ(123, test_num);

  // Ensure that the wait callback does not get triggered again when it is
  // actually released.
  test_num = 1;
  release_stream.client->ReleaseFenceSync(1);
  EXPECT_EQ(1, test_num);
}

TEST_F(SyncPointManagerTest, OnWaitCallbackTest) {
  const CommandBufferNamespace kNamespaceId =
      gpu::CommandBufferNamespace::GPU_IO;
  const CommandBufferId kBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  const CommandBufferId kBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kBufferId1);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kBufferId2);

  CommandBufferNamespace namespace_id = CommandBufferNamespace::INVALID;
  CommandBufferId client_id;
  release_stream.client->SetOnWaitCallback(
      base::Bind(&SyncPointManagerTest::OnWait, &namespace_id, &client_id));

  release_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());
  release_stream.AllocateOrderNum(sync_point_manager_.get());

  wait_stream.BeginProcessing();
  int test_num = 10;
  bool valid_wait = wait_stream.client->Wait(
      release_stream.client->client_state().get(), 1,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_EQ(kNamespaceId, namespace_id);
  EXPECT_EQ(kBufferId2, client_id);

  release_stream.BeginProcessing();
  release_stream.client->ReleaseFenceSync(1);
  EXPECT_EQ(123, test_num);

  wait_stream.EndProcessing();

  namespace_id = CommandBufferNamespace::INVALID;
  client_id = CommandBufferId();
  test_num = 10;
  valid_wait = wait_stream.client->WaitOutOfOrder(
      release_stream.client->client_state().get(), 2,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_EQ(kNamespaceId, namespace_id);
  EXPECT_EQ(kBufferId2, client_id);

  release_stream.client->ReleaseFenceSync(2);
  EXPECT_EQ(123, test_num);
}

}  // namespace gpu
