// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_PRIORITY_H_
#define CC_RESOURCES_TILE_PRIORITY_H_

#include <algorithm>
#include <limits>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/resources/picture_pile.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace base {
class Value;
}

namespace cc {

enum WhichTree {
  // Note: these must be 0 and 1 because we index with them in various places,
  // e.g. in Tile::priority_.
  ACTIVE_TREE = 0,
  PENDING_TREE = 1,
  NUM_TREES = 2
  // Be sure to update WhichTreeAsValue when adding new fields.
};
scoped_ptr<base::Value> WhichTreeAsValue(
    WhichTree tree);

enum TileResolution {
  LOW_RESOLUTION = 0 ,
  HIGH_RESOLUTION = 1,
  NON_IDEAL_RESOLUTION = 2,
};
scoped_ptr<base::Value> TileResolutionAsValue(
    TileResolution resolution);

struct CC_EXPORT TilePriority {
  enum PriorityBin { NOW, SOON, EVENTUALLY };

  TilePriority()
      : resolution(NON_IDEAL_RESOLUTION),
        required_for_activation(false),
        priority_bin(EVENTUALLY),
        distance_to_visible(std::numeric_limits<float>::infinity()) {}

  TilePriority(TileResolution resolution,
               PriorityBin bin,
               float distance_to_visible)
      : resolution(resolution),
        required_for_activation(false),
        priority_bin(bin),
        distance_to_visible(distance_to_visible) {}

  TilePriority(const TilePriority& active, const TilePriority& pending) {
    if (active.resolution == HIGH_RESOLUTION ||
        pending.resolution == HIGH_RESOLUTION)
      resolution = HIGH_RESOLUTION;
    else if (active.resolution == LOW_RESOLUTION ||
             pending.resolution == LOW_RESOLUTION)
      resolution = LOW_RESOLUTION;
    else
      resolution = NON_IDEAL_RESOLUTION;

    required_for_activation =
        active.required_for_activation || pending.required_for_activation;

    if (active.priority_bin < pending.priority_bin) {
      priority_bin = active.priority_bin;
      distance_to_visible = active.distance_to_visible;
    } else if (active.priority_bin > pending.priority_bin) {
      priority_bin = pending.priority_bin;
      distance_to_visible = pending.distance_to_visible;
    } else {
      priority_bin = active.priority_bin;
      distance_to_visible =
          std::min(active.distance_to_visible, pending.distance_to_visible);
    }
  }

  scoped_ptr<base::Value> AsValue() const;

  bool operator ==(const TilePriority& other) const {
    return resolution == other.resolution &&
           priority_bin == other.priority_bin &&
           distance_to_visible == other.distance_to_visible &&
           required_for_activation == other.required_for_activation;
  }

  bool operator !=(const TilePriority& other) const {
    return !(*this == other);
  }

  bool IsHigherPriorityThan(const TilePriority& other) const {
    return priority_bin < other.priority_bin ||
           (priority_bin == other.priority_bin &&
            distance_to_visible < other.distance_to_visible);
  }

  TileResolution resolution;
  bool required_for_activation;
  PriorityBin priority_bin;
  float distance_to_visible;
};

scoped_ptr<base::Value> TilePriorityBinAsValue(TilePriority::PriorityBin bin);

enum TileMemoryLimitPolicy {
  // Nothing.
  ALLOW_NOTHING = 0,

  // You might be made visible, but you're not being interacted with.
  ALLOW_ABSOLUTE_MINIMUM = 1,  // Tall.

  // You're being interacted with, but we're low on memory.
  ALLOW_PREPAINT_ONLY = 2,  // Grande.

  // You're the only thing in town. Go crazy.
  ALLOW_ANYTHING = 3,  // Venti.

  NUM_TILE_MEMORY_LIMIT_POLICIES = 4,

  // NOTE: Be sure to update TreePriorityAsValue and kBinPolicyMap when adding
  // or reordering fields.
};
scoped_ptr<base::Value> TileMemoryLimitPolicyAsValue(
    TileMemoryLimitPolicy policy);

enum TreePriority {
  SAME_PRIORITY_FOR_BOTH_TREES,
  SMOOTHNESS_TAKES_PRIORITY,
  NEW_CONTENT_TAKES_PRIORITY

  // Be sure to update TreePriorityAsValue when adding new fields.
};
scoped_ptr<base::Value> TreePriorityAsValue(TreePriority prio);

class GlobalStateThatImpactsTilePriority {
 public:
  GlobalStateThatImpactsTilePriority()
      : memory_limit_policy(ALLOW_NOTHING),
        soft_memory_limit_in_bytes(0),
        hard_memory_limit_in_bytes(0),
        num_resources_limit(0),
        tree_priority(SAME_PRIORITY_FOR_BOTH_TREES) {}

  TileMemoryLimitPolicy memory_limit_policy;

  size_t soft_memory_limit_in_bytes;
  size_t hard_memory_limit_in_bytes;
  size_t num_resources_limit;

  TreePriority tree_priority;

  bool operator==(const GlobalStateThatImpactsTilePriority& other) const {
    return memory_limit_policy == other.memory_limit_policy &&
           soft_memory_limit_in_bytes == other.soft_memory_limit_in_bytes &&
           hard_memory_limit_in_bytes == other.hard_memory_limit_in_bytes &&
           num_resources_limit == other.num_resources_limit &&
           tree_priority == other.tree_priority;
  }
  bool operator!=(const GlobalStateThatImpactsTilePriority& other) const {
    return !(*this == other);
  }

  scoped_ptr<base::Value> AsValue() const;
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_PRIORITY_H_
