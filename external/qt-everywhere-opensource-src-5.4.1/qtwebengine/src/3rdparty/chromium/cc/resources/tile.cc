// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile.h"

#include <algorithm>

#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/tile_manager.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

Tile::Id Tile::s_next_id_ = 0;

Tile::Tile(TileManager* tile_manager,
           PicturePileImpl* picture_pile,
           const gfx::Size& tile_size,
           const gfx::Rect& content_rect,
           const gfx::Rect& opaque_rect,
           float contents_scale,
           int layer_id,
           int source_frame_number,
           int flags)
    : RefCountedManaged<Tile>(tile_manager),
      tile_manager_(tile_manager),
      tile_size_(tile_size),
      content_rect_(content_rect),
      contents_scale_(contents_scale),
      opaque_rect_(opaque_rect),
      layer_id_(layer_id),
      source_frame_number_(source_frame_number),
      flags_(flags),
      id_(s_next_id_++) {
  set_picture_pile(picture_pile);
}

Tile::~Tile() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"),
      "cc::Tile", this);
}

void Tile::SetPriority(WhichTree tree, const TilePriority& priority) {
  if (priority == priority_[tree])
    return;

  priority_[tree] = priority;
  tile_manager_->DidChangeTilePriority(this);
}

void Tile::MarkRequiredForActivation() {
  if (priority_[PENDING_TREE].required_for_activation)
    return;

  priority_[PENDING_TREE].required_for_activation = true;
  tile_manager_->DidChangeTilePriority(this);
}

scoped_ptr<base::Value> Tile::AsValue() const {
  scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), res.get(), "cc::Tile", this);
  res->Set("picture_pile",
           TracedValue::CreateIDRef(picture_pile_.get()).release());
  res->SetDouble("contents_scale", contents_scale_);
  res->Set("content_rect", MathUtil::AsValue(content_rect_).release());
  res->SetInteger("layer_id", layer_id_);
  res->Set("active_priority", priority_[ACTIVE_TREE].AsValue().release());
  res->Set("pending_priority", priority_[PENDING_TREE].AsValue().release());
  res->Set("managed_state", managed_state_.AsValue().release());
  res->SetBoolean("use_picture_analysis", use_picture_analysis());
  return res.PassAs<base::Value>();
}

size_t Tile::GPUMemoryUsageInBytes() const {
  size_t total_size = 0;
  for (int mode = 0; mode < NUM_RASTER_MODES; ++mode)
    total_size += managed_state_.tile_versions[mode].GPUMemoryUsageInBytes();
  return total_size;
}

RasterMode Tile::DetermineRasterModeForTree(WhichTree tree) const {
  return DetermineRasterModeForResolution(priority(tree).resolution);
}

RasterMode Tile::DetermineOverallRasterMode() const {
  return DetermineRasterModeForResolution(managed_state_.resolution);
}

RasterMode Tile::DetermineRasterModeForResolution(
    TileResolution resolution) const {
  RasterMode current_mode = managed_state_.raster_mode;
  RasterMode raster_mode = resolution == LOW_RESOLUTION
                               ? LOW_QUALITY_RASTER_MODE
                               : HIGH_QUALITY_RASTER_MODE;
  return std::min(raster_mode, current_mode);
}

}  // namespace cc
