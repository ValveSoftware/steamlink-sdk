// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/ime/input_method_base.h"

#include "base/logging.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

InputMethodBase::InputMethodBase() : delegate_(NULL), widget_(NULL) {}

InputMethodBase::~InputMethodBase() {
  DetachFromWidget();
}

void InputMethodBase::SetDelegate(internal::InputMethodDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void InputMethodBase::Init(Widget* widget) {
  DCHECK(widget);
  DCHECK(widget->GetFocusManager());
  DCHECK(!widget_) << "The input method is already initialized.";

  widget_ = widget;
  // Alert the InputMethod of the Widget's currently focused view.
  View* focused = widget->GetFocusManager()->GetFocusedView();
  if (focused)
    OnWillChangeFocus(NULL, focused);
  widget->GetFocusManager()->AddFocusChangeListener(this);
}

views::View* InputMethodBase::GetFocusedView() const {
  return widget_ ? widget_->GetFocusManager()->GetFocusedView() : NULL;
}

void InputMethodBase::OnTextInputTypeChanged(View* view) {}

ui::TextInputClient* InputMethodBase::GetTextInputClient() const {
  return (widget_ && widget_->IsActive() && GetFocusedView()) ?
      GetFocusedView()->GetTextInputClient() : NULL;
}

ui::TextInputType InputMethodBase::GetTextInputType() const {
  ui::TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputType() : ui::TEXT_INPUT_TYPE_NONE;
}

bool InputMethodBase::IsMock() const {
  return false;
}

void InputMethodBase::OnWillChangeFocus(View* focused_before, View* focused) {}

void InputMethodBase::OnDidChangeFocus(View* focused_before, View* focused) {}

bool InputMethodBase::IsViewFocused(View* view) const {
  return widget_ && widget_->IsActive() && view && GetFocusedView() == view;
}

bool InputMethodBase::IsTextInputTypeNone() const {
  return GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE;
}

void InputMethodBase::OnInputMethodChanged() const {
  ui::TextInputClient* client = GetTextInputClient();
  if (client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE)
    client->OnInputMethodChanged();
}

void InputMethodBase::DispatchKeyEventPostIME(const ui::KeyEvent& key) const {
  if (delegate_)
    delegate_->DispatchKeyEventPostIME(key);
}

bool InputMethodBase::GetCaretBoundsInWidget(gfx::Rect* rect) const {
  DCHECK(rect);
  ui::TextInputClient* client = GetTextInputClient();
  if (!client || client->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return false;

  gfx::Rect caret_bounds = client->GetCaretBounds();
  gfx::Point caret_origin = caret_bounds.origin();
  View::ConvertPointFromScreen(GetFocusedView(), &caret_origin);
  caret_bounds.set_origin(caret_origin);
  *rect = GetFocusedView()->ConvertRectToWidget(caret_bounds);

  // Convert coordinates if the focused view is inside a child Widget.
  if (GetFocusedView()->GetWidget() != widget_)
    return Widget::ConvertRect(GetFocusedView()->GetWidget(), widget_, rect);
  return true;
}

void InputMethodBase::DetachFromWidget() {
  if (!widget_)
    return;

  widget_->GetFocusManager()->RemoveFocusChangeListener(this);
  widget_ = NULL;
}

}  // namespace views
