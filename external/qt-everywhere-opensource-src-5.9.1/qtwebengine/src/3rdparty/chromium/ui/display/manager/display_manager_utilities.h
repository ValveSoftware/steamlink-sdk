// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_MANAGER_UTILITIES_H_
#define UI_DISPLAY_MANAGER_DISPLAY_MANAGER_UTILITIES_H_

#include <set>

#include "ui/display/display.h"
#include "ui/display/display_export.h"
#include "ui/display/manager/display_layout.h"
#include "ui/display/manager/managed_display_info.h"

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace display {
class ManagedDisplayInfo;
class ManagedDisplayMode;

// Creates the display mode list for internal display
// based on |native_mode|.
DISPLAY_EXPORT ManagedDisplayInfo::ManagedDisplayModeList
CreateInternalManagedDisplayModeList(
    const scoped_refptr<display::ManagedDisplayMode>& native_mode);

// Creates the display mode list for unified display
// based on |native_mode| and |scales|.
DISPLAY_EXPORT ManagedDisplayInfo::ManagedDisplayModeList
CreateUnifiedManagedDisplayModeList(
    const scoped_refptr<display::ManagedDisplayMode>& native_mode,
    const std::set<std::pair<float, float>>& dsf_scale_list);

// Gets the display mode for |resolution|. Returns false if no display
// mode matches the resolution, or the display is an internal display.
DISPLAY_EXPORT scoped_refptr<ManagedDisplayMode> GetDisplayModeForResolution(
    const ManagedDisplayInfo& info,
    const gfx::Size& resolution);

// Gets the display mode for the next valid UI scale. Returns false
// if the display is not an internal display.
DISPLAY_EXPORT scoped_refptr<ManagedDisplayMode> GetDisplayModeForNextUIScale(
    const ManagedDisplayInfo& info,
    bool up);

// Gets the display mode for the next valid resolution. Returns false
// if the display is an internal display.
DISPLAY_EXPORT scoped_refptr<ManagedDisplayMode>
GetDisplayModeForNextResolution(const ManagedDisplayInfo& info, bool up);

// Tests if the |info| has display mode that matches |ui_scale|.
bool HasDisplayModeForUIScale(const display::ManagedDisplayInfo& info,
                              float ui_scale);

// Computes the bounds that defines the bounds between two displays.
// Returns false if two displays do not intersect.
DISPLAY_EXPORT bool ComputeBoundary(const display::Display& primary_display,
                                    const display::Display& secondary_display,
                                    gfx::Rect* primary_edge_in_screen,
                                    gfx::Rect* secondary_edge_in_screen);

// Returns the index in the displays whose bounds contains |point_in_screen|.
// Returns -1 if no such display exist.
DISPLAY_EXPORT int FindDisplayIndexContainingPoint(
    const std::vector<display::Display>& displays,
    const gfx::Point& point_in_screen);

// Sorts id list using |CompareDisplayIds| below.
DISPLAY_EXPORT void SortDisplayIdList(display::DisplayIdList* list);

// Default id generator.
class DefaultDisplayIdGenerator {
 public:
  int64_t operator()(int64_t id) { return id; }
};

// Generate sorted display::DisplayIdList from iterators.
template <class ForwardIterator, class Generator = DefaultDisplayIdGenerator>
display::DisplayIdList GenerateDisplayIdList(
    ForwardIterator first,
    ForwardIterator last,
    Generator generator = Generator()) {
  display::DisplayIdList list;
  while (first != last) {
    list.push_back(generator(*first));
    ++first;
  }
  SortDisplayIdList(&list);
  return list;
}

// Creates sorted display::DisplayIdList.
DISPLAY_EXPORT display::DisplayIdList CreateDisplayIdList(
    const display::Displays& list);

DISPLAY_EXPORT std::string DisplayIdListToString(
    const display::DisplayIdList& list);

// Returns true if one of following conditions is met.
// 1) id1 is internal.
// 2) output index of id1 < output index of id2 and id2 isn't internal.
DISPLAY_EXPORT bool CompareDisplayIds(int64_t id1, int64_t id2);

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_MANAGER_UTILITIES_H_
