// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_LOADER_WIN_H_
#define UI_BASE_CURSOR_CURSOR_LOADER_WIN_H_

#include "base/compiler_specific.h"
#include "ui/base/cursor/cursor_loader.h"

namespace ui {

class UI_BASE_EXPORT CursorLoaderWin : public CursorLoader {
 public:
  CursorLoaderWin();
  virtual ~CursorLoaderWin();

  // Overridden from CursorLoader:
  virtual void LoadImageCursor(int id,
                               int resource_id,
                               const gfx::Point& hot) OVERRIDE;
  virtual void LoadAnimatedCursor(int id,
                                  int resource_id,
                                  const gfx::Point& hot,
                                  int frame_delay_ms) OVERRIDE;
  virtual void UnloadAll() OVERRIDE;
  virtual void SetPlatformCursor(gfx::NativeCursor* cursor) OVERRIDE;

  // Used to pass the cursor resource module name to the cursor loader. This is
  // typically used to load non system cursors.
  static void SetCursorResourceModule(const base::string16& module_name);

 private:
  DISALLOW_COPY_AND_ASSIGN(CursorLoaderWin);
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOADER_WIN_H_
