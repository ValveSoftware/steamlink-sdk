// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_factory.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/resources/resource_provider.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);
static constexpr FrameSinkId kAnotherArbitraryFrameSinkId(2, 2);
static const base::UnguessableToken kArbitraryToken =
    base::UnguessableToken::Create();

class TestSurfaceFactoryClient : public SurfaceFactoryClient {
 public:
  TestSurfaceFactoryClient() : begin_frame_source_(nullptr) {}
  ~TestSurfaceFactoryClient() override {}

  void ReturnResources(const ReturnedResourceArray& resources) override {
    returned_resources_.insert(
        returned_resources_.end(), resources.begin(), resources.end());
  }

  void SetBeginFrameSource(BeginFrameSource* begin_frame_source) override {
    begin_frame_source_ = begin_frame_source;
  }

  const ReturnedResourceArray& returned_resources() const {
    return returned_resources_;
  }

  void clear_returned_resources() { returned_resources_.clear(); }

  BeginFrameSource* begin_frame_source() const { return begin_frame_source_; }

 private:
  ReturnedResourceArray returned_resources_;
  BeginFrameSource* begin_frame_source_;

  DISALLOW_COPY_AND_ASSIGN(TestSurfaceFactoryClient);
};

gpu::SyncToken GenTestSyncToken(int id) {
  gpu::SyncToken token;
  token.Set(gpu::CommandBufferNamespace::GPU_IO, 0,
            gpu::CommandBufferId::FromUnsafeValue(id), 1);
  return token;
}

class SurfaceFactoryTest : public testing::Test, public SurfaceObserver {
 public:
  SurfaceFactoryTest()
      : factory_(
            new SurfaceFactory(kArbitraryFrameSinkId, &manager_, &client_)),
        local_frame_id_(3, kArbitraryToken),
        frame_sync_token_(GenTestSyncToken(4)),
        consumer_sync_token_(GenTestSyncToken(5)) {
    manager_.AddObserver(this);
    factory_->Create(local_frame_id_);
  }

  const SurfaceId& last_created_surface_id() const {
    return last_created_surface_id_;
  }

  // SurfaceObserver implementation.
  void OnSurfaceCreated(const SurfaceId& surface_id,
                        const gfx::Size& frame,
                        float device_scale_factor) override {
    EXPECT_EQ(kArbitraryFrameSinkId, surface_id.frame_sink_id());
    last_created_surface_id_ = surface_id;
  }

  void OnSurfaceDamaged(const SurfaceId& id, bool* changed) override {
    *changed = true;
  }

  ~SurfaceFactoryTest() override {
    if (local_frame_id_.is_valid())
      factory_->Destroy(local_frame_id_);
    manager_.RemoveObserver(this);
  }

  void SubmitCompositorFrameWithResources(ResourceId* resource_ids,
                                          size_t num_resource_ids) {
    std::unique_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
    for (size_t i = 0u; i < num_resource_ids; ++i) {
      TransferableResource resource;
      resource.id = resource_ids[i];
      resource.mailbox_holder.texture_target = GL_TEXTURE_2D;
      resource.mailbox_holder.sync_token = frame_sync_token_;
      frame_data->resource_list.push_back(resource);
    }
    CompositorFrame frame;
    frame.delegated_frame_data = std::move(frame_data);
    factory_->SubmitCompositorFrame(local_frame_id_, std::move(frame),
                                    SurfaceFactory::DrawCallback());
    EXPECT_EQ(last_created_surface_id_.local_frame_id(), local_frame_id_);
  }

  void UnrefResources(ResourceId* ids_to_unref,
                      int* counts_to_unref,
                      size_t num_ids_to_unref) {
    ReturnedResourceArray unref_array;
    for (size_t i = 0; i < num_ids_to_unref; ++i) {
      ReturnedResource resource;
      resource.sync_token = consumer_sync_token_;
      resource.id = ids_to_unref[i];
      resource.count = counts_to_unref[i];
      unref_array.push_back(resource);
    }
    factory_->UnrefResources(unref_array);
  }

  void CheckReturnedResourcesMatchExpected(ResourceId* expected_returned_ids,
                                           int* expected_returned_counts,
                                           size_t expected_resources,
                                           gpu::SyncToken expected_sync_token) {
    const ReturnedResourceArray& actual_resources =
        client_.returned_resources();
    ASSERT_EQ(expected_resources, actual_resources.size());
    for (size_t i = 0; i < expected_resources; ++i) {
      ReturnedResource resource = actual_resources[i];
      EXPECT_EQ(expected_sync_token, resource.sync_token);
      EXPECT_EQ(expected_returned_ids[i], resource.id);
      EXPECT_EQ(expected_returned_counts[i], resource.count);
    }
    client_.clear_returned_resources();
  }

  void RefCurrentFrameResources() {
    Surface* surface = manager_.GetSurfaceForId(
        SurfaceId(factory_->frame_sink_id(), local_frame_id_));
    factory_->RefResources(
        surface->GetEligibleFrame().delegated_frame_data->resource_list);
  }

 protected:
  SurfaceManager manager_;
  TestSurfaceFactoryClient client_;
  std::unique_ptr<SurfaceFactory> factory_;
  LocalFrameId local_frame_id_;
  SurfaceId last_created_surface_id_;

  // This is the sync token submitted with the frame. It should never be
  // returned to the client.
  const gpu::SyncToken frame_sync_token_;

  // This is the sync token returned by the consumer. It should always be
  // returned to the client.
  const gpu::SyncToken consumer_sync_token_;
};

// Tests submitting a frame with resources followed by one with no resources
// with no resource provider action in between.
TEST_F(SurfaceFactoryTest, ResourceLifetimeSimple) {
  ResourceId first_frame_ids[] = {1, 2, 3};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u, client_.returned_resources().size());
  client_.clear_returned_resources();

  // The second frame references no resources and thus should make all resources
  // available to be returned.
  SubmitCompositorFrameWithResources(NULL, 0);

  ResourceId expected_returned_ids[] = {1, 2, 3};
  int expected_returned_counts[] = {1, 1, 1};
  // Resources were never consumed so no sync token should be set.
  CheckReturnedResourcesMatchExpected(
      expected_returned_ids, expected_returned_counts,
      arraysize(expected_returned_counts), gpu::SyncToken());
}

// Tests submitting a frame with resources followed by one with no resources
// with the resource provider holding everything alive.
TEST_F(SurfaceFactoryTest, ResourceLifetimeSimpleWithProviderHoldingAlive) {
  ResourceId first_frame_ids[] = {1, 2, 3};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u, client_.returned_resources().size());
  client_.clear_returned_resources();

  // Hold on to everything.
  RefCurrentFrameResources();

  // The second frame references no resources and thus should make all resources
  // available to be returned as soon as the resource provider releases them.
  SubmitCompositorFrameWithResources(NULL, 0);

  EXPECT_EQ(0u, client_.returned_resources().size());
  client_.clear_returned_resources();

  int release_counts[] = {1, 1, 1};
  UnrefResources(first_frame_ids, release_counts, arraysize(first_frame_ids));

  ResourceId expected_returned_ids[] = {1, 2, 3};
  int expected_returned_counts[] = {1, 1, 1};
  CheckReturnedResourcesMatchExpected(
      expected_returned_ids, expected_returned_counts,
      arraysize(expected_returned_counts), consumer_sync_token_);
}

// Tests referencing a resource, unref'ing it to zero, then using it again
// before returning it to the client.
TEST_F(SurfaceFactoryTest, ResourceReusedBeforeReturn) {
  ResourceId first_frame_ids[] = {7};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // This removes all references to resource id 7.
  SubmitCompositorFrameWithResources(NULL, 0);

  // This references id 7 again.
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // This removes it again.
  SubmitCompositorFrameWithResources(NULL, 0);

  // Now it should be returned.
  // We don't care how many entries are in the returned array for 7, so long as
  // the total returned count matches the submitted count.
  const ReturnedResourceArray& returned = client_.returned_resources();
  size_t return_count = 0;
  for (size_t i = 0; i < returned.size(); ++i) {
    EXPECT_EQ(7u, returned[i].id);
    return_count += returned[i].count;
  }
  EXPECT_EQ(2u, return_count);
}

// Tests having resources referenced multiple times, as if referenced by
// multiple providers.
TEST_F(SurfaceFactoryTest, ResourceRefMultipleTimes) {
  ResourceId first_frame_ids[] = {3, 4};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // Ref resources from the first frame twice.
  RefCurrentFrameResources();
  RefCurrentFrameResources();

  ResourceId second_frame_ids[] = {4, 5};
  SubmitCompositorFrameWithResources(second_frame_ids,
                                     arraysize(second_frame_ids));

  // Ref resources from the second frame 3 times.
  RefCurrentFrameResources();
  RefCurrentFrameResources();
  RefCurrentFrameResources();

  // Submit a frame with no resources to remove all current frame refs from
  // submitted resources.
  SubmitCompositorFrameWithResources(NULL, 0);

  EXPECT_EQ(0u, client_.returned_resources().size());
  client_.clear_returned_resources();

  // Expected current refs:
  //  3 -> 2
  //  4 -> 2 + 3 = 5
  //  5 -> 3
  {
    SCOPED_TRACE("unref all 3");
    ResourceId ids_to_unref[] = {3, 4, 5};
    int counts[] = {1, 1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));

    EXPECT_EQ(0u, client_.returned_resources().size());
    client_.clear_returned_resources();

    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));

    ResourceId expected_returned_ids[] = {3};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), consumer_sync_token_);
  }

  // Expected refs remaining:
  //  4 -> 3
  //  5 -> 1
  {
    SCOPED_TRACE("unref 4 and 5");
    ResourceId ids_to_unref[] = {4, 5};
    int counts[] = {1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));

    ResourceId expected_returned_ids[] = {5};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), consumer_sync_token_);
  }

  // Now, just 2 refs remaining on resource 4. Unref both at once and make sure
  // the returned count is correct.
  {
    SCOPED_TRACE("unref only 4");
    ResourceId ids_to_unref[] = {4};
    int counts[] = {2};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));

    ResourceId expected_returned_ids[] = {4};
    int expected_returned_counts[] = {2};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), consumer_sync_token_);
  }
}

TEST_F(SurfaceFactoryTest, ResourceLifetime) {
  ResourceId first_frame_ids[] = {1, 2, 3};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     arraysize(first_frame_ids));

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u, client_.returned_resources().size());
  client_.clear_returned_resources();

  // The second frame references some of the same resources, but some different
  // ones. We expect to receive back resource 1 with a count of 1 since it was
  // only referenced by the first frame.
  ResourceId second_frame_ids[] = {2, 3, 4};
  SubmitCompositorFrameWithResources(second_frame_ids,
                                     arraysize(second_frame_ids));

  {
    SCOPED_TRACE("second frame");
    ResourceId expected_returned_ids[] = {1};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), gpu::SyncToken());
  }

  // The third frame references a disjoint set of resources, so we expect to
  // receive back all resources from the first and second frames. Resource IDs 2
  // and 3 will have counts of 2, since they were used in both frames, and
  // resource ID 4 will have a count of 1.
  ResourceId third_frame_ids[] = {10, 11, 12, 13};
  SubmitCompositorFrameWithResources(third_frame_ids,
                                     arraysize(third_frame_ids));

  {
    SCOPED_TRACE("third frame");
    ResourceId expected_returned_ids[] = {2, 3, 4};
    int expected_returned_counts[] = {2, 2, 1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), gpu::SyncToken());
  }

  // Simulate a ResourceProvider taking a ref on all of the resources.
  RefCurrentFrameResources();

  ResourceId fourth_frame_ids[] = {12, 13};
  SubmitCompositorFrameWithResources(fourth_frame_ids,
                                     arraysize(fourth_frame_ids));

  EXPECT_EQ(0u, client_.returned_resources().size());

  RefCurrentFrameResources();

  // All resources are still being used by the external reference, so none can
  // be returned to the client.
  EXPECT_EQ(0u, client_.returned_resources().size());

  // Release resources associated with the first RefCurrentFrameResources() call
  // first.
  {
    ResourceId ids_to_unref[] = {10, 11, 12, 13};
    int counts[] = {1, 1, 1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));
  }

  {
    SCOPED_TRACE("fourth frame, first unref");
    ResourceId expected_returned_ids[] = {10, 11};
    int expected_returned_counts[] = {1, 1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), consumer_sync_token_);
  }

  {
    ResourceId ids_to_unref[] = {12, 13};
    int counts[] = {1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref));
  }

  // Resources 12 and 13 are still in use by the current frame, so they
  // shouldn't be available to be returned.
  EXPECT_EQ(0u, client_.returned_resources().size());

  // If we submit an empty frame, however, they should become available.
  SubmitCompositorFrameWithResources(NULL, 0u);

  {
    SCOPED_TRACE("fourth frame, second unref");
    ResourceId expected_returned_ids[] = {12, 13};
    int expected_returned_counts[] = {2, 2};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        arraysize(expected_returned_counts), consumer_sync_token_);
  }
}

TEST_F(SurfaceFactoryTest, BlankNoIndexIncrement) {
  LocalFrameId local_frame_id(6, kArbitraryToken);
  SurfaceId surface_id(kArbitraryFrameSinkId, local_frame_id);
  factory_->Create(local_frame_id);
  Surface* surface = manager_.GetSurfaceForId(surface_id);
  ASSERT_NE(nullptr, surface);
  EXPECT_EQ(2, surface->frame_index());
  CompositorFrame frame;
  frame.delegated_frame_data.reset(new DelegatedFrameData);

  factory_->SubmitCompositorFrame(local_frame_id, std::move(frame),
                                  SurfaceFactory::DrawCallback());
  EXPECT_EQ(2, surface->frame_index());
  EXPECT_EQ(last_created_surface_id().local_frame_id(), local_frame_id);
  factory_->Destroy(local_frame_id);
}

void CreateSurfaceDrawCallback(SurfaceFactory* factory,
                               uint32_t* execute_count) {
  LocalFrameId new_id(7, base::UnguessableToken::Create());
  factory->Create(new_id);
  factory->Destroy(new_id);
  *execute_count += 1;
}

TEST_F(SurfaceFactoryTest, AddDuringDestroy) {
  LocalFrameId local_frame_id(6, kArbitraryToken);
  factory_->Create(local_frame_id);
  CompositorFrame frame;
  frame.delegated_frame_data.reset(new DelegatedFrameData);

  uint32_t execute_count = 0;
  factory_->SubmitCompositorFrame(
      local_frame_id, std::move(frame),
      base::Bind(&CreateSurfaceDrawCallback, base::Unretained(factory_.get()),
                 &execute_count));
  EXPECT_EQ(0u, execute_count);
  factory_->Destroy(local_frame_id);
  EXPECT_EQ(1u, execute_count);
}

void DrawCallback(uint32_t* execute_count) {
  *execute_count += 1;
}

// Tests doing a DestroyAll before shutting down the factory;
TEST_F(SurfaceFactoryTest, DestroyAll) {
  LocalFrameId id(7, kArbitraryToken);
  factory_->Create(id);

  std::unique_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
  TransferableResource resource;
  resource.id = 1;
  resource.mailbox_holder.texture_target = GL_TEXTURE_2D;
  frame_data->resource_list.push_back(resource);
  CompositorFrame frame;
  frame.delegated_frame_data = std::move(frame_data);
  uint32_t execute_count = 0;
  factory_->SubmitCompositorFrame(id, std::move(frame),
                                  base::Bind(&DrawCallback, &execute_count));
  EXPECT_EQ(last_created_surface_id().local_frame_id(), id);
  local_frame_id_ = LocalFrameId();
  factory_->DestroyAll();
  EXPECT_EQ(1u, execute_count);
}

// Tests that SurfaceFactory doesn't return resources after Reset().
TEST_F(SurfaceFactoryTest, Reset) {
  LocalFrameId id(7, kArbitraryToken);
  factory_->Create(id);

  std::unique_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
  TransferableResource resource;
  resource.id = 1;
  resource.mailbox_holder.texture_target = GL_TEXTURE_2D;
  frame_data->resource_list.push_back(resource);
  CompositorFrame frame;
  frame.delegated_frame_data = std::move(frame_data);
  uint32_t execute_count = 0;
  factory_->SubmitCompositorFrame(id, std::move(frame),
                                  base::Bind(&DrawCallback, &execute_count));
  EXPECT_EQ(last_created_surface_id().local_frame_id(), id);

  SurfaceId surface_id(kArbitraryFrameSinkId, id);
  manager_.AddSurfaceReference(manager_.GetRootSurfaceId(), surface_id);
  factory_->Reset();
  EXPECT_TRUE(client_.returned_resources().empty());
  manager_.RemoveSurfaceReference(manager_.GetRootSurfaceId(), surface_id);
  EXPECT_TRUE(client_.returned_resources().empty());
  local_frame_id_ = LocalFrameId();
}

TEST_F(SurfaceFactoryTest, DestroySequence) {
  LocalFrameId local_frame_id2(5, kArbitraryToken);
  SurfaceId id2(kArbitraryFrameSinkId, local_frame_id2);
  factory_->Create(local_frame_id2);

  manager_.RegisterFrameSinkId(kArbitraryFrameSinkId);

  // Check that waiting before the sequence is satisfied works.
  manager_.GetSurfaceForId(id2)->AddDestructionDependency(
      SurfaceSequence(kArbitraryFrameSinkId, 4));
  factory_->Destroy(local_frame_id2);

  std::unique_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
  CompositorFrame frame;
  frame.metadata.satisfies_sequences.push_back(6);
  frame.metadata.satisfies_sequences.push_back(4);
  frame.delegated_frame_data = std::move(frame_data);
  DCHECK(manager_.GetSurfaceForId(id2));
  factory_->SubmitCompositorFrame(local_frame_id_, std::move(frame),
                                  SurfaceFactory::DrawCallback());
  EXPECT_EQ(last_created_surface_id().local_frame_id(), local_frame_id_);
  DCHECK(!manager_.GetSurfaceForId(id2));

  // Check that waiting after the sequence is satisfied works.
  factory_->Create(local_frame_id2);
  DCHECK(manager_.GetSurfaceForId(id2));
  manager_.GetSurfaceForId(id2)->AddDestructionDependency(
      SurfaceSequence(kAnotherArbitraryFrameSinkId, 6));
  factory_->Destroy(local_frame_id2);
  DCHECK(!manager_.GetSurfaceForId(id2));
}

// Tests that Surface ID namespace invalidation correctly allows
// Sequences to be ignored.
TEST_F(SurfaceFactoryTest, InvalidFrameSinkId) {
  FrameSinkId frame_sink_id(1234, 5678);

  LocalFrameId local_frame_id(5, kArbitraryToken);
  SurfaceId id(factory_->frame_sink_id(), local_frame_id);
  factory_->Create(local_frame_id);

  manager_.RegisterFrameSinkId(frame_sink_id);
  manager_.GetSurfaceForId(id)->AddDestructionDependency(
      SurfaceSequence(frame_sink_id, 4));
  factory_->Destroy(local_frame_id);

  // Verify the dependency has prevented the surface from getting destroyed.
  EXPECT_TRUE(manager_.GetSurfaceForId(id));

  manager_.InvalidateFrameSinkId(frame_sink_id);

  // Verify that the invalidated namespace caused the unsatisfied sequence
  // to be ignored.
  EXPECT_FALSE(manager_.GetSurfaceForId(id));
}

TEST_F(SurfaceFactoryTest, DestroyCycle) {
  LocalFrameId local_frame_id2(5, kArbitraryToken);
  SurfaceId id2(kArbitraryFrameSinkId, local_frame_id2);
  factory_->Create(local_frame_id2);

  manager_.RegisterFrameSinkId(kAnotherArbitraryFrameSinkId);

  manager_.GetSurfaceForId(id2)->AddDestructionDependency(
      SurfaceSequence(kAnotherArbitraryFrameSinkId, 4));

  // Give id2 a frame that references local_frame_id_.
  {
    std::unique_ptr<RenderPass> render_pass(RenderPass::Create());
    std::unique_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
    frame_data->render_pass_list.push_back(std::move(render_pass));
    CompositorFrame frame;
    frame.metadata.referenced_surfaces.push_back(
        SurfaceId(factory_->frame_sink_id(), local_frame_id_));
    frame.delegated_frame_data = std::move(frame_data);
    factory_->SubmitCompositorFrame(local_frame_id2, std::move(frame),
                                    SurfaceFactory::DrawCallback());
    EXPECT_EQ(last_created_surface_id().local_frame_id(), local_frame_id2);
  }
  factory_->Destroy(local_frame_id2);

  // Give local_frame_id_ a frame that references id2.
  {
    std::unique_ptr<RenderPass> render_pass(RenderPass::Create());
    std::unique_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
    frame_data->render_pass_list.push_back(std::move(render_pass));
    CompositorFrame frame;
    frame.metadata.referenced_surfaces.push_back(id2);
    frame.delegated_frame_data = std::move(frame_data);
    factory_->SubmitCompositorFrame(local_frame_id_, std::move(frame),
                                    SurfaceFactory::DrawCallback());
    EXPECT_EQ(last_created_surface_id().local_frame_id(), local_frame_id_);
  }
  factory_->Destroy(local_frame_id_);
  EXPECT_TRUE(manager_.GetSurfaceForId(id2));
  // local_frame_id_ should be retained by reference from id2.
  EXPECT_TRUE(manager_.GetSurfaceForId(
      SurfaceId(factory_->frame_sink_id(), local_frame_id_)));

  // Satisfy last destruction dependency for id2.
  std::vector<uint32_t> to_satisfy;
  to_satisfy.push_back(4);
  manager_.DidSatisfySequences(kAnotherArbitraryFrameSinkId, &to_satisfy);

  // id2 and local_frame_id_ are in a reference cycle that has no surface
  // sequences holding on to it, so they should be destroyed.
  EXPECT_TRUE(!manager_.GetSurfaceForId(id2));
  EXPECT_TRUE(!manager_.GetSurfaceForId(
      SurfaceId(factory_->frame_sink_id(), local_frame_id_)));

  local_frame_id_ = LocalFrameId();
}

void CopyRequestTestCallback(bool* called,
                             std::unique_ptr<CopyOutputResult> result) {
  *called = true;
}

TEST_F(SurfaceFactoryTest, DuplicateCopyRequest) {
  {
    std::unique_ptr<RenderPass> render_pass(RenderPass::Create());
    std::unique_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
    frame_data->render_pass_list.push_back(std::move(render_pass));
    CompositorFrame frame;
    frame.metadata.referenced_surfaces.push_back(
        SurfaceId(factory_->frame_sink_id(), local_frame_id_));
    frame.delegated_frame_data = std::move(frame_data);
    factory_->SubmitCompositorFrame(local_frame_id_, std::move(frame),
                                    SurfaceFactory::DrawCallback());
    EXPECT_EQ(last_created_surface_id().local_frame_id(), local_frame_id_);
  }
  void* source1 = &source1;
  void* source2 = &source2;

  bool called1 = false;
  std::unique_ptr<CopyOutputRequest> request;
  request = CopyOutputRequest::CreateRequest(
      base::Bind(&CopyRequestTestCallback, &called1));
  request->set_source(source1);

  factory_->RequestCopyOfSurface(local_frame_id_, std::move(request));
  EXPECT_FALSE(called1);

  bool called2 = false;
  request = CopyOutputRequest::CreateRequest(
      base::Bind(&CopyRequestTestCallback, &called2));
  request->set_source(source2);

  factory_->RequestCopyOfSurface(local_frame_id_, std::move(request));
  // Callbacks have different sources so neither should be called.
  EXPECT_FALSE(called1);
  EXPECT_FALSE(called2);

  bool called3 = false;
  request = CopyOutputRequest::CreateRequest(
      base::Bind(&CopyRequestTestCallback, &called3));
  request->set_source(source1);

  factory_->RequestCopyOfSurface(local_frame_id_, std::move(request));
  // Two callbacks are from source1, so the first should be called.
  EXPECT_TRUE(called1);
  EXPECT_FALSE(called2);
  EXPECT_FALSE(called3);

  factory_->Destroy(local_frame_id_);
  local_frame_id_ = LocalFrameId();
  EXPECT_TRUE(called1);
  EXPECT_TRUE(called2);
  EXPECT_TRUE(called3);
}

}  // namespace
}  // namespace cc
