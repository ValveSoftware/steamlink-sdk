// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_client.h"
#include "cc/surfaces/surface_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

namespace cc {
namespace {

TEST(SurfaceTest, SurfaceLifetime) {
  SurfaceManager manager;

  SurfaceId surface_id;
  {
    Surface surface(&manager, NULL, gfx::Size(5, 5));
    surface_id = surface.surface_id();
    EXPECT_TRUE(!surface_id.is_null());
    EXPECT_EQ(&surface, manager.GetSurfaceForId(surface_id));
  }

  EXPECT_EQ(NULL, manager.GetSurfaceForId(surface_id));
}

class TestSurfaceClient : public SurfaceClient {
 public:
  TestSurfaceClient() {}
  virtual ~TestSurfaceClient() {}

  virtual void ReturnResources(
      const ReturnedResourceArray& resources) OVERRIDE {
    returned_resources_ = resources;
  }

  const ReturnedResourceArray& returned_resources() const {
    return returned_resources_;
  }

  void clear_returned_resources() { returned_resources_.clear(); }

 private:
  ReturnedResourceArray returned_resources_;

  DISALLOW_COPY_AND_ASSIGN(TestSurfaceClient);
};

void SubmitFrameWithResources(ResourceProvider::ResourceId* resource_ids,
                              size_t num_resource_ids,
                              Surface* surface) {
  scoped_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
  for (size_t i = 0u; i < num_resource_ids; ++i) {
    TransferableResource resource;
    resource.id = resource_ids[i];
    resource.mailbox_holder.texture_target = GL_TEXTURE_2D;
    frame_data->resource_list.push_back(resource);
  }
  scoped_ptr<CompositorFrame> frame(new CompositorFrame);
  frame->delegated_frame_data = frame_data.Pass();
  surface->QueueFrame(frame.Pass());
}

void CheckReturnedResourcesMatchExpected(
    ResourceProvider::ResourceId* expected_returned_ids,
    int* expected_returned_counts,
    size_t expected_resources,
    const ReturnedResourceArray& actual_resources) {
  ASSERT_EQ(expected_resources, actual_resources.size());
  for (size_t i = 0; i < expected_resources; ++i) {
    ReturnedResource resource = actual_resources[i];
    EXPECT_EQ(expected_returned_ids[i], resource.id);
    EXPECT_EQ(expected_returned_counts[i], resource.count);
  }
}

void UnrefResources(ResourceProvider::ResourceId* ids_to_unref,
                    int* counts_to_unref,
                    size_t num_ids_to_unref,
                    Surface* surface) {
  ReturnedResourceArray unref_array;
  for (size_t i = 0; i < num_ids_to_unref; ++i) {
    ReturnedResource resource;
    resource.id = ids_to_unref[i];
    resource.count = counts_to_unref[i];
    unref_array.push_back(resource);
  }
  surface->UnrefResources(unref_array);
}

// Tests submitting a frame with resources followed by one with no resources
// with no resource provider action in between.
TEST(SurfaceTest, ResourceLifetimeSimple) {
  SurfaceManager manager;
  TestSurfaceClient client;
  Surface surface(&manager, &client, gfx::Size(5, 5));

  ResourceProvider::ResourceId first_frame_ids[] = {1, 2, 3};
  SubmitFrameWithResources(
      first_frame_ids, arraysize(first_frame_ids), &surface);

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  surface.ReturnUnusedResourcesToClient();
  EXPECT_EQ(0u, client.returned_resources().size());
  client.clear_returned_resources();

  // The second frame references no resources and thus should make all resources
  // available to be returned.
  SubmitFrameWithResources(NULL, 0, &surface);

  surface.ReturnUnusedResourcesToClient();
  ResourceProvider::ResourceId expected_returned_ids[] = {1, 2, 3};
  int expected_returned_counts[] = {1, 1, 1};
  CheckReturnedResourcesMatchExpected(expected_returned_ids,
                                      expected_returned_counts,
                                      arraysize(expected_returned_counts),
                                      client.returned_resources());
}

// Tests submitting a frame with resources followed by one with no resources
// with the resource provider holding everything alive.
TEST(SurfaceTest, ResourceLifetimeSimpleWithProviderHoldingAlive) {
  SurfaceManager manager;
  TestSurfaceClient client;
  Surface surface(&manager, &client, gfx::Size(5, 5));

  ResourceProvider::ResourceId first_frame_ids[] = {1, 2, 3};
  SubmitFrameWithResources(
      first_frame_ids, arraysize(first_frame_ids), &surface);

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  surface.ReturnUnusedResourcesToClient();
  EXPECT_EQ(0u, client.returned_resources().size());
  client.clear_returned_resources();

  // Hold on to everything.
  surface.RefCurrentFrameResources();

  // The second frame references no resources and thus should make all resources
  // available to be returned as soon as the resource provider releases them.
  SubmitFrameWithResources(NULL, 0, &surface);

  surface.ReturnUnusedResourcesToClient();
  EXPECT_EQ(0u, client.returned_resources().size());
  client.clear_returned_resources();

  int release_counts[] = {1, 1, 1};
  UnrefResources(
      first_frame_ids, release_counts, arraysize(first_frame_ids), &surface);

  surface.ReturnUnusedResourcesToClient();
  ResourceProvider::ResourceId expected_returned_ids[] = {1, 2, 3};
  int expected_returned_counts[] = {1, 1, 1};
  CheckReturnedResourcesMatchExpected(expected_returned_ids,
                                      expected_returned_counts,
                                      arraysize(expected_returned_counts),
                                      client.returned_resources());
}

// Tests referencing a resource, unref'ing it to zero, then using it again
// before returning it to the client.
TEST(SurfaceTest, ResourceReusedBeforeReturn) {
  SurfaceManager manager;
  TestSurfaceClient client;
  Surface surface(&manager, &client, gfx::Size(5, 5));

  ResourceProvider::ResourceId first_frame_ids[] = {7};
  SubmitFrameWithResources(
      first_frame_ids, arraysize(first_frame_ids), &surface);

  // This removes all references to resource id 7.
  SubmitFrameWithResources(NULL, 0, &surface);

  // This references id 7 again.
  SubmitFrameWithResources(
      first_frame_ids, arraysize(first_frame_ids), &surface);

  // This removes it again.
  SubmitFrameWithResources(NULL, 0, &surface);

  // Now it should be returned.
  surface.ReturnUnusedResourcesToClient();

  // We don't care how many entries are in the returned array for 7, so long as
  // the total returned count matches the submitted count.
  const ReturnedResourceArray& returned = client.returned_resources();
  size_t return_count = 0;
  for (size_t i = 0; i < returned.size(); ++i) {
    EXPECT_EQ(7u, returned[i].id);
    return_count += returned[i].count;
  }
  EXPECT_EQ(2u, return_count);
}

// Tests having resources referenced multiple times, as if referenced by
// multiple providers.
TEST(SurfaceTest, ResourceRefMultipleTimes) {
  SurfaceManager manager;
  TestSurfaceClient client;
  Surface surface(&manager, &client, gfx::Size(5, 5));

  ResourceProvider::ResourceId first_frame_ids[] = {3, 4};
  SubmitFrameWithResources(
      first_frame_ids, arraysize(first_frame_ids), &surface);

  // Ref resources from the first frame twice.
  surface.RefCurrentFrameResources();
  surface.RefCurrentFrameResources();

  ResourceProvider::ResourceId second_frame_ids[] = {4, 5};
  SubmitFrameWithResources(
      second_frame_ids, arraysize(second_frame_ids), &surface);

  // Ref resources from the second frame 3 times.
  surface.RefCurrentFrameResources();
  surface.RefCurrentFrameResources();
  surface.RefCurrentFrameResources();

  // Submit a frame with no resources to remove all current frame refs from
  // submitted resources.
  SubmitFrameWithResources(NULL, 0, &surface);

  surface.ReturnUnusedResourcesToClient();
  EXPECT_EQ(0u, client.returned_resources().size());
  client.clear_returned_resources();

  // Expected current refs:
  //  3 -> 2
  //  4 -> 2 + 3 = 5
  //  5 -> 3
  {
    SCOPED_TRACE("unref all 3");
    ResourceProvider::ResourceId ids_to_unref[] = {3, 4, 5};
    int counts[] = {1, 1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref), &surface);

    surface.ReturnUnusedResourcesToClient();
    EXPECT_EQ(0u, client.returned_resources().size());
    client.clear_returned_resources();

    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref), &surface);

    surface.ReturnUnusedResourcesToClient();
    ResourceProvider::ResourceId expected_returned_ids[] = {3};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(expected_returned_ids,
                                        expected_returned_counts,
                                        arraysize(expected_returned_counts),
                                        client.returned_resources());
  }

  // Expected refs remaining:
  //  4 -> 3
  //  5 -> 1
  {
    SCOPED_TRACE("unref 4 and 5");
    ResourceProvider::ResourceId ids_to_unref[] = {4, 5};
    int counts[] = {1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref), &surface);

    surface.ReturnUnusedResourcesToClient();
    ResourceProvider::ResourceId expected_returned_ids[] = {5};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(expected_returned_ids,
                                        expected_returned_counts,
                                        arraysize(expected_returned_counts),
                                        client.returned_resources());
  }

  // Now, just 2 refs remaining on resource 4. Unref both at once and make sure
  // the returned count is correct.
  {
    SCOPED_TRACE("unref only 4");
    ResourceProvider::ResourceId ids_to_unref[] = {4};
    int counts[] = {2};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref), &surface);

    surface.ReturnUnusedResourcesToClient();
    ResourceProvider::ResourceId expected_returned_ids[] = {4};
    int expected_returned_counts[] = {2};
    CheckReturnedResourcesMatchExpected(expected_returned_ids,
                                        expected_returned_counts,
                                        arraysize(expected_returned_counts),
                                        client.returned_resources());
  }
}

TEST(SurfaceTest, ResourceLifetime) {
  SurfaceManager manager;
  TestSurfaceClient client;
  Surface surface(&manager, &client, gfx::Size(5, 5));

  ResourceProvider::ResourceId first_frame_ids[] = {1, 2, 3};
  SubmitFrameWithResources(
      first_frame_ids, arraysize(first_frame_ids), &surface);

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  surface.ReturnUnusedResourcesToClient();
  EXPECT_EQ(0u, client.returned_resources().size());
  client.clear_returned_resources();

  // The second frame references some of the same resources, but some different
  // ones. We expect to receive back resource 1 with a count of 1 since it was
  // only referenced by the first frame.
  ResourceProvider::ResourceId second_frame_ids[] = {2, 3, 4};
  SubmitFrameWithResources(
      second_frame_ids, arraysize(second_frame_ids), &surface);

  surface.ReturnUnusedResourcesToClient();
  {
    SCOPED_TRACE("second frame");
    ResourceProvider::ResourceId expected_returned_ids[] = {1};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(expected_returned_ids,
                                        expected_returned_counts,
                                        arraysize(expected_returned_counts),
                                        client.returned_resources());
  }

  // The third frame references a disjoint set of resources, so we expect to
  // receive back all resources from the first and second frames. Resource IDs 2
  // and 3 will have counts of 2, since they were used in both frames, and
  // resource ID 4 will have a count of 1.
  ResourceProvider::ResourceId third_frame_ids[] = {10, 11, 12, 13};
  SubmitFrameWithResources(
      third_frame_ids, arraysize(third_frame_ids), &surface);

  surface.ReturnUnusedResourcesToClient();
  {
    SCOPED_TRACE("third frame");
    ResourceProvider::ResourceId expected_returned_ids[] = {2, 3, 4};
    int expected_returned_counts[] = {2, 2, 1};
    CheckReturnedResourcesMatchExpected(expected_returned_ids,
                                        expected_returned_counts,
                                        arraysize(expected_returned_counts),
                                        client.returned_resources());
  }

  // Simulate a ResourceProvider taking a ref on all of the resources.
  surface.RefCurrentFrameResources();

  ResourceProvider::ResourceId fourth_frame_ids[] = {12, 13};
  SubmitFrameWithResources(
      fourth_frame_ids, arraysize(fourth_frame_ids), &surface);

  surface.ReturnUnusedResourcesToClient();
  EXPECT_EQ(0u, client.returned_resources().size());

  surface.RefCurrentFrameResources();

  // All resources are still being used by the external reference, so none can
  // be returned to the client.
  surface.ReturnUnusedResourcesToClient();
  EXPECT_EQ(0u, client.returned_resources().size());

  // Release resources associated with the first RefCurrentFrameResources() call
  // first.
  {
    ResourceProvider::ResourceId ids_to_unref[] = {10, 11, 12, 13};
    int counts[] = {1, 1, 1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref), &surface);
  }

  surface.ReturnUnusedResourcesToClient();
  {
    SCOPED_TRACE("fourth frame, first unref");
    ResourceProvider::ResourceId expected_returned_ids[] = {10, 11};
    int expected_returned_counts[] = {1, 1};
    CheckReturnedResourcesMatchExpected(expected_returned_ids,
                                        expected_returned_counts,
                                        arraysize(expected_returned_counts),
                                        client.returned_resources());
  }

  {
    ResourceProvider::ResourceId ids_to_unref[] = {12, 13};
    int counts[] = {1, 1};
    UnrefResources(ids_to_unref, counts, arraysize(ids_to_unref), &surface);
  }

  // Resources 12 and 13 are still in use by the current frame, so they
  // shouldn't be available to be returned.
  surface.ReturnUnusedResourcesToClient();
  EXPECT_EQ(0u, client.returned_resources().size());

  // If we submit an empty frame, however, they should become available.
  SubmitFrameWithResources(NULL, 0u, &surface);

  surface.ReturnUnusedResourcesToClient();
  {
    SCOPED_TRACE("fourth frame, second unref");
    ResourceProvider::ResourceId expected_returned_ids[] = {12, 13};
    int expected_returned_counts[] = {2, 2};
    CheckReturnedResourcesMatchExpected(expected_returned_ids,
                                        expected_returned_counts,
                                        arraysize(expected_returned_counts),
                                        client.returned_resources());
  }
}

}  // namespace
}  // namespace cc
