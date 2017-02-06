// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_AX_TREE_ID_REGISTRY_H_
#define CONTENT_BROWSER_ACCESSIBILITY_AX_TREE_ID_REGISTRY_H_

#include <map>

#include "base/macros.h"
#include "base/stl_util.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {

// A class which generates a unique id given a process id and frame routing id.
class AXTreeIDRegistry {
 public:
  typedef std::pair<int, int> FrameID;

  typedef int AXTreeID;

  static const AXTreeID kNoAXTreeID;

  // Get the single instance of this class.
  static AXTreeIDRegistry* GetInstance();

  // Obtains a unique id given a |process_id| and |routing_id|. Placeholder
  // for full implementation once out of process iframe accessibility finalizes.
  AXTreeID GetOrCreateAXTreeID(int process_id, int routing_id);
  FrameID GetFrameID(AXTreeID ax_tree_id);
  void RemoveAXTreeID(AXTreeID ax_tree_id);

 private:
  friend struct base::DefaultSingletonTraits<AXTreeIDRegistry>;

  AXTreeIDRegistry();
  virtual ~AXTreeIDRegistry();

  // Tracks the current unique ax frame id.
  AXTreeID ax_tree_id_counter_;

  // Maps an accessibility tree to its frame via ids.
  std::map<AXTreeID, FrameID> ax_tree_to_frame_id_map_;

  // Maps frames to an accessibility tree via ids.
  std::map<FrameID, AXTreeID> frame_to_ax_tree_id_map_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeIDRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_AX_TREE_ID_REGISTRY_H_
