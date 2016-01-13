// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_memory_manager_client.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "content/common/gpu/gpu_memory_uma_stats.h"
#include "content/common/gpu/gpu_messages.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/service/gpu_switches.h"

using gpu::MemoryAllocation;

namespace content {
namespace {

const int kDelayedScheduleManageTimeoutMs = 67;

const uint64 kBytesAllocatedUnmanagedStep = 16 * 1024 * 1024;

void TrackValueChanged(uint64 old_size, uint64 new_size, uint64* total_size) {
  DCHECK(new_size > old_size || *total_size >= (old_size - new_size));
  *total_size += (new_size - old_size);
}

}

GpuMemoryManager::GpuMemoryManager(
    GpuChannelManager* channel_manager,
    uint64 max_surfaces_with_frontbuffer_soft_limit)
    : channel_manager_(channel_manager),
      manage_immediate_scheduled_(false),
      disable_schedule_manage_(false),
      max_surfaces_with_frontbuffer_soft_limit_(
          max_surfaces_with_frontbuffer_soft_limit),
      client_hard_limit_bytes_(0),
      bytes_allocated_managed_current_(0),
      bytes_allocated_unmanaged_current_(0),
      bytes_allocated_historical_max_(0)
{ }

GpuMemoryManager::~GpuMemoryManager() {
  DCHECK(tracking_groups_.empty());
  DCHECK(clients_visible_mru_.empty());
  DCHECK(clients_nonvisible_mru_.empty());
  DCHECK(clients_nonsurface_.empty());
  DCHECK(!bytes_allocated_managed_current_);
  DCHECK(!bytes_allocated_unmanaged_current_);
}

void GpuMemoryManager::UpdateAvailableGpuMemory() {
  // If the value was overridden on the command line, use the specified value.
  static bool client_hard_limit_bytes_overridden =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceGpuMemAvailableMb);
  if (client_hard_limit_bytes_overridden) {
    base::StringToUint64(
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kForceGpuMemAvailableMb),
        &client_hard_limit_bytes_);
    client_hard_limit_bytes_ *= 1024 * 1024;
    return;
  }

#if defined(OS_ANDROID)
  // On non-Android, we use an operating system query when possible.
  // We do not have a reliable concept of multiple GPUs existing in
  // a system, so just be safe and go with the minimum encountered.
  uint64 bytes_min = 0;

  // Only use the clients that are visible, because otherwise the set of clients
  // we are querying could become extremely large.
  for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
      it != clients_visible_mru_.end();
      ++it) {
    const GpuMemoryManagerClientState* client_state = *it;
    if (!client_state->has_surface_)
      continue;
    if (!client_state->visible_)
      continue;

    uint64 bytes = 0;
    if (client_state->client_->GetTotalGpuMemory(&bytes)) {
      if (!bytes_min || bytes < bytes_min)
        bytes_min = bytes;
    }
  }

  client_hard_limit_bytes_ = bytes_min;
  // Clamp the observed value to a specific range on Android.
  client_hard_limit_bytes_ = std::max(client_hard_limit_bytes_,
                                      static_cast<uint64>(16 * 1024 * 1024));
  client_hard_limit_bytes_ = std::min(client_hard_limit_bytes_,
                                      static_cast<uint64>(256 * 1024 * 1024));
#else
  // Ignore what the system said and give all clients the same maximum
  // allocation on desktop platforms.
  client_hard_limit_bytes_ = 256 * 1024 * 1024;
#endif
}

void GpuMemoryManager::ScheduleManage(
    ScheduleManageTime schedule_manage_time) {
  if (disable_schedule_manage_)
    return;
  if (manage_immediate_scheduled_)
    return;
  if (schedule_manage_time == kScheduleManageNow) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&GpuMemoryManager::Manage, AsWeakPtr()));
    manage_immediate_scheduled_ = true;
    if (!delayed_manage_callback_.IsCancelled())
      delayed_manage_callback_.Cancel();
  } else {
    if (!delayed_manage_callback_.IsCancelled())
      return;
    delayed_manage_callback_.Reset(base::Bind(&GpuMemoryManager::Manage,
                                              AsWeakPtr()));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        delayed_manage_callback_.callback(),
        base::TimeDelta::FromMilliseconds(kDelayedScheduleManageTimeoutMs));
  }
}

void GpuMemoryManager::TrackMemoryAllocatedChange(
    GpuMemoryTrackingGroup* tracking_group,
    uint64 old_size,
    uint64 new_size,
    gpu::gles2::MemoryTracker::Pool tracking_pool) {
  TrackValueChanged(old_size, new_size, &tracking_group->size_);
  switch (tracking_pool) {
    case gpu::gles2::MemoryTracker::kManaged:
      TrackValueChanged(old_size, new_size, &bytes_allocated_managed_current_);
      break;
    case gpu::gles2::MemoryTracker::kUnmanaged:
      TrackValueChanged(old_size,
                        new_size,
                        &bytes_allocated_unmanaged_current_);
      break;
    default:
      NOTREACHED();
      break;
  }
  if (new_size != old_size) {
    TRACE_COUNTER1("gpu",
                   "GpuMemoryUsage",
                   GetCurrentUsage());
  }

  if (GetCurrentUsage() > bytes_allocated_historical_max_ +
                          kBytesAllocatedUnmanagedStep) {
      bytes_allocated_historical_max_ = GetCurrentUsage();
      // If we're blowing into new memory usage territory, spam the browser
      // process with the most up-to-date information about our memory usage.
      SendUmaStatsToBrowser();
  }
}

bool GpuMemoryManager::EnsureGPUMemoryAvailable(uint64 /* size_needed */) {
  // TODO: Check if there is enough space. Lose contexts until there is.
  return true;
}

GpuMemoryManagerClientState* GpuMemoryManager::CreateClientState(
    GpuMemoryManagerClient* client,
    bool has_surface,
    bool visible) {
  TrackingGroupMap::iterator tracking_group_it =
      tracking_groups_.find(client->GetMemoryTracker());
  DCHECK(tracking_group_it != tracking_groups_.end());
  GpuMemoryTrackingGroup* tracking_group = tracking_group_it->second;

  GpuMemoryManagerClientState* client_state = new GpuMemoryManagerClientState(
      this, client, tracking_group, has_surface, visible);
  AddClientToList(client_state);
  ScheduleManage(kScheduleManageNow);
  return client_state;
}

void GpuMemoryManager::OnDestroyClientState(
    GpuMemoryManagerClientState* client_state) {
  RemoveClientFromList(client_state);
  ScheduleManage(kScheduleManageLater);
}

void GpuMemoryManager::SetClientStateVisible(
    GpuMemoryManagerClientState* client_state, bool visible) {
  DCHECK(client_state->has_surface_);
  if (client_state->visible_ == visible)
    return;

  RemoveClientFromList(client_state);
  client_state->visible_ = visible;
  AddClientToList(client_state);
  ScheduleManage(visible ? kScheduleManageNow : kScheduleManageLater);
}

uint64 GpuMemoryManager::GetClientMemoryUsage(
    const GpuMemoryManagerClient* client) const {
  TrackingGroupMap::const_iterator tracking_group_it =
      tracking_groups_.find(client->GetMemoryTracker());
  DCHECK(tracking_group_it != tracking_groups_.end());
  return tracking_group_it->second->GetSize();
}

GpuMemoryTrackingGroup* GpuMemoryManager::CreateTrackingGroup(
    base::ProcessId pid, gpu::gles2::MemoryTracker* memory_tracker) {
  GpuMemoryTrackingGroup* tracking_group = new GpuMemoryTrackingGroup(
      pid, memory_tracker, this);
  DCHECK(!tracking_groups_.count(tracking_group->GetMemoryTracker()));
  tracking_groups_.insert(std::make_pair(tracking_group->GetMemoryTracker(),
                                         tracking_group));
  return tracking_group;
}

void GpuMemoryManager::OnDestroyTrackingGroup(
    GpuMemoryTrackingGroup* tracking_group) {
  DCHECK(tracking_groups_.count(tracking_group->GetMemoryTracker()));
  tracking_groups_.erase(tracking_group->GetMemoryTracker());
}

void GpuMemoryManager::GetVideoMemoryUsageStats(
    GPUVideoMemoryUsageStats* video_memory_usage_stats) const {
  // For each context group, assign its memory usage to its PID
  video_memory_usage_stats->process_map.clear();
  for (TrackingGroupMap::const_iterator i =
       tracking_groups_.begin(); i != tracking_groups_.end(); ++i) {
    const GpuMemoryTrackingGroup* tracking_group = i->second;
    video_memory_usage_stats->process_map[
        tracking_group->GetPid()].video_memory += tracking_group->GetSize();
  }

  // Assign the total across all processes in the GPU process
  video_memory_usage_stats->process_map[
      base::GetCurrentProcId()].video_memory = GetCurrentUsage();
  video_memory_usage_stats->process_map[
      base::GetCurrentProcId()].has_duplicates = true;

  video_memory_usage_stats->bytes_allocated = GetCurrentUsage();
  video_memory_usage_stats->bytes_allocated_historical_max =
      bytes_allocated_historical_max_;
}

void GpuMemoryManager::Manage() {
  manage_immediate_scheduled_ = false;
  delayed_manage_callback_.Cancel();

  // Update the amount of GPU memory available on the system.
  UpdateAvailableGpuMemory();

  // Determine which clients are "hibernated" (which determines the
  // distribution of frontbuffers and memory among clients that don't have
  // surfaces).
  SetClientsHibernatedState();

  // Assign memory allocations to clients that have surfaces.
  AssignSurfacesAllocations();

  // Assign memory allocations to clients that don't have surfaces.
  AssignNonSurfacesAllocations();

  SendUmaStatsToBrowser();
}

void GpuMemoryManager::AssignSurfacesAllocations() {
  // Send that allocation to the clients.
  ClientStateList clients = clients_visible_mru_;
  clients.insert(clients.end(),
                 clients_nonvisible_mru_.begin(),
                 clients_nonvisible_mru_.end());
  for (ClientStateList::const_iterator it = clients.begin();
       it != clients.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;

    // Populate and send the allocation to the client
    MemoryAllocation allocation;
    allocation.bytes_limit_when_visible = client_hard_limit_bytes_;
#if defined(OS_ANDROID)
    // On Android, because there is only one visible tab at any time, allow
    // that renderer to cache as much as it can.
    allocation.priority_cutoff_when_visible =
        MemoryAllocation::CUTOFF_ALLOW_EVERYTHING;
#else
    // On desktop platforms, instruct the renderers to cache only a smaller
    // set, to play nice with other renderers and other applications. If this
    // if not done, then the system can become unstable.
    // http://crbug.com/145600 (Linux)
    // http://crbug.com/141377 (Mac)
    allocation.priority_cutoff_when_visible =
        MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
#endif

    client_state->client_->SetMemoryAllocation(allocation);
    client_state->client_->SuggestHaveFrontBuffer(!client_state->hibernated_);
  }
}

void GpuMemoryManager::AssignNonSurfacesAllocations() {
  for (ClientStateList::const_iterator it = clients_nonsurface_.begin();
       it != clients_nonsurface_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    MemoryAllocation allocation;

    if (!client_state->hibernated_) {
      allocation.bytes_limit_when_visible = client_hard_limit_bytes_;
      allocation.priority_cutoff_when_visible =
          MemoryAllocation::CUTOFF_ALLOW_EVERYTHING;
    }

    client_state->client_->SetMemoryAllocation(allocation);
  }
}

void GpuMemoryManager::SetClientsHibernatedState() const {
  // Re-set all tracking groups as being hibernated.
  for (TrackingGroupMap::const_iterator it = tracking_groups_.begin();
       it != tracking_groups_.end();
       ++it) {
    GpuMemoryTrackingGroup* tracking_group = it->second;
    tracking_group->hibernated_ = true;
  }
  // All clients with surfaces that are visible are non-hibernated.
  uint64 non_hibernated_clients = 0;
  for (ClientStateList::const_iterator it = clients_visible_mru_.begin();
       it != clients_visible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    client_state->hibernated_ = false;
    client_state->tracking_group_->hibernated_ = false;
    non_hibernated_clients++;
  }
  // Then an additional few clients with surfaces are non-hibernated too, up to
  // a fixed limit.
  for (ClientStateList::const_iterator it = clients_nonvisible_mru_.begin();
       it != clients_nonvisible_mru_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    if (non_hibernated_clients < max_surfaces_with_frontbuffer_soft_limit_) {
      client_state->hibernated_ = false;
      client_state->tracking_group_->hibernated_ = false;
      non_hibernated_clients++;
    } else {
      client_state->hibernated_ = true;
    }
  }
  // Clients that don't have surfaces are non-hibernated if they are
  // in a GL share group with a non-hibernated surface.
  for (ClientStateList::const_iterator it = clients_nonsurface_.begin();
       it != clients_nonsurface_.end();
       ++it) {
    GpuMemoryManagerClientState* client_state = *it;
    client_state->hibernated_ = client_state->tracking_group_->hibernated_;
  }
}

void GpuMemoryManager::SendUmaStatsToBrowser() {
  if (!channel_manager_)
    return;
  GPUMemoryUmaStats params;
  params.bytes_allocated_current = GetCurrentUsage();
  params.bytes_allocated_max = bytes_allocated_historical_max_;
  params.bytes_limit = client_hard_limit_bytes_;
  params.client_count = clients_visible_mru_.size() +
                        clients_nonvisible_mru_.size() +
                        clients_nonsurface_.size();
  params.context_group_count = tracking_groups_.size();
  channel_manager_->Send(new GpuHostMsg_GpuMemoryUmaStats(params));
}

GpuMemoryManager::ClientStateList* GpuMemoryManager::GetClientList(
    GpuMemoryManagerClientState* client_state) {
  if (client_state->has_surface_) {
    if (client_state->visible_)
      return &clients_visible_mru_;
    else
      return &clients_nonvisible_mru_;
  }
  return &clients_nonsurface_;
}

void GpuMemoryManager::AddClientToList(
    GpuMemoryManagerClientState* client_state) {
  DCHECK(!client_state->list_iterator_valid_);
  ClientStateList* client_list = GetClientList(client_state);
  client_state->list_iterator_ = client_list->insert(
      client_list->begin(), client_state);
  client_state->list_iterator_valid_ = true;
}

void GpuMemoryManager::RemoveClientFromList(
    GpuMemoryManagerClientState* client_state) {
  DCHECK(client_state->list_iterator_valid_);
  ClientStateList* client_list = GetClientList(client_state);
  client_list->erase(client_state->list_iterator_);
  client_state->list_iterator_valid_ = false;
}

}  // namespace content
