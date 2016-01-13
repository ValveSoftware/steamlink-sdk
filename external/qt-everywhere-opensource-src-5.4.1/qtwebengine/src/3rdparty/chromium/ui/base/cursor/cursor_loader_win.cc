// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader_win.h"

#include "base/lazy_instance.h"
#include "base/strings/string16.h"
#include "grit/ui_unscaled_resources.h"
#include "ui/base/cursor/cursor.h"

namespace ui {

namespace {

base::LazyInstance<base::string16> g_cursor_resource_module_name;

const wchar_t* GetCursorId(gfx::NativeCursor native_cursor) {
  switch (native_cursor.native_type()) {
    case kCursorNull:
      return IDC_ARROW;
    case kCursorPointer:
      return IDC_ARROW;
    case kCursorCross:
      return IDC_CROSS;
    case kCursorHand:
      return IDC_HAND;
    case kCursorIBeam:
      return IDC_IBEAM;
    case kCursorWait:
      return IDC_WAIT;
    case kCursorHelp:
      return IDC_HELP;
    case kCursorEastResize:
      return IDC_SIZEWE;
    case kCursorNorthResize:
      return IDC_SIZENS;
    case kCursorNorthEastResize:
      return IDC_SIZENESW;
    case kCursorNorthWestResize:
      return IDC_SIZENWSE;
    case kCursorSouthResize:
      return IDC_SIZENS;
    case kCursorSouthEastResize:
      return IDC_SIZENWSE;
    case kCursorSouthWestResize:
      return IDC_SIZENESW;
    case kCursorWestResize:
      return IDC_SIZEWE;
    case kCursorNorthSouthResize:
      return IDC_SIZENS;
    case kCursorEastWestResize:
      return IDC_SIZEWE;
    case kCursorNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case kCursorNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case kCursorMove:
      return IDC_SIZEALL;
    case kCursorProgress:
      return IDC_APPSTARTING;
    case kCursorNoDrop:
      return IDC_NO;
    case kCursorNotAllowed:
      return IDC_NO;
    case kCursorColumnResize:
      return MAKEINTRESOURCE(IDC_COLRESIZE);
    case kCursorRowResize:
      return MAKEINTRESOURCE(IDC_ROWRESIZE);
    case kCursorMiddlePanning:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE);
    case kCursorEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_EAST);
    case kCursorNorthPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH);
    case kCursorNorthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_EAST);
    case kCursorNorthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_WEST);
    case kCursorSouthPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH);
    case kCursorSouthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_EAST);
    case kCursorSouthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_WEST);
    case kCursorWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_WEST);
    case kCursorVerticalText:
      return MAKEINTRESOURCE(IDC_VERTICALTEXT);
    case kCursorCell:
      return MAKEINTRESOURCE(IDC_CELL);
    case kCursorZoomIn:
      return MAKEINTRESOURCE(IDC_ZOOMIN);
    case kCursorZoomOut:
      return MAKEINTRESOURCE(IDC_ZOOMOUT);
    case kCursorGrab:
      return MAKEINTRESOURCE(IDC_HAND_GRAB);
    case kCursorGrabbing:
      return MAKEINTRESOURCE(IDC_HAND_GRABBING);
    case kCursorCopy:
      return MAKEINTRESOURCE(IDC_COPYCUR);
    case kCursorAlias:
      return MAKEINTRESOURCE(IDC_ALIAS);
    case kCursorNone:
      return MAKEINTRESOURCE(IDC_CURSOR_NONE);
    case kCursorContextMenu:
    case kCursorCustom:
      NOTIMPLEMENTED();
      return IDC_ARROW;
    default:
      NOTREACHED();
      return IDC_ARROW;
  }
}

}  // namespace

CursorLoader* CursorLoader::Create() {
  return new CursorLoaderWin;
}

CursorLoaderWin::CursorLoaderWin() {
}

CursorLoaderWin::~CursorLoaderWin() {
}

void CursorLoaderWin::LoadImageCursor(int id,
                                      int resource_id,
                                      const gfx::Point& hot) {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::LoadAnimatedCursor(int id,
                                         int resource_id,
                                         const gfx::Point& hot,
                                         int frame_delay_ms) {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::UnloadAll() {
  // NOTIMPLEMENTED();
}

void CursorLoaderWin::SetPlatformCursor(gfx::NativeCursor* cursor) {
  if (cursor->native_type() != kCursorCustom) {
    if (cursor->platform()) {
      cursor->SetPlatformCursor(cursor->platform());
    } else {
      const wchar_t* cursor_id = GetCursorId(*cursor);
      PlatformCursor platform_cursor = LoadCursor(NULL, cursor_id);
      if (!platform_cursor && !g_cursor_resource_module_name.Get().empty()) {
        platform_cursor = LoadCursor(
            GetModuleHandle(g_cursor_resource_module_name.Get().c_str()),
                            cursor_id);
      }
      cursor->SetPlatformCursor(platform_cursor);
    }
  }
}

// static
void CursorLoaderWin::SetCursorResourceModule(
    const base::string16& module_name) {
  g_cursor_resource_module_name.Get() = module_name;
}

}  // namespace ui
