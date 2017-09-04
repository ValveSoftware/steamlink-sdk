// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_LOADER_X11_H_
#define UI_BASE_CURSOR_CURSOR_LOADER_X11_H_

#include <X11/Xcursor/Xcursor.h>
#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/base/ui_base_export.h"
#include "ui/base/x/x11_util.h"

namespace ui {

class UI_BASE_EXPORT CursorLoaderX11 : public CursorLoader {
 public:
  CursorLoaderX11();
  ~CursorLoaderX11() override;

  // Overridden from CursorLoader:
  void LoadImageCursor(int id, int resource_id, const gfx::Point& hot) override;
  void LoadAnimatedCursor(int id,
                          int resource_id,
                          const gfx::Point& hot,
                          int frame_delay_ms) override;
  void UnloadAll() override;
  void SetPlatformCursor(gfx::NativeCursor* cursor) override;

  const XcursorImage* GetXcursorImageForTest(int id);

 private:
  // Returns true if we have an image resource loaded for the |native_cursor|.
  bool IsImageCursor(gfx::NativeCursor native_cursor);

  // Gets the X Cursor corresponding to the |native_cursor|.
  ::Cursor ImageCursorFromNative(gfx::NativeCursor native_cursor);

  // A map to hold all image cursors. It maps the cursor ID to the X Cursor.
  typedef std::map<int, ::Cursor> ImageCursorMap;
  ImageCursorMap cursors_;

  // A map to hold all animated cursors. It maps the cursor ID to the pair of
  // the X Cursor and the corresponding XcursorImages. We need a pointer to the
  // images so that we can free them on destruction.
  typedef std::map<int, std::pair< ::Cursor, XcursorImages*> >
      AnimatedCursorMap;
  AnimatedCursorMap animated_cursors_;

  const XScopedCursor invisible_cursor_;

  DISALLOW_COPY_AND_ASSIGN(CursorLoaderX11);
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOADER_X11_H_
