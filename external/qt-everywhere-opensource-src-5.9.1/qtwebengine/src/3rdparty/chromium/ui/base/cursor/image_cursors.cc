// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/image_cursors.h"

#include <float.h>
#include <stddef.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

namespace {

const int kImageCursorIds[] = {
  kCursorNull,
  kCursorPointer,
  kCursorNoDrop,
  kCursorNotAllowed,
  kCursorCopy,
  kCursorHand,
  kCursorMove,
  kCursorNorthEastResize,
  kCursorSouthWestResize,
  kCursorSouthEastResize,
  kCursorNorthWestResize,
  kCursorNorthResize,
  kCursorSouthResize,
  kCursorEastResize,
  kCursorWestResize,
  kCursorIBeam,
  kCursorAlias,
  kCursorCell,
  kCursorContextMenu,
  kCursorCross,
  kCursorHelp,
  kCursorVerticalText,
  kCursorZoomIn,
  kCursorZoomOut,
  kCursorRowResize,
  kCursorColumnResize,
  kCursorEastWestResize,
  kCursorNorthSouthResize,
  kCursorNorthEastSouthWestResize,
  kCursorNorthWestSouthEastResize,
  kCursorGrab,
  kCursorGrabbing,
};

const int kAnimatedCursorIds[] = {
  kCursorWait,
  kCursorProgress
};

}  // namespace

ImageCursors::ImageCursors() : cursor_set_(CURSOR_SET_NORMAL) {
}

ImageCursors::~ImageCursors() {
}

float ImageCursors::GetScale() const {
  if (!cursor_loader_) {
    NOTREACHED();
    // Returning default on release build as it's not serious enough to crash
    // even if this ever happens.
    return 1.0f;
  }
  return cursor_loader_->scale();
}

display::Display::Rotation ImageCursors::GetRotation() const {
  if (!cursor_loader_) {
    NOTREACHED();
    // Returning default on release build as it's not serious enough to crash
    // even if this ever happens.
    return display::Display::ROTATE_0;
  }
  return cursor_loader_->rotation();
}

bool ImageCursors::SetDisplay(const display::Display& display,
                              float scale_factor) {
  if (!cursor_loader_) {
    cursor_loader_.reset(CursorLoader::Create());
  } else if (cursor_loader_->rotation() == display.rotation() &&
             cursor_loader_->scale() == scale_factor) {
    return false;
  }

  cursor_loader_->set_rotation(display.rotation());
  cursor_loader_->set_scale(scale_factor);
  ReloadCursors();
  return true;
}

void ImageCursors::ReloadCursors() {
  float device_scale_factor = cursor_loader_->scale();

  cursor_loader_->UnloadAll();

  for (size_t i = 0; i < arraysize(kImageCursorIds); ++i) {
    int resource_id = -1;
    gfx::Point hot_point;
    bool success = GetCursorDataFor(cursor_set_,
                                    kImageCursorIds[i],
                                    device_scale_factor,
                                    &resource_id,
                                    &hot_point);
    DCHECK(success);
    cursor_loader_->LoadImageCursor(kImageCursorIds[i], resource_id, hot_point);
  }
  for (size_t i = 0; i < arraysize(kAnimatedCursorIds); ++i) {
    int resource_id = -1;
    gfx::Point hot_point;
    bool success = GetAnimatedCursorDataFor(cursor_set_,
                                            kAnimatedCursorIds[i],
                                            device_scale_factor,
                                            &resource_id,
                                            &hot_point);
    DCHECK(success);
    cursor_loader_->LoadAnimatedCursor(kAnimatedCursorIds[i],
                                       resource_id,
                                       hot_point,
                                       kAnimatedCursorFrameDelayMs);
  }
}

void ImageCursors::SetCursorSet(CursorSetType cursor_set) {
  if (cursor_set_ == cursor_set)
    return;

  cursor_set_ = cursor_set;

  if (cursor_loader_.get())
    ReloadCursors();
}

void ImageCursors::SetPlatformCursor(gfx::NativeCursor* cursor) {
  cursor_loader_->SetPlatformCursor(cursor);
}

}  // namespace ui
