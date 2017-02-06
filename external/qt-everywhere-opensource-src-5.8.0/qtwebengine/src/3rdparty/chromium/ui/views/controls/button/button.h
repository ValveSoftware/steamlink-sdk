// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_BUTTON_H_

#include "base/macros.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/ink_drop_host_view.h"

namespace views {

class Button;
class Event;

// An interface implemented by an object to let it know that a button was
// pressed.
class VIEWS_EXPORT ButtonListener {
 public:
  virtual void ButtonPressed(Button* sender, const ui::Event& event) = 0;

 protected:
  virtual ~ButtonListener() {}
};

// A View representing a button. Depending on the specific type, the button
// could be implemented by a native control or custom rendered.
class VIEWS_EXPORT Button : public InkDropHostView {
 public:
  ~Button() override;

  // Button states for various button sub-types.
  enum ButtonState {
    STATE_NORMAL = 0,
    STATE_HOVERED,
    STATE_PRESSED,
    STATE_DISABLED,
    STATE_COUNT,
  };

  // Button styles with associated images and border painters.
  // TODO(msw): Add Menu, ComboBox, etc.
  enum ButtonStyle {
    STYLE_BUTTON = 0,
    STYLE_TEXTBUTTON,
    STYLE_COUNT,
  };

  static ButtonState GetButtonStateFrom(ui::NativeTheme::State state);

  // Make the button focusable as per the platform.
  void SetFocusForPlatform();

  void SetTooltipText(const base::string16& tooltip_text);

  int tag() const { return tag_; }
  void set_tag(int tag) { tag_ = tag; }

  void SetAccessibleName(const base::string16& name);

  // Overridden from View:
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;
  void GetAccessibleState(ui::AXViewState* state) override;

 protected:
  // Construct the Button with a Listener. The listener can be NULL. This can be
  // true of buttons that don't have a listener - e.g. menubuttons where there's
  // no default action and checkboxes.
  explicit Button(ButtonListener* listener);

  // Cause the button to notify the listener that a click occurred.
  virtual void NotifyClick(const ui::Event& event);

  // Called when a button gets released without triggering an action.
  // Note: This is only wired up for mouse button events and not gesture
  // events.
  virtual void OnClickCanceled(const ui::Event& event);

  // The button's listener. Notified when clicked.
  ButtonListener* listener_;

 private:
  // The text shown in a tooltip.
  base::string16 tooltip_text_;

  // Accessibility data.
  base::string16 accessible_name_;

  // The id tag associated with this button. Used to disambiguate buttons in
  // the ButtonListener implementation.
  int tag_;

  DISALLOW_COPY_AND_ASSIGN(Button);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_BUTTON_H_
