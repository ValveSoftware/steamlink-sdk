// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursors_aura.h"

#include <stddef.h>

#include "base/macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"

#if defined(OS_WIN)
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/gfx/icon_util.h"
#endif

namespace ui {
namespace {

struct HotPoint {
  int x;
  int y;
};

struct CursorData {
  int id;
  int resource_id;
  HotPoint hot_1x;
  HotPoint hot_2x;
};

struct CursorSet {
  const CursorSetType id;
  const CursorData* cursors;
  const int length;
  const CursorData* animated_cursors;
  const int animated_length;
};

const CursorData kNormalCursors[] = {
  {ui::kCursorNull, IDR_AURA_CURSOR_PTR, {4, 4}, {7, 7}},
  {ui::kCursorPointer, IDR_AURA_CURSOR_PTR, {4, 4}, {7, 7}},
  {ui::kCursorNoDrop, IDR_AURA_CURSOR_NO_DROP, {9, 9}, {18, 18}},
  {ui::kCursorNotAllowed, IDR_AURA_CURSOR_NO_DROP, {9, 9}, {18, 18}},
  {ui::kCursorCopy, IDR_AURA_CURSOR_COPY, {9, 9}, {18, 18}},
  {ui::kCursorHand, IDR_AURA_CURSOR_HAND, {9, 4}, {19, 8}},
  {ui::kCursorMove, IDR_AURA_CURSOR_MOVE, {11, 11}, {23, 23}},
  {ui::kCursorNorthEastResize, IDR_AURA_CURSOR_NORTH_EAST_RESIZE,
   {12, 11}, {25, 23}},
  {ui::kCursorSouthWestResize, IDR_AURA_CURSOR_SOUTH_WEST_RESIZE,
   {12, 11}, {25, 23}},
  {ui::kCursorSouthEastResize, IDR_AURA_CURSOR_SOUTH_EAST_RESIZE,
   {11, 11}, {24, 23}},
  {ui::kCursorNorthWestResize, IDR_AURA_CURSOR_NORTH_WEST_RESIZE,
   {11, 11}, {24, 23}},
  {ui::kCursorNorthResize, IDR_AURA_CURSOR_NORTH_RESIZE, {11, 12}, {23, 23}},
  {ui::kCursorSouthResize, IDR_AURA_CURSOR_SOUTH_RESIZE, {11, 12}, {23, 23}},
  {ui::kCursorEastResize, IDR_AURA_CURSOR_EAST_RESIZE, {12, 11}, {25, 23}},
  {ui::kCursorWestResize, IDR_AURA_CURSOR_WEST_RESIZE, {12, 11}, {25, 23}},
  {ui::kCursorIBeam, IDR_AURA_CURSOR_IBEAM, {12, 12}, {24, 25}},
  {ui::kCursorAlias, IDR_AURA_CURSOR_ALIAS, {8, 6}, {15, 11}},
  {ui::kCursorCell, IDR_AURA_CURSOR_CELL, {11, 11}, {24, 23}},
  {ui::kCursorContextMenu, IDR_AURA_CURSOR_CONTEXT_MENU, {4, 4}, {8, 9}},
  {ui::kCursorCross, IDR_AURA_CURSOR_CROSSHAIR, {12, 12}, {25, 23}},
  {ui::kCursorHelp, IDR_AURA_CURSOR_HELP, {4, 4}, {8, 9}},
  {ui::kCursorVerticalText, IDR_AURA_CURSOR_XTERM_HORIZ, {12, 11}, {26, 23}},
  {ui::kCursorZoomIn, IDR_AURA_CURSOR_ZOOM_IN, {10, 10}, {20, 20}},
  {ui::kCursorZoomOut, IDR_AURA_CURSOR_ZOOM_OUT, {10, 10}, {20, 20}},
  {ui::kCursorRowResize, IDR_AURA_CURSOR_ROW_RESIZE, {11, 12}, {23, 23}},
  {ui::kCursorColumnResize, IDR_AURA_CURSOR_COL_RESIZE, {12, 11}, {25, 23}},
  {ui::kCursorEastWestResize, IDR_AURA_CURSOR_EAST_WEST_RESIZE,
   {12, 11}, {25, 23}},
  {ui::kCursorNorthSouthResize, IDR_AURA_CURSOR_NORTH_SOUTH_RESIZE,
   {11, 12}, {23, 23}},
  {ui::kCursorNorthEastSouthWestResize,
   IDR_AURA_CURSOR_NORTH_EAST_SOUTH_WEST_RESIZE, {12, 11}, {25, 23}},
  {ui::kCursorNorthWestSouthEastResize,
   IDR_AURA_CURSOR_NORTH_WEST_SOUTH_EAST_RESIZE, {11, 11}, {24, 23}},
  {ui::kCursorGrab, IDR_AURA_CURSOR_GRAB, {8, 5}, {16, 10}},
  {ui::kCursorGrabbing, IDR_AURA_CURSOR_GRABBING, {9, 9}, {18, 18}},
};

const CursorData kLargeCursors[] = {
  // The 2x hotspots should be double of the 1x, even though the cursors are
  // shown as same size as 1x (64x64), because in 2x dpi screen, the 1x large
  // cursor assets (64x64) are internally enlarged to the double size (128x128)
  // by ResourceBundleImageSource.
  {ui::kCursorNull, IDR_AURA_CURSOR_BIG_PTR, {10, 10}, {20, 20}},
  {ui::kCursorPointer, IDR_AURA_CURSOR_BIG_PTR, {10, 10}, {20, 20}},
  {ui::kCursorNoDrop, IDR_AURA_CURSOR_BIG_NO_DROP, {10, 10}, {20, 20}},
  {ui::kCursorNotAllowed, IDR_AURA_CURSOR_BIG_NO_DROP, {10, 10}, {20, 20}},
  {ui::kCursorCopy, IDR_AURA_CURSOR_BIG_COPY, {10, 10}, {20, 20}},
  {ui::kCursorHand, IDR_AURA_CURSOR_BIG_HAND, {25, 7}, {50, 14}},
  {ui::kCursorMove, IDR_AURA_CURSOR_BIG_MOVE, {32, 31}, {64, 62}},
  {ui::kCursorNorthEastResize, IDR_AURA_CURSOR_BIG_NORTH_EAST_RESIZE,
   {31, 28}, {62, 56}},
  {ui::kCursorSouthWestResize, IDR_AURA_CURSOR_BIG_SOUTH_WEST_RESIZE,
   {31, 28}, {62, 56}},
  {ui::kCursorSouthEastResize, IDR_AURA_CURSOR_BIG_SOUTH_EAST_RESIZE,
   {28, 28}, {56, 56}},
  {ui::kCursorNorthWestResize, IDR_AURA_CURSOR_BIG_NORTH_WEST_RESIZE,
   {28, 28}, {56, 56}},
  {ui::kCursorNorthResize, IDR_AURA_CURSOR_BIG_NORTH_RESIZE,
   {29, 32}, {58, 64}},
  {ui::kCursorSouthResize, IDR_AURA_CURSOR_BIG_SOUTH_RESIZE,
   {29, 32}, {58, 64}},
  {ui::kCursorEastResize, IDR_AURA_CURSOR_BIG_EAST_RESIZE, {35, 29}, {70, 58}},
  {ui::kCursorWestResize, IDR_AURA_CURSOR_BIG_WEST_RESIZE, {35, 29}, {70, 58}},
  {ui::kCursorIBeam, IDR_AURA_CURSOR_BIG_IBEAM, {30, 32}, {60, 64}},
  {ui::kCursorAlias, IDR_AURA_CURSOR_BIG_ALIAS, {19, 11}, {38, 22}},
  {ui::kCursorCell, IDR_AURA_CURSOR_BIG_CELL, {30, 30}, {60, 60}},
  {ui::kCursorContextMenu, IDR_AURA_CURSOR_BIG_CONTEXT_MENU,
   {11, 11}, {22, 22}},
  {ui::kCursorCross, IDR_AURA_CURSOR_BIG_CROSSHAIR, {31, 30}, {62, 60}},
  {ui::kCursorHelp, IDR_AURA_CURSOR_BIG_HELP, {10, 11}, {20, 22}},
  {ui::kCursorVerticalText, IDR_AURA_CURSOR_BIG_XTERM_HORIZ,
   {32, 30}, {64, 60}},
  {ui::kCursorZoomIn, IDR_AURA_CURSOR_BIG_ZOOM_IN, {25, 26}, {50, 52}},
  {ui::kCursorZoomOut, IDR_AURA_CURSOR_BIG_ZOOM_OUT, {26, 26}, {52, 52}},
  {ui::kCursorRowResize, IDR_AURA_CURSOR_BIG_ROW_RESIZE, {29, 32}, {58, 64}},
  {ui::kCursorColumnResize, IDR_AURA_CURSOR_BIG_COL_RESIZE, {35, 29}, {70, 58}},
  {ui::kCursorEastWestResize, IDR_AURA_CURSOR_BIG_EAST_WEST_RESIZE,
   {35, 29}, {70, 58}},
  {ui::kCursorNorthSouthResize, IDR_AURA_CURSOR_BIG_NORTH_SOUTH_RESIZE,
   {29, 32}, {58, 64}},
  {ui::kCursorNorthEastSouthWestResize,
   IDR_AURA_CURSOR_BIG_NORTH_EAST_SOUTH_WEST_RESIZE, {32, 30}, {64, 60}},
  {ui::kCursorNorthWestSouthEastResize,
   IDR_AURA_CURSOR_BIG_NORTH_WEST_SOUTH_EAST_RESIZE, {32, 31}, {64, 62}},
  {ui::kCursorGrab, IDR_AURA_CURSOR_BIG_GRAB, {21, 11}, {42, 22}},
  {ui::kCursorGrabbing, IDR_AURA_CURSOR_BIG_GRABBING, {20, 12}, {40, 24}},
};

const CursorData kAnimatedCursors[] = {
  {ui::kCursorWait, IDR_AURA_CURSOR_THROBBER, {7, 7}, {14, 14}},
  {ui::kCursorProgress, IDR_AURA_CURSOR_THROBBER, {7, 7}, {14, 14}},
};

const CursorSet kCursorSets[] = {
  {
    CURSOR_SET_NORMAL,
    kNormalCursors, arraysize(kNormalCursors),
    kAnimatedCursors, arraysize(kAnimatedCursors)
  },
  {
    CURSOR_SET_LARGE,
    kLargeCursors, arraysize(kLargeCursors),
    // TODO(yoshiki): Replace animated cursors with big assets. crbug.com/247254
    kAnimatedCursors, arraysize(kAnimatedCursors)
  },
};

const CursorSet* GetCursorSetByType(CursorSetType cursor_set_id) {
  for (size_t i = 0; i < arraysize(kCursorSets); ++i) {
    if (kCursorSets[i].id == cursor_set_id)
      return &kCursorSets[i];
  }

  return NULL;
}

bool SearchTable(const CursorData* table,
                 size_t table_length,
                 int id,
                 float scale_factor,
                 int* resource_id,
                 gfx::Point* point) {
  bool resource_2x_available =
      ResourceBundle::GetSharedInstance().GetMaxScaleFactor() ==
      SCALE_FACTOR_200P;
  for (size_t i = 0; i < table_length; ++i) {
    if (table[i].id == id) {
      *resource_id = table[i].resource_id;
      *point = scale_factor == 1.0f || !resource_2x_available ?
               gfx::Point(table[i].hot_1x.x, table[i].hot_1x.y) :
               gfx::Point(table[i].hot_2x.x, table[i].hot_2x.y);
      return true;
    }
  }

  return false;
}

}  // namespace

bool GetCursorDataFor(CursorSetType cursor_set_id,
                      int id,
                      float scale_factor,
                      int* resource_id,
                      gfx::Point* point) {
  const CursorSet* cursor_set = GetCursorSetByType(cursor_set_id);
  if (cursor_set &&
      SearchTable(cursor_set->cursors,
                  cursor_set->length,
                  id, scale_factor, resource_id, point)) {
      return true;
  }

  // Falls back to the default cursor set.
  cursor_set = GetCursorSetByType(ui::CURSOR_SET_NORMAL);
  DCHECK(cursor_set);
  return SearchTable(cursor_set->cursors,
                     cursor_set->length,
                     id, scale_factor, resource_id, point);
}

bool GetAnimatedCursorDataFor(CursorSetType cursor_set_id,
                              int id,
                              float scale_factor,
                              int* resource_id,
                              gfx::Point* point) {
  const CursorSet* cursor_set = GetCursorSetByType(cursor_set_id);
  if (cursor_set &&
      SearchTable(cursor_set->animated_cursors,
                  cursor_set->animated_length,
                  id, scale_factor, resource_id, point)) {
    return true;
  }

  // Falls back to the default cursor set.
  cursor_set = GetCursorSetByType(ui::CURSOR_SET_NORMAL);
  DCHECK(cursor_set);
  return SearchTable(cursor_set->animated_cursors,
                     cursor_set->animated_length,
                     id, scale_factor, resource_id, point);
}

bool GetCursorBitmap(const Cursor& cursor,
                     SkBitmap* bitmap,
                     gfx::Point* point) {
  DCHECK(bitmap && point);
#if defined(OS_WIN)
  Cursor cursor_copy = cursor;
  ui::CursorLoaderWin cursor_loader;
  cursor_loader.SetPlatformCursor(&cursor_copy);
  const std::unique_ptr<SkBitmap> cursor_bitmap(
      IconUtil::CreateSkBitmapFromHICON(cursor_copy.platform()));
#else
  int resource_id;
  if (!GetCursorDataFor(ui::CURSOR_SET_NORMAL,
                        cursor.native_type(),
                        cursor.device_scale_factor(),
                        &resource_id,
                        point)) {
    return false;
  }

  const SkBitmap* cursor_bitmap = ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(resource_id)->bitmap();
#endif
  if (!cursor_bitmap)
    return false;
  *bitmap = *cursor_bitmap;
  return true;
}

}  // namespace ui
