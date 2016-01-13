// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_LOADER_OZONE_H_
#define UI_BASE_CURSOR_CURSOR_LOADER_OZONE_H_

#include <map>

#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_loader.h"

namespace ui {

class UI_BASE_EXPORT CursorLoaderOzone : public CursorLoader {
 public:
  CursorLoaderOzone();
  virtual ~CursorLoaderOzone();

  // CursorLoader overrides:
  virtual void LoadImageCursor(int id,
                               int resource_id,
                               const gfx::Point& hot) OVERRIDE;
  virtual void LoadAnimatedCursor(int id,
                                  int resource_id,
                                  const gfx::Point& hot,
                                  int frame_delay_ms) OVERRIDE;
  virtual void UnloadAll() OVERRIDE;
  virtual void SetPlatformCursor(gfx::NativeCursor* cursor) OVERRIDE;

 private:
  // Pointers are owned by ResourceBundle and must not be freed here.
  typedef std::map<int, PlatformCursor> ImageCursorMap;
  ImageCursorMap cursors_;

  DISALLOW_COPY_AND_ASSIGN(CursorLoaderOzone);
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOADER_OZONE_H_
