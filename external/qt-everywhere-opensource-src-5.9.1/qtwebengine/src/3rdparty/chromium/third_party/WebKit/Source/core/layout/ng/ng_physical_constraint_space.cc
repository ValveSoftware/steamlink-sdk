// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_constraint_space.h"

namespace blink {

NGPhysicalConstraintSpace::NGPhysicalConstraintSpace(
    NGPhysicalSize available_size,
    NGPhysicalSize percentage_resolution_size,
    bool fixed_width,
    bool fixed_height,
    bool width_direction_triggers_scrollbar,
    bool height_direction_triggers_scrollbar,
    NGFragmentationType width_direction_fragmentation_type,
    NGFragmentationType height_direction_fragmentation_type,
    bool is_new_fc)
    : available_size_(available_size),
      percentage_resolution_size_(percentage_resolution_size),
      fixed_width_(fixed_width),
      fixed_height_(fixed_height),
      width_direction_triggers_scrollbar_(width_direction_triggers_scrollbar),
      height_direction_triggers_scrollbar_(height_direction_triggers_scrollbar),
      width_direction_fragmentation_type_(width_direction_fragmentation_type),
      height_direction_fragmentation_type_(height_direction_fragmentation_type),
      is_new_fc_(is_new_fc),
      last_left_float_exclusion_(nullptr),
      last_right_float_exclusion_(nullptr) {}

void NGPhysicalConstraintSpace::AddExclusion(const NGExclusion& exclusion) {
  NGExclusion* exclusion_ptr = new NGExclusion(exclusion);
  exclusions_.append(WTF::wrapUnique(exclusion_ptr));
  if (exclusion.type == NGExclusion::NG_FLOAT_LEFT) {
    last_left_float_exclusion_ = exclusions_.rbegin()->get();
  } else if (exclusion.type == NGExclusion::NG_FLOAT_RIGHT) {
    last_right_float_exclusion_ = exclusions_.rbegin()->get();
  }
}

const Vector<std::unique_ptr<const NGExclusion>>&
NGPhysicalConstraintSpace::Exclusions(unsigned options) const {
  // TODO(layout-ng): Filter based on options? Perhaps layout Opportunities
  // should filter instead?
  return exclusions_;
}

}  // namespace blink
