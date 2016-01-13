// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/software_frame_manager.h"

#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/sys_info.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class FakeSoftwareFrameManagerClient : public SoftwareFrameManagerClient {
 public:
  FakeSoftwareFrameManagerClient()
      : evicted_count_(0), weak_ptr_factory_(this) {
    software_frame_manager_.reset(new SoftwareFrameManager(
        weak_ptr_factory_.GetWeakPtr()));
  }
  virtual ~FakeSoftwareFrameManagerClient() {
    HostSharedBitmapManager::current()->ProcessRemoved(
        base::GetCurrentProcessHandle());
  }
  virtual void SoftwareFrameWasFreed(uint32 output_surface_id,
                                     unsigned frame_id) OVERRIDE {
    freed_frames_.push_back(std::make_pair(output_surface_id, frame_id));
  }
  virtual void ReleaseReferencesToSoftwareFrame() OVERRIDE {
    ++evicted_count_;
  }

  bool SwapToNewFrame(uint32 output_surface, unsigned frame_id) {
    cc::SoftwareFrameData frame;
    frame.id = frame_id;
    frame.size = gfx::Size(1, 1);
    frame.damage_rect = gfx::Rect(frame.size);
    frame.bitmap_id = cc::SharedBitmap::GenerateId();
    scoped_ptr<base::SharedMemory> memory =
        make_scoped_ptr(new base::SharedMemory);
    memory->CreateAnonymous(4);
    HostSharedBitmapManager::current()->ChildAllocatedSharedBitmap(
        4, memory->handle(), base::GetCurrentProcessHandle(), frame.bitmap_id);
    allocated_memory_.push_back(memory.release());
    return software_frame_manager_->SwapToNewFrame(
        output_surface, &frame, 1.0, base::GetCurrentProcessHandle());
  }

  SoftwareFrameManager* software_frame_manager() {
    return software_frame_manager_.get();
  }
  size_t freed_frame_count() const { return freed_frames_.size(); }
  size_t evicted_frame_count() const { return evicted_count_; }

 private:
  std::vector<std::pair<uint32,unsigned> > freed_frames_;
  size_t evicted_count_;
  ScopedVector<base::SharedMemory> allocated_memory_;

  scoped_ptr<SoftwareFrameManager> software_frame_manager_;
  base::WeakPtrFactory<FakeSoftwareFrameManagerClient>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeSoftwareFrameManagerClient);
};

class SoftwareFrameManagerTest : public testing::Test {
 public:
  SoftwareFrameManagerTest() {}
  void AllocateClients(size_t num_clients) {
    for (size_t i = 0; i < num_clients; ++i)
      clients_.push_back(new FakeSoftwareFrameManagerClient);
  }
  void FreeClients() {
    for (size_t i = 0; i < clients_.size(); ++i)
      delete clients_[i];
    clients_.clear();
  }
  size_t MaxNumberOfSavedFrames() const {
    size_t result =
        RendererFrameManager::GetInstance()->max_number_of_saved_frames();
    return result;
  }

 protected:
  std::vector<FakeSoftwareFrameManagerClient*> clients_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SoftwareFrameManagerTest);
};

TEST_F(SoftwareFrameManagerTest, DoNotEvictVisible) {
  // Create twice as many frames as are allowed.
  AllocateClients(2 * MaxNumberOfSavedFrames());

  // Swap a visible frame to all clients_. Because they are all visible,
  // the should not be evicted.
  for (size_t i = 0; i < clients_.size(); ++i) {
    bool swap_result = clients_[i]->SwapToNewFrame(
        static_cast<uint32>(i), 0);
    clients_[i]->software_frame_manager()->SwapToNewFrameComplete(true);
    EXPECT_TRUE(swap_result);
    EXPECT_EQ(0u, clients_[i]->evicted_frame_count());
    EXPECT_EQ(0u, clients_[i]->freed_frame_count());
  }
  for (size_t i = 0; i < clients_.size(); ++i) {
    EXPECT_EQ(0u, clients_[i]->evicted_frame_count());
    EXPECT_EQ(0u, clients_[i]->freed_frame_count());
  }

  // Swap another frame and make sure the original was freed (but not evicted).
  for (size_t i = 0; i < clients_.size(); ++i) {
    bool swap_result = clients_[i]->SwapToNewFrame(
        static_cast<uint32>(i), 1);
    clients_[i]->software_frame_manager()->SwapToNewFrameComplete(true);
    EXPECT_TRUE(swap_result);
    EXPECT_EQ(0u, clients_[i]->evicted_frame_count());
    EXPECT_EQ(1u, clients_[i]->freed_frame_count());
  }
  for (size_t i = 0; i < clients_.size(); ++i) {
    EXPECT_EQ(0u, clients_[i]->evicted_frame_count());
    EXPECT_EQ(1u, clients_[i]->freed_frame_count());
  }

  // Mark the frames as nonvisible and make sure they start getting evicted.
  for (size_t i = 0; i < clients_.size(); ++i) {
    clients_[i]->software_frame_manager()->SetVisibility(false);
    if (clients_.size() - i > MaxNumberOfSavedFrames()) {
      EXPECT_EQ(1u, clients_[i]->evicted_frame_count());
      EXPECT_EQ(2u, clients_[i]->freed_frame_count());
    } else {
      EXPECT_EQ(0u, clients_[i]->evicted_frame_count());
      EXPECT_EQ(1u, clients_[i]->freed_frame_count());
    }
  }

  // Clean up.
  FreeClients();
}

TEST_F(SoftwareFrameManagerTest, DoNotEvictDuringSwap) {
  // Create twice as many frames as are allowed.
  AllocateClients(2 * MaxNumberOfSavedFrames());

  // Swap a visible frame to all clients_. Because they are all visible,
  // the should not be evicted.
  for (size_t i = 0; i < clients_.size(); ++i) {
    bool swap_result = clients_[i]->SwapToNewFrame(static_cast<uint32>(i), 0);
    clients_[i]->software_frame_manager()->SwapToNewFrameComplete(true);
    EXPECT_TRUE(swap_result);
    EXPECT_EQ(0u, clients_[i]->evicted_frame_count());
    EXPECT_EQ(0u, clients_[i]->freed_frame_count());
  }
  for (size_t i = 0; i < clients_.size(); ++i) {
    EXPECT_EQ(0u, clients_[i]->evicted_frame_count());
    EXPECT_EQ(0u, clients_[i]->freed_frame_count());
  }

  // Now create a test non-visible client, and swap a non-visible frame in.
  scoped_ptr<FakeSoftwareFrameManagerClient> test_client(
      new FakeSoftwareFrameManagerClient);
  test_client->software_frame_manager()->SetVisibility(false);
  {
    bool swap_result = test_client->SwapToNewFrame(
        static_cast<uint32>(500), 0);
    EXPECT_TRUE(swap_result);
    EXPECT_EQ(0u, test_client->evicted_frame_count());
    EXPECT_EQ(0u, test_client->freed_frame_count());
    test_client->software_frame_manager()->SwapToNewFrameComplete(false);
    EXPECT_EQ(1u, test_client->evicted_frame_count());
    EXPECT_EQ(1u, test_client->freed_frame_count());
  }

  // Clean up.
  FreeClients();
}

TEST_F(SoftwareFrameManagerTest, Cleanup) {
  // Create twice as many frames as are allowed.
  AllocateClients(2 * MaxNumberOfSavedFrames());

  // Swap a visible frame to all clients_. Because they are all visible,
  // the should not be evicted.
  for (size_t i = 0; i < clients_.size(); ++i) {
    bool swap_result = clients_[i]->SwapToNewFrame(static_cast<uint32>(i), 0);
    clients_[i]->software_frame_manager()->SwapToNewFrameComplete(true);
    EXPECT_TRUE(swap_result);
    EXPECT_EQ(0u, clients_[i]->evicted_frame_count());
    EXPECT_EQ(0u, clients_[i]->freed_frame_count());
  }

  // Destroy them.
  FreeClients();

  // Create the maximum number of frames, all non-visible. They should not
  // be evicted, because the previous frames were cleaned up at destruction.
  AllocateClients(MaxNumberOfSavedFrames());
  for (size_t i = 0; i < clients_.size(); ++i) {
    cc::SoftwareFrameData frame;
    bool swap_result = clients_[i]->SwapToNewFrame(static_cast<uint32>(i), 0);
    clients_[i]->software_frame_manager()->SwapToNewFrameComplete(true);
    EXPECT_TRUE(swap_result);
    EXPECT_EQ(0u, clients_[i]->evicted_frame_count());
    EXPECT_EQ(0u, clients_[i]->freed_frame_count());
  }
  for (size_t i = 0; i < clients_.size(); ++i) {
    EXPECT_EQ(0u, clients_[i]->evicted_frame_count());
    EXPECT_EQ(0u, clients_[i]->freed_frame_count());
  }

  // Clean up.
  FreeClients();
}

TEST_F(SoftwareFrameManagerTest, EvictVersusFree) {
  // Create twice as many frames as are allowed and swap a visible frame to all
  // clients_. Because they are all visible, the should not be evicted.
  AllocateClients(2 * MaxNumberOfSavedFrames());
  for (size_t i = 0; i < clients_.size(); ++i) {
    clients_[i]->SwapToNewFrame(static_cast<uint32>(i), 0);
    clients_[i]->software_frame_manager()->SwapToNewFrameComplete(true);
  }

  // Create a test client with a frame that is not evicted.
  scoped_ptr<FakeSoftwareFrameManagerClient> test_client(
      new FakeSoftwareFrameManagerClient);
  bool swap_result = test_client->SwapToNewFrame(static_cast<uint32>(500), 0);
  EXPECT_TRUE(swap_result);
  test_client->software_frame_manager()->SwapToNewFrameComplete(true);
  EXPECT_EQ(0u, test_client->evicted_frame_count());
  EXPECT_EQ(0u, test_client->freed_frame_count());

  // Take out a reference on the current frame and make the memory manager
  // evict it. The frame will not be freed until this reference is released.
  cc::TextureMailbox mailbox;
  scoped_ptr<cc::SingleReleaseCallback> callback;
  test_client->software_frame_manager()->GetCurrentFrameMailbox(
      &mailbox, &callback);
  test_client->software_frame_manager()->SetVisibility(false);
  EXPECT_EQ(1u, test_client->evicted_frame_count());
  EXPECT_EQ(0u, test_client->freed_frame_count());

  // Swap a few frames. The frames will be freed as they are swapped out.
  for (size_t frame = 0; frame < 10; ++frame) {
    bool swap_result = test_client->SwapToNewFrame(
        static_cast<uint32>(500), 1 + static_cast<int>(frame));
    EXPECT_TRUE(swap_result);
    test_client->software_frame_manager()->SwapToNewFrameComplete(true);
    EXPECT_EQ(frame, test_client->freed_frame_count());
    EXPECT_EQ(1u, test_client->evicted_frame_count());
  }

  // The reference to the frame that we didn't free is in the callback
  // object. It will go away when the callback is destroyed.
  EXPECT_EQ(9u, test_client->freed_frame_count());
  EXPECT_EQ(1u, test_client->evicted_frame_count());
  callback->Run(0, false);
  callback.reset();
  EXPECT_EQ(10u, test_client->freed_frame_count());
  EXPECT_EQ(1u, test_client->evicted_frame_count());

  FreeClients();
}

}  // namespace content
