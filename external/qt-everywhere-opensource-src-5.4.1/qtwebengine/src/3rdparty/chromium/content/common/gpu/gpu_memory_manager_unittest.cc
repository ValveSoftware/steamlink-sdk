// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_memory_manager_client.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "ui/gfx/size_conversions.h"

#include "testing/gtest/include/gtest/gtest.h"

using gpu::MemoryAllocation;

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<content::GpuMemoryManagerClient*> {
  uint64 operator()(content::GpuMemoryManagerClient* ptr) const {
    return hash<uint64>()(reinterpret_cast<uint64>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

class FakeMemoryTracker : public gpu::gles2::MemoryTracker {
 public:
  virtual void TrackMemoryAllocatedChange(
      size_t /* old_size */,
      size_t /* new_size */,
      gpu::gles2::MemoryTracker::Pool /* pool */) OVERRIDE {
  }
  virtual bool EnsureGPUMemoryAvailable(size_t /* size_needed */) OVERRIDE {
    return true;
  }
 private:
  virtual ~FakeMemoryTracker() {
  }
};

namespace content {

// This class is used to collect all stub assignments during a
// Manage() call.
class ClientAssignmentCollector {
 public:
  struct ClientMemoryStat {
    MemoryAllocation allocation;
  };
  typedef base::hash_map<GpuMemoryManagerClient*, ClientMemoryStat>
      ClientMemoryStatMap;

  static const ClientMemoryStatMap& GetClientStatsForLastManage() {
    return client_memory_stats_for_last_manage_;
  }
  static void ClearAllStats() {
    client_memory_stats_for_last_manage_.clear();
  }
  static void AddClientStat(GpuMemoryManagerClient* client,
                          const MemoryAllocation& allocation) {
    DCHECK(!client_memory_stats_for_last_manage_.count(client));
    client_memory_stats_for_last_manage_[client].allocation = allocation;
  }

 private:
  static ClientMemoryStatMap client_memory_stats_for_last_manage_;
};

ClientAssignmentCollector::ClientMemoryStatMap
    ClientAssignmentCollector::client_memory_stats_for_last_manage_;

class FakeClient : public GpuMemoryManagerClient {
 public:
  GpuMemoryManager* memmgr_;
  bool suggest_have_frontbuffer_;
  MemoryAllocation allocation_;
  uint64 total_gpu_memory_;
  gfx::Size surface_size_;
  GpuMemoryManagerClient* share_group_;
  scoped_refptr<gpu::gles2::MemoryTracker> memory_tracker_;
  scoped_ptr<GpuMemoryTrackingGroup> tracking_group_;
  scoped_ptr<GpuMemoryManagerClientState> client_state_;

  // This will create a client with no surface
  FakeClient(GpuMemoryManager* memmgr, GpuMemoryManagerClient* share_group)
      : memmgr_(memmgr),
        suggest_have_frontbuffer_(false),
        total_gpu_memory_(0),
        share_group_(share_group),
        memory_tracker_(NULL) {
    if (!share_group_) {
      memory_tracker_ = new FakeMemoryTracker();
      tracking_group_.reset(
          memmgr_->CreateTrackingGroup(0, memory_tracker_.get()));
    }
    client_state_.reset(memmgr_->CreateClientState(this, false, true));
  }

  // This will create a client with a surface
  FakeClient(GpuMemoryManager* memmgr, int32 surface_id, bool visible)
      : memmgr_(memmgr),
        suggest_have_frontbuffer_(false),
        total_gpu_memory_(0),
        share_group_(NULL),
        memory_tracker_(NULL) {
    memory_tracker_ = new FakeMemoryTracker();
    tracking_group_.reset(
        memmgr_->CreateTrackingGroup(0, memory_tracker_.get()));
    client_state_.reset(
        memmgr_->CreateClientState(this, surface_id != 0, visible));
  }

  virtual ~FakeClient() {
    client_state_.reset();
    tracking_group_.reset();
    memory_tracker_ = NULL;
  }

  virtual void SetMemoryAllocation(const MemoryAllocation& alloc) OVERRIDE {
    allocation_ = alloc;
    ClientAssignmentCollector::AddClientStat(this, alloc);
  }

  virtual void SuggestHaveFrontBuffer(bool suggest_have_frontbuffer) OVERRIDE {
    suggest_have_frontbuffer_ = suggest_have_frontbuffer;
  }

  virtual bool GetTotalGpuMemory(uint64* bytes) OVERRIDE {
    if (total_gpu_memory_) {
      *bytes = total_gpu_memory_;
      return true;
    }
    return false;
  }
  void SetTotalGpuMemory(uint64 bytes) { total_gpu_memory_ = bytes; }

  virtual gpu::gles2::MemoryTracker* GetMemoryTracker() const OVERRIDE {
    if (share_group_)
      return share_group_->GetMemoryTracker();
    return memory_tracker_.get();
  }

  virtual gfx::Size GetSurfaceSize() const OVERRIDE {
    return surface_size_;
  }
  void SetSurfaceSize(gfx::Size size) { surface_size_ = size; }

  void SetVisible(bool visible) {
    client_state_->SetVisible(visible);
  }

  uint64 BytesWhenVisible() const {
    return allocation_.bytes_limit_when_visible;
  }
};

class GpuMemoryManagerTest : public testing::Test {
 protected:
  static const uint64 kFrontbufferLimitForTest = 3;

  GpuMemoryManagerTest()
      : memmgr_(0, kFrontbufferLimitForTest) {
    memmgr_.TestingDisableScheduleManage();
  }

  virtual void SetUp() {
  }

  static int32 GenerateUniqueSurfaceId() {
    static int32 surface_id_ = 1;
    return surface_id_++;
  }

  bool IsAllocationForegroundForSurfaceYes(
      const MemoryAllocation& alloc) {
    return true;
  }
  bool IsAllocationBackgroundForSurfaceYes(
      const MemoryAllocation& alloc) {
    return true;
  }
  bool IsAllocationHibernatedForSurfaceYes(
      const MemoryAllocation& alloc) {
    return true;
  }
  bool IsAllocationForegroundForSurfaceNo(
      const MemoryAllocation& alloc) {
    return alloc.bytes_limit_when_visible != 0;
  }
  bool IsAllocationBackgroundForSurfaceNo(
      const MemoryAllocation& alloc) {
    return alloc.bytes_limit_when_visible != 0;
  }
  bool IsAllocationHibernatedForSurfaceNo(
      const MemoryAllocation& alloc) {
    return alloc.bytes_limit_when_visible == 0;
  }

  void Manage() {
    ClientAssignmentCollector::ClearAllStats();
    memmgr_.Manage();
  }

  GpuMemoryManager memmgr_;
};

// Test GpuMemoryManager::Manage basic functionality.
// Expect memory allocation to set suggest_have_frontbuffer/backbuffer
// according to visibility and last used time for stubs with surface.
// Expect memory allocation to be shared according to share groups for stubs
// without a surface.
TEST_F(GpuMemoryManagerTest, TestManageBasicFunctionality) {
  // Test stubs with surface.
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), false);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));

  // Test stubs without surface, with share group of 1 stub.
  FakeClient stub3(&memmgr_, &stub1), stub4(&memmgr_, &stub2);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));

  // Test stub without surface, with share group of multiple stubs.
  FakeClient stub5(&memmgr_ , &stub2);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));
}

// Test GpuMemoryManager::Manage functionality: changing visibility.
// Expect memory allocation to set suggest_have_frontbuffer/backbuffer
// according to visibility and last used time for stubs with surface.
// Expect memory allocation to be shared according to share groups for stubs
// without a surface.
TEST_F(GpuMemoryManagerTest, TestManageChangingVisibility) {
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), false);

  FakeClient stub3(&memmgr_, &stub1), stub4(&memmgr_, &stub2);
  FakeClient stub5(&memmgr_ , &stub2);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub5.allocation_));

  stub1.SetVisible(false);
  stub2.SetVisible(true);

  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub5.allocation_));
}

// Test GpuMemoryManager::Manage functionality: Test more than threshold number
// of visible stubs.
// Expect all allocations to continue to have frontbuffer.
TEST_F(GpuMemoryManagerTest, TestManageManyVisibleStubs) {
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub3(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub4(&memmgr_, GenerateUniqueSurfaceId(), true);

  FakeClient stub5(&memmgr_ , &stub1), stub6(&memmgr_ , &stub2);
  FakeClient stub7(&memmgr_ , &stub2);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub3.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub4.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub5.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub6.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub7.allocation_));
}

// Test GpuMemoryManager::Manage functionality: Test more than threshold number
// of not visible stubs.
// Expect the stubs surpassing the threshold to not have a backbuffer.
TEST_F(GpuMemoryManagerTest, TestManageManyNotVisibleStubs) {
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub3(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub4(&memmgr_, GenerateUniqueSurfaceId(), true);
  stub4.SetVisible(false);
  stub3.SetVisible(false);
  stub2.SetVisible(false);
  stub1.SetVisible(false);

  FakeClient stub5(&memmgr_ , &stub1), stub6(&memmgr_ , &stub4);
  FakeClient stub7(&memmgr_ , &stub1);

  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub3.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub4.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub5.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub6.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub7.allocation_));
}

// Test GpuMemoryManager::Manage functionality: Test changing the last used
// time of stubs when doing so causes change in which stubs surpass threshold.
// Expect frontbuffer to be dropped for the older stub.
TEST_F(GpuMemoryManagerTest, TestManageChangingLastUsedTime) {
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub3(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub4(&memmgr_, GenerateUniqueSurfaceId(), true);

  FakeClient stub5(&memmgr_ , &stub3), stub6(&memmgr_ , &stub4);
  FakeClient stub7(&memmgr_ , &stub3);

  // Make stub4 be the least-recently-used client
  stub4.SetVisible(false);
  stub3.SetVisible(false);
  stub2.SetVisible(false);
  stub1.SetVisible(false);

  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub3.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub4.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub5.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub6.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub7.allocation_));

  // Make stub3 become the least-recently-used client.
  stub2.SetVisible(true);
  stub2.SetVisible(false);
  stub4.SetVisible(true);
  stub4.SetVisible(false);

  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub4.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub5.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub6.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub7.allocation_));
}

// Test GpuMemoryManager::Manage functionality: Test changing importance of
// enough stubs so that every stub in share group crosses threshold.
// Expect memory allocation of the stubs without surface to share memory
// allocation with the most visible stub in share group.
TEST_F(GpuMemoryManagerTest, TestManageChangingImportanceShareGroup) {
  FakeClient stub_ignore_a(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub_ignore_b(&memmgr_, GenerateUniqueSurfaceId(), false),
             stub_ignore_c(&memmgr_, GenerateUniqueSurfaceId(), false);
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), false),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), false);

  FakeClient stub3(&memmgr_, &stub2), stub4(&memmgr_, &stub2);

  // stub1 and stub2 keep their non-hibernated state because they're
  // either visible or the 2 most recently used clients (through the
  // first three checks).
  stub1.SetVisible(true);
  stub2.SetVisible(true);
  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));

  stub1.SetVisible(false);
  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));

  stub2.SetVisible(false);
  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));

  // stub_ignore_b will cause stub1 to become hibernated (because
  // stub_ignore_a, stub_ignore_b, and stub2 are all non-hibernated and more
  // important).
  stub_ignore_b.SetVisible(true);
  stub_ignore_b.SetVisible(false);
  Manage();
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));

  // stub_ignore_c will cause stub2 to become hibernated (because
  // stub_ignore_a, stub_ignore_b, and stub_ignore_c are all non-hibernated
  // and more important).
  stub_ignore_c.SetVisible(true);
  stub_ignore_c.SetVisible(false);
  Manage();
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub4.allocation_));
}

}  // namespace content
