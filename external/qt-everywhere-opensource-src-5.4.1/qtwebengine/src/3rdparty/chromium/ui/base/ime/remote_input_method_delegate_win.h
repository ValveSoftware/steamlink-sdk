// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_REMOTE_INPUT_METHOD_DELEGATE_WIN_H_
#define UI_BASE_IME_REMOTE_INPUT_METHOD_DELEGATE_WIN_H_

#include <vector>

#include "base/basictypes.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/rect.h"

namespace ui {
namespace internal {

// An interface implemented by the object to forward events that should be
// handled by the IME which is running in the remote metro_driver process.
class UI_BASE_EXPORT RemoteInputMethodDelegateWin {
 public:
  virtual ~RemoteInputMethodDelegateWin() {}

  // Notifies that composition should be canceled (if any).
  virtual void CancelComposition() = 0;

  // Notifies that properties of the focused TextInputClient is changed.
  // Note that an empty |input_scopes| represents that TextInputType is
  // TEXT_INPUT_TYPE_NONE.
  // Caveats: |input_scopes| is defined as std::vector<int32> rather than
  // std::vector<InputScope> because the wire format of IPC message
  // MetroViewerHostMsg_ImeTextInputClientUpdated uses std::vector<int32> to
  // avoid dependency on <InputScope.h> header.
  virtual void OnTextInputClientUpdated(
      const std::vector<int32>& input_scopes,
      const std::vector<gfx::Rect>& composition_character_bounds) = 0;
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_REMOTE_INPUT_METHOD_DELEGATE_WIN_H_
