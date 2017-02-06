// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_VIEW_STATE_H_
#define UI_ACCESSIBILITY_AX_VIEW_STATE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

////////////////////////////////////////////////////////////////////////////////
//
// AXViewState
//
//   A cross-platform struct for storing the core accessibility information
//   that should be provided about any UI view to assistive technology (AT).
//
////////////////////////////////////////////////////////////////////////////////
struct AX_EXPORT AXViewState {
 public:
  AXViewState();
  ~AXViewState();

  // Helper to check whether |state_flag| is set in the given |state|.
  static bool IsFlagSet(uint32_t state, ui::AXState state_flag);

  // Set or check bits in |state_|.
  void AddStateFlag(ui::AXState state_flag);
  bool HasStateFlag(ui::AXState state_flag) const;

  // The view's state, a bitmask containing fields such as checked
  // (for a checkbox) and protected (for a password text box). This "state"
  // should not be confused with the class's name.
  uint32_t state() { return state_; }

  // The view's role, like button or list box.
  AXRole role;

  // The view's name / label.
  base::string16 name;

  // The view's value, for example the text content.
  base::string16 value;

  // The name of the default action if the user clicks on this view.
  base::string16 default_action;

  // The keyboard shortcut to activate this view, if any.
  base::string16 keyboard_shortcut;

  // The view's placeholder value, used only for views with editable text.
  base::string16 placeholder;

  // The selection start and end. Only applies to views with text content,
  // such as a text box or combo box; start and end should be -1 otherwise.
  int selection_start;
  int selection_end;

  // The selected item's index and the count of the number of items.
  // Only applies to views with multiple choices like a listbox; both
  // index and count should be -1 otherwise.
  int index;
  int count;

  // An optional callback that can be used by accessibility clients to
  // set the string value of this view. This only applies to roles where
  // setting the value makes sense, like a text box. Not often used by
  // screen readers, but often used by automation software to script
  // things like logging into portals or filling forms.
  //
  // This callback is only valid for the lifetime of the view, and should
  // be a safe no-op if the view is deleted. Typically, accessible views
  // should use a WeakPtr when binding the callback.
  base::Callback<void(const base::string16&)> set_value_callback;

 private:
  uint32_t state_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_VIEW_STATE_H_
