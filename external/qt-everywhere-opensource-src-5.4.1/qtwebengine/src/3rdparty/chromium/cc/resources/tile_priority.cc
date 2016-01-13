// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile_priority.h"

#include "base/values.h"
#include "cc/base/math_util.h"

namespace cc {

scoped_ptr<base::Value> WhichTreeAsValue(WhichTree tree) {
  switch (tree) {
  case ACTIVE_TREE:
      return scoped_ptr<base::Value>(new base::StringValue("ACTIVE_TREE"));
  case PENDING_TREE:
      return scoped_ptr<base::Value>(new base::StringValue("PENDING_TREE"));
  default:
      DCHECK(false) << "Unrecognized WhichTree value " << tree;
      return scoped_ptr<base::Value>(new base::StringValue(
          "<unknown WhichTree value>"));
  }
}

scoped_ptr<base::Value> TileResolutionAsValue(
    TileResolution resolution) {
  switch (resolution) {
  case LOW_RESOLUTION:
    return scoped_ptr<base::Value>(new base::StringValue("LOW_RESOLUTION"));
  case HIGH_RESOLUTION:
    return scoped_ptr<base::Value>(new base::StringValue("HIGH_RESOLUTION"));
  case NON_IDEAL_RESOLUTION:
      return scoped_ptr<base::Value>(new base::StringValue(
        "NON_IDEAL_RESOLUTION"));
  }
  DCHECK(false) << "Unrecognized TileResolution value " << resolution;
  return scoped_ptr<base::Value>(new base::StringValue(
      "<unknown TileResolution value>"));
}

scoped_ptr<base::Value> TilePriorityBinAsValue(TilePriority::PriorityBin bin) {
  switch (bin) {
    case TilePriority::NOW:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue("NOW"));
    case TilePriority::SOON:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue("SOON"));
    case TilePriority::EVENTUALLY:
      return scoped_ptr<base::Value>(
          base::Value::CreateStringValue("EVENTUALLY"));
  }
  DCHECK(false) << "Unrecognized TilePriority::PriorityBin value " << bin;
  return scoped_ptr<base::Value>(base::Value::CreateStringValue(
      "<unknown TilePriority::PriorityBin value>"));
}

scoped_ptr<base::Value> TilePriority::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->Set("resolution", TileResolutionAsValue(resolution).release());
  state->Set("priority_bin", TilePriorityBinAsValue(priority_bin).release());
  state->Set("distance_to_visible",
             MathUtil::AsValueSafely(distance_to_visible).release());
  return state.PassAs<base::Value>();
}

scoped_ptr<base::Value> TileMemoryLimitPolicyAsValue(
    TileMemoryLimitPolicy policy) {
  switch (policy) {
  case ALLOW_NOTHING:
      return scoped_ptr<base::Value>(new base::StringValue("ALLOW_NOTHING"));
  case ALLOW_ABSOLUTE_MINIMUM:
      return scoped_ptr<base::Value>(new base::StringValue(
          "ALLOW_ABSOLUTE_MINIMUM"));
  case ALLOW_PREPAINT_ONLY:
      return scoped_ptr<base::Value>(new base::StringValue(
          "ALLOW_PREPAINT_ONLY"));
  case ALLOW_ANYTHING:
      return scoped_ptr<base::Value>(new base::StringValue(
          "ALLOW_ANYTHING"));
  default:
      DCHECK(false) << "Unrecognized policy value";
      return scoped_ptr<base::Value>(new base::StringValue(
          "<unknown>"));
  }
}

scoped_ptr<base::Value> TreePriorityAsValue(TreePriority prio) {
  switch (prio) {
  case SAME_PRIORITY_FOR_BOTH_TREES:
      return scoped_ptr<base::Value>(new base::StringValue(
          "SAME_PRIORITY_FOR_BOTH_TREES"));
  case SMOOTHNESS_TAKES_PRIORITY:
      return scoped_ptr<base::Value>(new base::StringValue(
          "SMOOTHNESS_TAKES_PRIORITY"));
  case NEW_CONTENT_TAKES_PRIORITY:
      return scoped_ptr<base::Value>(new base::StringValue(
          "NEW_CONTENT_TAKES_PRIORITY"));
  }
  DCHECK(false) << "Unrecognized priority value " << prio;
  return scoped_ptr<base::Value>(new base::StringValue(
      "<unknown>"));
}

scoped_ptr<base::Value> GlobalStateThatImpactsTilePriority::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->Set("memory_limit_policy",
             TileMemoryLimitPolicyAsValue(memory_limit_policy).release());
  state->SetInteger("soft_memory_limit_in_bytes", soft_memory_limit_in_bytes);
  state->SetInteger("hard_memory_limit_in_bytes", hard_memory_limit_in_bytes);
  state->SetInteger("num_resources_limit", num_resources_limit);
  state->Set("tree_priority", TreePriorityAsValue(tree_priority).release());
  return state.PassAs<base::Value>();
}

}  // namespace cc
