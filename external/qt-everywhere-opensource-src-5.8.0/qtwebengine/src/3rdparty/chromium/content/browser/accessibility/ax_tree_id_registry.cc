// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/ax_tree_id_registry.h"

#include "base/memory/singleton.h"

namespace content {

// static
const AXTreeIDRegistry::AXTreeID AXTreeIDRegistry::kNoAXTreeID = -1;

// static
AXTreeIDRegistry* AXTreeIDRegistry::GetInstance() {
  return base::Singleton<AXTreeIDRegistry>::get();
}

AXTreeIDRegistry::AXTreeID AXTreeIDRegistry::GetOrCreateAXTreeID(
    int process_id, int routing_id) {
  FrameID frame_id(process_id, routing_id);
  std::map<FrameID, AXTreeID>::iterator it;
  it = frame_to_ax_tree_id_map_.find(frame_id);
  if (it != frame_to_ax_tree_id_map_.end())
    return it->second;

  AXTreeID new_id = ++ax_tree_id_counter_;
  frame_to_ax_tree_id_map_[frame_id] = new_id;
  ax_tree_to_frame_id_map_[new_id] = frame_id;

  return new_id;
}

AXTreeIDRegistry::FrameID AXTreeIDRegistry::GetFrameID(
    AXTreeIDRegistry::AXTreeID ax_tree_id) {
  std::map<AXTreeID, FrameID>::iterator it;
  it = ax_tree_to_frame_id_map_.find(ax_tree_id);
  if (it != ax_tree_to_frame_id_map_.end())
    return it->second;

  return FrameID(-1, -1);
}

void AXTreeIDRegistry::RemoveAXTreeID(AXTreeIDRegistry::AXTreeID ax_tree_id) {
  std::map<AXTreeID, FrameID>::iterator it;
  it = ax_tree_to_frame_id_map_.find(ax_tree_id);
  if (it != ax_tree_to_frame_id_map_.end()) {
    frame_to_ax_tree_id_map_.erase(it->second);
    ax_tree_to_frame_id_map_.erase(it);
  }
}

AXTreeIDRegistry::AXTreeIDRegistry() : ax_tree_id_counter_(-1) {
  // Always populate default desktop tree value (0 -> 0, 0).
  GetOrCreateAXTreeID(0, 0);
}

AXTreeIDRegistry::~AXTreeIDRegistry() {
}

}  // namespace content
