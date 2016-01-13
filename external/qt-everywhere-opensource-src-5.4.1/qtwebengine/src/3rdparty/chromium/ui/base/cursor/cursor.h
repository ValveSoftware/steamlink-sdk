// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_H_
#define UI_BASE_CURSOR_CURSOR_H_

#include "build/build_config.h"
#include "ui/base/ui_base_export.h"

namespace gfx {
class Point;
class Size;
}

#if defined(OS_WIN)
typedef struct HINSTANCE__* HINSTANCE;
typedef struct HICON__* HICON;
typedef HICON HCURSOR;
#endif

namespace ui {

#if defined(OS_WIN)
typedef ::HCURSOR PlatformCursor;
#elif defined(USE_X11)
typedef unsigned long PlatformCursor;
#else
typedef void* PlatformCursor;
#endif

// TODO(jamescook): Once we're on C++0x we could change these constants
// to an enum and forward declare it in native_widget_types.h.

// Equivalent to a NULL HCURSOR on Windows.
const int kCursorNull = 0;

// These cursors mirror WebKit cursors from WebCursorInfo, but are replicated
// here so we don't introduce a WebKit dependency.
const int kCursorPointer = 1;
const int kCursorCross = 2;
const int kCursorHand = 3;
const int kCursorIBeam = 4;
const int kCursorWait = 5;
const int kCursorHelp = 6;
const int kCursorEastResize = 7;
const int kCursorNorthResize = 8;
const int kCursorNorthEastResize = 9;
const int kCursorNorthWestResize = 10;
const int kCursorSouthResize = 11;
const int kCursorSouthEastResize = 12;
const int kCursorSouthWestResize = 13;
const int kCursorWestResize = 14;
const int kCursorNorthSouthResize = 15;
const int kCursorEastWestResize = 16;
const int kCursorNorthEastSouthWestResize = 17;
const int kCursorNorthWestSouthEastResize = 18;
const int kCursorColumnResize = 19;
const int kCursorRowResize = 20;
const int kCursorMiddlePanning = 21;
const int kCursorEastPanning = 22;
const int kCursorNorthPanning = 23;
const int kCursorNorthEastPanning = 24;
const int kCursorNorthWestPanning = 25;
const int kCursorSouthPanning = 26;
const int kCursorSouthEastPanning = 27;
const int kCursorSouthWestPanning = 28;
const int kCursorWestPanning = 29;
const int kCursorMove = 30;
const int kCursorVerticalText = 31;
const int kCursorCell = 32;
const int kCursorContextMenu = 33;
const int kCursorAlias = 34;
const int kCursorProgress = 35;
const int kCursorNoDrop = 36;
const int kCursorCopy = 37;
const int kCursorNone = 38;
const int kCursorNotAllowed = 39;
const int kCursorZoomIn = 40;
const int kCursorZoomOut = 41;
const int kCursorGrab = 42;
const int kCursorGrabbing = 43;
const int kCursorCustom = 44;

enum CursorSetType {
  CURSOR_SET_NORMAL,
  CURSOR_SET_LARGE
};

// Ref-counted cursor that supports both default and custom cursors.
class UI_BASE_EXPORT Cursor {
 public:
  Cursor();

  // Implicit constructor.
  Cursor(int type);

  // Allow copy.
  Cursor(const Cursor& cursor);

  ~Cursor();

  void SetPlatformCursor(const PlatformCursor& platform);

  void RefCustomCursor();
  void UnrefCustomCursor();

  int native_type() const { return native_type_; }
  PlatformCursor platform() const { return platform_cursor_; }
  float device_scale_factor() const {
    return device_scale_factor_;
  }
  void set_device_scale_factor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }

  bool operator==(int type) const { return native_type_ == type; }
  bool operator==(const Cursor& cursor) const {
    return native_type_ == cursor.native_type_ &&
           platform_cursor_ == cursor.platform_cursor_ &&
           device_scale_factor_ == cursor.device_scale_factor_;
  }
  bool operator!=(int type) const { return native_type_ != type; }
  bool operator!=(const Cursor& cursor) const {
    return native_type_ != cursor.native_type_ ||
           platform_cursor_ != cursor.platform_cursor_ ||
           device_scale_factor_ != cursor.device_scale_factor_;
  }

  void operator=(const Cursor& cursor) {
    Assign(cursor);
  }

 private:
  void Assign(const Cursor& cursor);

  // See definitions above.
  int native_type_;

  PlatformCursor platform_cursor_;

  // The device scale factor for the cursor.
  float device_scale_factor_;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_H_
