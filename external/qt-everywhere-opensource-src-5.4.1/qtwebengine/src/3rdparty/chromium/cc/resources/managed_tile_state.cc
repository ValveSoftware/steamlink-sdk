// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/managed_tile_state.h"

#include <limits>

#include "cc/base/math_util.h"

namespace cc {

scoped_ptr<base::Value> ManagedTileBinAsValue(ManagedTileBin bin) {
  switch (bin) {
    case NOW_AND_READY_TO_DRAW_BIN:
      return scoped_ptr<base::Value>(
          new base::StringValue("NOW_AND_READY_TO_DRAW_BIN"));
    case NOW_BIN:
      return scoped_ptr<base::Value>(new base::StringValue("NOW_BIN"));
    case SOON_BIN:
      return scoped_ptr<base::Value>(
          new base::StringValue("SOON_BIN"));
    case EVENTUALLY_AND_ACTIVE_BIN:
      return scoped_ptr<base::Value>(
          new base::StringValue("EVENTUALLY_AND_ACTIVE_BIN"));
    case EVENTUALLY_BIN:
      return scoped_ptr<base::Value>(
          new base::StringValue("EVENTUALLY_BIN"));
    case AT_LAST_AND_ACTIVE_BIN:
      return scoped_ptr<base::Value>(
          new base::StringValue("AT_LAST_AND_ACTIVE_BIN"));
    case AT_LAST_BIN:
      return scoped_ptr<base::Value>(
          new base::StringValue("AT_LAST_BIN"));
    case NEVER_BIN:
      return scoped_ptr<base::Value>(
          new base::StringValue("NEVER_BIN"));
    case NUM_BINS:
      NOTREACHED();
      return scoped_ptr<base::Value>(
          new base::StringValue("Invalid Bin (NUM_BINS)"));
  }
  return scoped_ptr<base::Value>(
      new base::StringValue("Invalid Bin (UNKNOWN)"));
}

ManagedTileState::ManagedTileState()
    : raster_mode(LOW_QUALITY_RASTER_MODE),
      bin(NEVER_BIN),
      resolution(NON_IDEAL_RESOLUTION),
      required_for_activation(false),
      priority_bin(TilePriority::EVENTUALLY),
      distance_to_visible(std::numeric_limits<float>::infinity()),
      visible_and_ready_to_draw(false),
      scheduled_priority(0) {}

ManagedTileState::TileVersion::TileVersion() : mode_(RESOURCE_MODE) {
}

ManagedTileState::TileVersion::~TileVersion() { DCHECK(!resource_); }

bool ManagedTileState::TileVersion::IsReadyToDraw() const {
  switch (mode_) {
    case RESOURCE_MODE:
      return !!resource_;
    case SOLID_COLOR_MODE:
    case PICTURE_PILE_MODE:
      return true;
  }
  NOTREACHED();
  return false;
}

size_t ManagedTileState::TileVersion::GPUMemoryUsageInBytes() const {
  if (!resource_)
    return 0;
  return resource_->bytes();
}

ManagedTileState::~ManagedTileState() {}

scoped_ptr<base::Value> ManagedTileState::AsValue() const {
  bool has_resource = false;
  bool has_active_task = false;
  for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
    has_resource |= (tile_versions[mode].resource_.get() != 0);
    has_active_task |= (tile_versions[mode].raster_task_.get() != 0);
  }

  bool is_using_gpu_memory = has_resource || has_active_task;

  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetBoolean("has_resource", has_resource);
  state->SetBoolean("is_using_gpu_memory", is_using_gpu_memory);
  state->Set("bin", ManagedTileBinAsValue(bin).release());
  state->Set("resolution", TileResolutionAsValue(resolution).release());
  state->Set("priority_bin", TilePriorityBinAsValue(priority_bin).release());
  state->Set("distance_to_visible",
             MathUtil::AsValueSafely(distance_to_visible).release());
  state->SetBoolean("required_for_activation", required_for_activation);
  state->SetBoolean(
      "is_solid_color",
      tile_versions[raster_mode].mode_ == TileVersion::SOLID_COLOR_MODE);
  state->SetBoolean(
      "is_transparent",
      tile_versions[raster_mode].mode_ == TileVersion::SOLID_COLOR_MODE &&
          !SkColorGetA(tile_versions[raster_mode].solid_color_));
  state->SetInteger("scheduled_priority", scheduled_priority);
  return state.PassAs<base::Value>();
}

}  // namespace cc
