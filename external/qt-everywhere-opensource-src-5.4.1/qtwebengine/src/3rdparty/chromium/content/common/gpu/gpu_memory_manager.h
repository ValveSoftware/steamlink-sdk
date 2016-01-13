// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_

#include <list>
#include <map>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/gpu_memory_stats.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/service/memory_tracking.h"

namespace content {

class GpuChannelManager;
class GpuMemoryManagerClient;
class GpuMemoryManagerClientState;
class GpuMemoryTrackingGroup;

class CONTENT_EXPORT GpuMemoryManager :
    public base::SupportsWeakPtr<GpuMemoryManager> {
 public:
#if defined(OS_ANDROID) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  enum { kDefaultMaxSurfacesWithFrontbufferSoftLimit = 1 };
#else
  enum { kDefaultMaxSurfacesWithFrontbufferSoftLimit = 8 };
#endif
  enum ScheduleManageTime {
    // Add a call to Manage to the thread's message loop immediately.
    kScheduleManageNow,
    // Add a Manage call to the thread's message loop for execution 1/60th of
    // of a second from now.
    kScheduleManageLater,
  };

  GpuMemoryManager(GpuChannelManager* channel_manager,
                   uint64 max_surfaces_with_frontbuffer_soft_limit);
  ~GpuMemoryManager();

  // Schedule a Manage() call. If immediate is true, we PostTask without delay.
  // Otherwise PostDelayedTask using a CancelableClosure and allow multiple
  // delayed calls to "queue" up. This way, we do not spam clients in certain
  // lower priority situations. An immediate schedule manage will cancel any
  // queued delayed manage.
  void ScheduleManage(ScheduleManageTime schedule_manage_time);

  // Retrieve GPU Resource consumption statistics for the task manager
  void GetVideoMemoryUsageStats(
      content::GPUVideoMemoryUsageStats* video_memory_usage_stats) const;

  GpuMemoryManagerClientState* CreateClientState(
      GpuMemoryManagerClient* client, bool has_surface, bool visible);

  GpuMemoryTrackingGroup* CreateTrackingGroup(
      base::ProcessId pid, gpu::gles2::MemoryTracker* memory_tracker);

  uint64 GetClientMemoryUsage(const GpuMemoryManagerClient* client) const;
  uint64 GetMaximumClientAllocation() const {
    return client_hard_limit_bytes_;
  }

 private:
  friend class GpuMemoryManagerTest;
  friend class GpuMemoryTrackingGroup;
  friend class GpuMemoryManagerClientState;

  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageBasicFunctionality);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageChangingVisibility);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageManyVisibleStubs);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageManyNotVisibleStubs);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageChangingLastUsedTime);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageChangingImportanceShareGroup);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestForegroundStubsGetBonusAllocation);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestUpdateAvailableGpuMemory);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           GpuMemoryAllocationCompareTests);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           StubMemoryStatsForLastManageTests);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManagedUsageTracking);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           BackgroundMru);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           AllowNonvisibleMemory);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           BackgroundDiscardPersistent);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           UnmanagedTracking);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           DefaultAllocation);

  typedef std::map<gpu::gles2::MemoryTracker*, GpuMemoryTrackingGroup*>
      TrackingGroupMap;

  typedef std::list<GpuMemoryManagerClientState*> ClientStateList;

  void Manage();
  void SetClientsHibernatedState() const;
  void AssignSurfacesAllocations();
  void AssignNonSurfacesAllocations();

  // Update the amount of GPU memory we think we have in the system, based
  // on what the stubs' contexts report.
  void UpdateAvailableGpuMemory();

  // Send memory usage stats to the browser process.
  void SendUmaStatsToBrowser();

  // Get the current number of bytes allocated.
  uint64 GetCurrentUsage() const {
    return bytes_allocated_managed_current_ +
        bytes_allocated_unmanaged_current_;
  }

  // GpuMemoryTrackingGroup interface
  void TrackMemoryAllocatedChange(
      GpuMemoryTrackingGroup* tracking_group,
      uint64 old_size,
      uint64 new_size,
      gpu::gles2::MemoryTracker::Pool tracking_pool);
  void OnDestroyTrackingGroup(GpuMemoryTrackingGroup* tracking_group);
  bool EnsureGPUMemoryAvailable(uint64 size_needed);

  // GpuMemoryManagerClientState interface
  void SetClientStateVisible(
      GpuMemoryManagerClientState* client_state, bool visible);
  void OnDestroyClientState(GpuMemoryManagerClientState* client);

  // Add or remove a client from its clients list (visible, nonvisible, or
  // nonsurface). When adding the client, add it to the front of the list.
  void AddClientToList(GpuMemoryManagerClientState* client_state);
  void RemoveClientFromList(GpuMemoryManagerClientState* client_state);
  ClientStateList* GetClientList(GpuMemoryManagerClientState* client_state);

  // Interfaces for testing
  void TestingDisableScheduleManage() { disable_schedule_manage_ = true; }

  GpuChannelManager* channel_manager_;

  // A list of all visible and nonvisible clients, in most-recently-used
  // order (most recently used is first).
  ClientStateList clients_visible_mru_;
  ClientStateList clients_nonvisible_mru_;

  // A list of all clients that don't have a surface.
  ClientStateList clients_nonsurface_;

  // All context groups' tracking structures
  TrackingGroupMap tracking_groups_;

  base::CancelableClosure delayed_manage_callback_;
  bool manage_immediate_scheduled_;
  bool disable_schedule_manage_;

  uint64 max_surfaces_with_frontbuffer_soft_limit_;

  // The maximum amount of memory that may be allocated for a single client.
  uint64 client_hard_limit_bytes_;

  // The current total memory usage, and historical maximum memory usage
  uint64 bytes_allocated_managed_current_;
  uint64 bytes_allocated_unmanaged_current_;
  uint64 bytes_allocated_historical_max_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryManager);
};

}  // namespace content

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
