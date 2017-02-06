// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
namespace {

class FakeSurfaceFactoryClient : public SurfaceFactoryClient {
 public:
  FakeSurfaceFactoryClient() : begin_frame_source_(nullptr) {}

  void ReturnResources(const ReturnedResourceArray& resources) override {}

  void SetBeginFrameSource(BeginFrameSource* begin_frame_source) override {
    begin_frame_source_ = begin_frame_source;
  }

  BeginFrameSource* begin_frame_source() { return begin_frame_source_; }

 private:
  BeginFrameSource* begin_frame_source_;
};

TEST(SurfaceTest, SurfaceLifetime) {
  SurfaceManager manager;
  FakeSurfaceFactoryClient surface_factory_client;
  SurfaceFactory factory(&manager, &surface_factory_client);

  SurfaceId surface_id(0, 6, 0);
  {
    factory.Create(surface_id);
    EXPECT_TRUE(manager.GetSurfaceForId(surface_id));
    factory.Destroy(surface_id);
  }

  EXPECT_EQ(NULL, manager.GetSurfaceForId(surface_id));
}

TEST(SurfaceTest, SurfaceIds) {
  uint32_t namespaces[] = {0u, 37u, ~0u};
  for (size_t i = 0; i < 3; ++i) {
    uint32_t id_namespace = namespaces[i];
    SurfaceIdAllocator allocator(id_namespace);
    SurfaceId id1 = allocator.GenerateId();
    EXPECT_EQ(id1.id_namespace(), id_namespace);
    SurfaceId id2 = allocator.GenerateId();
    EXPECT_EQ(id2.id_namespace(), id_namespace);
    EXPECT_NE(id1.local_id(), id2.local_id());
    EXPECT_NE(id1.nonce(), id2.nonce());
  }
}

}  // namespace
}  // namespace cc
