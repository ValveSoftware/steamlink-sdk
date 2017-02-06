// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class FakeSurfaceFactoryClient : public SurfaceFactoryClient {
 public:
  explicit FakeSurfaceFactoryClient(int id_namespace)
      : source_(nullptr), manager_(nullptr), id_namespace_(id_namespace) {}
  FakeSurfaceFactoryClient(int id_namespace, SurfaceManager* manager)
      : source_(nullptr), manager_(nullptr), id_namespace_(id_namespace) {
    DCHECK(manager);
    Register(manager);
  }

  ~FakeSurfaceFactoryClient() override {
    if (manager_) {
      Unregister();
    }
    EXPECT_EQ(source_, nullptr);
  }

  BeginFrameSource* source() { return source_; }
  uint32_t id_namespace() { return id_namespace_; }

  void Register(SurfaceManager* manager) {
    EXPECT_EQ(manager_, nullptr);
    manager_ = manager;
    manager_->RegisterSurfaceFactoryClient(id_namespace_, this);
  }

  void Unregister() {
    EXPECT_NE(manager_, nullptr);
    manager_->UnregisterSurfaceFactoryClient(id_namespace_);
    manager_ = nullptr;
  }

  // SurfaceFactoryClient implementation.
  void ReturnResources(const ReturnedResourceArray& resources) override {}
  void SetBeginFrameSource(BeginFrameSource* begin_frame_source) override {
    DCHECK(!source_ || !begin_frame_source);
    source_ = begin_frame_source;
  };

 private:
  BeginFrameSource* source_;
  SurfaceManager* manager_;
  uint32_t id_namespace_;
};

class EmptyBeginFrameSource : public BeginFrameSource {
 public:
  void DidFinishFrame(BeginFrameObserver* obs,
                      size_t remaining_frames) override {}
  void AddObserver(BeginFrameObserver* obs) override {}
  void RemoveObserver(BeginFrameObserver* obs) override {}
};

class SurfaceManagerTest : public testing::Test {
 public:
  // These tests don't care about namespace registration, so just preregister
  // a set of namespaces that tests can use freely without worrying if they're
  // valid or not.
  enum { MAX_NAMESPACE = 10 };

  SurfaceManagerTest() {
    for (size_t i = 0; i < MAX_NAMESPACE; ++i)
      manager_.RegisterSurfaceIdNamespace(i);
  }

  ~SurfaceManagerTest() override {
    for (size_t i = 0; i < MAX_NAMESPACE; ++i)
      manager_.InvalidateSurfaceIdNamespace(i);
  }

 protected:
  SurfaceManager manager_;
};

TEST_F(SurfaceManagerTest, SingleClients) {
  FakeSurfaceFactoryClient client(1);
  FakeSurfaceFactoryClient other_client(2);
  EmptyBeginFrameSource source;

  EXPECT_EQ(client.source(), nullptr);
  EXPECT_EQ(other_client.source(), nullptr);
  client.Register(&manager_);
  other_client.Register(&manager_);
  EXPECT_EQ(client.source(), nullptr);
  EXPECT_EQ(other_client.source(), nullptr);

  // Test setting unsetting BFS
  manager_.RegisterBeginFrameSource(&source, client.id_namespace());
  EXPECT_EQ(client.source(), &source);
  EXPECT_EQ(other_client.source(), nullptr);
  manager_.UnregisterBeginFrameSource(&source);
  EXPECT_EQ(client.source(), nullptr);
  EXPECT_EQ(other_client.source(), nullptr);

  // Set BFS for other namespace
  manager_.RegisterBeginFrameSource(&source, other_client.id_namespace());
  EXPECT_EQ(other_client.source(), &source);
  EXPECT_EQ(client.source(), nullptr);
  manager_.UnregisterBeginFrameSource(&source);
  EXPECT_EQ(client.source(), nullptr);
  EXPECT_EQ(other_client.source(), nullptr);

  // Re-set BFS for original
  manager_.RegisterBeginFrameSource(&source, client.id_namespace());
  EXPECT_EQ(client.source(), &source);
  manager_.UnregisterBeginFrameSource(&source);
  EXPECT_EQ(client.source(), nullptr);
}

TEST_F(SurfaceManagerTest, MultipleDisplays) {
  EmptyBeginFrameSource root1_source;
  EmptyBeginFrameSource root2_source;

  // root1 -> A -> B
  // root2 -> C
  FakeSurfaceFactoryClient root1(1, &manager_);
  FakeSurfaceFactoryClient root2(2, &manager_);
  FakeSurfaceFactoryClient client_a(3, &manager_);
  FakeSurfaceFactoryClient client_b(4, &manager_);
  FakeSurfaceFactoryClient client_c(5, &manager_);

  manager_.RegisterBeginFrameSource(&root1_source, root1.id_namespace());
  manager_.RegisterBeginFrameSource(&root2_source, root2.id_namespace());
  EXPECT_EQ(root1.source(), &root1_source);
  EXPECT_EQ(root2.source(), &root2_source);

  // Set up initial hierarchy.
  manager_.RegisterSurfaceNamespaceHierarchy(root1.id_namespace(),
                                             client_a.id_namespace());
  EXPECT_EQ(client_a.source(), root1.source());
  manager_.RegisterSurfaceNamespaceHierarchy(client_a.id_namespace(),
                                             client_b.id_namespace());
  EXPECT_EQ(client_b.source(), root1.source());
  manager_.RegisterSurfaceNamespaceHierarchy(root2.id_namespace(),
                                             client_c.id_namespace());
  EXPECT_EQ(client_c.source(), root2.source());

  // Attach A into root2's subtree, like a window moving across displays.
  // root1 -> A -> B
  // root2 -> C -> A -> B
  manager_.RegisterSurfaceNamespaceHierarchy(client_c.id_namespace(),
                                             client_a.id_namespace());
  // With the heuristic of just keeping existing BFS in the face of multiple,
  // no client sources should change.
  EXPECT_EQ(client_a.source(), root1.source());
  EXPECT_EQ(client_b.source(), root1.source());
  EXPECT_EQ(client_c.source(), root2.source());

  // Detach A from root1.  A and B should now be updated to root2.
  manager_.UnregisterSurfaceNamespaceHierarchy(root1.id_namespace(),
                                               client_a.id_namespace());
  EXPECT_EQ(client_a.source(), root2.source());
  EXPECT_EQ(client_b.source(), root2.source());
  EXPECT_EQ(client_c.source(), root2.source());

  // Detach root1 from BFS.  root1 should now have no source.
  manager_.UnregisterBeginFrameSource(&root1_source);
  EXPECT_EQ(root1.source(), nullptr);
  EXPECT_NE(root2.source(), nullptr);

  // Detatch root2 from BFS.
  manager_.UnregisterBeginFrameSource(&root2_source);
  EXPECT_EQ(client_a.source(), nullptr);
  EXPECT_EQ(client_b.source(), nullptr);
  EXPECT_EQ(client_c.source(), nullptr);
  EXPECT_EQ(root2.source(), nullptr);

  // Cleanup hierarchy.
  manager_.UnregisterSurfaceNamespaceHierarchy(root2.id_namespace(),
                                               client_c.id_namespace());
  manager_.UnregisterSurfaceNamespaceHierarchy(client_c.id_namespace(),
                                               client_a.id_namespace());
  manager_.UnregisterSurfaceNamespaceHierarchy(client_a.id_namespace(),
                                               client_b.id_namespace());
}

// In practice, registering and unregistering both parent/child relationships
// and SurfaceFactoryClients can happen in any ordering with respect to
// each other.  These following tests verify that all the data structures
// are properly set up and cleaned up under the four permutations of orderings
// of this nesting.

class SurfaceManagerOrderingTest : public SurfaceManagerTest {
 public:
  SurfaceManagerOrderingTest()
      : client_a_(1),
        client_b_(2),
        client_c_(3),
        hierarchy_registered_(false),
        clients_registered_(false),
        bfs_registered_(false) {
    AssertCorrectBFSState();
  }

  ~SurfaceManagerOrderingTest() override {
    EXPECT_EQ(hierarchy_registered_, false);
    EXPECT_EQ(clients_registered_, false);
    EXPECT_EQ(bfs_registered_, false);
    AssertCorrectBFSState();
  }

  void RegisterHierarchy() {
    DCHECK(!hierarchy_registered_);
    hierarchy_registered_ = true;
    manager_.RegisterSurfaceNamespaceHierarchy(client_a_.id_namespace(),
                                               client_b_.id_namespace());
    manager_.RegisterSurfaceNamespaceHierarchy(client_b_.id_namespace(),
                                               client_c_.id_namespace());
    AssertCorrectBFSState();
  }
  void UnregisterHierarchy() {
    DCHECK(hierarchy_registered_);
    hierarchy_registered_ = false;
    manager_.UnregisterSurfaceNamespaceHierarchy(client_a_.id_namespace(),
                                                 client_b_.id_namespace());
    manager_.UnregisterSurfaceNamespaceHierarchy(client_b_.id_namespace(),
                                                 client_c_.id_namespace());
    AssertCorrectBFSState();
  }

  void RegisterClients() {
    DCHECK(!clients_registered_);
    clients_registered_ = true;
    client_a_.Register(&manager_);
    client_b_.Register(&manager_);
    client_c_.Register(&manager_);
    AssertCorrectBFSState();
  }

  void UnregisterClients() {
    DCHECK(clients_registered_);
    clients_registered_ = false;
    client_a_.Unregister();
    client_b_.Unregister();
    client_c_.Unregister();
    AssertCorrectBFSState();
  }

  void RegisterBFS() {
    DCHECK(!bfs_registered_);
    bfs_registered_ = true;
    manager_.RegisterBeginFrameSource(&source_, client_a_.id_namespace());
    AssertCorrectBFSState();
  }
  void UnregisterBFS() {
    DCHECK(bfs_registered_);
    bfs_registered_ = false;
    manager_.UnregisterBeginFrameSource(&source_);
    AssertCorrectBFSState();
  }

  void AssertEmptyBFS() {
    EXPECT_EQ(client_a_.source(), nullptr);
    EXPECT_EQ(client_b_.source(), nullptr);
    EXPECT_EQ(client_c_.source(), nullptr);
  }

  void AssertAllValidBFS() {
    EXPECT_EQ(client_a_.source(), &source_);
    EXPECT_EQ(client_b_.source(), &source_);
    EXPECT_EQ(client_c_.source(), &source_);
  }

 protected:
  void AssertCorrectBFSState() {
    if (!clients_registered_ || !bfs_registered_) {
      AssertEmptyBFS();
      return;
    }
    if (!hierarchy_registered_) {
      // A valid but not attached to anything.
      EXPECT_EQ(client_a_.source(), &source_);
      EXPECT_EQ(client_b_.source(), nullptr);
      EXPECT_EQ(client_c_.source(), nullptr);
      return;
    }

    AssertAllValidBFS();
  }

  EmptyBeginFrameSource source_;
  // A -> B -> C hierarchy, with A always having the BFS.
  FakeSurfaceFactoryClient client_a_;
  FakeSurfaceFactoryClient client_b_;
  FakeSurfaceFactoryClient client_c_;

  bool hierarchy_registered_;
  bool clients_registered_;
  bool bfs_registered_;
};

enum RegisterOrder { REGISTER_HIERARCHY_FIRST, REGISTER_CLIENTS_FIRST };
enum UnregisterOrder { UNREGISTER_HIERARCHY_FIRST, UNREGISTER_CLIENTS_FIRST };
enum BFSOrder { BFS_FIRST, BFS_SECOND, BFS_THIRD };

static const RegisterOrder kRegisterOrderList[] = {REGISTER_HIERARCHY_FIRST,
                                                   REGISTER_CLIENTS_FIRST};
static const UnregisterOrder kUnregisterOrderList[] = {
    UNREGISTER_HIERARCHY_FIRST, UNREGISTER_CLIENTS_FIRST};
static const BFSOrder kBFSOrderList[] = {BFS_FIRST, BFS_SECOND, BFS_THIRD};

class SurfaceManagerOrderingParamTest
    : public SurfaceManagerOrderingTest,
      public ::testing::WithParamInterface<
          std::tr1::tuple<RegisterOrder, UnregisterOrder, BFSOrder>> {};

TEST_P(SurfaceManagerOrderingParamTest, Ordering) {
  // Test the four permutations of client/hierarchy setting/unsetting and test
  // each place the BFS can be added and removed.  The BFS and the
  // client/hierarchy are less related, so BFS is tested independently instead
  // of every permutation of BFS setting and unsetting.
  // The register/unregister functions themselves test most of the state.
  RegisterOrder register_order = std::tr1::get<0>(GetParam());
  UnregisterOrder unregister_order = std::tr1::get<1>(GetParam());
  BFSOrder bfs_order = std::tr1::get<2>(GetParam());

  // Attach everything up in the specified order.
  if (bfs_order == BFS_FIRST)
    RegisterBFS();

  if (register_order == REGISTER_HIERARCHY_FIRST)
    RegisterHierarchy();
  else
    RegisterClients();

  if (bfs_order == BFS_SECOND)
    RegisterBFS();

  if (register_order == REGISTER_HIERARCHY_FIRST)
    RegisterClients();
  else
    RegisterHierarchy();

  if (bfs_order == BFS_THIRD)
    RegisterBFS();

  // Everything hooked up, so should be valid.
  AssertAllValidBFS();

  // Detach everything in the specified order.
  if (bfs_order == BFS_THIRD)
    UnregisterBFS();

  if (unregister_order == UNREGISTER_HIERARCHY_FIRST)
    UnregisterHierarchy();
  else
    UnregisterClients();

  if (bfs_order == BFS_SECOND)
    UnregisterBFS();

  if (unregister_order == UNREGISTER_HIERARCHY_FIRST)
    UnregisterClients();
  else
    UnregisterHierarchy();

  if (bfs_order == BFS_FIRST)
    UnregisterBFS();
}

INSTANTIATE_TEST_CASE_P(
    SurfaceManagerOrderingParamTestInstantiation,
    SurfaceManagerOrderingParamTest,
    ::testing::Combine(::testing::ValuesIn(kRegisterOrderList),
                       ::testing::ValuesIn(kUnregisterOrderList),
                       ::testing::ValuesIn(kBFSOrderList)));

}  // namespace cc
