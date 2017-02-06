// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_cursor_loader_updater_auralinux.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/display/display.h"

namespace views {
namespace {

// Cursors that we need to supply our own image resources for. This was
// generated from webcursor_gtk.cc; any cursor that either was NOTIMPLEMENTED()
// or already was implemented with a pixmap is on this list.
const int kImageCursorIds[] = {
  ui::kCursorNorthEastSouthWestResize,
  ui::kCursorNorthWestSouthEastResize,
  ui::kCursorVerticalText,
  ui::kCursorCell,
  ui::kCursorContextMenu,
  ui::kCursorAlias,
  ui::kCursorNoDrop,
  ui::kCursorCopy,
  ui::kCursorNotAllowed,
  ui::kCursorZoomIn,
  ui::kCursorZoomOut,
  ui::kCursorGrab,
  ui::kCursorGrabbing,
};

void LoadImageCursors(float device_scale_factor, ui::CursorLoader* loader) {
  int resource_id;
  gfx::Point point;
  for (size_t i = 0; i < arraysize(kImageCursorIds); ++i) {
    bool success = ui::GetCursorDataFor(
        ui::CURSOR_SET_NORMAL,  // Not support custom cursor set.
        kImageCursorIds[i],
        device_scale_factor,
        &resource_id,
        &point);
    DCHECK(success);
    loader->LoadImageCursor(kImageCursorIds[i], resource_id, point);
  }
}

}  // namespace

DesktopCursorLoaderUpdaterAuraLinux::DesktopCursorLoaderUpdaterAuraLinux() {}

DesktopCursorLoaderUpdaterAuraLinux::~DesktopCursorLoaderUpdaterAuraLinux() {}

void DesktopCursorLoaderUpdaterAuraLinux::OnCreate(
    float device_scale_factor,
    ui::CursorLoader* loader) {
  LoadImageCursors(device_scale_factor, loader);
}

void DesktopCursorLoaderUpdaterAuraLinux::OnDisplayUpdated(
    const display::Display& display,
    ui::CursorLoader* loader) {
  LoadImageCursors(display.device_scale_factor(), loader);
}

// static
std::unique_ptr<DesktopCursorLoaderUpdater>
DesktopCursorLoaderUpdater::Create() {
  return base::WrapUnique(new DesktopCursorLoaderUpdaterAuraLinux);
}

}  // namespace views
