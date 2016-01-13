// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/checkbox.h"

#include "grit/ui_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/painter.h"

namespace views {

// static
const char Checkbox::kViewClassName[] = "Checkbox";

Checkbox::Checkbox(const base::string16& label)
    : LabelButton(NULL, label),
      checked_(false) {
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  scoped_ptr<LabelButtonBorder> button_border(new LabelButtonBorder(style()));
  button_border->SetPainter(false, STATE_HOVERED, NULL);
  button_border->SetPainter(false, STATE_PRESSED, NULL);
  // Inset the trailing side by a couple pixels for the focus border.
  button_border->set_insets(gfx::Insets(0, 0, 0, 2));
  SetBorder(button_border.PassAs<Border>());
  SetFocusable(true);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // Unchecked/Unfocused images.
  SetCustomImage(false, false, STATE_NORMAL,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX));
  SetCustomImage(false, false, STATE_HOVERED,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX_HOVER));
  SetCustomImage(false, false, STATE_PRESSED,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX_PRESSED));
  SetCustomImage(false, false, STATE_DISABLED,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX_DISABLED));

  // Checked/Unfocused images.
  SetCustomImage(true, false, STATE_NORMAL,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX_CHECKED));
  SetCustomImage(true, false, STATE_HOVERED,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX_CHECKED_HOVER));
  SetCustomImage(true, false, STATE_PRESSED,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX_CHECKED_PRESSED));
  SetCustomImage(true, false, STATE_DISABLED,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX_CHECKED_DISABLED));

  // Unchecked/Focused images.
  SetCustomImage(false, true, STATE_NORMAL,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX_FOCUSED));
  SetCustomImage(false, true, STATE_HOVERED,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX_FOCUSED_HOVER));
  SetCustomImage(false, true, STATE_PRESSED,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX_FOCUSED_PRESSED));

  // Checked/Focused images.
  SetCustomImage(true, true, STATE_NORMAL,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX_FOCUSED_CHECKED));
  SetCustomImage(true, true, STATE_HOVERED,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX_FOCUSED_CHECKED_HOVER));
  SetCustomImage(true, true, STATE_PRESSED,
                 *rb.GetImageSkiaNamed(IDR_CHECKBOX_FOCUSED_CHECKED_PRESSED));

  // Limit the checkbox height to match the legacy appearance.
  const gfx::Size preferred_size(LabelButton::GetPreferredSize());
  set_min_size(gfx::Size(0, preferred_size.height() + 4));
}

Checkbox::~Checkbox() {
}

void Checkbox::SetChecked(bool checked) {
  checked_ = checked;
  UpdateImage();
}

void Checkbox::Layout() {
  LabelButton::Layout();

  // Construct a focus painter that only surrounds the label area.
  gfx::Rect rect = label()->GetMirroredBounds();
  rect.Inset(-2, 3);
  SetFocusPainter(Painter::CreateDashedFocusPainterWithInsets(
                      gfx::Insets(rect.y(), rect.x(),
                                  height() - rect.bottom(),
                                  width() - rect.right())));
}

const char* Checkbox::GetClassName() const {
  return kViewClassName;
}

void Checkbox::GetAccessibleState(ui::AXViewState* state) {
  LabelButton::GetAccessibleState(state);
  state->role = ui::AX_ROLE_CHECK_BOX;
  if (checked())
    state->AddStateFlag(ui::AX_STATE_CHECKED);
}

void Checkbox::OnFocus() {
  LabelButton::OnFocus();
  UpdateImage();
}

void Checkbox::OnBlur() {
  LabelButton::OnBlur();
  UpdateImage();
}

const gfx::ImageSkia& Checkbox::GetImage(ButtonState for_state) {
  const size_t checked_index = checked_ ? 1 : 0;
  const size_t focused_index = HasFocus() ? 1 : 0;
  if (for_state != STATE_NORMAL &&
      images_[checked_index][focused_index][for_state].isNull())
    return images_[checked_index][focused_index][STATE_NORMAL];
  return images_[checked_index][focused_index][for_state];
}

void Checkbox::SetCustomImage(bool checked,
                              bool focused,
                              ButtonState for_state,
                              const gfx::ImageSkia& image) {
  const size_t checked_index = checked ? 1 : 0;
  const size_t focused_index = focused ? 1 : 0;
  images_[checked_index][focused_index][for_state] = image;
  UpdateImage();
}

void Checkbox::NotifyClick(const ui::Event& event) {
  SetChecked(!checked());
  LabelButton::NotifyClick(event);
}

ui::NativeTheme::Part Checkbox::GetThemePart() const {
  return ui::NativeTheme::kCheckbox;
}

void Checkbox::GetExtraParams(ui::NativeTheme::ExtraParams* params) const {
  LabelButton::GetExtraParams(params);
  params->button.checked = checked_;
}

}  // namespace views
