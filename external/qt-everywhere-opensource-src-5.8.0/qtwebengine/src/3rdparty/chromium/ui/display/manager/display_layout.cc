// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_layout.h"

#include <algorithm>
#include <set>
#include <sstream>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/insets.h"

namespace display {
namespace  {

// DisplayPlacement Positions
const char kTop[] = "top";
const char kRight[] = "right";
const char kBottom[] = "bottom";
const char kLeft[] = "left";
const char kUnknown[] = "unknown";

// The maximum value for 'offset' in DisplayLayout in case of outliers.  Need
// to change this value in case to support even larger displays.
const int kMaxValidOffset = 10000;

bool IsIdInList(int64_t id, const DisplayIdList& list) {
  const auto iter =
      std::find_if(list.begin(), list.end(),
                   [id](int64_t display_id) { return display_id == id; });
  return iter != list.end();
}

display::Display* FindDisplayById(DisplayList* display_list, int64_t id) {
  auto iter = std::find_if(
      display_list->begin(), display_list->end(),
      [id](const display::Display& display) { return display.id() == id; });
  return &(*iter);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DisplayPlacement

DisplayPlacement::DisplayPlacement()
    : DisplayPlacement(display::Display::kInvalidDisplayID,
                       display::Display::kInvalidDisplayID,
                       DisplayPlacement::RIGHT,
                       0,
                       DisplayPlacement::TOP_LEFT) {}

DisplayPlacement::DisplayPlacement(Position position, int offset)
    : DisplayPlacement(display::Display::kInvalidDisplayID,
                       display::Display::kInvalidDisplayID,
                       position,
                       offset,
                       DisplayPlacement::TOP_LEFT) {}

DisplayPlacement::DisplayPlacement(Position position,
                                   int offset,
                                   OffsetReference offset_reference)
    : DisplayPlacement(display::Display::kInvalidDisplayID,
                       display::Display::kInvalidDisplayID,
                       position,
                       offset,
                       offset_reference) {}

DisplayPlacement::DisplayPlacement(int64_t display_id,
                                   int64_t parent_display_id,
                                   Position position,
                                   int offset,
                                   OffsetReference offset_reference)
    : display_id(display_id),
      parent_display_id(parent_display_id),
      position(position),
      offset(offset),
      offset_reference(offset_reference) {
  DCHECK_LE(TOP, position);
  DCHECK_GE(LEFT, position);
  // Set the default value to |position| in case position is invalid.  DCHECKs
  // above doesn't stop in Release builds.
  if (TOP > position || LEFT < position)
    this->position = RIGHT;

  DCHECK_GE(kMaxValidOffset, abs(offset));
}

DisplayPlacement::DisplayPlacement(const DisplayPlacement& placement)
    : display_id(placement.display_id),
      parent_display_id(placement.parent_display_id),
      position(placement.position),
      offset(placement.offset),
      offset_reference(placement.offset_reference) {}

DisplayPlacement& DisplayPlacement::Swap() {
  switch (position) {
    case TOP:
      position = BOTTOM;
      break;
    case BOTTOM:
      position = TOP;
      break;
    case RIGHT:
      position = LEFT;
      break;
    case LEFT:
      position = RIGHT;
      break;
  }
  offset = -offset;
  std::swap(display_id, parent_display_id);
  return *this;
}

std::string DisplayPlacement::ToString() const {
  std::stringstream s;
  if (display_id != display::Display::kInvalidDisplayID)
    s << "id=" << display_id << ", ";
  if (parent_display_id != display::Display::kInvalidDisplayID)
    s << "parent=" << parent_display_id << ", ";
  s << PositionToString(position) << ", ";
  s << offset;
  return s.str();
}

// static
std::string DisplayPlacement::PositionToString(Position position) {
  switch (position) {
    case TOP:
      return kTop;
    case RIGHT:
      return kRight;
    case BOTTOM:
      return kBottom;
    case LEFT:
      return kLeft;
  }
  return kUnknown;
}

// static
bool DisplayPlacement::StringToPosition(const base::StringPiece& string,
                                        Position* position) {
  if (string == kTop) {
    *position = TOP;
    return true;
  }

  if (string == kRight) {
    *position = RIGHT;
    return true;
  }

  if (string == kBottom) {
    *position = BOTTOM;
    return true;
  }

  if (string == kLeft) {
    *position = LEFT;
    return true;
  }

  LOG(ERROR) << "Invalid position value:" << string;

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// DisplayLayout

DisplayLayout::DisplayLayout()
    : mirrored(false),
      default_unified(true),
      primary_id(display::Display::kInvalidDisplayID) {}

DisplayLayout::~DisplayLayout() {}

void DisplayLayout::ApplyToDisplayList(DisplayList* display_list,
                                       std::vector<int64_t>* updated_ids,
                                       int minimum_offset_overlap) const {
  // Layout from primary, then dependent displays.
  std::set<int64_t> parents;
  parents.insert(primary_id);
  while (parents.size()) {
    int64_t parent_id = *parents.begin();
    parents.erase(parent_id);
    for (const DisplayPlacement& placement : placement_list) {
      if (placement.parent_display_id == parent_id) {
        if (ApplyDisplayPlacement(placement,
                                  display_list,
                                  minimum_offset_overlap) &&
            updated_ids) {
          updated_ids->push_back(placement.display_id);
        }
        parents.insert(placement.display_id);
      }
    }
  }
}

// static
bool DisplayLayout::Validate(const DisplayIdList& list,
                             const DisplayLayout& layout) {
  // The primary display should be in the list.
  DCHECK(IsIdInList(layout.primary_id, list));

  // Unified mode, or mirror mode switched from unified mode,
  // may not have the placement yet.
  if (layout.placement_list.size() == 0u)
    return true;

  bool has_primary_as_parent = false;
  int64_t id = 0;

  for (const auto& placement : layout.placement_list) {
    // Placements are sorted by display_id.
    if (id >= placement.display_id) {
      LOG(ERROR) << "PlacementList must be sorted by display_id";
      return false;
    }
    if (placement.display_id == display::Display::kInvalidDisplayID) {
      LOG(ERROR) << "display_id is not initialized";
      return false;
    }
    if (placement.parent_display_id == display::Display::kInvalidDisplayID) {
      LOG(ERROR) << "display_parent_id is not initialized";
      return false;
    }
    if (placement.display_id == placement.parent_display_id) {
      LOG(ERROR) << "display_id must not be same as parent_display_id";
      return false;
    }
    if (!IsIdInList(placement.display_id, list)) {
      LOG(ERROR) << "display_id is not in the id list:" << placement.ToString();
      return false;
    }

    if (!IsIdInList(placement.parent_display_id, list)) {
      LOG(ERROR) << "parent_display_id is not in the id list:"
                 << placement.ToString();
      return false;
    }
    has_primary_as_parent |= layout.primary_id == placement.parent_display_id;
  }
  if (!has_primary_as_parent)
    LOG(ERROR) << "At least, one placement must have the primary as a parent.";
  return has_primary_as_parent;
}

std::unique_ptr<DisplayLayout> DisplayLayout::Copy() const {
  std::unique_ptr<DisplayLayout> copy(new DisplayLayout);
  for (const auto& placement : placement_list)
    copy->placement_list.push_back(placement);
  copy->mirrored = mirrored;
  copy->default_unified = default_unified;
  copy->primary_id = primary_id;
  return copy;
}

bool DisplayLayout::HasSamePlacementList(const DisplayLayout& layout) const {
  if (placement_list.size() != layout.placement_list.size())
    return false;
  for (size_t index = 0; index < placement_list.size(); index++) {
    const DisplayPlacement& placement1 = placement_list[index];
    const DisplayPlacement& placement2 = layout.placement_list[index];
    if (placement1.position != placement2.position ||
        placement1.offset != placement2.offset ||
        placement1.display_id != placement2.display_id ||
        placement1.parent_display_id != placement2.parent_display_id) {
      return false;
    }
  }
  return true;
}

std::string DisplayLayout::ToString() const {
  std::stringstream s;
  s << "primary=" << primary_id;
  if (mirrored)
    s << ", mirrored";
  if (default_unified)
    s << ", default_unified";
  bool added = false;
  for (const auto& placement : placement_list) {
    s << (added ? "),(" : " [(");
    s << placement.ToString();
    added = true;
  }
  if (added)
    s << ")]";
  return s.str();
}

DisplayPlacement DisplayLayout::FindPlacementById(int64_t display_id) const {
  const auto iter =
      std::find_if(placement_list.begin(), placement_list.end(),
                   [display_id](const DisplayPlacement& placement) {
                     return placement.display_id == display_id;
                   });
  return (iter == placement_list.end()) ? DisplayPlacement()
                                        : DisplayPlacement(*iter);
}

// static
bool DisplayLayout::ApplyDisplayPlacement(const DisplayPlacement& placement,
                                          DisplayList* display_list,
                                          int minimum_offset_overlap) {
  const display::Display& parent_display =
      *FindDisplayById(display_list, placement.parent_display_id);
  DCHECK(parent_display.is_valid());
  display::Display* target_display =
      FindDisplayById(display_list, placement.display_id);
  gfx::Rect old_bounds(target_display->bounds());
  DCHECK(target_display);

  const gfx::Rect& parent_bounds = parent_display.bounds();
  const gfx::Rect& target_bounds = target_display->bounds();
  gfx::Point new_target_origin = parent_bounds.origin();

  DisplayPlacement::Position position = placement.position;

  // Ignore the offset in case the target display doesn't share edges with
  // the parent display.
  int offset = placement.offset;
  if (position == DisplayPlacement::TOP ||
      position == DisplayPlacement::BOTTOM) {
    if (placement.offset_reference == DisplayPlacement::BOTTOM_RIGHT)
      offset = parent_bounds.width() - offset - target_bounds.width();

    offset = std::min(
        offset, parent_bounds.width() - minimum_offset_overlap);
    offset = std::max(
        offset, -target_bounds.width() + minimum_offset_overlap);
  } else {
    if (placement.offset_reference == DisplayPlacement::BOTTOM_RIGHT)
      offset = parent_bounds.height() - offset - target_bounds.height();

    offset = std::min(
        offset, parent_bounds.height() - minimum_offset_overlap);
    offset = std::max(
        offset, -target_bounds.height() + minimum_offset_overlap);
  }
  switch (position) {
    case DisplayPlacement::TOP:
      new_target_origin.Offset(offset, -target_bounds.height());
      break;
    case DisplayPlacement::RIGHT:
      new_target_origin.Offset(parent_bounds.width(), offset);
      break;
    case DisplayPlacement::BOTTOM:
      new_target_origin.Offset(offset, parent_bounds.height());
      break;
    case DisplayPlacement::LEFT:
      new_target_origin.Offset(-target_bounds.width(), offset);
      break;
  }

  gfx::Insets insets = target_display->GetWorkAreaInsets();
  target_display->set_bounds(
      gfx::Rect(new_target_origin, target_bounds.size()));
  target_display->UpdateWorkAreaFromInsets(insets);

  return old_bounds != target_display->bounds();
}

}  // namespace display
