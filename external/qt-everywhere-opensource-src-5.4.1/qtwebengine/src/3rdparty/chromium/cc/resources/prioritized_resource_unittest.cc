// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/prioritized_resource.h"

#include <vector>

#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_provider.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/tiled_layer_test_common.h"
#include "cc/trees/single_thread_proxy.h"  // For DebugScopedSetImplThread
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class PrioritizedResourceTest : public testing::Test {
 public:
  PrioritizedResourceTest()
      : texture_size_(256, 256),
        texture_format_(RGBA_8888),
        output_surface_(FakeOutputSurface::Create3d()) {
    DebugScopedSetImplThread impl_thread(&proxy_);
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ = ResourceProvider::Create(
        output_surface_.get(), shared_bitmap_manager_.get(), 0, false, 1,
        false);
  }

  virtual ~PrioritizedResourceTest() {
    DebugScopedSetImplThread impl_thread(&proxy_);
    resource_provider_.reset();
  }

  size_t TexturesMemorySize(size_t texture_count) {
    return Resource::MemorySizeBytes(texture_size_, texture_format_) *
           texture_count;
  }

  scoped_ptr<PrioritizedResourceManager> CreateManager(size_t max_textures) {
    scoped_ptr<PrioritizedResourceManager> manager =
        PrioritizedResourceManager::Create(&proxy_);
    manager->SetMaxMemoryLimitBytes(TexturesMemorySize(max_textures));
    return manager.Pass();
  }

  bool ValidateTexture(PrioritizedResource* texture,
                       bool request_late) {
    ResourceManagerAssertInvariants(texture->resource_manager());
    if (request_late)
      texture->RequestLate();
    ResourceManagerAssertInvariants(texture->resource_manager());
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    bool success = texture->can_acquire_backing_texture();
    if (success)
      texture->AcquireBackingTexture(resource_provider());
    return success;
  }

  void PrioritizeTexturesAndBackings(
      PrioritizedResourceManager* resource_manager) {
    resource_manager->PrioritizeTextures();
    ResourceManagerUpdateBackingsPriorities(resource_manager);
  }

  void ResourceManagerUpdateBackingsPriorities(
      PrioritizedResourceManager* resource_manager) {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->PushTexturePrioritiesToBackings();
  }

  ResourceProvider* resource_provider() { return resource_provider_.get(); }

  void ResourceManagerAssertInvariants(
      PrioritizedResourceManager* resource_manager) {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->AssertInvariants();
  }

  bool TextureBackingIsAbovePriorityCutoff(PrioritizedResource* texture) {
    return texture->backing()->
        was_above_priority_cutoff_at_last_priority_update();
  }

  size_t EvictedBackingCount(PrioritizedResourceManager* resource_manager) {
    return resource_manager->evicted_backings_.size();
  }

  std::vector<unsigned> BackingResources(
      PrioritizedResourceManager* resource_manager) {
    std::vector<unsigned> resources;
    for (PrioritizedResourceManager::BackingList::iterator it =
             resource_manager->backings_.begin();
         it != resource_manager->backings_.end();
         ++it)
      resources.push_back((*it)->id());
    return resources;
  }

 protected:
  FakeProxy proxy_;
  const gfx::Size texture_size_;
  const ResourceFormat texture_format_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<OutputSurface> output_surface_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> resource_provider_;
};

namespace {

TEST_F(PrioritizedResourceTest, RequestTextureExceedingMaxLimit) {
  const size_t kMaxTextures = 8;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);

  // Create textures for double our memory limit.
  scoped_ptr<PrioritizedResource> textures[kMaxTextures * 2];

  for (size_t i = 0; i < kMaxTextures * 2; ++i)
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);

  // Set decreasing priorities
  for (size_t i = 0; i < kMaxTextures * 2; ++i)
    textures[i]->set_request_priority(100 + i);

  // Only lower half should be available.
  PrioritizeTexturesAndBackings(resource_manager.get());
  EXPECT_TRUE(ValidateTexture(textures[0].get(), false));
  EXPECT_TRUE(ValidateTexture(textures[7].get(), false));
  EXPECT_FALSE(ValidateTexture(textures[8].get(), false));
  EXPECT_FALSE(ValidateTexture(textures[15].get(), false));

  // Set increasing priorities
  for (size_t i = 0; i < kMaxTextures * 2; ++i)
    textures[i]->set_request_priority(100 - i);

  // Only upper half should be available.
  PrioritizeTexturesAndBackings(resource_manager.get());
  EXPECT_FALSE(ValidateTexture(textures[0].get(), false));
  EXPECT_FALSE(ValidateTexture(textures[7].get(), false));
  EXPECT_TRUE(ValidateTexture(textures[8].get(), false));
  EXPECT_TRUE(ValidateTexture(textures[15].get(), false));

  EXPECT_EQ(TexturesMemorySize(kMaxTextures),
            resource_manager->MemoryAboveCutoffBytes());
  EXPECT_LE(resource_manager->MemoryUseBytes(),
            resource_manager->MemoryAboveCutoffBytes());
  EXPECT_EQ(TexturesMemorySize(2*kMaxTextures),
            resource_manager->MaxMemoryNeededBytes());

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager->ClearAllMemory(resource_provider());
}

TEST_F(PrioritizedResourceTest, ChangeMemoryLimits) {
  const size_t kMaxTextures = 8;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);
  scoped_ptr<PrioritizedResource> textures[kMaxTextures];

  for (size_t i = 0; i < kMaxTextures; ++i) {
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);
  }
  for (size_t i = 0; i < kMaxTextures; ++i)
    textures[i]->set_request_priority(100 + i);

  // Set max limit to 8 textures
  resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(8));
  PrioritizeTexturesAndBackings(resource_manager.get());
  for (size_t i = 0; i < kMaxTextures; ++i)
    ValidateTexture(textures[i].get(), false);
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ReduceMemory(resource_provider());
  }

  EXPECT_EQ(TexturesMemorySize(8), resource_manager->MemoryAboveCutoffBytes());
  EXPECT_LE(resource_manager->MemoryUseBytes(),
            resource_manager->MemoryAboveCutoffBytes());

  // Set max limit to 5 textures
  resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(5));
  PrioritizeTexturesAndBackings(resource_manager.get());
  for (size_t i = 0; i < kMaxTextures; ++i)
    EXPECT_EQ(ValidateTexture(textures[i].get(), false), i < 5);
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ReduceMemory(resource_provider());
  }

  EXPECT_EQ(TexturesMemorySize(5), resource_manager->MemoryAboveCutoffBytes());
  EXPECT_LE(resource_manager->MemoryUseBytes(),
            resource_manager->MemoryAboveCutoffBytes());
  EXPECT_EQ(TexturesMemorySize(kMaxTextures),
            resource_manager->MaxMemoryNeededBytes());

  // Set max limit to 4 textures
  resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(4));
  PrioritizeTexturesAndBackings(resource_manager.get());
  for (size_t i = 0; i < kMaxTextures; ++i)
    EXPECT_EQ(ValidateTexture(textures[i].get(), false), i < 4);
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ReduceMemory(resource_provider());
  }

  EXPECT_EQ(TexturesMemorySize(4), resource_manager->MemoryAboveCutoffBytes());
  EXPECT_LE(resource_manager->MemoryUseBytes(),
            resource_manager->MemoryAboveCutoffBytes());
  EXPECT_EQ(TexturesMemorySize(kMaxTextures),
            resource_manager->MaxMemoryNeededBytes());

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager->ClearAllMemory(resource_provider());
}

TEST_F(PrioritizedResourceTest, ReduceWastedMemory) {
  const size_t kMaxTextures = 20;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);
  scoped_ptr<PrioritizedResource> textures[kMaxTextures];

  for (size_t i = 0; i < kMaxTextures; ++i) {
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);
  }
  for (size_t i = 0; i < kMaxTextures; ++i)
    textures[i]->set_request_priority(100 + i);

  // Set the memory limit to the max number of textures.
  resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(kMaxTextures));
  PrioritizeTexturesAndBackings(resource_manager.get());

  // Create backings and textures for all of the textures.
  for (size_t i = 0; i < kMaxTextures; ++i) {
    ValidateTexture(textures[i].get(), false);

    {
      DebugScopedSetImplThreadAndMainThreadBlocked
          impl_thread_and_main_thread_blocked(&proxy_);
      uint8_t image[4] = {0};
      textures[i]->SetPixels(resource_provider_.get(),
                             image,
                             gfx::Rect(1, 1),
                             gfx::Rect(1, 1),
                             gfx::Vector2d());
    }
  }
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ReduceMemory(resource_provider());
  }

  // 20 textures have backings allocated.
  EXPECT_EQ(TexturesMemorySize(20), resource_manager->MemoryUseBytes());

  // Destroy one texture, not enough is wasted to cause cleanup.
  textures[0] = scoped_ptr<PrioritizedResource>();
  PrioritizeTexturesAndBackings(resource_manager.get());
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->UpdateBackingsState(resource_provider());
    resource_manager->ReduceWastedMemory(resource_provider());
  }
  EXPECT_EQ(TexturesMemorySize(20), resource_manager->MemoryUseBytes());

  // Destroy half the textures, leaving behind the backings. Now a cleanup
  // should happen.
  for (size_t i = 0; i < kMaxTextures / 2; ++i)
    textures[i] = scoped_ptr<PrioritizedResource>();
  PrioritizeTexturesAndBackings(resource_manager.get());
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->UpdateBackingsState(resource_provider());
    resource_manager->ReduceWastedMemory(resource_provider());
  }
  EXPECT_GT(TexturesMemorySize(20), resource_manager->MemoryUseBytes());

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager->ClearAllMemory(resource_provider());
}

TEST_F(PrioritizedResourceTest, InUseNotWastedMemory) {
  const size_t kMaxTextures = 20;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);
  scoped_ptr<PrioritizedResource> textures[kMaxTextures];

  for (size_t i = 0; i < kMaxTextures; ++i) {
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);
  }
  for (size_t i = 0; i < kMaxTextures; ++i)
    textures[i]->set_request_priority(100 + i);

  // Set the memory limit to the max number of textures.
  resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(kMaxTextures));
  PrioritizeTexturesAndBackings(resource_manager.get());

  // Create backings and textures for all of the textures.
  for (size_t i = 0; i < kMaxTextures; ++i) {
    ValidateTexture(textures[i].get(), false);

    {
      DebugScopedSetImplThreadAndMainThreadBlocked
          impl_thread_and_main_thread_blocked(&proxy_);
      uint8_t image[4] = {0};
      textures[i]->SetPixels(resource_provider_.get(),
                             image,
                             gfx::Rect(1, 1),
                             gfx::Rect(1, 1),
                             gfx::Vector2d());
    }
  }
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ReduceMemory(resource_provider());
  }

  // 20 textures have backings allocated.
  EXPECT_EQ(TexturesMemorySize(20), resource_manager->MemoryUseBytes());

  // Send half the textures to a parent compositor.
  ResourceProvider::ResourceIdArray to_send;
  TransferableResourceArray transferable;
  for (size_t i = 0; i < kMaxTextures / 2; ++i)
    to_send.push_back(textures[i]->resource_id());
  resource_provider_->PrepareSendToParent(to_send, &transferable);

  // Destroy half the textures, leaving behind the backings. The backings are
  // sent to a parent compositor though, so they should not be considered wasted
  // and a cleanup should not happen.
  for (size_t i = 0; i < kMaxTextures / 2; ++i)
    textures[i] = scoped_ptr<PrioritizedResource>();
  PrioritizeTexturesAndBackings(resource_manager.get());
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->UpdateBackingsState(resource_provider());
    resource_manager->ReduceWastedMemory(resource_provider());
  }
  EXPECT_EQ(TexturesMemorySize(20), resource_manager->MemoryUseBytes());

  // Receive the textures back from the parent compositor. Now a cleanup should
  // happen.
  ReturnedResourceArray returns;
  TransferableResource::ReturnResources(transferable, &returns);
  resource_provider_->ReceiveReturnsFromParent(returns);
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->UpdateBackingsState(resource_provider());
    resource_manager->ReduceWastedMemory(resource_provider());
  }
  EXPECT_GT(TexturesMemorySize(20), resource_manager->MemoryUseBytes());

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager->ClearAllMemory(resource_provider());
}

TEST_F(PrioritizedResourceTest, ChangePriorityCutoff) {
  const size_t kMaxTextures = 8;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);
  scoped_ptr<PrioritizedResource> textures[kMaxTextures];

  for (size_t i = 0; i < kMaxTextures; ++i) {
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);
  }
  for (size_t i = 0; i < kMaxTextures; ++i)
    textures[i]->set_request_priority(100 + i);

  // Set the cutoff to drop two textures. Try to request_late on all textures,
  // and make sure that request_late doesn't work on a texture with equal
  // priority to the cutoff.
  resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(8));
  resource_manager->SetExternalPriorityCutoff(106);
  PrioritizeTexturesAndBackings(resource_manager.get());
  for (size_t i = 0; i < kMaxTextures; ++i)
    EXPECT_EQ(ValidateTexture(textures[i].get(), true), i < 6);
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ReduceMemory(resource_provider());
  }
  EXPECT_EQ(TexturesMemorySize(6), resource_manager->MemoryAboveCutoffBytes());
  EXPECT_LE(resource_manager->MemoryUseBytes(),
            resource_manager->MemoryAboveCutoffBytes());

  // Set the cutoff to drop two more textures.
  resource_manager->SetExternalPriorityCutoff(104);
  PrioritizeTexturesAndBackings(resource_manager.get());
  for (size_t i = 0; i < kMaxTextures; ++i)
    EXPECT_EQ(ValidateTexture(textures[i].get(), false), i < 4);
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ReduceMemory(resource_provider());
  }
  EXPECT_EQ(TexturesMemorySize(4), resource_manager->MemoryAboveCutoffBytes());

  // Do a one-time eviction for one more texture based on priority cutoff
  resource_manager->UnlinkAndClearEvictedBackings();
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ReduceMemoryOnImplThread(
        TexturesMemorySize(8), 104, resource_provider());
    EXPECT_EQ(0u, EvictedBackingCount(resource_manager.get()));
    resource_manager->ReduceMemoryOnImplThread(
        TexturesMemorySize(8), 103, resource_provider());
    EXPECT_EQ(1u, EvictedBackingCount(resource_manager.get()));
  }
  resource_manager->UnlinkAndClearEvictedBackings();
  EXPECT_EQ(TexturesMemorySize(3), resource_manager->MemoryUseBytes());

  // Re-allocate the the texture after the one-time drop.
  PrioritizeTexturesAndBackings(resource_manager.get());
  for (size_t i = 0; i < kMaxTextures; ++i)
    EXPECT_EQ(ValidateTexture(textures[i].get(), false), i < 4);
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ReduceMemory(resource_provider());
  }
  EXPECT_EQ(TexturesMemorySize(4), resource_manager->MemoryAboveCutoffBytes());

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager->ClearAllMemory(resource_provider());
}

TEST_F(PrioritizedResourceTest, EvictingTexturesInParent) {
  const size_t kMaxTextures = 8;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);
  scoped_ptr<PrioritizedResource> textures[kMaxTextures];
  unsigned texture_resource_ids[kMaxTextures];

  for (size_t i = 0; i < kMaxTextures; ++i) {
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);
    textures[i]->set_request_priority(100 + i);
  }

  PrioritizeTexturesAndBackings(resource_manager.get());
  for (size_t i = 0; i < kMaxTextures; ++i) {
    EXPECT_TRUE(ValidateTexture(textures[i].get(), true));

    {
      DebugScopedSetImplThreadAndMainThreadBlocked
          impl_thread_and_main_thread_blocked(&proxy_);
      uint8_t image[4] = {0};
      textures[i]->SetPixels(resource_provider_.get(),
                             image,
                             gfx::Rect(1, 1),
                             gfx::Rect(1, 1),
                             gfx::Vector2d());
    }
  }
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ReduceMemory(resource_provider());
  }
  EXPECT_EQ(TexturesMemorySize(8), resource_manager->MemoryAboveCutoffBytes());

  for (size_t i = 0; i < 8; ++i)
    texture_resource_ids[i] = textures[i]->resource_id();

  // Evict four textures. It will be the last four.
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ReduceMemoryOnImplThread(
        TexturesMemorySize(4), 200, resource_provider());

    EXPECT_EQ(4u, EvictedBackingCount(resource_manager.get()));

    // The last four backings are evicted.
    std::vector<unsigned> remaining = BackingResources(resource_manager.get());
    EXPECT_TRUE(std::find(remaining.begin(),
                          remaining.end(),
                          texture_resource_ids[0]) != remaining.end());
    EXPECT_TRUE(std::find(remaining.begin(),
                          remaining.end(),
                          texture_resource_ids[1]) != remaining.end());
    EXPECT_TRUE(std::find(remaining.begin(),
                          remaining.end(),
                          texture_resource_ids[2]) != remaining.end());
    EXPECT_TRUE(std::find(remaining.begin(),
                          remaining.end(),
                          texture_resource_ids[3]) != remaining.end());
  }
  resource_manager->UnlinkAndClearEvictedBackings();
  EXPECT_EQ(TexturesMemorySize(4), resource_manager->MemoryUseBytes());

  // Re-allocate the the texture after the eviction.
  PrioritizeTexturesAndBackings(resource_manager.get());
  for (size_t i = 0; i < kMaxTextures; ++i) {
    EXPECT_TRUE(ValidateTexture(textures[i].get(), true));

    {
      DebugScopedSetImplThreadAndMainThreadBlocked
          impl_thread_and_main_thread_blocked(&proxy_);
      uint8_t image[4] = {0};
      textures[i]->SetPixels(resource_provider_.get(),
                             image,
                             gfx::Rect(1, 1),
                             gfx::Rect(1, 1),
                             gfx::Vector2d());
    }
  }
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ReduceMemory(resource_provider());
  }
  EXPECT_EQ(TexturesMemorySize(8), resource_manager->MemoryAboveCutoffBytes());

  // Send the last two of the textures to a parent compositor.
  ResourceProvider::ResourceIdArray to_send;
  TransferableResourceArray transferable;
  for (size_t i = 6; i < 8; ++i)
    to_send.push_back(textures[i]->resource_id());
  resource_provider_->PrepareSendToParent(to_send, &transferable);

  // Set the last two textures to be tied for prioity with the two
  // before them. Being sent to the parent will break the tie.
  textures[4]->set_request_priority(100 + 4);
  textures[5]->set_request_priority(100 + 5);
  textures[6]->set_request_priority(100 + 4);
  textures[7]->set_request_priority(100 + 5);

  for (size_t i = 0; i < 8; ++i)
    texture_resource_ids[i] = textures[i]->resource_id();

  // Drop all the textures. Now we have backings that can be recycled.
  for (size_t i = 0; i < 8; ++i)
    textures[0].reset();
  PrioritizeTexturesAndBackings(resource_manager.get());

  // The next commit finishes.
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->UpdateBackingsState(resource_provider());
  }

  // Evict four textures. It would be the last four again, except that 2 of them
  // are sent to the parent, so they are evicted last.
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ReduceMemoryOnImplThread(
        TexturesMemorySize(4), 200, resource_provider());

    EXPECT_EQ(4u, EvictedBackingCount(resource_manager.get()));
    // The last 2 backings remain this time.
    std::vector<unsigned> remaining = BackingResources(resource_manager.get());
    EXPECT_TRUE(std::find(remaining.begin(),
                          remaining.end(),
                          texture_resource_ids[6]) == remaining.end());
    EXPECT_TRUE(std::find(remaining.begin(),
                          remaining.end(),
                          texture_resource_ids[7]) == remaining.end());
  }
  resource_manager->UnlinkAndClearEvictedBackings();
  EXPECT_EQ(TexturesMemorySize(4), resource_manager->MemoryUseBytes());

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager->ClearAllMemory(resource_provider());
}

TEST_F(PrioritizedResourceTest, ResourceManagerPartialUpdateTextures) {
  const size_t kMaxTextures = 4;
  const size_t kNumTextures = 4;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);
  scoped_ptr<PrioritizedResource> textures[kNumTextures];
  scoped_ptr<PrioritizedResource> more_textures[kNumTextures];

  for (size_t i = 0; i < kNumTextures; ++i) {
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);
    more_textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);
  }

  for (size_t i = 0; i < kNumTextures; ++i)
    textures[i]->set_request_priority(200 + i);
  PrioritizeTexturesAndBackings(resource_manager.get());

  // Allocate textures which are currently high priority.
  EXPECT_TRUE(ValidateTexture(textures[0].get(), false));
  EXPECT_TRUE(ValidateTexture(textures[1].get(), false));
  EXPECT_TRUE(ValidateTexture(textures[2].get(), false));
  EXPECT_TRUE(ValidateTexture(textures[3].get(), false));

  EXPECT_TRUE(textures[0]->have_backing_texture());
  EXPECT_TRUE(textures[1]->have_backing_texture());
  EXPECT_TRUE(textures[2]->have_backing_texture());
  EXPECT_TRUE(textures[3]->have_backing_texture());

  for (size_t i = 0; i < kNumTextures; ++i)
    more_textures[i]->set_request_priority(100 + i);
  PrioritizeTexturesAndBackings(resource_manager.get());

  // Textures are now below cutoff.
  EXPECT_FALSE(ValidateTexture(textures[0].get(), false));
  EXPECT_FALSE(ValidateTexture(textures[1].get(), false));
  EXPECT_FALSE(ValidateTexture(textures[2].get(), false));
  EXPECT_FALSE(ValidateTexture(textures[3].get(), false));

  // But they are still valid to use.
  EXPECT_TRUE(textures[0]->have_backing_texture());
  EXPECT_TRUE(textures[1]->have_backing_texture());
  EXPECT_TRUE(textures[2]->have_backing_texture());
  EXPECT_TRUE(textures[3]->have_backing_texture());

  // Higher priority textures are finally needed.
  EXPECT_TRUE(ValidateTexture(more_textures[0].get(), false));
  EXPECT_TRUE(ValidateTexture(more_textures[1].get(), false));
  EXPECT_TRUE(ValidateTexture(more_textures[2].get(), false));
  EXPECT_TRUE(ValidateTexture(more_textures[3].get(), false));

  // Lower priority have been fully evicted.
  EXPECT_FALSE(textures[0]->have_backing_texture());
  EXPECT_FALSE(textures[1]->have_backing_texture());
  EXPECT_FALSE(textures[2]->have_backing_texture());
  EXPECT_FALSE(textures[3]->have_backing_texture());

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager->ClearAllMemory(resource_provider());
}

TEST_F(PrioritizedResourceTest, ResourceManagerPrioritiesAreEqual) {
  const size_t kMaxTextures = 16;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);
  scoped_ptr<PrioritizedResource> textures[kMaxTextures];

  for (size_t i = 0; i < kMaxTextures; ++i) {
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);
  }

  // All 16 textures have the same priority except 2 higher priority.
  for (size_t i = 0; i < kMaxTextures; ++i)
    textures[i]->set_request_priority(100);
  textures[0]->set_request_priority(99);
  textures[1]->set_request_priority(99);

  // Set max limit to 8 textures
  resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(8));
  PrioritizeTexturesAndBackings(resource_manager.get());

  // The two high priority textures should be available, others should not.
  for (size_t i = 0; i < 2; ++i)
    EXPECT_TRUE(ValidateTexture(textures[i].get(), false));
  for (size_t i = 2; i < kMaxTextures; ++i)
    EXPECT_FALSE(ValidateTexture(textures[i].get(), false));
  EXPECT_EQ(TexturesMemorySize(2), resource_manager->MemoryAboveCutoffBytes());
  EXPECT_LE(resource_manager->MemoryUseBytes(),
            resource_manager->MemoryAboveCutoffBytes());

  // Manually reserving textures should only succeed on the higher priority
  // textures, and on remaining textures up to the memory limit.
  for (size_t i = 0; i < 8; i++)
    EXPECT_TRUE(ValidateTexture(textures[i].get(), true));
  for (size_t i = 9; i < kMaxTextures; i++)
    EXPECT_FALSE(ValidateTexture(textures[i].get(), true));
  EXPECT_EQ(TexturesMemorySize(8), resource_manager->MemoryAboveCutoffBytes());
  EXPECT_LE(resource_manager->MemoryUseBytes(),
            resource_manager->MemoryAboveCutoffBytes());

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager->ClearAllMemory(resource_provider());
}

TEST_F(PrioritizedResourceTest, ResourceManagerDestroyedFirst) {
  scoped_ptr<PrioritizedResourceManager> resource_manager = CreateManager(1);
  scoped_ptr<PrioritizedResource> texture =
      resource_manager->CreateTexture(texture_size_, texture_format_);

  // Texture is initially invalid, but it will become available.
  EXPECT_FALSE(texture->have_backing_texture());

  texture->set_request_priority(100);
  PrioritizeTexturesAndBackings(resource_manager.get());

  EXPECT_TRUE(ValidateTexture(texture.get(), false));
  EXPECT_TRUE(texture->can_acquire_backing_texture());
  EXPECT_TRUE(texture->have_backing_texture());
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ClearAllMemory(resource_provider());
  }
  resource_manager.reset();

  EXPECT_FALSE(texture->can_acquire_backing_texture());
  EXPECT_FALSE(texture->have_backing_texture());
}

TEST_F(PrioritizedResourceTest, TextureMovedToNewManager) {
  scoped_ptr<PrioritizedResourceManager> resource_manager_one =
      CreateManager(1);
  scoped_ptr<PrioritizedResourceManager> resource_manager_two =
      CreateManager(1);
  scoped_ptr<PrioritizedResource> texture =
      resource_manager_one->CreateTexture(texture_size_, texture_format_);

  // Texture is initially invalid, but it will become available.
  EXPECT_FALSE(texture->have_backing_texture());

  texture->set_request_priority(100);
  PrioritizeTexturesAndBackings(resource_manager_one.get());

  EXPECT_TRUE(ValidateTexture(texture.get(), false));
  EXPECT_TRUE(texture->can_acquire_backing_texture());
  EXPECT_TRUE(texture->have_backing_texture());

  texture->SetTextureManager(NULL);
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager_one->ClearAllMemory(resource_provider());
  }
  resource_manager_one.reset();

  EXPECT_FALSE(texture->can_acquire_backing_texture());
  EXPECT_FALSE(texture->have_backing_texture());

  texture->SetTextureManager(resource_manager_two.get());

  PrioritizeTexturesAndBackings(resource_manager_two.get());

  EXPECT_TRUE(ValidateTexture(texture.get(), false));
  EXPECT_TRUE(texture->can_acquire_backing_texture());
  EXPECT_TRUE(texture->have_backing_texture());

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager_two->ClearAllMemory(resource_provider());
}

TEST_F(PrioritizedResourceTest,
       RenderSurfacesReduceMemoryAvailableOutsideRootSurface) {
  const size_t kMaxTextures = 8;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);

  // Half of the memory is taken by surfaces (with high priority place-holder)
  scoped_ptr<PrioritizedResource> render_surface_place_holder =
      resource_manager->CreateTexture(texture_size_, texture_format_);
  render_surface_place_holder->SetToSelfManagedMemoryPlaceholder(
      TexturesMemorySize(4));
  render_surface_place_holder->set_request_priority(
      PriorityCalculator::RenderSurfacePriority());

  // Create textures to fill our memory limit.
  scoped_ptr<PrioritizedResource> textures[kMaxTextures];

  for (size_t i = 0; i < kMaxTextures; ++i) {
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);
  }

  // Set decreasing non-visible priorities outside root surface.
  for (size_t i = 0; i < kMaxTextures; ++i)
    textures[i]->set_request_priority(100 + i);

  // Only lower half should be available.
  PrioritizeTexturesAndBackings(resource_manager.get());
  EXPECT_TRUE(ValidateTexture(textures[0].get(), false));
  EXPECT_TRUE(ValidateTexture(textures[3].get(), false));
  EXPECT_FALSE(ValidateTexture(textures[4].get(), false));
  EXPECT_FALSE(ValidateTexture(textures[7].get(), false));

  // Set increasing non-visible priorities outside root surface.
  for (size_t i = 0; i < kMaxTextures; ++i)
    textures[i]->set_request_priority(100 - i);

  // Only upper half should be available.
  PrioritizeTexturesAndBackings(resource_manager.get());
  EXPECT_FALSE(ValidateTexture(textures[0].get(), false));
  EXPECT_FALSE(ValidateTexture(textures[3].get(), false));
  EXPECT_TRUE(ValidateTexture(textures[4].get(), false));
  EXPECT_TRUE(ValidateTexture(textures[7].get(), false));

  EXPECT_EQ(TexturesMemorySize(4), resource_manager->MemoryAboveCutoffBytes());
  EXPECT_EQ(TexturesMemorySize(4),
            resource_manager->MemoryForSelfManagedTextures());
  EXPECT_LE(resource_manager->MemoryUseBytes(),
            resource_manager->MemoryAboveCutoffBytes());
  EXPECT_EQ(TexturesMemorySize(8),
            resource_manager->MaxMemoryNeededBytes());

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager->ClearAllMemory(resource_provider());
}

TEST_F(PrioritizedResourceTest,
       RenderSurfacesReduceMemoryAvailableForRequestLate) {
  const size_t kMaxTextures = 8;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);

  // Half of the memory is taken by surfaces (with high priority place-holder)
  scoped_ptr<PrioritizedResource> render_surface_place_holder =
      resource_manager->CreateTexture(texture_size_, texture_format_);
  render_surface_place_holder->SetToSelfManagedMemoryPlaceholder(
      TexturesMemorySize(4));
  render_surface_place_holder->set_request_priority(
      PriorityCalculator::RenderSurfacePriority());

  // Create textures to fill our memory limit.
  scoped_ptr<PrioritizedResource> textures[kMaxTextures];

  for (size_t i = 0; i < kMaxTextures; ++i) {
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);
  }

  // Set equal priorities.
  for (size_t i = 0; i < kMaxTextures; ++i)
    textures[i]->set_request_priority(100);

  // The first four to be requested late will be available.
  PrioritizeTexturesAndBackings(resource_manager.get());
  for (unsigned i = 0; i < kMaxTextures; ++i)
    EXPECT_FALSE(ValidateTexture(textures[i].get(), false));
  for (unsigned i = 0; i < kMaxTextures; i += 2)
    EXPECT_TRUE(ValidateTexture(textures[i].get(), true));
  for (unsigned i = 1; i < kMaxTextures; i += 2)
    EXPECT_FALSE(ValidateTexture(textures[i].get(), true));

  EXPECT_EQ(TexturesMemorySize(4), resource_manager->MemoryAboveCutoffBytes());
  EXPECT_EQ(TexturesMemorySize(4),
            resource_manager->MemoryForSelfManagedTextures());
  EXPECT_LE(resource_manager->MemoryUseBytes(),
            resource_manager->MemoryAboveCutoffBytes());
  EXPECT_EQ(TexturesMemorySize(8),
            resource_manager->MaxMemoryNeededBytes());

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager->ClearAllMemory(resource_provider());
}

TEST_F(PrioritizedResourceTest,
       WhenRenderSurfaceNotAvailableTexturesAlsoNotAvailable) {
  const size_t kMaxTextures = 8;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);

  // Half of the memory is taken by surfaces (with high priority place-holder)
  scoped_ptr<PrioritizedResource> render_surface_place_holder =
      resource_manager->CreateTexture(texture_size_, texture_format_);
  render_surface_place_holder->SetToSelfManagedMemoryPlaceholder(
      TexturesMemorySize(4));
  render_surface_place_holder->set_request_priority(
      PriorityCalculator::RenderSurfacePriority());

  // Create textures to fill our memory limit.
  scoped_ptr<PrioritizedResource> textures[kMaxTextures];

  for (size_t i = 0; i < kMaxTextures; ++i)
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);

  // Set 6 visible textures in the root surface, and 2 in a child surface.
  for (size_t i = 0; i < 6; ++i) {
    textures[i]->
        set_request_priority(PriorityCalculator::VisiblePriority(true));
  }
  for (size_t i = 6; i < 8; ++i) {
    textures[i]->
        set_request_priority(PriorityCalculator::VisiblePriority(false));
  }

  PrioritizeTexturesAndBackings(resource_manager.get());

  // Unable to request_late textures in the child surface.
  EXPECT_FALSE(ValidateTexture(textures[6].get(), true));
  EXPECT_FALSE(ValidateTexture(textures[7].get(), true));

  // Root surface textures are valid.
  for (size_t i = 0; i < 6; ++i)
    EXPECT_TRUE(ValidateTexture(textures[i].get(), false));

  EXPECT_EQ(TexturesMemorySize(6), resource_manager->MemoryAboveCutoffBytes());
  EXPECT_EQ(TexturesMemorySize(2),
            resource_manager->MemoryForSelfManagedTextures());
  EXPECT_LE(resource_manager->MemoryUseBytes(),
            resource_manager->MemoryAboveCutoffBytes());

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager->ClearAllMemory(resource_provider());
}

TEST_F(PrioritizedResourceTest, RequestLateBackingsSorting) {
  const size_t kMaxTextures = 8;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);
  resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(kMaxTextures));

  // Create textures to fill our memory limit.
  scoped_ptr<PrioritizedResource> textures[kMaxTextures];
  for (size_t i = 0; i < kMaxTextures; ++i)
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);

  // Set equal priorities, and allocate backings for all textures.
  for (size_t i = 0; i < kMaxTextures; ++i)
    textures[i]->set_request_priority(100);
  PrioritizeTexturesAndBackings(resource_manager.get());
  for (unsigned i = 0; i < kMaxTextures; ++i)
    EXPECT_TRUE(ValidateTexture(textures[i].get(), false));

  // Drop the memory limit and prioritize (none will be above the threshold,
  // but they still have backings because ReduceMemory hasn't been called).
  resource_manager->SetMaxMemoryLimitBytes(
      TexturesMemorySize(kMaxTextures / 2));
  PrioritizeTexturesAndBackings(resource_manager.get());

  // Push half of them back over the limit.
  for (size_t i = 0; i < kMaxTextures; i += 2)
    EXPECT_TRUE(textures[i]->RequestLate());

  // Push the priorities to the backings array and sort the backings array
  ResourceManagerUpdateBackingsPriorities(resource_manager.get());

  // Assert that the backings list be sorted with the below-limit backings
  // before the above-limit backings.
  ResourceManagerAssertInvariants(resource_manager.get());

  // Make sure that we have backings for all of the textures.
  for (size_t i = 0; i < kMaxTextures; ++i)
    EXPECT_TRUE(textures[i]->have_backing_texture());

  // Make sure that only the request_late textures are above the priority
  // cutoff
  for (size_t i = 0; i < kMaxTextures; i += 2)
    EXPECT_TRUE(TextureBackingIsAbovePriorityCutoff(textures[i].get()));
  for (size_t i = 1; i < kMaxTextures; i += 2)
    EXPECT_FALSE(TextureBackingIsAbovePriorityCutoff(textures[i].get()));

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager->ClearAllMemory(resource_provider());
}

TEST_F(PrioritizedResourceTest, ClearUploadsToEvictedResources) {
  const size_t kMaxTextures = 4;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);
  resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(kMaxTextures));

  // Create textures to fill our memory limit.
  scoped_ptr<PrioritizedResource> textures[kMaxTextures];

  for (size_t i = 0; i < kMaxTextures; ++i)
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);

  // Set equal priorities, and allocate backings for all textures.
  for (size_t i = 0; i < kMaxTextures; ++i)
    textures[i]->set_request_priority(100);
  PrioritizeTexturesAndBackings(resource_manager.get());
  for (unsigned i = 0; i < kMaxTextures; ++i)
    EXPECT_TRUE(ValidateTexture(textures[i].get(), false));

  ResourceUpdateQueue queue;
  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  for (size_t i = 0; i < kMaxTextures; ++i) {
    const ResourceUpdate upload = ResourceUpdate::Create(
        textures[i].get(), NULL, gfx::Rect(), gfx::Rect(), gfx::Vector2d());
    queue.AppendFullUpload(upload);
  }

  // Make sure that we have backings for all of the textures.
  for (size_t i = 0; i < kMaxTextures; ++i)
    EXPECT_TRUE(textures[i]->have_backing_texture());

  queue.ClearUploadsToEvictedResources();
  EXPECT_EQ(4u, queue.FullUploadSize());

  resource_manager->ReduceMemoryOnImplThread(
      TexturesMemorySize(1),
      PriorityCalculator::AllowEverythingCutoff(),
      resource_provider());
  queue.ClearUploadsToEvictedResources();
  EXPECT_EQ(1u, queue.FullUploadSize());

  resource_manager->ReduceMemoryOnImplThread(
      0, PriorityCalculator::AllowEverythingCutoff(), resource_provider());
  queue.ClearUploadsToEvictedResources();
  EXPECT_EQ(0u, queue.FullUploadSize());
}

TEST_F(PrioritizedResourceTest, UsageStatistics) {
  const size_t kMaxTextures = 5;
  scoped_ptr<PrioritizedResourceManager> resource_manager =
      CreateManager(kMaxTextures);
  scoped_ptr<PrioritizedResource> textures[kMaxTextures];

  for (size_t i = 0; i < kMaxTextures; ++i) {
    textures[i] =
        resource_manager->CreateTexture(texture_size_, texture_format_);
  }

  textures[0]->
      set_request_priority(PriorityCalculator::AllowVisibleOnlyCutoff() - 1);
  textures[1]->
      set_request_priority(PriorityCalculator::AllowVisibleOnlyCutoff());
  textures[2]->set_request_priority(
      PriorityCalculator::AllowVisibleAndNearbyCutoff() - 1);
  textures[3]->
      set_request_priority(PriorityCalculator::AllowVisibleAndNearbyCutoff());
  textures[4]->set_request_priority(
      PriorityCalculator::AllowVisibleAndNearbyCutoff() + 1);

  // Set max limit to 2 textures.
  resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(2));
  PrioritizeTexturesAndBackings(resource_manager.get());

  // The first two textures should be available, others should not.
  for (size_t i = 0; i < 2; ++i)
    EXPECT_TRUE(ValidateTexture(textures[i].get(), false));
  for (size_t i = 2; i < kMaxTextures; ++i)
    EXPECT_FALSE(ValidateTexture(textures[i].get(), false));

  // Validate the statistics.
  {
    DebugScopedSetImplThread impl_thread(&proxy_);
    EXPECT_EQ(TexturesMemorySize(2), resource_manager->MemoryUseBytes());
    EXPECT_EQ(TexturesMemorySize(1), resource_manager->MemoryVisibleBytes());
    EXPECT_EQ(TexturesMemorySize(3),
              resource_manager->MemoryVisibleAndNearbyBytes());
  }

  // Re-prioritize the textures, but do not push the values to backings.
  textures[0]->
      set_request_priority(PriorityCalculator::AllowVisibleOnlyCutoff() - 1);
  textures[1]->
      set_request_priority(PriorityCalculator::AllowVisibleOnlyCutoff() - 1);
  textures[2]->
      set_request_priority(PriorityCalculator::AllowVisibleOnlyCutoff() - 1);
  textures[3]->set_request_priority(
      PriorityCalculator::AllowVisibleAndNearbyCutoff() - 1);
  textures[4]->
      set_request_priority(PriorityCalculator::AllowVisibleAndNearbyCutoff());
  resource_manager->PrioritizeTextures();

  // Verify that we still see the old values.
  {
    DebugScopedSetImplThread impl_thread(&proxy_);
    EXPECT_EQ(TexturesMemorySize(2), resource_manager->MemoryUseBytes());
    EXPECT_EQ(TexturesMemorySize(1), resource_manager->MemoryVisibleBytes());
    EXPECT_EQ(TexturesMemorySize(3),
              resource_manager->MemoryVisibleAndNearbyBytes());
  }

  // Push priorities to backings, and verify we see the new values.
  {
    DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->PushTexturePrioritiesToBackings();
    EXPECT_EQ(TexturesMemorySize(2), resource_manager->MemoryUseBytes());
    EXPECT_EQ(TexturesMemorySize(3), resource_manager->MemoryVisibleBytes());
    EXPECT_EQ(TexturesMemorySize(4),
              resource_manager->MemoryVisibleAndNearbyBytes());
  }

  DebugScopedSetImplThreadAndMainThreadBlocked
      impl_thread_and_main_thread_blocked(&proxy_);
  resource_manager->ClearAllMemory(resource_provider());
}

}  // namespace
}  // namespace cc
