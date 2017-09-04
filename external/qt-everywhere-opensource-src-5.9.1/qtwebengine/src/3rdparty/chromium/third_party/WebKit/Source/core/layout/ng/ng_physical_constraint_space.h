// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalConstraintSpace_h
#define NGPhysicalConstraintSpace_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_units.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

// TODO(glebl@): unused, delete.
enum NGExclusionType {
  NGClearNone = 0,
  NGClearFloatLeft = 1,
  NGClearFloatRight = 2,
  NGClearFragment = 4
};

enum NGFragmentationType {
  FragmentNone,
  FragmentPage,
  FragmentColumn,
  FragmentRegion
};

// The NGPhysicalConstraintSpace contains the underlying data for the
// NGConstraintSpace. It is not meant to be used directly as all members are in
// the physical coordinate space. Instead NGConstraintSpace should be used.
class CORE_EXPORT NGPhysicalConstraintSpace final
    : public GarbageCollectedFinalized<NGPhysicalConstraintSpace> {
 public:
  NGPhysicalConstraintSpace(
      NGPhysicalSize available_size,
      NGPhysicalSize percentage_resolution_size,
      bool fixed_width,
      bool fixed_height,
      bool width_direction_triggers_scrollbar,
      bool height_direction_triggers_scrollbar,
      NGFragmentationType width_direction_fragmentation_type,
      NGFragmentationType height_direction_fragmentation_type,
      bool is_new_fc);

  void AddExclusion(const NGExclusion&);
  const Vector<std::unique_ptr<const NGExclusion>>& Exclusions(
      unsigned options = 0) const;

  // Read only getters.
  const NGExclusion* LastLeftFloatExclusion() const {
    return last_left_float_exclusion_;
  }

  const NGExclusion* LastRightFloatExclusion() const {
    return last_right_float_exclusion_;
  }

  DEFINE_INLINE_TRACE() {}

 private:
  friend class NGConstraintSpace;

  NGPhysicalSize available_size_;
  NGPhysicalSize percentage_resolution_size_;

  unsigned fixed_width_ : 1;
  unsigned fixed_height_ : 1;
  unsigned width_direction_triggers_scrollbar_ : 1;
  unsigned height_direction_triggers_scrollbar_ : 1;
  unsigned width_direction_fragmentation_type_ : 2;
  unsigned height_direction_fragmentation_type_ : 2;

  // Whether the current constraint space is for the newly established
  // formatting Context
  unsigned is_new_fc_ : 1;

  // Last left/right float exclusions are used to enforce the top edge alignment
  // rule for floats and for the support of CSS "clear" property.
  const NGExclusion* last_left_float_exclusion_;   // Owned by exclusions_.
  const NGExclusion* last_right_float_exclusion_;  // Owned by exclusions_.

  Vector<std::unique_ptr<const NGExclusion>> exclusions_;
};

}  // namespace blink

#endif  // NGPhysicalConstraintSpace_h
