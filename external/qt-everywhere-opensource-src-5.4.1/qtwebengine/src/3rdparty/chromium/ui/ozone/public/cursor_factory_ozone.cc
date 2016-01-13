// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/cursor_factory_ozone.h"

#include "base/logging.h"

namespace ui {

// static
CursorFactoryOzone* CursorFactoryOzone::impl_ = NULL;

CursorFactoryOzone::CursorFactoryOzone() {
  CHECK(!impl_) << "There should only be a single CursorFactoryOzone.";
  impl_ = this;
}

CursorFactoryOzone::~CursorFactoryOzone() {
  CHECK_EQ(impl_, this);
  impl_ = NULL;
}

CursorFactoryOzone* CursorFactoryOzone::GetInstance() {
  CHECK(impl_) << "No CursorFactoryOzone implementation set.";
  return impl_;
}

PlatformCursor CursorFactoryOzone::GetDefaultCursor(int type) {
  NOTIMPLEMENTED();
  return NULL;
}

PlatformCursor CursorFactoryOzone::CreateImageCursor(
    const SkBitmap& bitmap,
    const gfx::Point& hotspot) {
  NOTIMPLEMENTED();
  return NULL;
}

void CursorFactoryOzone::RefImageCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void CursorFactoryOzone::UnrefImageCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void CursorFactoryOzone::SetCursor(gfx::AcceleratedWidget widget,
                                   PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

gfx::AcceleratedWidget CursorFactoryOzone::GetCursorWindow() {
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace ui
