// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <unordered_map>
#include <vector>

#include "base/memory/ptr_util.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_sequence_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

constexpr FrameSinkId kFrameSink1(1, 0);
constexpr FrameSinkId kFrameSink2(2, 0);
constexpr FrameSinkId kFrameSink3(3, 0);
const LocalFrameId kLocalFrame1(1, base::UnguessableToken::Create());
const LocalFrameId kLocalFrame2(2, base::UnguessableToken::Create());

// Tests for reference tracking in SurfaceManager.
class SurfaceManagerRefTest : public testing::Test {
 public:
  SurfaceManager& manager() { return *manager_; }

  // Creates a new Surface with the provided SurfaceId. Will first create the
  // SurfaceFactory for |frame_sink_id| if necessary.
  SurfaceId CreateSurface(const FrameSinkId& frame_sink_id,
                          const LocalFrameId& local_frame_id) {
    GetFactory(frame_sink_id).Create(local_frame_id);
    return SurfaceId(frame_sink_id, local_frame_id);
  }

  // Destroy Surface with |surface_id|.
  void DestroySurface(const SurfaceId& surface_id) {
    GetFactory(surface_id.frame_sink_id()).Destroy(surface_id.local_frame_id());
  }

 protected:
  SurfaceFactory& GetFactory(const FrameSinkId& frame_sink_id) {
    auto& factory_ptr = factories_[frame_sink_id];
    if (!factory_ptr)
      factory_ptr = base::MakeUnique<SurfaceFactory>(frame_sink_id,
                                                     manager_.get(), nullptr);
    return *factory_ptr;
  }

  // testing::Test:
  void SetUp() override {
    // Start each test with a fresh SurfaceManager instance.
    manager_ = base::MakeUnique<SurfaceManager>();
  }
  void TearDown() override {
    factories_.clear();
    manager_.reset();
  }

  std::unordered_map<FrameSinkId,
                     std::unique_ptr<SurfaceFactory>,
                     FrameSinkIdHash>
      factories_;
  std::unique_ptr<SurfaceManager> manager_;
};

}  // namespace

TEST_F(SurfaceManagerRefTest, AddReference) {
  SurfaceId id1 = CreateSurface(kFrameSink1, kLocalFrame1);
  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);

  EXPECT_EQ(manager().GetSurfaceReferenceCount(id1), 1u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id1), 0u);
}

TEST_F(SurfaceManagerRefTest, AddRemoveReference) {
  SurfaceId id1 = CreateSurface(kFrameSink1, kLocalFrame1);
  SurfaceId id2 = CreateSurface(kFrameSink2, kLocalFrame1);
  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  manager().AddSurfaceReference(id1, id2);

  EXPECT_EQ(manager().GetSurfaceReferenceCount(id1), 1u);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id2), 1u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id1), 1u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id2), 0u);

  manager().RemoveSurfaceReference(id1, id2);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id1), 1u);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id2), 0u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id1), 0u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id2), 0u);
}

TEST_F(SurfaceManagerRefTest, AddRemoveReferenceRecursive) {
  SurfaceId id1 = CreateSurface(kFrameSink1, kLocalFrame1);
  SurfaceId id2 = CreateSurface(kFrameSink2, kLocalFrame1);
  SurfaceId id3 = CreateSurface(kFrameSink3, kLocalFrame1);

  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  manager().AddSurfaceReference(id1, id2);
  manager().AddSurfaceReference(id2, id3);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id1), 1u);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id2), 1u);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id3), 1u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id1), 1u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id2), 1u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id3), 0u);

  // Should remove reference from id1 -> id2 and then since id2 has zero
  // references all references it holds should be removed.
  manager().RemoveSurfaceReference(id1, id2);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id1), 1u);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id2), 0u);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id3), 0u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id1), 0u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id2), 0u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id3), 0u);
}

TEST_F(SurfaceManagerRefTest, NewSurfaceFromFrameSink) {
  SurfaceId id1 = CreateSurface(kFrameSink1, kLocalFrame1);
  SurfaceId id2 = CreateSurface(kFrameSink2, kLocalFrame1);
  SurfaceId id3 = CreateSurface(kFrameSink3, kLocalFrame1);

  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  manager().AddSurfaceReference(id1, id2);
  manager().AddSurfaceReference(id2, id3);

  // |kFramesink2| received a CompositorFrame with a new size, so it destroys
  // |id2| and creates |id2_next|. No reference have been removed yet.
  DestroySurface(id2);
  SurfaceId id2_next = CreateSurface(kFrameSink2, kLocalFrame2);
  EXPECT_NE(manager().GetSurfaceForId(id2), nullptr);
  EXPECT_NE(manager().GetSurfaceForId(id2_next), nullptr);

  // Add references to and from |id2_next|.
  manager().AddSurfaceReference(id1, id2_next);
  manager().AddSurfaceReference(id2_next, id3);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id2), 1u);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id2_next), 1u);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id3), 2u);

  manager().RemoveSurfaceReference(id1, id2);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id2), 0u);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id2_next), 1u);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id3), 1u);

  // |id2| should be deleted during GC but other surfaces shouldn't.
  EXPECT_EQ(manager().GetSurfaceForId(id2), nullptr);
  EXPECT_NE(manager().GetSurfaceForId(id2_next), nullptr);
  EXPECT_NE(manager().GetSurfaceForId(id3), nullptr);
}

TEST_F(SurfaceManagerRefTest, CheckGC) {
  SurfaceId id1 = CreateSurface(kFrameSink1, kLocalFrame1);
  SurfaceId id2 = CreateSurface(kFrameSink2, kLocalFrame1);

  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  manager().AddSurfaceReference(id1, id2);

  EXPECT_NE(manager().GetSurfaceForId(id1), nullptr);
  EXPECT_NE(manager().GetSurfaceForId(id2), nullptr);

  // Destroying the surfaces shouldn't delete them yet, since there is still an
  // active reference on all surfaces.
  DestroySurface(id1);
  DestroySurface(id2);
  EXPECT_NE(manager().GetSurfaceForId(id1), nullptr);
  EXPECT_NE(manager().GetSurfaceForId(id2), nullptr);

  // Should delete |id2| when the only reference to it is removed.
  manager().RemoveSurfaceReference(id1, id2);
  EXPECT_EQ(manager().GetSurfaceForId(id2), nullptr);

  // Should delete |id1| when the only reference to it is removed.
  manager().RemoveSurfaceReference(manager().GetRootSurfaceId(), id1);
  EXPECT_EQ(manager().GetSurfaceForId(id1), nullptr);
}

TEST_F(SurfaceManagerRefTest, CheckGCRecusiveFull) {
  SurfaceId id1 = CreateSurface(kFrameSink1, kLocalFrame1);
  SurfaceId id2 = CreateSurface(kFrameSink2, kLocalFrame1);
  SurfaceId id3 = CreateSurface(kFrameSink3, kLocalFrame1);

  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  manager().AddSurfaceReference(id1, id2);
  manager().AddSurfaceReference(id2, id3);

  DestroySurface(id3);
  DestroySurface(id2);
  DestroySurface(id1);

  // Destroying the surfaces shouldn't delete them yet, since there is still an
  // active reference on all surfaces.
  EXPECT_NE(manager().GetSurfaceForId(id3), nullptr);
  EXPECT_NE(manager().GetSurfaceForId(id2), nullptr);
  EXPECT_NE(manager().GetSurfaceForId(id1), nullptr);

  manager().RemoveSurfaceReference(manager().GetRootSurfaceId(), id1);

  // Removing the reference from the root to id1 should allow all three surfaces
  // to be deleted during GC.
  EXPECT_EQ(manager().GetSurfaceForId(id1), nullptr);
  EXPECT_EQ(manager().GetSurfaceForId(id2), nullptr);
  EXPECT_EQ(manager().GetSurfaceForId(id3), nullptr);
}

TEST_F(SurfaceManagerRefTest, CheckGCWithSequences) {
  SurfaceId id1 = CreateSurface(kFrameSink1, kLocalFrame1);
  SurfaceId id2 = CreateSurface(kFrameSink2, kLocalFrame1);

  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  manager().AddSurfaceReference(id1, id2);

  SurfaceId id3 = CreateSurface(kFrameSink3, kLocalFrame1);
  Surface* surface3 = manager().GetSurfaceForId(id3);

  // Add destruction dependency from |id2| to |id3|.
  manager().RegisterFrameSinkId(kFrameSink2);
  SurfaceSequence sequence(kFrameSink2, 1u);
  surface3->AddDestructionDependency(sequence);
  EXPECT_EQ(surface3->GetDestructionDependencyCount(), 1u);

  // Surface for |id3| isn't delete yet because it has a valid destruction
  // dependency from |kFrameSink2|.
  DestroySurface(id3);
  EXPECT_NE(manager().GetSurfaceForId(id3), nullptr);

  // Surface for |id2| isn't deleted because it has a reference.
  DestroySurface(id2);
  EXPECT_NE(manager().GetSurfaceForId(id2), nullptr);

  // Satisfy destruction dependency on |id3| and delete during GC.
  std::vector<uint32_t> satisfied({sequence.sequence});
  manager().DidSatisfySequences(kFrameSink2, &satisfied);
  EXPECT_EQ(manager().GetSurfaceForId(id3), nullptr);

  // Remove ref on |id2| and delete during GC.
  manager().RemoveSurfaceReference(id1, id2);
  EXPECT_EQ(manager().GetSurfaceForId(id2), nullptr);
}

TEST_F(SurfaceManagerRefTest, TryAddReferenceToBadSurface) {
  // Not creating an accompanying Surface and SurfaceFactory.
  SurfaceId id(FrameSinkId(100u, 200u),
               LocalFrameId(1u, base::UnguessableToken::Create()));

  // Adding reference from root to the Surface should do nothing because
  // SurfaceManager doesn't know Surface for |id| exists.
  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id), 0u);
}

TEST_F(SurfaceManagerRefTest, TryDoubleAddReference) {
  SurfaceId id1 = CreateSurface(kFrameSink1, kLocalFrame1);
  SurfaceId id2 = CreateSurface(kFrameSink2, kLocalFrame1);

  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  manager().AddSurfaceReference(id1, id2);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id2), 1u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id1), 1u);

  // The second request should be ignored without crashing.
  manager().AddSurfaceReference(id1, id2);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id2), 1u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id1), 1u);
}

TEST_F(SurfaceManagerRefTest, TryAddSelfReference) {
  SurfaceId id1 = CreateSurface(kFrameSink1, kLocalFrame1);

  // Adding a self reference should be ignored without crashing.
  manager().AddSurfaceReference(id1, id1);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id1), 0u);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id1), 0u);
}

TEST_F(SurfaceManagerRefTest, TryRemoveBadReference) {
  SurfaceId id1 = CreateSurface(kFrameSink1, kLocalFrame1);
  SurfaceId id2 = CreateSurface(kFrameSink2, kLocalFrame1);

  // Removing non-existent reference should be ignored.
  manager().AddSurfaceReference(id1, id2);
  manager().RemoveSurfaceReference(id2, id1);
  EXPECT_EQ(manager().GetReferencedSurfaceCount(id1), 1u);
  EXPECT_EQ(manager().GetSurfaceReferenceCount(id2), 1u);
}

}  // namespace cc
