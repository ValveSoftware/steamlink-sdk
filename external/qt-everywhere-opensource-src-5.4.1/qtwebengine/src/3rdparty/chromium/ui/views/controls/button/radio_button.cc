// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/radio_button.h"

#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/widget.h"

namespace views {

// static
const char RadioButton::kViewClassName[] = "RadioButton";

RadioButton::RadioButton(const base::string16& label, int group_id)
    : Checkbox(label) {
  SetGroup(group_id);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // Unchecked/Unfocused images.
  SetCustomImage(false, false, STATE_NORMAL,
                 *rb.GetImageSkiaNamed(IDR_RADIO));
  SetCustomImage(false, false, STATE_HOVERED,
                 *rb.GetImageSkiaNamed(IDR_RADIO_HOVER));
  SetCustomImage(false, false, STATE_PRESSED,
                 *rb.GetImageSkiaNamed(IDR_RADIO_PRESSED));
  SetCustomImage(false, false, STATE_DISABLED,
                 *rb.GetImageSkiaNamed(IDR_RADIO_DISABLED));

  // Checked/Unfocused images.
  SetCustomImage(true, false, STATE_NORMAL,
                 *rb.GetImageSkiaNamed(IDR_RADIO_CHECKED));
  SetCustomImage(true, false, STATE_HOVERED,
                 *rb.GetImageSkiaNamed(IDR_RADIO_CHECKED_HOVER));
  SetCustomImage(true, false, STATE_PRESSED,
                 *rb.GetImageSkiaNamed(IDR_RADIO_CHECKED_PRESSED));
  SetCustomImage(true, false, STATE_DISABLED,
                 *rb.GetImageSkiaNamed(IDR_RADIO_CHECKED_DISABLED));

  // Unchecked/Focused images.
  SetCustomImage(false, true, STATE_NORMAL,
                 *rb.GetImageSkiaNamed(IDR_RADIO_FOCUSED));
  SetCustomImage(false, true, STATE_HOVERED,
                 *rb.GetImageSkiaNamed(IDR_RADIO_FOCUSED_HOVER));
  SetCustomImage(false, true, STATE_PRESSED,
                 *rb.GetImageSkiaNamed(IDR_RADIO_FOCUSED_PRESSED));

  // Checked/Focused images.
  SetCustomImage(true, true, STATE_NORMAL,
                 *rb.GetImageSkiaNamed(IDR_RADIO_FOCUSED_CHECKED));
  SetCustomImage(true, true, STATE_HOVERED,
                 *rb.GetImageSkiaNamed(IDR_RADIO_FOCUSED_CHECKED_HOVER));
  SetCustomImage(true, true, STATE_PRESSED,
                 *rb.GetImageSkiaNamed(IDR_RADIO_FOCUSED_CHECKED_PRESSED));
}

RadioButton::~RadioButton() {
}

void RadioButton::SetChecked(bool checked) {
  if (checked == RadioButton::checked())
    return;
  if (checked) {
    // We can't just get the root view here because sometimes the radio
    // button isn't attached to a root view (e.g., if it's part of a tab page
    // that is currently not active).
    View* container = parent();
    while (container && container->parent())
      container = container->parent();
    if (container) {
      Views other;
      container->GetViewsInGroup(GetGroup(), &other);
      for (Views::iterator i(other.begin()); i != other.end(); ++i) {
        if (*i != this) {
          if (strcmp((*i)->GetClassName(), kViewClassName)) {
            NOTREACHED() << "radio-button-nt has same group as other non "
                            "radio-button-nt views.";
            continue;
          }
          RadioButton* peer = static_cast<RadioButton*>(*i);
          peer->SetChecked(false);
        }
      }
    }
  }
  Checkbox::SetChecked(checked);
}

const char* RadioButton::GetClassName() const {
  return kViewClassName;
}

void RadioButton::GetAccessibleState(ui::AXViewState* state) {
  Checkbox::GetAccessibleState(state);
  state->role = ui::AX_ROLE_RADIO_BUTTON;
}

View* RadioButton::GetSelectedViewForGroup(int group) {
  Views views;
  GetWidget()->GetRootView()->GetViewsInGroup(group, &views);
  if (views.empty())
    return NULL;

  for (Views::const_iterator i(views.begin()); i != views.end(); ++i) {
    // REVIEW: why don't we check the runtime type like is done above?
    RadioButton* radio_button = static_cast<RadioButton*>(*i);
    if (radio_button->checked())
      return radio_button;
  }
  return NULL;
}

bool RadioButton::IsGroupFocusTraversable() const {
  // When focusing a radio button with tab/shift+tab, only the selected button
  // from the group should be focused.
  return false;
}

void RadioButton::OnFocus() {
  Checkbox::OnFocus();
  SetChecked(true);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), 0, 0);
  LabelButton::NotifyClick(event);
}

void RadioButton::NotifyClick(const ui::Event& event) {
  // Set the checked state to true only if we are unchecked, since we can't
  // be toggled on and off like a checkbox.
  if (!checked())
    SetChecked(true);
  RequestFocus();
  LabelButton::NotifyClick(event);
}

ui::NativeTheme::Part RadioButton::GetThemePart() const {
  return ui::NativeTheme::kRadio;
}

}  // namespace views
