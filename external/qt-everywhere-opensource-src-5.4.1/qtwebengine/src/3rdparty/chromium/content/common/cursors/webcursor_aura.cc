// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "ui/base/cursor/cursor.h"

using blink::WebCursorInfo;

namespace content {

gfx::NativeCursor WebCursor::GetNativeCursor() {
  switch (type_) {
    case WebCursorInfo::TypePointer:
      return ui::kCursorPointer;
    case WebCursorInfo::TypeCross:
      return ui::kCursorCross;
    case WebCursorInfo::TypeHand:
      return ui::kCursorHand;
    case WebCursorInfo::TypeIBeam:
      return ui::kCursorIBeam;
    case WebCursorInfo::TypeWait:
      return ui::kCursorWait;
    case WebCursorInfo::TypeHelp:
      return ui::kCursorHelp;
    case WebCursorInfo::TypeEastResize:
      return ui::kCursorEastResize;
    case WebCursorInfo::TypeNorthResize:
      return ui::kCursorNorthResize;
    case WebCursorInfo::TypeNorthEastResize:
      return ui::kCursorNorthEastResize;
    case WebCursorInfo::TypeNorthWestResize:
      return ui::kCursorNorthWestResize;
    case WebCursorInfo::TypeSouthResize:
      return ui::kCursorSouthResize;
    case WebCursorInfo::TypeSouthEastResize:
      return ui::kCursorSouthEastResize;
    case WebCursorInfo::TypeSouthWestResize:
      return ui::kCursorSouthWestResize;
    case WebCursorInfo::TypeWestResize:
      return ui::kCursorWestResize;
    case WebCursorInfo::TypeNorthSouthResize:
      return ui::kCursorNorthSouthResize;
    case WebCursorInfo::TypeEastWestResize:
      return ui::kCursorEastWestResize;
    case WebCursorInfo::TypeNorthEastSouthWestResize:
      return ui::kCursorNorthEastSouthWestResize;
    case WebCursorInfo::TypeNorthWestSouthEastResize:
      return ui::kCursorNorthWestSouthEastResize;
    case WebCursorInfo::TypeColumnResize:
      return ui::kCursorColumnResize;
    case WebCursorInfo::TypeRowResize:
      return ui::kCursorRowResize;
    case WebCursorInfo::TypeMiddlePanning:
      return ui::kCursorMiddlePanning;
    case WebCursorInfo::TypeEastPanning:
      return ui::kCursorEastPanning;
    case WebCursorInfo::TypeNorthPanning:
      return ui::kCursorNorthPanning;
    case WebCursorInfo::TypeNorthEastPanning:
      return ui::kCursorNorthEastPanning;
    case WebCursorInfo::TypeNorthWestPanning:
      return ui::kCursorNorthWestPanning;
    case WebCursorInfo::TypeSouthPanning:
      return ui::kCursorSouthPanning;
    case WebCursorInfo::TypeSouthEastPanning:
      return ui::kCursorSouthEastPanning;
    case WebCursorInfo::TypeSouthWestPanning:
      return ui::kCursorSouthWestPanning;
    case WebCursorInfo::TypeWestPanning:
      return ui::kCursorWestPanning;
    case WebCursorInfo::TypeMove:
      return ui::kCursorMove;
    case WebCursorInfo::TypeVerticalText:
      return ui::kCursorVerticalText;
    case WebCursorInfo::TypeCell:
      return ui::kCursorCell;
    case WebCursorInfo::TypeContextMenu:
      return ui::kCursorContextMenu;
    case WebCursorInfo::TypeAlias:
      return ui::kCursorAlias;
    case WebCursorInfo::TypeProgress:
      return ui::kCursorProgress;
    case WebCursorInfo::TypeNoDrop:
      return ui::kCursorNoDrop;
    case WebCursorInfo::TypeCopy:
      return ui::kCursorCopy;
    case WebCursorInfo::TypeNone:
      return ui::kCursorNone;
    case WebCursorInfo::TypeNotAllowed:
      return ui::kCursorNotAllowed;
    case WebCursorInfo::TypeZoomIn:
      return ui::kCursorZoomIn;
    case WebCursorInfo::TypeZoomOut:
      return ui::kCursorZoomOut;
    case WebCursorInfo::TypeGrab:
      return ui::kCursorGrab;
    case WebCursorInfo::TypeGrabbing:
      return ui::kCursorGrabbing;
    case WebCursorInfo::TypeCustom: {
      ui::Cursor cursor(ui::kCursorCustom);
      cursor.SetPlatformCursor(GetPlatformCursor());
      return cursor;
    }
    default:
      NOTREACHED();
      return gfx::kNullCursor;
  }
}

}  // namespace content
