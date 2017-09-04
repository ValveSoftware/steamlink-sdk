// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_IME_ARC_IME_BRIDGE_H_
#define COMPONENTS_ARC_IME_ARC_IME_BRIDGE_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/ime/text_input_type.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
struct CompositionText;
}  // namespace ui

namespace arc {

// This interface class encapsulates the detail of IME related IPC between
// Chromium and the ARC container.
class ArcImeBridge {
 public:
  virtual ~ArcImeBridge() {}

  // Received IPCs are deserialized and passed to this delegate.
  class Delegate {
   public:
    virtual void OnTextInputTypeChanged(ui::TextInputType type) = 0;
    virtual void OnCursorRectChanged(const gfx::Rect& rect) = 0;
    virtual void OnCancelComposition() = 0;
    virtual void ShowImeIfNeeded() = 0;
  };

  // Serializes and sends IME related requests through IPCs.
  virtual void SendSetCompositionText(
      const ui::CompositionText& composition) = 0;
  virtual void SendConfirmCompositionText() = 0;
  virtual void SendInsertText(const base::string16& text) = 0;
  virtual void SendOnKeyboardBoundsChanging(const gfx::Rect& new_bounds) = 0;
  virtual void SendExtendSelectionAndDelete(size_t before, size_t after) = 0;

 protected:
  ArcImeBridge() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcImeBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_IME_ARC_IME_BRIDGE_H_
