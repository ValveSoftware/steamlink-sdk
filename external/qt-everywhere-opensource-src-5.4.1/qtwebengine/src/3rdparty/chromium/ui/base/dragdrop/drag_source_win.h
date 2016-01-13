// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_DRAG_SOURCE_WIN_H_
#define UI_BASE_DRAGDROP_DRAG_SOURCE_WIN_H_

#include <objidl.h>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "ui/base/ui_base_export.h"

namespace ui {

// A base IDropSource implementation. Handles notifications sent by an active
// drag-drop operation as the user mouses over other drop targets on their
// system. This object tells Windows whether or not the drag should continue,
// and supplies the appropriate cursors.
class UI_BASE_EXPORT DragSourceWin
    : public IDropSource,
      public base::RefCountedThreadSafe<DragSourceWin> {
 public:
  DragSourceWin();
  virtual ~DragSourceWin() {}

  // Stop the drag operation at the next chance we get.  This doesn't
  // synchronously stop the drag (since Windows is controlling that),
  // but lets us tell Windows to cancel the drag the next chance we get.
  void CancelDrag() {
    cancel_drag_ = true;
  }

  // IDropSource implementation:
  HRESULT __stdcall QueryContinueDrag(BOOL escape_pressed, DWORD key_state);
  HRESULT __stdcall GiveFeedback(DWORD effect);

  // IUnknown implementation:
  HRESULT __stdcall QueryInterface(const IID& iid, void** object);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();

 protected:
  virtual void OnDragSourceCancel() {}
  virtual void OnDragSourceDrop() {}
  virtual void OnDragSourceMove() {}

 private:
  // Set to true if we want to cancel the drag operation.
  bool cancel_drag_;

  DISALLOW_COPY_AND_ASSIGN(DragSourceWin);
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_DRAG_SOURCE_WIN_H_
